// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <regex>
#include <thread>
#include <vector>
#include "common/assert.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/logging/text_formatter.h"
#include "common/string_util.h"
#include "common/threadsafe_queue.h"

namespace Log {

class Impl {
public:
    static Impl& Instance() {
        static Impl instance;
        return instance;
    }

    Impl(Impl const&) = delete;
    const Impl& operator=(Impl const&) = delete;

    void PushEntry(const Class log_class, const Level level, const char* file,
                   const unsigned int line, const char* function, std::string message) {
        queue.Push(CreateEntry(log_class, level, file, line, function, std::move(message)));
    }

    void AddBackend(std::unique_ptr<Backend> backend) {
        std::lock_guard<std::mutex> lock(writing_mutex);

        backends.push_back(std::move(backend));
    }

    void RemoveBackend(std::string_view name) {
        std::lock_guard<std::mutex> lock(writing_mutex);

        backends.erase(std::remove_if(backends.begin(), backends.end(),
                                      [&name](const std::unique_ptr<Log::Backend>& backend) {
                                          return name == backend->GetName();
                                      }),
                       backends.end());
    }

    const Filter& GetGlobalFilter() const {
        return filter;
    }

    void SetGlobalFilter(const Filter& f) {
        filter = f;
    }

private:
    Impl() {
        backend_thread = std::thread([this] {
            Entry entry;

            for (;;) {
                entry = queue.PopWait();

                if (entry.final_entry) {
                    break;
                }

                std::lock_guard<std::mutex> lock(writing_mutex);

                for (std::unique_ptr<Backend>& backend : backends) {
                    backend->Write(entry);
                }
            }

            int logs_written = 0;

            while (logs_written++ < 100 && queue.Pop(entry)) {
                std::lock_guard<std::mutex> lock(writing_mutex);

                for (std::unique_ptr<Backend>& backend : backends) {
                    backend->Write(entry);
                }
            }
        });
    }

    ~Impl() {
        Entry entry;
        entry.final_entry = true;
        queue.Push(entry);
        backend_thread.join();
    }

    Entry CreateEntry(const Class log_class, const Level level, const char* file,
                      const unsigned int line, const char* function, std::string message) const {
        Entry entry;
        entry.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - time_origin);
        entry.log_class = log_class;
        entry.level = level;
        entry.file = file;
        entry.line = line;
        entry.function = function;
        entry.message = std::move(message);
        return entry;
    }

    std::mutex writing_mutex;
    std::thread backend_thread;
    std::vector<std::unique_ptr<Backend>> backends;
    Common::MPSCQueue<Log::Entry> queue;
    Filter filter;
    std::chrono::steady_clock::time_point time_origin = std::chrono::steady_clock::now();
};

void ColorConsoleBackend::Write(const Entry& entry) {
    PrintColoredMessage(entry);
}

/// Macro listing all log classes. Code should define CLS and SUB as desired before invoking this.
#define ALL_LOG_CLASSES()                                                                          \
    CLS(Log)                                                                                       \
    CLS(Common)                                                                                    \
    SUB(Common, Filesystem)                                                                        \
    SUB(Common, Memory)                                                                            \
    CLS(Core)                                                                                      \
    SUB(Core, ARM11)                                                                               \
    SUB(Core, Timing)                                                                              \
    SUB(Core, Cheats)                                                                              \
    CLS(Settings)                                                                                  \
    CLS(Debug)                                                                                     \
    SUB(Debug, Emulated)                                                                           \
    CLS(Kernel)                                                                                    \
    SUB(Kernel, SVC)                                                                               \
    CLS(Applet)                                                                                    \
    SUB(Applet, SWKBD)                                                                             \
    CLS(Service)                                                                                   \
    SUB(Service, SRV)                                                                              \
    SUB(Service, FRD)                                                                              \
    SUB(Service, FS)                                                                               \
    SUB(Service, ERR)                                                                              \
    SUB(Service, APT)                                                                              \
    SUB(Service, BOSS)                                                                             \
    SUB(Service, GSP)                                                                              \
    SUB(Service, AC)                                                                               \
    SUB(Service, AM)                                                                               \
    SUB(Service, PTM)                                                                              \
    SUB(Service, LDR)                                                                              \
    SUB(Service, MIC)                                                                              \
    SUB(Service, NDM)                                                                              \
    SUB(Service, NFC)                                                                              \
    SUB(Service, NIM)                                                                              \
    SUB(Service, NS)                                                                               \
    SUB(Service, NWM)                                                                              \
    SUB(Service, CAM)                                                                              \
    SUB(Service, CECD)                                                                             \
    SUB(Service, CFG)                                                                              \
    SUB(Service, CSND)                                                                             \
    SUB(Service, DSP)                                                                              \
    SUB(Service, DLP)                                                                              \
    SUB(Service, HID)                                                                              \
    SUB(Service, HTTP)                                                                             \
    SUB(Service, SOC)                                                                              \
    SUB(Service, IR)                                                                               \
    SUB(Service, Y2R)                                                                              \
    SUB(Service, PS)                                                                               \
    CLS(HW)                                                                                        \
    SUB(HW, Memory)                                                                                \
    SUB(HW, LCD)                                                                                   \
    SUB(HW, GPU)                                                                                   \
    SUB(HW, AES)                                                                                   \
    CLS(Frontend)                                                                                  \
    CLS(Render)                                                                                    \
    SUB(Render, Software)                                                                          \
    SUB(Render, OpenGL)                                                                            \
    CLS(Audio)                                                                                     \
    SUB(Audio, DSP)                                                                                \
    SUB(Audio, Sink)                                                                               \
    CLS(Input)                                                                                     \
    CLS(Movie)                                                                                     \
    CLS(Loader)                                                                                    \
    CLS(Plugins)                                                                                   \
    CLS(Network)

// GetClassName is a macro defined by Windows.h, grrr...
const char* GetLogClassName(Class log_class) {
    switch (log_class) {
#define CLS(x)                                                                                     \
    case Class::x:                                                                                 \
        return #x;
#define SUB(x, y)                                                                                  \
    case Class::x##_##y:                                                                           \
        return #x "." #y;
        ALL_LOG_CLASSES()
#undef CLS
#undef SUB
    case Class::Count:
        break;
    }
    UNREACHABLE();
    return "Invalid";
}

const char* GetLevelName(Level level) {
#define LVL(x)                                                                                     \
    case Level::x:                                                                                 \
        return #x
    switch (level) {
        LVL(Trace);
        LVL(Debug);
        LVL(Info);
        LVL(Warning);
        LVL(Error);
        LVL(Critical);
    case Level::Count:
        break;
    }
#undef LVL
    UNREACHABLE();
    return "Invalid";
}

void SetGlobalFilter(const Filter& filter) {
    Impl::Instance().SetGlobalFilter(filter);
}

void AddBackend(std::unique_ptr<Backend> backend) {
    Impl::Instance().AddBackend(std::move(backend));
}

void RemoveBackend(std::string_view name) {
    Impl::Instance().RemoveBackend(name);
}

void FmtLogMessageImpl(const Class log_class, const Level level, const char* file,
                       const unsigned int line, const char* function, const char* format,
                       const fmt::format_args& args) {
    Impl& instance = Impl::Instance();
    const Filter& filter = instance.GetGlobalFilter();

    if (!filter.CheckMessage(log_class, level)) {
        return;
    }

    instance.PushEntry(log_class, level, file, line, function, fmt::vformat(format, args));
}

} // namespace Log
