// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <utility>
#include "audio_core/dsp_interface.h"
#include "audio_core/hle/hle.h"
#include "audio_core/lle/lle.h"
#include "common/logging/log.h"
#include "common/texture.h"
#include "core/arm/arm_interface.h"
#include "enet/enet.h"
#ifdef ARCHITECTURE_x86_64
#include <dynarmic/exclusive_monitor.h>
#include "core/arm/dynarmic/arm_dynarmic.h"
#endif
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/cheats/cheats.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/custom_tex_cache.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/service/apt/applet_manager.h"
#include "core/hle/service/apt/apt.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hw/hw.h"
#include "core/loader/loader.h"
#include "core/movie.h"
#include "core/settings.h"
#include "network/room_member.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Core {

System System::s_instance;

System::System() {
    if (enet_initialize() != 0) {
        return;
    }

    room_member = std::make_shared<Network::RoomMember>();
}

System::~System() {
    if (room_member->IsConnected()) {
        room_member->Leave();
    }

    enet_deinitialize();
}

System::ResultStatus System::Run() {
    bool step = false;

    status = ResultStatus::Success;

    if (!IsInitialized()) {
        return ResultStatus::ErrorNotInitialized;
    }

    if (GDBStub::IsServerEnabled()) {
        if (running_core != nullptr) {
            if (Kernel::Thread* current_thread =
                    kernel->GetCurrentThreadManager().GetCurrentThread()) {
                running_core->SaveContext(current_thread->context);
            }
        }

        GDBStub::HandlePacket();

        if (GDBStub::GetCpuHaltFlag()) {
            if (GDBStub::GetCpuStepFlag()) {
                step = true;
            } else {
                return ResultStatus::Success;
            }
        }
    }

    u64 global_ticks = timing->GetGlobalTicks();
    s64 max_delay = 0;
    ARM_Interface* current_core_to_execute = nullptr;

    for (auto& cpu_core : cpu_cores) {
        if (cpu_core->GetTimer().GetTicks() < global_ticks) {
            s64 delay = global_ticks - cpu_core->GetTimer().GetTicks();

            kernel->SetRunningCPU(cpu_core.get());
            cpu_core->GetTimer().Advance();
            cpu_core->PrepareReschedule();
            kernel->GetThreadManager(cpu_core->GetID()).Reschedule();
            cpu_core->GetTimer().SetNextSlice(delay);

            if (max_delay < delay) {
                max_delay = delay;
                current_core_to_execute = cpu_core.get();
            }
        }
    }

    static constexpr s64 min_delay = 100;

    if (max_delay > min_delay) {
        LOG_TRACE(Core_ARM11, "Core {} running (delayed) for {} ticks",
                  current_core_to_execute->GetID(),

                  current_core_to_execute->GetTimer().GetDowncount());

        if (running_core != current_core_to_execute) {
            running_core = current_core_to_execute;
            kernel->SetRunningCPU(running_core);
        }

        if (kernel->GetCurrentThreadManager().GetCurrentThread() == nullptr) {
            LOG_TRACE(Core_ARM11, "Core {} idling", current_core_to_execute->GetID());
            current_core_to_execute->GetTimer().Idle();
            PrepareReschedule();
        } else {
            if (step) {
                current_core_to_execute->Step();
            } else {
                current_core_to_execute->Run();
            }
        }
    } else {
        s64 max_slice = Settings::values.core_system_run_default_max_slice_value;

        for (const auto& cpu_core : cpu_cores) {
            kernel->SetRunningCPU(cpu_core.get());
            cpu_core->GetTimer().Advance();
            cpu_core->PrepareReschedule();
            kernel->GetThreadManager(cpu_core->GetID()).Reschedule();
            max_slice = std::min(max_slice, cpu_core->GetTimer().GetMaxSliceLength());
        }

        for (std::shared_ptr<ARM_Interface>& cpu_core : cpu_cores) {
            cpu_core->GetTimer().SetNextSlice(max_slice);
            const u64 start_ticks = cpu_core->GetTimer().GetTicks();

            LOG_TRACE(Core_ARM11, "Core {} running for {} ticks", cpu_core->GetID(),
                      cpu_core->GetTimer().GetDowncount());

            running_core = cpu_core.get();
            kernel->SetRunningCPU(running_core);

            if (kernel->GetCurrentThreadManager().GetCurrentThread() == nullptr) {
                LOG_TRACE(Core_ARM11, "Core {} idling", cpu_core->GetID());
                cpu_core->GetTimer().Idle();
                PrepareReschedule();
            } else {
                if (step) {
                    cpu_core->Step();
                } else {
                    cpu_core->Run();
                }
            }

            max_slice = cpu_core->GetTimer().GetTicks() - start_ticks;
        }
    }

    if (GDBStub::IsServerEnabled()) {
        GDBStub::SetCpuStepFlag(false);
    }

    Reschedule();

    if (reset_requested.exchange(false)) {
        Reset();
    } else if (shutdown_requested.exchange(false)) {
        return ResultStatus::ShutdownRequested;
    }

    return status;
}

