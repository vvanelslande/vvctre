// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include "common/common_types.h"
#include "core/custom_tex_cache.h"
#include "core/frontend/applets/mii_selector.h"
#include "core/frontend/applets/swkbd.h"
#include "core/loader/loader.h"
#include "core/memory.h"
#include "core/perf_stats.h"

class ARM_Interface;

namespace Frontend {
class EmuWindow;
} // namespace Frontend

namespace Memory {
class MemorySystem;
} // namespace Memory

namespace AudioCore {
class DspInterface;
} // namespace AudioCore

namespace Service {

namespace SM {
class ServiceManager;
} // namespace SM

namespace FS {
class ArchiveManager;
} // namespace FS

} // namespace Service

namespace Kernel {
class KernelSystem;
} // namespace Kernel

namespace Cheats {
class CheatEngine;
} // namespace Cheats

namespace Network {
class RoomMember;
} // namespace Network

class RendererBase;

namespace Core {

class Timing;

class System {
public:
    System();
    ~System();

    /**
     * Gets the instance of the System singleton class.
     * @returns Reference to the instance of the System singleton class.
     */
    static System& GetInstance() {
        return s_instance;
    }

    /// Enumeration representing the return values of the System Init, Load, and Run functions.
    enum class ResultStatus : u32 {
        Success,                    ///< Succeeded
        ErrorNotInitialized,        ///< Error trying to use core prior to initialization
        ErrorSystemMode,            ///< Error determining the system mode
        ErrorLoader_ErrorEncrypted, ///< Error loading the specified application due to encryption
        ErrorLoader_ErrorUnsupportedFormat, ///< Unsupported file format
        ErrorFileNotFound,                  ///< File not found
        ShutdownRequested,                  ///< Emulated program requested a system shutdown
        FatalError,                         ///< A fatal error
    };

    /**
     * Run the core CPU loop
     * This function runs the core for the specified number of CPU instructions before trying to
     * update hardware. NOTE: the number of instructions requested is not guaranteed to run, as this
     * will be interrupted preemptively if a hardware update is requested (e.g. on a thread switch).
     * @return Result status, indicating whethor or not the operation succeeded.
     */
    ResultStatus Run();

    /// Shutdown the emulated system.
    void Shutdown();

    /// Shutdown and then load again
    void Reset();

    /// Request reset of the system
    void RequestReset();

    /// Request shutdown of the system
    void RequestShutdown();

    void SetResetFilePath(const std::string filepath);

    /**
     * Load an executable application.
     * @param emu_window Reference to the host-system window used for video output and keyboard
     *                   input.
     * @param filepath String path to the executable application to load on the host file system.
     * @returns ResultStatus code, indicating if the operation succeeded.
     */
    ResultStatus Load(Frontend::EmuWindow& emu_window, const std::string& filepath);

    bool IsInitialized() const;
    void PrepareReschedule();

    ARM_Interface& GetRunningCore() {
        return *running_core;
    }

    ARM_Interface& GetCore(u32 core_id) {
        return *cpu_cores[core_id];
    }

    u32 GetNumCores() const {
        return cpu_cores.size();
    }

    void InvalidateCacheRange(u32 start_address, std::size_t length) {
        for (const auto& cpu : cpu_cores) {
            cpu->InvalidateCacheRange(start_address, length);
        }
    }

    /**
     * Gets a reference to the emulated DSP.
     * @returns A reference to the emulated DSP.
     */
    AudioCore::DspInterface& DSP() {
        return *dsp_core;
    }

    RendererBase& Renderer();

    /**
     * Gets a reference to the service manager.
     * @returns A reference to the service manager.
     */
    Service::SM::ServiceManager& ServiceManager();

    /**
     * Gets a const reference to the service manager.
     * @returns A const reference to the service manager.
     */
    const Service::SM::ServiceManager& ServiceManager() const;

    /// Gets a reference to the archive manager
    Service::FS::ArchiveManager& ArchiveManager();

    /// Gets a const reference to the archive manager
    const Service::FS::ArchiveManager& ArchiveManager() const;

    /// Gets a reference to the kernel
    Kernel::KernelSystem& Kernel();

    /// Gets a const reference to the kernel
    const Kernel::KernelSystem& Kernel() const;

    /// Gets a reference to the timing system
    Timing& CoreTiming();

    /// Gets a const reference to the timing system
    const Timing& CoreTiming() const;

