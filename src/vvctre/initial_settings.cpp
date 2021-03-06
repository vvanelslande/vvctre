// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <string>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <imgui_stdlib.h>
#include <portable-file-dialogs.h>
#include <whereami.h>
#ifdef HAVE_CUBEB
#include "audio_core/cubeb_input.h"
#endif
#include "audio_core/sdl2_input.h"
#include "audio_core/sink.h"
#include "audio_core/sink_details.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/param_package.h"
#include "common/string_util.h"
#include "core/3ds.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/movie.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "network/room.h"
#include "video_core/renderer_opengl/texture_filters/texture_filterer.h"
#include "vvctre/common.h"
#include "vvctre/initial_settings.h"
#include "vvctre/plugins.h"

static bool is_open = true;

InitialSettings::InitialSettings(PluginManager& plugin_manager, SDL_Window* window,
                                 Service::CFG::Module& cfg, std::atomic<bool>& update_found,
                                 bool& ok_multiplayer) {
    signal(SIGINT, [](int) {
        Settings::values.file_path.clear();
        is_open = false;
    });
    signal(SIGTERM, [](int) {
        Settings::values.file_path.clear();
        is_open = false;
    });

    SDL_Event event;
    CitraRoomList all_public_rooms;
    CitraRoomList public_rooms_search_results;
    std::string public_rooms_search_text;
    std::string public_rooms_search_text_;
    bool first_time_in_multiplayer = true;
    u16 play_coins = 0xDEAD;
    bool play_coins_changed = false;
    bool config_savegame_changed = false;
    std::vector<std::tuple<std::string, std::string>> all_installed;
    std::vector<std::tuple<std::string, std::string>> installed_search_results;
    std::string installed_search_text;
    std::string installed_search_text_;
    std::string host_multiplayer_room_ip = "0.0.0.0";
    u16 host_multiplayer_room_port = Network::DEFAULT_PORT;
    u32 host_multiplayer_room_member_slots = Network::DEFAULT_MEMBER_SLOTS;
    bool host_multiplayer_room_room_created = false;

    while (is_open) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) {
                if (pfd::message("vvctre", "Would you like to exit now?", pfd::choice::yes_no,
                                 pfd::icon::question)
                        .result() == pfd::button::yes) {
                    Settings::values.file_path.clear();
                    return;
                }
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2());
        ImGui::SetNextWindowSize(io.DisplaySize);
        if (ImGui::Begin("Initial Settings", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_HorizontalScrollbar)) {
            if (ImGui::BeginTabBar("Tab Bar", ImGuiTabBarFlags_TabListPopupButton |
                                                  ImGuiTabBarFlags_FittingPolicyScroll)) {
                if (ImGui::BeginTabItem("Start")) {
                    ImGui::Button("...##file");
                    if (ImGui::BeginPopupContextItem("File", ImGuiPopupFlags_MouseButtonLeft)) {
                        if (ImGui::MenuItem("Browse")) {
                            int length = wai_getExecutablePath(nullptr, 0, nullptr);
                            std::string vvctre_folder(length, '\0');
                            int dirname_length = 0;
                            wai_getExecutablePath(&vvctre_folder[0], length, &dirname_length);
                            vvctre_folder = vvctre_folder.substr(0, dirname_length);

                            const std::vector<std::string> result =
                                pfd::open_file("Browse", vvctre_folder,
                                               {"All supported files",
                                                "*.cci *.CCI *.3ds *.3DS *.cxi *.CXI *.3dsx *.3DSX "
                                                "*.app *.APP *.elf *.ELF *.axf *.AXF",
                                                "Cartridges", "*.cci *.CCI *.3ds *.3DS", "NCCHs",
                                                "*.cxi *.CXI *.app *.APP", "Homebrew",
                                                "*.3dsx *.3DSX *.elf *.ELF *.axf *.AXF"})
                                    .result();
                            if (!result.empty()) {
                                Settings::values.file_path = result[0];
                            }
                        }
                        if (ImGui::MenuItem("Install CIA")) {
                            int length = wai_getExecutablePath(nullptr, 0, nullptr);
                            std::string vvctre_folder(length, '\0');
                            int dirname_length = 0;
                            wai_getExecutablePath(&vvctre_folder[0], length, &dirname_length);
                            vvctre_folder = vvctre_folder.substr(0, dirname_length);

                            const std::vector<std::string> files =
                                pfd::open_file("Install CIA", vvctre_folder,
                                               {"CTR Importable Archive", "*.cia *.CIA"},
                                               pfd::opt::multiselect)
                                    .result();

                            if (files.empty()) {
                                continue;
                            }

                            std::atomic<bool> installing{true};
                            std::mutex mutex;
                            std::string current_file;
                            std::size_t current_file_current = 0;
                            std::size_t current_file_total = 0;

                            std::thread([&] {
                                for (const auto& file : files) {
                                    {
                                        std::lock_guard<std::mutex> lock(mutex);
                                        current_file = file;
                                    }

                                    const Service::AM::InstallStatus status =
                                        Service::AM::InstallCIA(
                                            file, [&](std::size_t current, std::size_t total) {
                                                std::lock_guard<std::mutex> lock(mutex);
                                                current_file_current = current;
                                                current_file_total = total;
                                            });

                                    switch (status) {
                                    case Service::AM::InstallStatus::Success:
                                        break;
                                    case Service::AM::InstallStatus::ErrorFailedToOpenFile:
                                        pfd::message("vvctre",
                                                     fmt::format("Failed to open {}", file),
                                                     pfd::choice::ok, pfd::icon::error);
                                        break;
                                    case Service::AM::InstallStatus::ErrorFileNotFound:
                                        pfd::message("vvctre", fmt::format("{} not found", file),
                                                     pfd::choice::ok, pfd::icon::error);
                                        break;
                                    case Service::AM::InstallStatus::ErrorAborted:
                                        pfd::message("vvctre",
                                                     fmt::format("{} installation aborted", file),
                                                     pfd::choice::ok, pfd::icon::error);
                                        break;
                                    case Service::AM::InstallStatus::ErrorInvalid:
                                        pfd::message("vvctre", fmt::format("{} is invalid", file),
                                                     pfd::choice::ok, pfd::icon::error);
                                        break;
                                    case Service::AM::InstallStatus::ErrorEncrypted:
                                        pfd::message("vvctre", fmt::format("{} is encrypted", file),
                                                     pfd::choice::ok, pfd::icon::error);
                                        break;
                                    }
                                }

                                installing = false;
                            }).detach();

                            while (installing) {
                                while (SDL_PollEvent(&event)) {
                                    ImGui_ImplSDL2_ProcessEvent(&event);

                                    if (event.type == SDL_QUIT) {
                                        if (pfd::message("vvctre", "Would you like to exit now?",
                                                         pfd::choice::yes_no, pfd::icon::question)
                                                .result() == pfd::button::yes) {
                                            vvctreShutdown(&plugin_manager);
                                            std::exit(0);
                                        }
                                    }
                                }

                                ImGui_ImplOpenGL3_NewFrame();
                                ImGui_ImplSDL2_NewFrame(window);
                                ImGui::NewFrame();

                                ImGui::OpenPopup("Installing CIA");
                                ImGui::SetNextWindowPos(
                                    ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                                if (ImGui::BeginPopupModal("Installing CIA", nullptr,
                                                           ImGuiWindowFlags_NoSavedSettings |
                                                               ImGuiWindowFlags_NoMove |
                                                               ImGuiWindowFlags_AlwaysAutoResize)) {
                                    std::lock_guard<std::mutex> lock(mutex);
                                    ImGui::PushTextWrapPos(io.DisplaySize.x * 0.9f);
                                    ImGui::Text("Installing %s", current_file.c_str());
                                    ImGui::PopTextWrapPos();
                                    ImGui::ProgressBar(static_cast<float>(current_file_current) /
                                                       static_cast<float>(current_file_total));
                                    ImGui::EndPopup();
                                }

                                glClearColor(Settings::values.background_color_red,
                                             Settings::values.background_color_green,
                                             Settings::values.background_color_blue, 0.0f);
                                glClear(GL_COLOR_BUFFER_BIT);
                                ImGui::Render();
                                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                                SDL_GL_SwapWindow(window);
                            }

                            continue;
                        }
                        if (ImGui::MenuItem("Installed")) {
                            all_installed = GetInstalledList();
                        }
                        if (ImGui::MenuItem("HOME Menu")) {
                            if (Settings::values.region_value == Settings::Region::AutoSelect) {
                                pfd::message("vvctre", "Region is Auto-select", pfd::choice::ok,
                                             pfd::icon::error);
                            } else {
                                const u64 title_id = Service::APT::GetTitleIdForApplet(
                                    Service::APT::AppletId::HomeMenu,
                                    static_cast<u32>(Settings::values.region_value));
                                const std::string path = Service::AM::GetTitleContentPath(
                                    Service::FS::MediaType::NAND, title_id);
                                if (FileUtil::Exists(path)) {
                                    Settings::values.file_path = path;
                                } else {
                                    pfd::message("vvctre", "HOME Menu not installed",
                                                 pfd::choice::ok, pfd::icon::error);
                                }
                            }
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::SameLine();
                    ImGui::InputText("File", &Settings::values.file_path);

                    if (Settings::values.record_movie.empty()) {
                        if (ImGui::Button("...##playmovie")) {
                            int length = wai_getExecutablePath(nullptr, 0, nullptr);
                            std::string vvctre_folder(length, '\0');
                            int dirname_length = 0;
                            wai_getExecutablePath(&vvctre_folder[0], length, &dirname_length);
                            vvctre_folder = vvctre_folder.substr(0, dirname_length);

                            const std::vector<std::string> result =
                                pfd::open_file("Play Movie", vvctre_folder,
                                               {"VvCtre Movie", "*.vcm"})
                                    .result();
                            if (!result.empty()) {
                                Settings::values.play_movie = result[0];
                            }
                        }
                        ImGui::SameLine();
                        ImGui::InputText("Play Movie", &Settings::values.play_movie);
                    }

                    if (Settings::values.play_movie.empty()) {
                        if (ImGui::Button("...##recordmovie")) {
                            const std::string record_movie =
                                pfd::save_file("Record Movie", "movie.vcm",
                                               {"VvCtre Movie", "*.vcm"})
                                    .result();
                            if (!record_movie.empty()) {
                                Settings::values.record_movie = record_movie;
                            }
                        }
                        ImGui::SameLine();
                        ImGui::InputText("Record Movie", &Settings::values.record_movie);
                    }

                    if (ImGui::BeginCombo("Region", [&] {
                            switch (Settings::values.region_value) {
                            case Settings::Region::AutoSelect:
                                return "Auto-select";
                            case Settings::Region::Japan:
                                return "Japan";
                            case Settings::Region::USA:
                                return "USA";
                            case Settings::Region::Europe:
                                return "Europe";
                            case Settings::Region::Australia:
                                return "Australia";
                            case Settings::Region::China:
                                return "China";
                            case Settings::Region::Korea:
                                return "Korea";
                            case Settings::Region::Taiwan:
                                return "Taiwan";
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Auto-select", Settings::values.region_value ==
                                                                 Settings::Region::AutoSelect)) {
                            Settings::values.region_value = Settings::Region::AutoSelect;
                        }

                        if (ImGui::Selectable("Japan", Settings::values.region_value ==
                                                           Settings::Region::Japan)) {
                            Settings::values.region_value = Settings::Region::Japan;
                        }

                        if (ImGui::Selectable("USA", Settings::values.region_value ==
                                                         Settings::Region::USA)) {
                            Settings::values.region_value = Settings::Region::USA;
                        }

                        if (ImGui::Selectable("Europe", Settings::values.region_value ==
                                                            Settings::Region::Europe)) {
                            Settings::values.region_value = Settings::Region::Europe;
                        }

                        if (ImGui::Selectable("Australia", Settings::values.region_value ==
                                                               Settings::Region::Australia)) {
                            Settings::values.region_value = Settings::Region::Australia;
                        }

                        if (ImGui::Selectable("China", Settings::values.region_value ==
                                                           Settings::Region::China)) {
                            Settings::values.region_value = Settings::Region::China;
                        }

                        if (ImGui::Selectable("Korea", Settings::values.region_value ==
                                                           Settings::Region::Korea)) {
                            Settings::values.region_value = Settings::Region::Korea;
                        }

                        if (ImGui::Selectable("Taiwan", Settings::values.region_value ==
                                                            Settings::Region::Taiwan)) {
                            Settings::values.region_value = Settings::Region::Taiwan;
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::InputText("Log Filter", &Settings::values.log_filter);

                    if (ImGui::BeginCombo("Initial Time", [] {
                            switch (Settings::values.initial_clock) {
                            case Settings::InitialClock::System:
                                return "System";
                            case Settings::InitialClock::UnixTimestamp:
                                return "Unix Timestamp";
                            default:
                                break;
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("System", Settings::values.initial_clock ==
                                                            Settings::InitialClock::System)) {
                            Settings::values.initial_clock = Settings::InitialClock::System;
                        }

                        if (ImGui::Selectable("Unix Timestamp",
                                              Settings::values.initial_clock ==
                                                  Settings::InitialClock::UnixTimestamp)) {
                            Settings::values.initial_clock = Settings::InitialClock::UnixTimestamp;
                        }

                        ImGui::EndCombo();
                    }
                    if (Settings::values.initial_clock == Settings::InitialClock::UnixTimestamp) {
                        ImGui::InputScalar("Unix Timestamp", ImGuiDataType_U64,
                                           &Settings::values.unix_timestamp);
                    }

                    ImGui::Checkbox("Use Virtual SD Card", &Settings::values.use_virtual_sd);

                    ImGui::Checkbox("Record Frame Times", &Settings::values.record_frame_times);

                    ImGui::Checkbox("Enable GDB Stub", &Settings::values.use_gdbstub);
                    if (Settings::values.use_gdbstub) {
                        ImGui::InputScalar("GDB Stub Port", ImGuiDataType_U16,
                                           &Settings::values.gdbstub_port);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("General")) {
                    ImGui::Checkbox("Enable Core 2", &Settings::values.enable_core_2);
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::PushTextWrapPos(io.DisplaySize.x * 0.5f);
                        ImGui::TextUnformatted("This is needed to play some games (including "
                                               "Donkey Kong Country Returns 3D, Sonic Boom: Fire & "
                                               "Ice, and Sonic Boom: Shattered Crystal)");
                        ImGui::PopTextWrapPos();
                        ImGui::EndTooltip();
                    }
                    ImGui::Checkbox("Limit Speed", &Settings::values.limit_speed);
                    ImGui::Checkbox("Enable Custom CPU Ticks",
                                    &Settings::values.use_custom_cpu_ticks);

                    if (Settings::values.limit_speed) {
                        ImGui::InputScalar("Speed Limit", ImGuiDataType_U16,
                                           &Settings::values.speed_limit, nullptr, nullptr, "%d%%");
                    }

                    if (Settings::values.use_custom_cpu_ticks) {
                        ImGui::InputScalar("Custom CPU Ticks", ImGuiDataType_U64,
                                           &Settings::values.custom_cpu_ticks);
                    }

                    const u32 min = 5;
                    const u32 max = 400;
                    ImGui::SliderScalar("CPU Clock Percentage", ImGuiDataType_U32,
                                        &Settings::values.cpu_clock_percentage, &min, &max, "%d%%");

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Core::System::Run()");
                    ImGui::Separator();

                    ImGui::InputScalar("Default Max Slice Length", ImGuiDataType_S64,
                                       &Settings::values.core_system_run_default_max_slice_value);

                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(fmt::format("Default value after 39.3.2: "
                                                           "{}\nDefault value before 40.0.0: 20000",
                                                           BASE_CLOCK_RATE_ARM11 / 234)
                                                   .c_str());
                        ImGui::EndTooltip();
                    }

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Core::Timing::Timer::Timer()");
                    ImGui::Separator();

                    ImGui::InputScalar(
                        "Set Slice Length To This", ImGuiDataType_S64,
                        &Settings::values.set_slice_length_to_this_in_core_timing_timer_timer);

                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(fmt::format("Default value after 39.3.2: "
                                                           "{}\nDefault value before 40.0.0: 20000",
                                                           BASE_CLOCK_RATE_ARM11 / 234)
                                                   .c_str());
                        ImGui::EndTooltip();
                    }

                    ImGui::InputScalar(
                        "Set Downcount To This", ImGuiDataType_S64,
                        &Settings::values.set_downcount_to_this_in_core_timing_timer_timer);

                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(fmt::format("Default value after 39.3.2: "
                                                           "{}\nDefault value before 40.0.0: 20000",
                                                           BASE_CLOCK_RATE_ARM11 / 234)
                                                   .c_str());
                        ImGui::EndTooltip();
                    }

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Core::Timing::Timer::GetMaxSliceLength()");
                    ImGui::Separator();

                    ImGui::PushTextWrapPos();
                    ImGui::TextUnformatted("Return this if the event queue is empty:");
                    ImGui::PopTextWrapPos();

                    ImGui::InputScalar(
                        "##return_this_if_the_event_queue_is_empty_in_core_timing_timer_"
                        "getmaxslicelength",
                        ImGuiDataType_S64,
                        &Settings::values
                             .return_this_if_the_event_queue_is_empty_in_core_timing_timer_getmaxslicelength);

                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(fmt::format("Default value after 39.3.2: "
                                                           "{}\nDefault value before 40.0.0: 20000",
                                                           BASE_CLOCK_RATE_ARM11 / 234)
                                                   .c_str());
                        ImGui::EndTooltip();
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Audio")) {
                    ImGui::Checkbox("Enable DSP LLE", &Settings::values.enable_dsp_lle);

                    if (Settings::values.enable_dsp_lle) {
                        ImGui::Indent();
                        ImGui::Checkbox("Use Multiple Threads",
                                        &Settings::values.enable_dsp_lle_multithread);
                        ImGui::Unindent();
                    }

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Output");
                    ImGui::Separator();

                    ImGui::Checkbox("Enable Stretching##Output",
                                    &Settings::values.enable_audio_stretching);

                    ImGui::SliderFloat("Volume##Output", &Settings::values.audio_volume, 0.0f,
                                       1.0f);

                    if (ImGui::BeginCombo("Sink##Output", Settings::values.audio_sink_id.c_str())) {
                        if (ImGui::Selectable("auto", Settings::values.audio_sink_id == "auto")) {
                            Settings::values.audio_sink_id = "auto";
                        }

                        for (const auto& sink : AudioCore::GetSinkIDs()) {
                            if (ImGui::Selectable(sink, Settings::values.audio_sink_id == sink)) {
                                Settings::values.audio_sink_id = sink;
                            }
                        }

                        ImGui::EndCombo();
                    }

                    if (ImGui::BeginCombo("Device##Output",
                                          Settings::values.audio_device_id.c_str())) {
                        if (ImGui::Selectable("auto", Settings::values.audio_device_id == "auto")) {
                            Settings::values.audio_device_id = "auto";
                        }

                        for (const std::string& device :
                             AudioCore::GetDeviceListForSink(Settings::values.audio_sink_id)) {
                            if (ImGui::Selectable(device.c_str(),
                                                  Settings::values.audio_device_id == device)) {
                                Settings::values.audio_device_id = device;
                            }
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Microphone");
                    ImGui::Separator();

                    if (ImGui::BeginCombo("Source##Microphone", [] {
                            switch (Settings::values.microphone_input_type) {
                            case Settings::MicrophoneInputType::None:
                                return "Disabled";
                            case Settings::MicrophoneInputType::Real:
                                return "Real Device";
                            case Settings::MicrophoneInputType::Static:
                                return "Static Noise";
                            default:
                                break;
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Disabled",
                                              Settings::values.microphone_input_type ==
                                                  Settings::MicrophoneInputType::None)) {
                            Settings::values.microphone_input_type =
                                Settings::MicrophoneInputType::None;
                        }

                        if (ImGui::Selectable("Real Device",
                                              Settings::values.microphone_input_type ==
                                                  Settings::MicrophoneInputType::Real)) {
                            Settings::values.microphone_input_type =
                                Settings::MicrophoneInputType::Real;
                        }

                        if (ImGui::Selectable("Static Noise",
                                              Settings::values.microphone_input_type ==
                                                  Settings::MicrophoneInputType::Static)) {
                            Settings::values.microphone_input_type =
                                Settings::MicrophoneInputType::Static;
                        }

                        ImGui::EndCombo();
                    }

                    if (Settings::values.microphone_input_type ==
                        Settings::MicrophoneInputType::Real) {
                        if (ImGui::BeginCombo("Backend##Microphone", [] {
                                switch (Settings::values.microphone_real_device_backend) {
                                case Settings::MicrophoneRealDeviceBackend::Auto:
                                    return "Auto";
#ifdef HAVE_CUBEB
                                case Settings::MicrophoneRealDeviceBackend::Cubeb:
                                    return "Cubeb";
#endif
                                case Settings::MicrophoneRealDeviceBackend::SDL2:
                                    return "SDL2";
                                case Settings::MicrophoneRealDeviceBackend::Null:
                                    return "Null";
                                default:
                                    break;
                                }

                                return "Invalid";
                            }())) {
                            if (ImGui::Selectable(
                                    "Auto", Settings::values.microphone_real_device_backend ==
                                                Settings::MicrophoneRealDeviceBackend::Auto)) {
                                Settings::values.microphone_real_device_backend =
                                    Settings::MicrophoneRealDeviceBackend::Auto;
                            }

#ifdef HAVE_CUBEB
                            if (ImGui::Selectable(
                                    "Cubeb", Settings::values.microphone_real_device_backend ==
                                                 Settings::MicrophoneRealDeviceBackend::Cubeb)) {
                                Settings::values.microphone_real_device_backend =
                                    Settings::MicrophoneRealDeviceBackend::Cubeb;
                            }
#endif

                            if (ImGui::Selectable(
                                    "SDL2", Settings::values.microphone_real_device_backend ==
                                                Settings::MicrophoneRealDeviceBackend::SDL2)) {
                                Settings::values.microphone_real_device_backend =
                                    Settings::MicrophoneRealDeviceBackend::SDL2;
                            }

                            if (ImGui::Selectable(
                                    "Null", Settings::values.microphone_real_device_backend ==
                                                Settings::MicrophoneRealDeviceBackend::Null)) {
                                Settings::values.microphone_real_device_backend =
                                    Settings::MicrophoneRealDeviceBackend::Null;
                            }

                            ImGui::EndCombo();
                        }

                        if (ImGui::BeginCombo("Device##Microphone",
                                              Settings::values.microphone_device.c_str())) {
                            if (ImGui::Selectable("auto",
                                                  Settings::values.microphone_device == "auto")) {
                                Settings::values.microphone_device = "auto";
                            }

                            switch (Settings::values.microphone_real_device_backend) {
                            case Settings::MicrophoneRealDeviceBackend::Auto:
#ifdef HAVE_CUBEB
                                for (const std::string& device :
                                     AudioCore::ListCubebInputDevices()) {
                                    if (ImGui::Selectable(device.c_str(),
                                                          Settings::values.microphone_device ==
                                                              device)) {
                                        Settings::values.microphone_device = device;
                                    }
                                }
#else
                                for (const std::string& device :
                                     AudioCore::ListSDL2InputDevices()) {
                                    if (ImGui::Selectable(device.c_str(),
                                                          Settings::values.microphone_device ==
                                                              device)) {
                                        Settings::values.microphone_device = device;
                                    }
                                }
#endif

                                break;
                            case Settings::MicrophoneRealDeviceBackend::Cubeb:
#ifdef HAVE_CUBEB
                                for (const auto& device : AudioCore::ListCubebInputDevices()) {
                                    if (ImGui::Selectable(device.c_str(),
                                                          Settings::values.microphone_device ==
                                                              device)) {
                                        Settings::values.microphone_device = device;
                                    }
                                }
#endif

                                break;
                            case Settings::MicrophoneRealDeviceBackend::SDL2:
                                for (const auto& device : AudioCore::ListSDL2InputDevices()) {
                                    if (ImGui::Selectable(device.c_str(),
                                                          Settings::values.microphone_device ==
                                                              device)) {
                                        Settings::values.microphone_device = device;
                                    }
                                }

                                break;
                            case Settings::MicrophoneRealDeviceBackend::Null:
                                if (ImGui::Selectable("null", Settings::values.microphone_device ==
                                                                  "null")) {
                                    Settings::values.microphone_device = "null";
                                }

                                break;
                            default:
                                break;
                            }

                            ImGui::EndCombo();
                        }
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Camera")) {
                    ImGui::TextUnformatted("Inner");
                    ImGui::Separator();

                    if (ImGui::BeginCombo("Engine##Inner",
                                          Settings::values
                                              .camera_engine[static_cast<std::size_t>(
                                                  Service::CAM::CameraIndex::InnerCamera)]
                                              .c_str())) {
                        if (ImGui::Selectable(
                                "blank", Settings::values.camera_engine[static_cast<std::size_t>(
                                             Service::CAM::CameraIndex::InnerCamera)] == "blank")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "blank";
                        }

                        if (ImGui::Selectable(
                                "image", Settings::values.camera_engine[static_cast<std::size_t>(
                                             Service::CAM::CameraIndex::InnerCamera)] == "image")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "image";
                        }

                        if (ImGui::Selectable(
                                "tcp_client_rgb24_640x480",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] ==
                                    "tcp_client_rgb24_640x480")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] =
                                "tcp_client_rgb24_640x480";
                        }

                        ImGui::EndCombo();
                    }

                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::InnerCamera)] == "image") {
                        GUI_CameraAddBrowse(
                            "...##Inner",
                            static_cast<std::size_t>(Service::CAM::CameraIndex::InnerCamera));

                        ImGui::InputText(
                            "File Path/URL##Inner",
                            &Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)]);
                    } else if (Settings::values.camera_engine[static_cast<std::size_t>(
                                   Service::CAM::CameraIndex::InnerCamera)] ==
                               "tcp_client_rgb24_640x480") {
                        Common::ParamPackage params(
                            Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)]);

                        std::string ip = params.Get("ip", "127.0.0.1");
                        u16 port = static_cast<u16>(params.Get("port", 8000));

                        if (ImGui::InputText("IP##Inner", &ip)) {
                            params.Set("ip", ip);

                            Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = params.Serialize();
                        }

                        if (ImGui::InputScalar("Port##Inner", ImGuiDataType_U16, &port, 0, 0)) {
                            params.Set("port", static_cast<int>(port));

                            Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = params.Serialize();
                        }
                    }

                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::InnerCamera)] != "blank") {
                        if (ImGui::BeginCombo("Flip##Inner", [] {
                                switch (Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)]) {
                                case Service::CAM::Flip::None:
                                    return "None";
                                case Service::CAM::Flip::Horizontal:
                                    return "Horizontal";
                                case Service::CAM::Flip::Vertical:
                                    return "Vertical";
                                case Service::CAM::Flip::Reverse:
                                    return "Reverse";
                                default:
                                    return "Invalid";
                                }
                            }())) {
                            if (ImGui::Selectable(
                                    "None", Settings::values.camera_flip[static_cast<std::size_t>(
                                                Service::CAM::CameraIndex::InnerCamera)] ==
                                                Service::CAM::Flip::None)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::None;
                            }

                            if (ImGui::Selectable(
                                    "Horizontal",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::InnerCamera)] ==
                                        Service::CAM::Flip::Horizontal)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Horizontal;
                            }

                            if (ImGui::Selectable(
                                    "Vertical",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::InnerCamera)] ==
                                        Service::CAM::Flip::Vertical)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Vertical;
                            }

                            if (ImGui::Selectable(
                                    "Reverse",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::InnerCamera)] ==
                                        Service::CAM::Flip::Reverse)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Reverse;
                            }

                            ImGui::EndCombo();
                        }
                    }

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Outer Left");
                    ImGui::Separator();

                    if (ImGui::BeginCombo("Engine##Outer Left",
                                          Settings::values
                                              .camera_engine[static_cast<std::size_t>(
                                                  Service::CAM::CameraIndex::OuterLeftCamera)]
                                              .c_str())) {
                        if (ImGui::Selectable(
                                "blank",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] == "blank")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "blank";
                        }

                        if (ImGui::Selectable(
                                "image",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] == "image")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "image";
                        }

                        if (ImGui::Selectable(
                                "tcp_client_rgb24_640x480",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] ==
                                    "tcp_client_rgb24_640x480")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] =
                                "tcp_client_rgb24_640x480";
                        }

                        ImGui::EndCombo();
                    }

                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::OuterLeftCamera)] == "image") {
                        GUI_CameraAddBrowse(
                            "...##Outer Left",
                            static_cast<std::size_t>(Service::CAM::CameraIndex::OuterLeftCamera));

                        ImGui::InputText(
                            "File Path/URL##Outer Left",
                            &Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)]);
                    } else if (Settings::values.camera_engine[static_cast<std::size_t>(
                                   Service::CAM::CameraIndex::OuterLeftCamera)] ==
                               "tcp_client_rgb24_640x480") {
                        Common::ParamPackage params(
                            Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)]);

                        std::string ip = params.Get("ip", "127.0.0.1");
                        u16 port = static_cast<u16>(params.Get("port", 8000));

                        if (ImGui::InputText("IP##Outer Left", &ip)) {
                            params.Set("ip", ip);

                            Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = params.Serialize();
                        }

                        if (ImGui::InputScalar("Port##Outer Left", ImGuiDataType_U16, &port, 0,
                                               0)) {
                            params.Set("port", static_cast<int>(port));

                            Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = params.Serialize();
                        }
                    }

                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::OuterLeftCamera)] != "blank") {
                        if (ImGui::BeginCombo("Flip##Outer Left", [] {
                                switch (Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)]) {
                                case Service::CAM::Flip::None:
                                    return "None";
                                case Service::CAM::Flip::Horizontal:
                                    return "Horizontal";
                                case Service::CAM::Flip::Vertical:
                                    return "Vertical";
                                case Service::CAM::Flip::Reverse:
                                    return "Reverse";
                                default:
                                    return "Invalid";
                                }
                            }())) {
                            if (ImGui::Selectable(
                                    "None", Settings::values.camera_flip[static_cast<std::size_t>(
                                                Service::CAM::CameraIndex::OuterLeftCamera)] ==
                                                Service::CAM::Flip::None)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::None;
                            }

                            if (ImGui::Selectable(
                                    "Horizontal",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::OuterLeftCamera)] ==
                                        Service::CAM::Flip::Horizontal)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Horizontal;
                            }

                            if (ImGui::Selectable(
                                    "Vertical",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::OuterLeftCamera)] ==
                                        Service::CAM::Flip::Vertical)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Vertical;
                            }

                            if (ImGui::Selectable(
                                    "Reverse",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::OuterLeftCamera)] ==
                                        Service::CAM::Flip::Reverse)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Reverse;
                            }

                            ImGui::EndCombo();
                        }
                    }

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Outer Right");
                    ImGui::Separator();

                    if (ImGui::BeginCombo("Engine##Outer Right",
                                          Settings::values
                                              .camera_engine[static_cast<std::size_t>(
                                                  Service::CAM::CameraIndex::OuterRightCamera)]
                                              .c_str())) {
                        if (ImGui::Selectable(
                                "blank",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] == "blank")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "blank";
                        }

                        if (ImGui::Selectable(
                                "image",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] == "image")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "image";
                        }

                        if (ImGui::Selectable(
                                "tcp_client_rgb24_640x480",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] ==
                                    "tcp_client_rgb24_640x480")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] =
                                "tcp_client_rgb24_640x480";
                        }

                        ImGui::EndCombo();
                    }

                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::OuterRightCamera)] == "image") {
                        GUI_CameraAddBrowse(
                            "...##Outer Right",
                            static_cast<std::size_t>(Service::CAM::CameraIndex::OuterRightCamera));

                        ImGui::InputText(
                            "File Path/URL##Outer Right",
                            &Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)]);
                    } else if (Settings::values.camera_engine[static_cast<std::size_t>(
                                   Service::CAM::CameraIndex::OuterRightCamera)] ==
                               "tcp_client_rgb24_640x480") {
                        Common::ParamPackage params(
                            Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)]);

                        std::string ip = params.Get("ip", "127.0.0.1");
                        u16 port = static_cast<u16>(params.Get("port", 8000));

                        if (ImGui::InputText("IP##Outer Right", &ip)) {
                            params.Set("ip", ip);

                            Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = params.Serialize();
                        }

                        if (ImGui::InputScalar("Port##Outer Right", ImGuiDataType_U16, &port, 0,
                                               0)) {
                            params.Set("port", static_cast<int>(port));

                            Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = params.Serialize();
                        }
                    }

                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::OuterRightCamera)] != "blank") {
                        if (ImGui::BeginCombo("Flip##Outer Right", [] {
                                switch (Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)]) {
                                case Service::CAM::Flip::None:
                                    return "None";
                                case Service::CAM::Flip::Horizontal:
                                    return "Horizontal";
                                case Service::CAM::Flip::Vertical:
                                    return "Vertical";
                                case Service::CAM::Flip::Reverse:
                                    return "Reverse";
                                default:
                                    return "Invalid";
                                }
                            }())) {
                            if (ImGui::Selectable(
                                    "None", Settings::values.camera_flip[static_cast<std::size_t>(
                                                Service::CAM::CameraIndex::OuterRightCamera)] ==
                                                Service::CAM::Flip::None)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::None;
                            }

                            if (ImGui::Selectable(
                                    "Horizontal",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::OuterRightCamera)] ==
                                        Service::CAM::Flip::Horizontal)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Horizontal;
                            }

                            if (ImGui::Selectable(
                                    "Vertical",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::OuterRightCamera)] ==
                                        Service::CAM::Flip::Vertical)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Vertical;
                            }

                            if (ImGui::Selectable(
                                    "Reverse",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::OuterRightCamera)] ==
                                        Service::CAM::Flip::Reverse)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Reverse;
                            }

                            ImGui::EndCombo();
                        }
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("System")) {
                    ImGui::TextUnformatted("Config Savegame");
                    ImGui::Separator();

                    std::string username = Common::UTF16ToUTF8(cfg.GetUsername());
                    if (ImGui::InputText("Username", &username)) {
                        cfg.SetUsername(Common::UTF8ToUTF16(username));
                        config_savegame_changed = true;
                    }

                    auto [month, day] = cfg.GetBirthday();

                    if (ImGui::BeginCombo("Birthday Month", [&] {
                            switch (month) {
                            case 1:
                                return "January";
                            case 2:
                                return "February";
                            case 3:
                                return "March";
                            case 4:
                                return "April";
                            case 5:
                                return "May";
                            case 6:
                                return "June";
                            case 7:
                                return "July";
                            case 8:
                                return "August";
                            case 9:
                                return "September";
                            case 10:
                                return "October";
                            case 11:
                                return "November";
                            case 12:
                                return "December";
                            default:
                                break;
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("January", month == 1)) {
                            cfg.SetBirthday(1, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("February", month == 2)) {
                            cfg.SetBirthday(2, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("March", month == 3)) {
                            cfg.SetBirthday(3, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("April", month == 4)) {
                            cfg.SetBirthday(4, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("May", month == 5)) {
                            cfg.SetBirthday(5, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("June", month == 6)) {
                            cfg.SetBirthday(6, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("July", month == 7)) {
                            cfg.SetBirthday(7, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("August", month == 8)) {
                            cfg.SetBirthday(8, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("September", month == 9)) {
                            cfg.SetBirthday(9, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("October", month == 10)) {
                            cfg.SetBirthday(10, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("November", month == 11)) {
                            cfg.SetBirthday(11, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("December", month == 12)) {
                            cfg.SetBirthday(12, day);
                            config_savegame_changed = true;
                        }

                        ImGui::EndCombo();
                    }

                    if (ImGui::InputScalar("Birthday Day", ImGuiDataType_U8, &day)) {
                        cfg.SetBirthday(month, day);
                        config_savegame_changed = true;
                    }

                    const Service::CFG::SystemLanguage language = cfg.GetSystemLanguage();

                    if (ImGui::BeginCombo("Language", [&] {
                            switch (language) {
                            case Service::CFG::SystemLanguage::LANGUAGE_JP:
                                return "Japanese";
                            case Service::CFG::SystemLanguage::LANGUAGE_EN:
                                return "English";
                            case Service::CFG::SystemLanguage::LANGUAGE_FR:
                                return "French";
                            case Service::CFG::SystemLanguage::LANGUAGE_DE:
                                return "German";
                            case Service::CFG::SystemLanguage::LANGUAGE_IT:
                                return "Italian";
                            case Service::CFG::SystemLanguage::LANGUAGE_ES:
                                return "Spanish";
                            case Service::CFG::SystemLanguage::LANGUAGE_ZH:
                                return "Simplified Chinese";
                            case Service::CFG::SystemLanguage::LANGUAGE_KO:
                                return "Korean";
                            case Service::CFG::SystemLanguage::LANGUAGE_NL:
                                return "Dutch";
                            case Service::CFG::SystemLanguage::LANGUAGE_PT:
                                return "Portuguese";
                            case Service::CFG::SystemLanguage::LANGUAGE_RU:
                                return "Russian";
                            case Service::CFG::SystemLanguage::LANGUAGE_TW:
                                return "Traditional Chinese";
                            default:
                                break;
                            }

                            return "Invalid language";
                        }())) {
                        if (ImGui::Selectable("Japanese",
                                              language ==
                                                  Service::CFG::SystemLanguage::LANGUAGE_JP)) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_JP);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable(
                                "English", language == Service::CFG::SystemLanguage::LANGUAGE_EN)) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_EN);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable(
                                "French", language == Service::CFG::SystemLanguage::LANGUAGE_FR)) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_FR);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable(
                                "German", language == Service::CFG::SystemLanguage::LANGUAGE_DE)) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_DE);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable(
                                "Italian", language == Service::CFG::SystemLanguage::LANGUAGE_IT)) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_IT);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable(
                                "Spanish", language == Service::CFG::SystemLanguage::LANGUAGE_ES)) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ES);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Simplified Chinese",
                                              language ==
                                                  Service::CFG::SystemLanguage::LANGUAGE_ZH)) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ZH);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable(
                                "Korean", language == Service::CFG::SystemLanguage::LANGUAGE_KO)) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_KO);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable(
                                "Dutch", language == Service::CFG::SystemLanguage::LANGUAGE_NL)) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_NL);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Portuguese",
                                              language ==
                                                  Service::CFG::SystemLanguage::LANGUAGE_PT)) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_PT);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable(
                                "Russian", language == Service::CFG::SystemLanguage::LANGUAGE_RU)) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_RU);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Traditional Chinese",
                                              language ==
                                                  Service::CFG::SystemLanguage::LANGUAGE_TW)) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_TW);
                            config_savegame_changed = true;
                        }

                        ImGui::EndCombo();
                    }

                    const Service::CFG::SoundOutputMode sound_output_mode =
                        cfg.GetSoundOutputMode();

                    if (ImGui::BeginCombo("Sound Output Mode", [&] {
                            switch (sound_output_mode) {
                            case Service::CFG::SoundOutputMode::SOUND_MONO:
                                return "Mono";
                            case Service::CFG::SoundOutputMode::SOUND_STEREO:
                                return "Stereo";
                            case Service::CFG::SoundOutputMode::SOUND_SURROUND:
                                return "Surround";
                            default:
                                break;
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Mono",
                                              sound_output_mode ==
                                                  Service::CFG::SoundOutputMode::SOUND_MONO)) {
                            cfg.SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_MONO);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Stereo",
                                              sound_output_mode ==
                                                  Service::CFG::SoundOutputMode::SOUND_STEREO)) {
                            cfg.SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_STEREO);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Surround",
                                              sound_output_mode ==
                                                  Service::CFG::SoundOutputMode::SOUND_SURROUND)) {
                            cfg.SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_SURROUND);
                            config_savegame_changed = true;
                        }

                        ImGui::EndCombo();
                    }

                    const u8 country_code = cfg.GetCountryCode();

                    if (ImGui::BeginCombo("Country", [&] {
                            switch (country_code) {
                            case 1:
                                return "Japan";
                            case 8:
                                return "Anguilla";
                            case 9:
                                return "Antigua and Barbuda";
                            case 10:
                                return "Argentina";
                            case 11:
                                return "Aruba";
                            case 12:
                                return "Bahamas";
                            case 13:
                                return "Barbados";
                            case 14:
                                return "Belize";
                            case 15:
                                return "Bolivia";
                            case 16:
                                return "Brazil";
                            case 17:
                                return "British Virgin Islands";
                            case 18:
                                return "Canada";
                            case 19:
                                return "Cayman Islands";
                            case 20:
                                return "Chile";
                            case 21:
                                return "Colombia";
                            case 22:
                                return "Costa Rica";
                            case 23:
                                return "Dominica";
                            case 24:
                                return "Dominican Republic";
                            case 25:
                                return "Ecuador";
                            case 26:
                                return "El Salvador";
                            case 27:
                                return "French Guiana";
                            case 28:
                                return "Grenada";
                            case 29:
                                return "Guadeloupe";
                            case 30:
                                return "Guatemala";
                            case 31:
                                return "Guyana";
                            case 32:
                                return "Haiti";
                            case 33:
                                return "Honduras";
                            case 34:
                                return "Jamaica";
                            case 35:
                                return "Martinique";
                            case 36:
                                return "Mexico";
                            case 37:
                                return "Montserrat";
                            case 38:
                                return "Netherlands Antilles";
                            case 39:
                                return "Nicaragua";
                            case 40:
                                return "Panama";
                            case 41:
                                return "Paraguay";
                            case 42:
                                return "Peru";
                            case 43:
                                return "Saint Kitts and Nevis";
                            case 44:
                                return "Saint Lucia";
                            case 45:
                                return "Saint Vincent and the Grenadines";
                            case 46:
                                return "Suriname";
                            case 47:
                                return "Trinidad and Tobago";
                            case 48:
                                return "Turks and Caicos Islands";
                            case 49:
                                return "United States";
                            case 50:
                                return "Uruguay";
                            case 51:
                                return "US Virgin Islands";
                            case 52:
                                return "Venezuela";
                            case 64:
                                return "Albania";
                            case 65:
                                return "Australia";
                            case 66:
                                return "Austria";
                            case 67:
                                return "Belgium";
                            case 68:
                                return "Bosnia and Herzegovina";
                            case 69:
                                return "Botswana";
                            case 70:
                                return "Bulgaria";
                            case 71:
                                return "Croatia";
                            case 72:
                                return "Cyprus";
                            case 73:
                                return "Czech Republic";
                            case 74:
                                return "Denmark";
                            case 75:
                                return "Estonia";
                            case 76:
                                return "Finland";
                            case 77:
                                return "France";
                            case 78:
                                return "Germany";
                            case 79:
                                return "Greece";
                            case 80:
                                return "Hungary";
                            case 81:
                                return "Iceland";
                            case 82:
                                return "Ireland";
                            case 83:
                                return "Italy";
                            case 84:
                                return "Latvia";
                            case 85:
                                return "Lesotho";
                            case 86:
                                return "Liechtenstein";
                            case 87:
                                return "Lithuania";
                            case 88:
                                return "Luxembourg";
                            case 89:
                                return "Macedonia";
                            case 90:
                                return "Malta";
                            case 91:
                                return "Montenegro";
                            case 92:
                                return "Mozambique";
                            case 93:
                                return "Namibia";
                            case 94:
                                return "Netherlands";
                            case 95:
                                return "New Zealand";
                            case 96:
                                return "Norway";
                            case 97:
                                return "Poland";
                            case 98:
                                return "Portugal";
                            case 99:
                                return "Romania";
                            case 100:
                                return "Russia";
                            case 101:
                                return "Serbia";
                            case 102:
                                return "Slovakia";
                            case 103:
                                return "Slovenia";
                            case 104:
                                return "South Africa";
                            case 105:
                                return "Spain";
                            case 106:
                                return "Swaziland";
                            case 107:
                                return "Sweden";
                            case 108:
                                return "Switzerland";
                            case 109:
                                return "Turkey";
                            case 110:
                                return "United Kingdom";
                            case 111:
                                return "Zambia";
                            case 112:
                                return "Zimbabwe";
                            case 113:
                                return "Azerbaijan";
                            case 114:
                                return "Mauritania";
                            case 115:
                                return "Mali";
                            case 116:
                                return "Niger";
                            case 117:
                                return "Chad";
                            case 118:
                                return "Sudan";
                            case 119:
                                return "Eritrea";
                            case 120:
                                return "Djibouti";
                            case 121:
                                return "Somalia";
                            case 122:
                                return "Andorra";
                            case 123:
                                return "Gibraltar";
                            case 124:
                                return "Guernsey";
                            case 125:
                                return "Isle of Man";
                            case 126:
                                return "Jersey";
                            case 127:
                                return "Monaco";
                            case 128:
                                return "Taiwan";
                            case 136:
                                return "South Korea";
                            case 144:
                                return "Hong Kong";
                            case 145:
                                return "Macau";
                            case 152:
                                return "Indonesia";
                            case 153:
                                return "Singapore";
                            case 154:
                                return "Thailand";
                            case 155:
                                return "Philippines";
                            case 156:
                                return "Malaysia";
                            case 160:
                                return "China";
                            case 168:
                                return "United Arab Emirates";
                            case 169:
                                return "India";
                            case 170:
                                return "Egypt";
                            case 171:
                                return "Oman";
                            case 172:
                                return "Qatar";
                            case 173:
                                return "Kuwait";
                            case 174:
                                return "Saudi Arabia";
                            case 175:
                                return "Syria";
                            case 176:
                                return "Bahrain";
                            case 177:
                                return "Jordan";
                            case 184:
                                return "San Marino";
                            case 185:
                                return "Vatican City";
                            case 186:
                                return "Bermuda";
                            default:
                                break;
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Japan", country_code == 1)) {
                            cfg.SetCountry(1);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Anguilla", country_code == 8)) {
                            cfg.SetCountry(8);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Antigua and Barbuda", country_code == 9)) {
                            cfg.SetCountry(9);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Argentina", country_code == 10)) {
                            cfg.SetCountry(10);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Aruba", country_code == 11)) {
                            cfg.SetCountry(11);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Bahamas", country_code == 12)) {
                            cfg.SetCountry(12);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Barbados", country_code == 13)) {
                            cfg.SetCountry(13);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Belize", country_code == 14)) {
                            cfg.SetCountry(14);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Bolivia", country_code == 15)) {
                            cfg.SetCountry(15);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Brazil", country_code == 16)) {
                            cfg.SetCountry(16);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("British Virgin Islands", country_code == 17)) {
                            cfg.SetCountry(17);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Canada", country_code == 18)) {
                            cfg.SetCountry(18);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Cayman Islands", country_code == 19)) {
                            cfg.SetCountry(19);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Chile", country_code == 20)) {
                            cfg.SetCountry(20);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Colombia", country_code == 21)) {
                            cfg.SetCountry(21);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Costa Rica", country_code == 22)) {
                            cfg.SetCountry(22);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Dominica", country_code == 23)) {
                            cfg.SetCountry(23);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Dominican Republic", country_code == 24)) {
                            cfg.SetCountry(24);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Ecuador", country_code == 25)) {
                            cfg.SetCountry(25);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("El Salvador", country_code == 26)) {
                            cfg.SetCountry(26);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("French Guiana", country_code == 27)) {
                            cfg.SetCountry(27);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Grenada", country_code == 28)) {
                            cfg.SetCountry(28);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Guadeloupe", country_code == 29)) {
                            cfg.SetCountry(29);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Guatemala", country_code == 30)) {
                            cfg.SetCountry(30);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Guyana", country_code == 31)) {
                            cfg.SetCountry(31);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Haiti", country_code == 32)) {
                            cfg.SetCountry(32);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Honduras", country_code == 33)) {
                            cfg.SetCountry(33);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Jamaica", country_code == 34)) {
                            cfg.SetCountry(34);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Martinique", country_code == 35)) {
                            cfg.SetCountry(35);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Mexico", country_code == 36)) {
                            cfg.SetCountry(36);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Montserrat", country_code == 37)) {
                            cfg.SetCountry(37);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Netherlands Antilles", country_code == 38)) {
                            cfg.SetCountry(38);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Nicaragua", country_code == 39)) {
                            cfg.SetCountry(39);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Panama", country_code == 40)) {
                            cfg.SetCountry(40);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Paraguay", country_code == 41)) {
                            cfg.SetCountry(41);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Peru", country_code == 42)) {
                            cfg.SetCountry(42);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Saint Kitts and Nevis", country_code == 43)) {
                            cfg.SetCountry(43);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Saint Lucia", country_code == 44)) {
                            cfg.SetCountry(44);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Saint Vincent and the Grenadines",
                                              country_code == 45)) {
                            cfg.SetCountry(45);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Suriname", country_code == 46)) {
                            cfg.SetCountry(46);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Trinidad and Tobago", country_code == 47)) {
                            cfg.SetCountry(47);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Turks and Caicos Islands", country_code == 48)) {
                            cfg.SetCountry(48);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("United States", country_code == 49)) {
                            cfg.SetCountry(49);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Uruguay", country_code == 50)) {
                            cfg.SetCountry(50);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("US Virgin Islands", country_code == 51)) {
                            cfg.SetCountry(51);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Venezuela", country_code == 52)) {
                            cfg.SetCountry(52);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Albania", country_code == 64)) {
                            cfg.SetCountry(64);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Australia", country_code == 65)) {
                            cfg.SetCountry(65);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Austria", country_code == 66)) {
                            cfg.SetCountry(66);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Belgium", country_code == 67)) {
                            cfg.SetCountry(67);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Bosnia and Herzegovina", country_code == 68)) {
                            cfg.SetCountry(68);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Botswana", country_code == 69)) {
                            cfg.SetCountry(69);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Bulgaria", country_code == 70)) {
                            cfg.SetCountry(70);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Croatia", country_code == 71)) {
                            cfg.SetCountry(71);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Cyprus", country_code == 72)) {
                            cfg.SetCountry(72);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Czech Republic", country_code == 73)) {
                            cfg.SetCountry(73);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Denmark", country_code == 74)) {
                            cfg.SetCountry(74);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Estonia", country_code == 75)) {
                            cfg.SetCountry(75);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Finland", country_code == 76)) {
                            cfg.SetCountry(76);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("France", country_code == 77)) {
                            cfg.SetCountry(77);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Germany", country_code == 78)) {
                            cfg.SetCountry(78);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Greece", country_code == 79)) {
                            cfg.SetCountry(79);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Hungary", country_code == 80)) {
                            cfg.SetCountry(80);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Iceland", country_code == 81)) {
                            cfg.SetCountry(81);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Ireland", country_code == 82)) {
                            cfg.SetCountry(82);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Italy", country_code == 83)) {
                            cfg.SetCountry(83);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Latvia", country_code == 84)) {
                            cfg.SetCountry(84);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Lesotho", country_code == 85)) {
                            cfg.SetCountry(85);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Liechtenstein", country_code == 86)) {
                            cfg.SetCountry(86);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Lithuania", country_code == 87)) {
                            cfg.SetCountry(87);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Luxembourg", country_code == 88)) {
                            cfg.SetCountry(88);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Macedonia", country_code == 89)) {
                            cfg.SetCountry(89);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Malta", country_code == 90)) {
                            cfg.SetCountry(90);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Montenegro", country_code == 91)) {
                            cfg.SetCountry(91);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Mozambique", country_code == 92)) {
                            cfg.SetCountry(92);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Namibia", country_code == 93)) {
                            cfg.SetCountry(93);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Netherlands", country_code == 94)) {
                            cfg.SetCountry(94);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("New Zealand", country_code == 95)) {
                            cfg.SetCountry(95);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Norway", country_code == 96)) {
                            cfg.SetCountry(96);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Poland", country_code == 97)) {
                            cfg.SetCountry(97);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Portugal", country_code == 98)) {
                            cfg.SetCountry(98);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Romania", country_code == 99)) {
                            cfg.SetCountry(99);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Russia", country_code == 100)) {
                            cfg.SetCountry(100);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Serbia", country_code == 101)) {
                            cfg.SetCountry(101);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Slovakia", country_code == 102)) {
                            cfg.SetCountry(102);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Slovenia", country_code == 103)) {
                            cfg.SetCountry(103);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("South Africa", country_code == 104)) {
                            cfg.SetCountry(104);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Spain", country_code == 105)) {
                            cfg.SetCountry(105);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Swaziland", country_code == 106)) {
                            cfg.SetCountry(106);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Sweden", country_code == 107)) {
                            cfg.SetCountry(107);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Switzerland", country_code == 108)) {
                            cfg.SetCountry(108);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Turkey", country_code == 109)) {
                            cfg.SetCountry(109);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("United Kingdom", country_code == 110)) {
                            cfg.SetCountry(110);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Zambia", country_code == 111)) {
                            cfg.SetCountry(111);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Zimbabwe", country_code == 112)) {
                            cfg.SetCountry(112);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Azerbaijan", country_code == 113)) {
                            cfg.SetCountry(113);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Mauritania", country_code == 114)) {
                            cfg.SetCountry(114);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Mali", country_code == 115)) {
                            cfg.SetCountry(115);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Niger", country_code == 116)) {
                            cfg.SetCountry(116);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Chad", country_code == 117)) {
                            cfg.SetCountry(117);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Sudan", country_code == 118)) {
                            cfg.SetCountry(118);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Eritrea", country_code == 119)) {
                            cfg.SetCountry(119);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Djibouti", country_code == 120)) {
                            cfg.SetCountry(120);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Somalia", country_code == 121)) {
                            cfg.SetCountry(121);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Andorra", country_code == 122)) {
                            cfg.SetCountry(122);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Gibraltar", country_code == 123)) {
                            cfg.SetCountry(123);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Guernsey", country_code == 124)) {
                            cfg.SetCountry(124);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Isle of Man", country_code == 125)) {
                            cfg.SetCountry(125);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Jersey", country_code == 126)) {
                            cfg.SetCountry(126);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Monaco", country_code == 127)) {
                            cfg.SetCountry(127);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Taiwan", country_code == 128)) {
                            cfg.SetCountry(128);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("South Korea", country_code == 136)) {
                            cfg.SetCountry(136);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Hong Kong", country_code == 144)) {
                            cfg.SetCountry(144);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Macau", country_code == 145)) {
                            cfg.SetCountry(145);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Indonesia", country_code == 152)) {
                            cfg.SetCountry(152);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Singapore", country_code == 153)) {
                            cfg.SetCountry(153);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Thailand", country_code == 154)) {
                            cfg.SetCountry(154);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Philippines", country_code == 155)) {
                            cfg.SetCountry(155);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Malaysia", country_code == 156)) {
                            cfg.SetCountry(156);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("China", country_code == 160)) {
                            cfg.SetCountry(160);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("United Arab Emirates", country_code == 168)) {
                            cfg.SetCountry(168);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("India", country_code == 169)) {
                            cfg.SetCountry(169);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Egypt", country_code == 170)) {
                            cfg.SetCountry(170);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Oman", country_code == 171)) {
                            cfg.SetCountry(171);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Qatar", country_code == 172)) {
                            cfg.SetCountry(172);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Kuwait", country_code == 173)) {
                            cfg.SetCountry(173);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Saudi Arabia", country_code == 174)) {
                            cfg.SetCountry(174);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Syria", country_code == 175)) {
                            cfg.SetCountry(175);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Bahrain", country_code == 176)) {
                            cfg.SetCountry(176);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Jordan", country_code == 177)) {
                            cfg.SetCountry(177);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("San Marino", country_code == 184)) {
                            cfg.SetCountry(184);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Vatican City", country_code == 185)) {
                            cfg.SetCountry(185);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Bermuda", country_code == 186)) {
                            cfg.SetCountry(186);
                            config_savegame_changed = true;
                        }

                        ImGui::EndCombo();
                    }

                    if (ImGui::Button("Regenerate Console ID")) {
                        u32 random_number;
                        u64 console_id;
                        cfg.GenerateConsoleUniqueId(random_number, console_id);
                        cfg.SetConsoleUniqueId(random_number, console_id);
                        config_savegame_changed = true;
                    }

                    if (ImGui::BeginPopupContextItem("Console ID",
                                                     ImGuiPopupFlags_MouseButtonRight)) {
                        std::string console_id = fmt::format("0x{:016X}", cfg.GetConsoleUniqueId());
                        ImGui::InputText("##Console ID", &console_id[0], 18,
                                         ImGuiInputTextFlags_ReadOnly);
                        ImGui::EndPopup();
                    }

                    ImGui::NewLine();
                    ImGui::TextUnformatted("Play Coins");
                    ImGui::Separator();

                    const u16 min = 0;
                    const u16 max = 300;

                    if (play_coins == 0xDEAD) {
                        play_coins = Service::PTM::Module::GetPlayCoins();
                    }

                    if (ImGui::SliderScalar("Play Coins", ImGuiDataType_U16, &play_coins, &min,
                                            &max)) {
                        play_coins_changed = true;
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Graphics")) {
                    ImGui::Checkbox("Use Hardware Renderer",
                                    &Settings::values.use_hardware_renderer);

                    if (Settings::values.use_hardware_renderer) {
                        ImGui::Indent();

                        ImGui::Checkbox("Use Hardware Shader",
                                        &Settings::values.use_hardware_shader);

                        if (Settings::values.use_hardware_shader) {
                            ImGui::Indent();

                            ImGui::Checkbox(
                                "Accurate Multiplication",
                                &Settings::values.hardware_shader_accurate_multiplication);

                            ImGui::Checkbox("Enable Disk Shader Cache",
                                            &Settings::values.enable_disk_shader_cache);

                            ImGui::Unindent();
                        }

                        ImGui::Checkbox("Sharper Distant Objects",
                                        &Settings::values.sharper_distant_objects);

                        ImGui::Checkbox("Use Custom Textures",
                                        &Settings::values.use_custom_textures);

                        ImGui::Checkbox("Preload Custom Textures",
                                        &Settings::values.preload_custom_textures);

                        if (Settings::values.preload_custom_textures) {
                            ImGui::Indent();

                            if (ImGui::BeginCombo(
                                    "Folder", Settings::values.preload_custom_textures_folder ==
                                                      Settings::PreloadCustomTexturesFolder::Load
                                                  ? "load"
                                                  : "preload")) {
                                if (ImGui::Selectable(
                                        "load", Settings::values.preload_custom_textures_folder ==
                                                    Settings::PreloadCustomTexturesFolder::Load)) {
                                    Settings::values.preload_custom_textures_folder =
                                        Settings::PreloadCustomTexturesFolder::Load;
                                }

                                if (ImGui::Selectable(
                                        "preload",
                                        Settings::values.preload_custom_textures_folder ==
                                            Settings::PreloadCustomTexturesFolder::Preload)) {
                                    Settings::values.preload_custom_textures_folder =
                                        Settings::PreloadCustomTexturesFolder::Preload;
                                }

                                ImGui::EndCombo();
                            }

                            ImGui::Unindent();
                        }

                        ImGui::Checkbox("Dump Textures", &Settings::values.dump_textures);

                        const u16 min = 0;
                        const u16 max = 10;
                        ImGui::SliderScalar(
                            "Resolution", ImGuiDataType_U16, &Settings::values.resolution, &min,
                            &max, Settings::values.resolution == 0 ? "Window Size" : "%d");

                        if (ImGui::BeginCombo("Texture Filter",
                                              Settings::values.texture_filter.c_str())) {
                            const std::vector<std::string_view>& filters =
                                OpenGL::TextureFilterer::GetFilterNames();

                            for (const std::string_view& filter : filters) {
                                if (ImGui::Selectable(std::string(filter).c_str(),
                                                      Settings::values.texture_filter == filter)) {
                                    Settings::values.texture_filter = filter;
                                }
                            }

                            ImGui::EndCombo();
                        }

                        ImGui::Unindent();
                    }

                    ImGui::Checkbox("Use Shader JIT", &Settings::values.use_shader_jit);
                    ImGui::Checkbox("Enable VSync", &Settings::values.enable_vsync);

                    ImGui::Checkbox("Enable Linear Filtering",
                                    &Settings::values.enable_linear_filtering);
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(
                            "This is required for some shaders to work correctly");
                        ImGui::EndTooltip();
                    }

                    ImGui::ColorEdit3("Background Color", &Settings::values.background_color_red,
                                      ImGuiColorEditFlags_NoInputs);

                    ImGui::InputText("Post Processing Shader",
                                     &Settings::values.post_processing_shader);

                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::PushTextWrapPos(io.DisplaySize.x * 0.5f);

                        if (Settings::values.render_3d == Settings::StereoRenderOption::Anaglyph) {
                            ImGui::TextUnformatted(
                                "This can be a file name without the extension and folder, none "
                                "(builtin), or dubois (builtin)");
                        } else if (Settings::values.render_3d ==
                                       Settings::StereoRenderOption::Interlaced ||
                                   Settings::values.render_3d ==
                                       Settings::StereoRenderOption::ReverseInterlaced) {
                            ImGui::TextUnformatted(
                                "This can be a file name without the extension and folder, none "
                                "(builtin), or horizontal (builtin)");
                        } else {
                            ImGui::TextUnformatted("This can be a file name without the extension "
                                                   "and folder, or none (builtin)");
                        }

                        ImGui::PopTextWrapPos();
                        ImGui::EndTooltip();
                    }

                    if (ImGui::BeginCombo("3D Mode", [] {
                            switch (Settings::values.render_3d) {
                            case Settings::StereoRenderOption::Off:
                                return "Off";
                            case Settings::StereoRenderOption::SideBySide:
                                return "Side by Side";
                            case Settings::StereoRenderOption::Anaglyph:
                                return "Anaglyph";
                            case Settings::StereoRenderOption::Interlaced:
                                return "Interlaced";
                            case Settings::StereoRenderOption::ReverseInterlaced:
                                return "Reverse Interlaced";
                            default:
                                break;
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Off", Settings::values.render_3d ==
                                                         Settings::StereoRenderOption::Off)) {
                            if (Settings::values.render_3d ==
                                    Settings::StereoRenderOption::Anaglyph &&
                                Settings::values.post_processing_shader == "dubois (builtin)") {
                                Settings::values.post_processing_shader = "none (builtin)";
                            }

                            if ((Settings::values.render_3d ==
                                     Settings::StereoRenderOption::Interlaced ||
                                 Settings::values.render_3d ==
                                     Settings::StereoRenderOption::ReverseInterlaced) &&
                                Settings::values.post_processing_shader == "horizontal (builtin)") {
                                Settings::values.post_processing_shader = "none (builtin)";
                            }

                            Settings::values.render_3d = Settings::StereoRenderOption::Off;
                        }

                        if (ImGui::Selectable("Side by Side",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::SideBySide)) {
                            if (Settings::values.render_3d ==
                                    Settings::StereoRenderOption::Anaglyph &&
                                Settings::values.post_processing_shader == "dubois (builtin)") {
                                Settings::values.post_processing_shader = "none (builtin)";
                            }

                            if ((Settings::values.render_3d ==
                                     Settings::StereoRenderOption::Interlaced ||
                                 Settings::values.render_3d ==
                                     Settings::StereoRenderOption::ReverseInterlaced) &&
                                Settings::values.post_processing_shader == "horizontal (builtin)") {
                                Settings::values.post_processing_shader = "none (builtin)";
                            }

                            Settings::values.render_3d = Settings::StereoRenderOption::SideBySide;
                        }

                        if (ImGui::Selectable("Anaglyph",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Anaglyph)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Anaglyph;

                            if (Settings::values.post_processing_shader != "dubois (builtin)") {
                                Settings::values.post_processing_shader = "dubois (builtin)";
                            }
                        }

                        if (ImGui::Selectable("Interlaced",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Interlaced)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Interlaced;

                            if (Settings::values.post_processing_shader != "horizontal (builtin)") {
                                Settings::values.post_processing_shader = "horizontal (builtin)";
                            }
                        }

                        if (ImGui::Selectable(
                                "Reverse Interlaced",
                                Settings::values.render_3d ==
                                    Settings::StereoRenderOption::ReverseInterlaced)) {
                            Settings::values.render_3d =
                                Settings::StereoRenderOption::ReverseInterlaced;

                            if (Settings::values.post_processing_shader != "horizontal (builtin)") {
                                Settings::values.post_processing_shader = "horizontal (builtin)";
                            }
                        }

                        ImGui::EndCombo();
                    }

                    u8 factor_3d = Settings::values.factor_3d;
                    const u8 factor_3d_min = 0;
                    const u8 factor_3d_max = 100;
                    if (ImGui::SliderScalar("3D Factor", ImGuiDataType_U8, &factor_3d,
                                            &factor_3d_min, &factor_3d_max, "%d%%")) {
                        Settings::values.factor_3d = factor_3d;
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Controls")) {
                    GUI_AddControlsSettings(is_open, nullptr, plugin_manager, io, nullptr);

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Layout")) {
                    if (!Settings::values.use_custom_layout) {
                        if (ImGui::BeginCombo("Layout", [] {
                                switch (Settings::values.layout) {
                                case Settings::Layout::Default:
                                    return "Default";
                                case Settings::Layout::SingleScreen:
                                    return "Single Screen";
                                case Settings::Layout::LargeScreen:
                                    return "Large Screen";
                                case Settings::Layout::SideScreen:
                                    return "Side by Side";
                                case Settings::Layout::MediumScreen:
                                    return "Medium Screen";
                                default:
                                    break;
                                }

                                return "Invalid";
                            }())) {
                            if (ImGui::Selectable("Default", Settings::values.layout ==
                                                                 Settings::Layout::Default)) {
                                Settings::values.layout = Settings::Layout::Default;
                            }

                            if (ImGui::Selectable("Single Screen",
                                                  Settings::values.layout ==
                                                      Settings::Layout::SingleScreen)) {
                                Settings::values.layout = Settings::Layout::SingleScreen;
                            }

                            if (ImGui::Selectable("Large Screen",
                                                  Settings::values.layout ==
                                                      Settings::Layout::LargeScreen)) {
                                Settings::values.layout = Settings::Layout::LargeScreen;
                            }

                            if (ImGui::Selectable("Side by Side",
                                                  Settings::values.layout ==
                                                      Settings::Layout::SideScreen)) {
                                Settings::values.layout = Settings::Layout::SideScreen;
                            }

                            if (ImGui::Selectable("Medium Screen",
                                                  Settings::values.layout ==
                                                      Settings::Layout::MediumScreen)) {
                                Settings::values.layout = Settings::Layout::MediumScreen;
                            }

                            ImGui::EndCombo();
                        }
                    }

                    ImGui::Checkbox("Use Custom Layout", &Settings::values.use_custom_layout);
                    ImGui::Checkbox("Swap Screens", &Settings::values.swap_screens);
                    ImGui::Checkbox("Upright Screens", &Settings::values.upright_screens);

                    if (Settings::values.use_custom_layout) {
                        ImGui::NewLine();

                        ImGui::TextUnformatted("Top Screen");
                        ImGui::Separator();

                        ImGui::InputScalar("Left##Top Screen", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_top_left);

                        ImGui::InputScalar("Top##Top Screen", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_top_top);

                        ImGui::InputScalar("Right##Top Screen", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_top_right);

                        ImGui::InputScalar("Bottom##Top Screen", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_top_bottom);

                        ImGui::NewLine();

                        ImGui::TextUnformatted("Bottom Screen");
                        ImGui::Separator();

                        ImGui::InputScalar("Left##Bottom Screen", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_bottom_left);

                        ImGui::InputScalar("Top##Bottom Screen", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_bottom_top);

                        ImGui::InputScalar("Right##Bottom Screen", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_bottom_right);

                        ImGui::InputScalar("Bottom##Bottom Screen", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_bottom_bottom);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Multiplayer")) {
                    if (first_time_in_multiplayer) {
                        if (!ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT)) {
                            all_public_rooms = GetPublicCitraRooms();
                        }
                        first_time_in_multiplayer = false;
                    }

                    ImGui::InputText("IP", &Settings::values.multiplayer_ip);
                    ImGui::InputScalar("Port", ImGuiDataType_U16,
                                       &Settings::values.multiplayer_port);

                    ImGui::InputText("Nickname", &Settings::values.multiplayer_nickname);
                    ImGui::InputText("Password", &Settings::values.multiplayer_password);

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Public Rooms");

                    if (ImGui::Button("Refresh")) {
                        all_public_rooms = GetPublicCitraRooms();

                        public_rooms_search_text = public_rooms_search_text_;
                        public_rooms_search_results.clear();

                        const std::string lower_case_text =
                            Common::ToLower(public_rooms_search_text);

                        if (!public_rooms_search_text.empty()) {
                            for (const CitraRoom& room : all_public_rooms) {
                                if ((Common::ToLower(room.name).find(lower_case_text) !=
                                     std::string::npos) ||
                                    (Common::ToLower(GetRoomPopupText(room))
                                         .find(lower_case_text) != std::string::npos)) {
                                    public_rooms_search_results.push_back(room);
                                }
                            }
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::InputTextWithHint("##search", "Search", &public_rooms_search_text_,
                                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                        public_rooms_search_text = public_rooms_search_text_;
                        public_rooms_search_results.clear();

                        const std::string lower_case_text =
                            Common::ToLower(public_rooms_search_text);

                        if (!public_rooms_search_text.empty()) {
                            for (const CitraRoom& room : all_public_rooms) {
                                if ((Common::ToLower(room.name).find(lower_case_text) !=
                                     std::string::npos) ||
                                    (Common::ToLower(GetRoomPopupText(room))
                                         .find(lower_case_text) != std::string::npos)) {
                                    public_rooms_search_results.push_back(room);
                                }
                            }
                        }
                    }

                    if (ImGui::BeginChildFrame(
                            ImGui::GetID("Public Room List"),
                            ImVec2(-1.0f, (!Settings::values.file_path.empty() || update_found)
                                              ? -ImGui::GetFrameHeightWithSpacing() * 2.0f
                                              : -1.0f),
                            ImGuiWindowFlags_HorizontalScrollbar)) {
                        const CitraRoomList& rooms = public_rooms_search_text.empty()
                                                         ? all_public_rooms
                                                         : public_rooms_search_results;

                        ImGuiListClipper clipper;
                        clipper.Begin(rooms.size());

                        while (clipper.Step()) {
                            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                                const CitraRoom& room = rooms[i];
                                const std::string popup_text = GetRoomPopupText(room);
                                const std::string id = fmt::format("{}##i={}", room.name, i);

                                if (room.has_password) {
                                    ImGui::PushStyleColor(ImGuiCol_Text,
                                                          ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                                    ImGui::Selectable(id.c_str());
                                    ImGui::PopStyleColor();
                                } else {
                                    ImGui::Selectable(id.c_str());
                                }

                                if (ImGui::IsItemClicked()) {
                                    ImGui::OpenPopup(id.c_str());
                                }

                                if (ImGui::BeginPopup(id.c_str(),
                                                      ImGuiWindowFlags_HorizontalScrollbar)) {
                                    ImGui::TextUnformatted(popup_text.c_str());

                                    if (ImGui::Button("Set IP And Port")) {
                                        Settings::values.multiplayer_ip = room.ip;
                                        Settings::values.multiplayer_port = room.port;
                                        ImGui::CloseCurrentPopup();
                                    }

                                    ImGui::SameLine();

                                    if (ImGui::Button("Close")) {
                                        ImGui::CloseCurrentPopup();
                                    }

                                    ImGui::EndPopup();
                                }
                            }
                        }
                    }
                    ImGui::EndChildFrame();

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("LLE Modules")) {
                    for (auto& module : Settings::values.lle_modules) {
                        ImGui::Checkbox(module.first.c_str(), &module.second);
                    }

                    ImGui::EndTabItem();
                }

                if (!host_multiplayer_room_room_created &&
                    ImGui::BeginTabItem("Host Multiplayer Room")) {
                    ImGui::InputText("IP", &host_multiplayer_room_ip);

                    ImGui::InputScalar("Port", ImGuiDataType_U16, &host_multiplayer_room_port);

                    ImGui::InputScalar("Member Slots", ImGuiDataType_U32,
                                       &host_multiplayer_room_member_slots);

                    ImGui::NewLine();

                    ImGui::PushTextWrapPos(io.DisplaySize.x);
                    ImGui::TextUnformatted("No more settings will be added here, this doesn't "
                                           "have message length limit, and there's no "
                                           "nickname/console ID/MAC address checks.");
                    ImGui::PopTextWrapPos();

                    ImGui::NewLine();

                    if (ImGui::Button("Create Room & Close This Tab")) {
                        new Network::Room(host_multiplayer_room_ip, host_multiplayer_room_port,
                                          host_multiplayer_room_member_slots);
                        host_multiplayer_room_room_created = true;
                    }

                    ImGui::EndTabItem();
                }

                plugin_manager.AddTabs();

                ImGui::EndTabBar();
            }

            if (!Settings::values.file_path.empty()) {
                ImGui::Dummy(
                    ImVec2(0.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFontSize() - 10.0f));

                if (ImGui::Button("OK")) {
                    Settings::Apply();
                    if (config_savegame_changed) {
                        cfg.UpdateConfigNANDSavegame();
                    }
                    if (play_coins_changed) {
                        Service::PTM::Module::SetPlayCoins(play_coins);
                    }
                    return;
                }

                ImGui::SameLine();

                if (ImGui::Button("OK (Multiplayer)")) {
                    Settings::Apply();
                    if (config_savegame_changed) {
                        cfg.UpdateConfigNANDSavegame();
                    }
                    if (play_coins_changed) {
                        Service::PTM::Module::SetPlayCoins(play_coins);
                    }
                    ok_multiplayer = true;
                    return;
                }

                if (update_found) {
                    ImGui::SameLine();

                    if (ImGui::Button("Update")) {
#ifdef _WIN32
                        [[maybe_unused]] const int code = std::system(
                            "start https://github.com/vvanelslande/vvctre/releases/latest");
#else
                        [[maybe_unused]] const int code =
                            std::system("xdg-open "
                                        "https://github.com/vvanelslande/vvctre/releases/latest");
#endif

                        Settings::values.file_path.clear();
                        return;
                    }
                }
            } else if (update_found) {
                ImGui::Dummy(
                    ImVec2(0.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFontSize() - 10.0f));

                if (ImGui::Button("Update")) {
#ifdef _WIN32
                    [[maybe_unused]] const int code =
                        std::system("start https://github.com/vvanelslande/vvctre/releases/latest");
#else
                    [[maybe_unused]] const int code =
                        std::system("xdg-open "
                                    "https://github.com/vvanelslande/vvctre/releases/latest");
#endif

                    return;
                }
            }
        }

        if (!all_installed.empty()) {
            ImGui::OpenPopup("Installed");

            ImGui::SetNextWindowPos(ImVec2());
            ImGui::SetNextWindowSize(io.DisplaySize);

            bool open = true;
            if (ImGui::BeginPopupModal("Installed", &open,
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoResize)) {
                if (ImGui::InputTextWithHint("##search", "Search", &installed_search_text_,
                                             ImGuiInputTextFlags_EnterReturnsTrue)) {
                    installed_search_text = installed_search_text_;
                    installed_search_results.clear();

                    if (!installed_search_text.empty()) {
                        for (const auto& title : all_installed) {
                            const auto [path, name] = title;

                            if (Common::ToLower(name).find(
                                    Common::ToLower(installed_search_text)) != std::string::npos) {
                                installed_search_results.push_back(title);
                            }
                        }
                    }
                }

                if (ImGui::BeginChildFrame(ImGui::GetID("Installed"), ImVec2(-1.0f, -1.0f),
                                           ImGuiWindowFlags_HorizontalScrollbar)) {
                    const auto& v =
                        installed_search_text.empty() ? all_installed : installed_search_results;

                    ImGuiListClipper clipper;
                    clipper.Begin(v.size());

                    while (clipper.Step()) {
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            const auto [path, name] = v[i];

                            if (ImGui::Selectable(name.c_str())) {
                                Settings::values.file_path = path;
                                all_installed.clear();
                                installed_search_results.clear();
                                installed_search_text.clear();
                                installed_search_text_.clear();
                                break;
                            }
                        }
                    }
                    ImGui::EndChildFrame();
                }

                ImGui::EndPopup();
            }
            if (!open) {
                all_installed.clear();
                installed_search_results.clear();
                installed_search_text.clear();
                installed_search_text_.clear();
            }
        }

        ImGui::End();

        glClearColor(Settings::values.background_color_red, Settings::values.background_color_green,
                     Settings::values.background_color_blue, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }
}