System::ResultStatus System::Load(Frontend::EmuWindow& emu_window, const std::string& filepath) {
    m_emu_window = &emu_window;

    if (!FileUtil::Exists(filepath)) {
        LOG_CRITICAL(Core, "File not found");

        if (on_load_failed) {
            on_load_failed(ResultStatus::ErrorFileNotFound);
        }

        return ResultStatus::ErrorFileNotFound;
    }

    app_loader = Loader::GetLoader(filepath);

    if (app_loader == nullptr) {
        LOG_CRITICAL(Core, "Unsupported file format");

        if (on_load_failed) {
            on_load_failed(ResultStatus::ErrorLoader_ErrorUnsupportedFormat);
        }

        return ResultStatus::ErrorLoader_ErrorUnsupportedFormat;
    }

    std::pair<std::optional<u32>, Loader::ResultStatus> system_mode =
        app_loader->LoadKernelSystemMode();

    if (system_mode.second != Loader::ResultStatus::Success) {
        LOG_CRITICAL(Core, "Failed to determine system mode (Error {})!",
                     static_cast<int>(system_mode.second));

        switch (system_mode.second) {
        case Loader::ResultStatus::ErrorEncrypted:
            if (on_load_failed) {
                on_load_failed(ResultStatus::ErrorLoader_ErrorEncrypted);
            }

            return ResultStatus::ErrorLoader_ErrorEncrypted;
        case Loader::ResultStatus::ErrorInvalidFormat:
            if (on_load_failed) {
                on_load_failed(ResultStatus::ErrorLoader_ErrorUnsupportedFormat);
            }

            return ResultStatus::ErrorLoader_ErrorUnsupportedFormat;
        default:
            if (on_load_failed) {
                on_load_failed(ResultStatus::ErrorSystemMode);
            }

            return ResultStatus::ErrorSystemMode;
        }
    }

    ASSERT(system_mode.first);

    const ResultStatus init_result = Init(emu_window, *system_mode.first);

    if (init_result != ResultStatus::Success) {
        LOG_CRITICAL(Core, "Failed to initialize system (Error {})!",
                     static_cast<u32>(init_result));

        System::Shutdown();

        if (on_load_failed) {
            on_load_failed(init_result);
        }

        return init_result;
    }

    std::shared_ptr<Kernel::Process> process;
    const Loader::ResultStatus load_result = app_loader->Load(process);
    kernel->SetCurrentProcess(process);

    if (load_result != Loader::ResultStatus::Success) {
        LOG_CRITICAL(Core, "Failed to load ROM (Error {})!", static_cast<u32>(load_result));
        System::Shutdown();

        switch (load_result) {
        case Loader::ResultStatus::ErrorEncrypted:
            if (on_load_failed) {
                on_load_failed(ResultStatus::ErrorLoader_ErrorEncrypted);
            }

            return ResultStatus::ErrorLoader_ErrorEncrypted;
        case Loader::ResultStatus::ErrorInvalidFormat:
            if (on_load_failed) {
                on_load_failed(ResultStatus::ErrorLoader_ErrorUnsupportedFormat);
            }

            return ResultStatus::ErrorLoader_ErrorUnsupportedFormat;
        default:
            UNREACHABLE();
        }
    }

    cheat_engine = std::make_shared<Cheats::CheatEngine>(*this);
    perf_stats = std::make_unique<PerfStats>();
    custom_tex_cache = std::make_unique<Core::CustomTexCache>();

    if (Settings::values.use_custom_textures) {
        FileUtil::CreateFullPath(fmt::format("{}textures/{:016X}/",
                                             FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                                             Kernel().GetCurrentProcess()->codeset->program_id));

        FileUtil::CreateFullPath(fmt::format("{}textures/{:016X}/",
                                             FileUtil::GetUserPath(FileUtil::UserPath::PreloadDir),
                                             Kernel().GetCurrentProcess()->codeset->program_id));

        custom_tex_cache->FindCustomTextures();
    }

    if (Settings::values.use_hardware_renderer && Settings::values.preload_custom_textures) {
        preload_custom_textures_function();
    }

    if (Settings::values.use_hardware_renderer && Settings::values.use_hardware_shader &&
        Settings::values.enable_disk_shader_cache) {
        VideoCore::g_renderer->Rasterizer()->LoadDiskShaderCache();
    }

    status = ResultStatus::Success;
    m_filepath = filepath;

    perf_stats->BeginSystemFrame();

    return status;
}

