// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/format.h>
#include <fstream>
#include <functional>
#include "common/file_util.h"
#include "common/string_util.h"
#include "core/cheats/cheat.h"
#include "core/cheats/engine.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/process.h"
#include "core/hw/gpu.h"

namespace Cheats {

// Luma3DS uses this interval for applying cheats, so to keep consistent behavior
// we use the same value
constexpr u64 run_interval_ticks = 50'000'000;

Engine::Engine(Core::System& system_) : system(system_) {
    Load();

    event = system.CoreTiming().RegisterEvent(
        "CheatCore::run_event",
        [this](u64 thread_id, s64 cycle_late) { RunCallback(thread_id, cycle_late); });

    system.CoreTiming().ScheduleEvent(run_interval_ticks, event);
}

Engine::~Engine() {
    system.CoreTiming().UnscheduleEvent(event, 0);
}

std::vector<std::shared_ptr<Cheat>> Engine::GetCheats() const {
    std::shared_lock<std::shared_mutex> lock(cheats_mutex);
    return cheats;
}

void Engine::Add(const std::shared_ptr<Cheat>& cheat) {
    std::unique_lock<std::shared_mutex> lock(cheats_mutex);
    cheats.push_back(cheat);
}

void Engine::Remove(const int index) {
    std::unique_lock<std::shared_mutex> lock(cheats_mutex);

    if (index < 0 || index >= cheats.size()) {
        LOG_ERROR(Core_Cheats, "Invalid index {}", index);
        return;
    }

    cheats.erase(cheats.begin() + index);
}

void Engine::Update(const int index, const std::shared_ptr<Cheat>& new_cheat) {
    std::unique_lock<std::shared_mutex> lock(cheats_mutex);

    if (index < 0 || index >= cheats.size()) {
        LOG_ERROR(Core_Cheats, "Invalid index {}", index);
        return;
    }

    cheats[index] = new_cheat;
}

void Engine::Load() {
    std::vector<std::unique_ptr<Cheat>> new_cheats;
    std::string comments;
    std::vector<Cheat::Line> lines;
    std::string name;
    bool enabled = false;
    std::string line;
    std::ifstream file;
    const std::string cheats_dir = FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir);
    const std::string filepath = fmt::format(
        "{}{:016X}.txt", cheats_dir, system.Kernel().GetCurrentProcess()->codeset->program_id);

    if (!FileUtil::IsDirectory(cheats_dir)) {
        FileUtil::CreateDir(cheats_dir);
    }

    if (!FileUtil::Exists(filepath)) {
        return;
    }

    OpenFStream(file, filepath, std::ifstream::in);

    if (!file) {
        return;
    }

    while (std::getline(file, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\0'), line.end());
        line = Common::StripSpaces(line); // Remove spaces at front and end

        if (line.length() >= 2 && line.front() == '[') {
            if (!lines.empty()) {
                new_cheats.push_back(std::make_unique<Cheat>(name, lines, comments, enabled));
                enabled = false;
            }

            name = line.substr(1, line.length() - 2);
            lines.clear();
            comments.erase();
        } else if (!line.empty() && line.front() == '*') {
            if (line == "*vvctre_enabled") {
                enabled = true;
            } else {
                comments += line.substr(1, line.length() - 1) + '\n';
            }
        } else if (!line.empty()) {
            lines.emplace_back(std::move(line));
        }
    }

    if (!lines.empty()) {
        new_cheats.push_back(std::make_unique<Cheat>(name, lines, comments, enabled));
    }

    std::unique_lock<std::shared_mutex> lock(cheats_mutex);
    cheats.clear();
    std::move(new_cheats.begin(), new_cheats.end(), std::back_inserter(cheats));
}

void Engine::Save() const {
    const std::string cheats_dir = FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir);

    const std::string filepath = fmt::format(
        "{}{:016X}.txt", cheats_dir, system.Kernel().GetCurrentProcess()->codeset->program_id);

    if (!FileUtil::IsDirectory(cheats_dir)) {
        FileUtil::CreateDir(cheats_dir);
    }

    std::ofstream file;
    OpenFStream(file, filepath, std::ofstream::out);

    const std::vector<std::shared_ptr<Cheat>> cheats = GetCheats();

    for (const std::shared_ptr<Cheat>& cheat : cheats) {
        file << cheat->ToString();
    }
}

void Engine::RunCallback([[maybe_unused]] std::uintptr_t user_data, int cycles_late) {
    {
        std::shared_lock<std::shared_mutex> lock(cheats_mutex);

        for (std::shared_ptr<Cheat>& cheat : cheats) {
            if (cheat->IsEnabled()) {
                cheat->Execute(system);
            }
        }
    }

    system.CoreTiming().ScheduleEvent(run_interval_ticks - cycles_late, event);
}

} // namespace Cheats