    /// Gets a reference to the memory system
    Memory::MemorySystem& Memory();

    /// Gets a const reference to the memory system
    const Memory::MemorySystem& Memory() const;

    /// Gets a reference to the cheat engine
    Cheats::CheatEngine& CheatEngine();

    /// Gets a const reference to the cheat engine
    const Cheats::CheatEngine& CheatEngine() const;

    /// Gets a reference to the custom texture cache system
    Core::CustomTexCache& CustomTexCache();

    /// Gets a const reference to the custom texture cache system
    const Core::CustomTexCache& CustomTexCache() const;

    /// Gets a reference to the room member
    Network::RoomMember& RoomMember();

    /// Gets a reference to the AppLoader
    Loader::AppLoader& GetAppLoader() const;

    std::unique_ptr<PerfStats> perf_stats;
    FrameLimiter frame_limiter;

    void SetStatus(ResultStatus status) {
        this->status = status;
    }

    /// Frontend Applets
    void RegisterMiiSelector(std::shared_ptr<Frontend::MiiSelector> mii_selector);
    void RegisterSoftwareKeyboard(std::shared_ptr<Frontend::SoftwareKeyboard> swkbd);
    std::shared_ptr<Frontend::MiiSelector> GetMiiSelector() const {
        return registered_mii_selector;
    }
    std::shared_ptr<Frontend::SoftwareKeyboard> GetSoftwareKeyboard() const {
        return registered_swkbd;
    }

    void SetBeforeLoadingAfterFirstTime(std::function<void()> function);
    void SetEmulationStartingAfterFirstTime(std::function<void()> function);
    void SetOnLoadFailed(std::function<void(ResultStatus)> function);
    void SetPreloadCustomTexturesFunction(std::function<void()> function);
    void SetDiskShaderCacheCallback(std::function<void(bool, std::size_t, std::size_t)> function);
    void DiskShaderCacheCallback(bool loading, std::size_t current, std::size_t total);
    const bool IsOnLoadFailedSet() const;

private:
    /**
     * Initialize the emulated system.
     * @param emu_window Reference to the host-system window used for video output and keyboard
     *                   input.
     * @param system_mode The system mode.
     * @return ResultStatus code, indicating if the operation succeeded.
     */
    ResultStatus Init(Frontend::EmuWindow& emu_window, u32 system_mode);

    /// Reschedule the core emulation
    void Reschedule();

    /// AppLoader used to load the current executing application
    std::unique_ptr<Loader::AppLoader> app_loader;

    /// ARM11 CPU core
    std::vector<std::shared_ptr<ARM_Interface>> cpu_cores;
    ARM_Interface* running_core = nullptr;

    /// DSP core
    std::shared_ptr<AudioCore::DspInterface> dsp_core;

    /// When true, signals that a reschedule should happen
    bool reschedule_pending{};

    /// Service manager
    std::shared_ptr<Service::SM::ServiceManager> service_manager;

    /// Frontend applets
    std::shared_ptr<Frontend::MiiSelector> registered_mii_selector;
    std::shared_ptr<Frontend::SoftwareKeyboard> registered_swkbd;

    /// Cheats manager
    std::shared_ptr<Cheats::CheatEngine> cheat_engine;

    /// Custom texture cache system
    std::unique_ptr<Core::CustomTexCache> custom_tex_cache;

    std::unique_ptr<Service::FS::ArchiveManager> archive_manager;

    std::unique_ptr<Memory::MemorySystem> memory;
    std::unique_ptr<Kernel::KernelSystem> kernel;
    std::unique_ptr<Timing> timing;

    // Room member
    std::shared_ptr<Network::RoomMember> room_member;

    static System s_instance;

    bool powered_on = false;

    ResultStatus status = ResultStatus::Success;

    /// Saved variables for reset and application jump
    Frontend::EmuWindow* m_emu_window = nullptr;
    std::string m_filepath;

    std::atomic<bool> reset_requested{false};
    std::atomic<bool> shutdown_requested{false};

    std::function<void()> before_loading_after_first_time;
    std::function<void()> emulation_starting_after_first_time;
    std::function<void(ResultStatus)> on_load_failed;
    std::function<void()> preload_custom_textures_function;
    std::function<void(bool, std::size_t, std::size_t)> disk_shader_cache_callback;
};

} // namespace Core