bool System::IsInitialized() const {
    return cpu_cores.size() > 0 &&
           !std::any_of(cpu_cores.begin(), cpu_cores.end(),
                        [](std::shared_ptr<ARM_Interface> ptr) { return ptr == nullptr; });
}

void System::PrepareReschedule() {
    running_core->PrepareReschedule();
    reschedule_pending = true;
}

void System::Reschedule() {
    if (!reschedule_pending) {
        return;
    }

    reschedule_pending = false;
    for (const auto& core : cpu_cores) {
        LOG_TRACE(Core_ARM11, "Reschedule core {}", core->GetID());
        kernel->GetThreadManager(core->GetID()).Reschedule();
    }
}

System::ResultStatus System::Init(Frontend::EmuWindow& emu_window, u32 system_mode) {
    memory = std::make_unique<Memory::MemorySystem>();
    timing = std::make_unique<Timing>();

    kernel = std::make_unique<Kernel::KernelSystem>(
        *memory, *timing, [this] { PrepareReschedule(); }, system_mode);

    if (Settings::values.use_cpu_jit) {
#ifdef ARCHITECTURE_x86_64
        exclusive_monitor =
            std::make_shared<Dynarmic::ExclusiveMonitor>(Settings::values.enable_core_2 ? 2 : 1);

        cpu_cores.push_back(std::make_shared<ARM_Dynarmic>(this, *memory, 0, timing->GetTimer(0),
                                                           exclusive_monitor.get()));

        if (Settings::values.enable_core_2) {
            cpu_cores.push_back(std::make_shared<ARM_Dynarmic>(
                this, *memory, 1, timing->GetTimer(1), exclusive_monitor.get()));
        }
#else
        cpu_cores.push_back(std::make_shared<ARM_DynCom>(this, *memory, 0, timing->GetTimer(0)));

        if (Settings::values.enable_core_2) {
            cpu_cores.push_back(
                std::make_shared<ARM_DynCom>(this, *memory, 1, timing->GetTimer(1)));
        }

        LOG_WARNING(Core, "CPU JIT requested, but Dynarmic not available");
#endif
    } else {
        cpu_cores.push_back(std::make_shared<ARM_DynCom>(this, *memory, 0, timing->GetTimer(0)));

        if (Settings::values.enable_core_2) {
            cpu_cores.push_back(
                std::make_shared<ARM_DynCom>(this, *memory, 1, timing->GetTimer(1)));
        }
    }

    running_core = cpu_cores[0].get();

    kernel->SetCPUs(cpu_cores);
    kernel->SetRunningCPU(cpu_cores[0].get());

    if (Settings::values.enable_dsp_lle) {
        dsp_core = std::make_shared<AudioCore::DspLle>(*memory,
                                                       Settings::values.enable_dsp_lle_multithread);
    } else {
        dsp_core = std::make_shared<AudioCore::DspHle>(*memory);
    }

    memory->SetDSP(*dsp_core);

    dsp_core->SetSink(Settings::values.audio_sink_id, Settings::values.audio_device_id);
    dsp_core->EnableStretching(Settings::values.enable_audio_stretching);

    service_manager = std::make_shared<Service::SM::ServiceManager>(*this);
    archive_manager = std::make_unique<Service::FS::ArchiveManager>(*this);

    HW::Init(*memory);
    Service::Init(*this);
    GDBStub::DeferStart();

    VideoCore::Init(emu_window, *memory);

    return ResultStatus::Success;
}

RendererBase& System::Renderer() {
    return *VideoCore::g_renderer;
}

Service::SM::ServiceManager& System::ServiceManager() {
    return *service_manager;
}

const Service::SM::ServiceManager& System::ServiceManager() const {
    return *service_manager;
}

Service::FS::ArchiveManager& System::ArchiveManager() {
    return *archive_manager;
}

const Service::FS::ArchiveManager& System::ArchiveManager() const {
    return *archive_manager;
}

Kernel::KernelSystem& System::Kernel() {
    return *kernel;
}

const Kernel::KernelSystem& System::Kernel() const {
    return *kernel;
}

Timing& System::CoreTiming() {
    return *timing;
}

const Timing& System::CoreTiming() const {
    return *timing;
}

Memory::MemorySystem& System::Memory() {
    return *memory;
}

const Memory::MemorySystem& System::Memory() const {
    return *memory;
}

Cheats::CheatEngine& System::CheatEngine() {
    return *cheat_engine;
}

const Cheats::CheatEngine& System::CheatEngine() const {
    return *cheat_engine;
}

Core::CustomTexCache& System::CustomTexCache() {
    return *custom_tex_cache;
}

const Core::CustomTexCache& System::CustomTexCache() const {
    return *custom_tex_cache;
}

Network::RoomMember& System::RoomMember() {
    return *room_member;
}

Loader::AppLoader& System::GetAppLoader() const {
    return *app_loader;
}

void System::RegisterMiiSelector(std::shared_ptr<Frontend::MiiSelector> mii_selector) {
    registered_mii_selector = std::move(mii_selector);
}

void System::RegisterSoftwareKeyboard(std::shared_ptr<Frontend::SoftwareKeyboard> swkbd) {
    registered_swkbd = std::move(swkbd);
}

void System::Shutdown() {
    GDBStub::Shutdown();
    VideoCore::Shutdown();
    perf_stats.reset();
    cheat_engine.reset();
    archive_manager.reset();
    service_manager.reset();
    dsp_core.reset();
    cpu_cores.clear();
    kernel.reset();
    timing.reset();
    app_loader.reset();
    exclusive_monitor.reset();
    custom_tex_cache.reset();
    running_core = nullptr;
    room_member->SendGameInfo(Network::GameInfo{});
}

void System::Reset() {
    if (IsInitialized()) {
        std::optional<Service::APT::DeliverArg> deliver_arg;
        std::vector<u8> wireless_reboot_info;
        std::string current_gamecard_path;
        std::unordered_map<u32, Service::FS::ProgramInfo> program_info_map;

        if (std::shared_ptr<Service::APT::Module> apt = Service::APT::GetModule(*this)) {
            deliver_arg = apt->GetAppletManager()->ReceiveDeliverArg();
            wireless_reboot_info = apt->GetWirelessRebootInfo();
        }

        if (std::shared_ptr<Service::FS::FS_USER> fs_user =
                service_manager->GetService<Service::FS::FS_USER>("fs:USER")) {
            current_gamecard_path = fs_user->GetCurrentGamecardPath();
            program_info_map = fs_user->GetProgramInfoMap();
        }

        Shutdown();

        before_loading_after_first_time();

        Load(*m_emu_window, m_filepath);

        if (std::shared_ptr<Service::APT::Module> apt = Service::APT::GetModule(*this)) {
            apt->GetAppletManager()->SetDeliverArg(std::move(deliver_arg));
            apt->SetWirelessRebootInfo(wireless_reboot_info);
        }

        if (std::shared_ptr<Service::FS::FS_USER> fs_user =
                service_manager->GetService<Service::FS::FS_USER>("fs:USER")) {
            fs_user->SetCurrentGamecardPath(current_gamecard_path);
            fs_user->SetProgramInfoMap(program_info_map);
        }

        emulation_starting_after_first_time();
    } else {
        before_loading_after_first_time();

        Load(*m_emu_window, m_filepath);

        emulation_starting_after_first_time();
    }
}

void System::RequestReset() {
    reset_requested = true;
}

void System::RequestShutdown() {
    shutdown_requested = true;
}

void System::SetResetFilePath(const std::string filepath) {
    m_filepath = filepath;
}

void System::SetBeforeLoadingAfterFirstTime(std::function<void()> function) {
    before_loading_after_first_time = function;
}

void System::SetEmulationStartingAfterFirstTime(std::function<void()> function) {
    emulation_starting_after_first_time = function;
}

void System::SetOnLoadFailed(std::function<void(System::ResultStatus)> function) {
    on_load_failed = function;
}

void System::SetPreloadCustomTexturesFunction(std::function<void()> function) {
    preload_custom_textures_function = function;
}

void System::SetDiskShaderCacheCallback(
    std::function<void(bool, std::size_t, std::size_t)> function) {
    disk_shader_cache_callback = function;
}

void System::DiskShaderCacheCallback(bool loading, std::size_t current, std::size_t total) {
    disk_shader_cache_callback(loading, current, total);
}

const bool System::IsOnLoadFailedSet() const {
    return static_cast<bool>(on_load_failed);
}

const std::string& System::GetFilePath() const {
    return m_filepath;
}

} // namespace Core
