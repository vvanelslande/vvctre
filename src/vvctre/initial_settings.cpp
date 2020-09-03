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
#ifdef HAVE_CUBEB
#include "audio_core/cubeb_input.h"
#endif
#include <asl/JSON.h>
#include <asl/Process.h>
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
    CitraRoomList public_rooms;
    bool first_time_in_multiplayer = true;
    std::string public_rooms_query;
    u16 play_coins = 0xDEAD;
    bool play_coins_changed = false;
    bool config_savegame_changed = false;
    std::vector<std::tuple<std::string, std::string>> installed;
    std::string installed_query;
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
        if (ImGui::Begin("Initial Settings", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoMove)) {
            ImGui::SetWindowPos(ImVec2(), ImGuiCond_Once);
            ImGui::SetWindowSize(io.DisplaySize);
            if (ImGui::BeginTabBar("Tab Bar", ImGuiTabBarFlags_TabListPopupButton |
                                                  ImGuiTabBarFlags_FittingPolicyScroll)) {
                if (ImGui::BeginTabItem("Start")) {
                    ImGui::Button("...##file");
                    if (ImGui::BeginPopupContextItem("File", ImGuiPopupFlags_MouseButtonLeft)) {
                        if (ImGui::MenuItem("Browse")) {
                            const std::vector<std::string> result =
                                pfd::open_file("Browse", *asl::Process::myDir(),
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
                            const std::vector<std::string> files =
                                pfd::open_file("Install CIA", *asl::Process::myDir(),
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

                                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                                glClear(GL_COLOR_BUFFER_BIT);
                                ImGui::Render();
                                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                                SDL_GL_SwapWindow(window);
                            }

                            continue;
                        }
                        if (ImGui::MenuItem("Installed")) {
                            installed = GetInstalledList();
                        }
                        if (ImGui::MenuItem("HOME Menu")) {
                            if (Settings::values.region_value ==
                                Settings::REGION_VALUE_AUTO_SELECT) {
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
                            const std::vector<std::string> result =
                                pfd::open_file("Play Movie", *asl::Process::myDir(),
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
                            case -1:
                                return "Auto-select";
                            case 0:
                                return "Japan";
                            case 1:
                                return "USA";
                            case 2:
                                return "Europe";
                            case 3:
                                return "Australia";
                            case 4:
                                return "China";
                            case 5:
                                return "Korea";
                            case 6:
                                return "Taiwan";
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Auto-select")) {
                            Settings::values.region_value = Settings::REGION_VALUE_AUTO_SELECT;
                        }
                        if (ImGui::Selectable("Japan")) {
                            Settings::values.region_value = 0;
                        }
                        if (ImGui::Selectable("USA")) {
                            Settings::values.region_value = 1;
                        }
                        if (ImGui::Selectable("Europe")) {
                            Settings::values.region_value = 2;
                        }
                        if (ImGui::Selectable("Australia")) {
                            Settings::values.region_value = 3;
                        }
                        if (ImGui::Selectable("China")) {
                            Settings::values.region_value = 4;
                        }
                        if (ImGui::Selectable("Korea")) {
                            Settings::values.region_value = 5;
                        }
                        if (ImGui::Selectable("Taiwan")) {
                            Settings::values.region_value = 6;
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
                        if (ImGui::Selectable("System")) {
                            Settings::values.initial_clock = Settings::InitialClock::System;
                        }

                        if (ImGui::Selectable("Unix Timestamp")) {
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
                    ImGui::Checkbox("Use CPU JIT", &Settings::values.use_cpu_jit);
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
                    ImGui::SliderScalar("CPU Clock\nPercentage", ImGuiDataType_U32,
                                        &Settings::values.cpu_clock_percentage, &min, &max, "%d%%");

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Audio")) {
                    ImGui::Checkbox("Enable DSP LLE", &Settings::values.enable_dsp_lle);

                    if (Settings::values.enable_dsp_lle) {
                        ImGui::Indent();
                        ImGui::Checkbox("Use multiple threads",
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
                        if (ImGui::Selectable("auto")) {
                            Settings::values.audio_sink_id = "auto";
                        }
                        for (const auto& sink : AudioCore::GetSinkIDs()) {
                            if (ImGui::Selectable(sink)) {
                                Settings::values.audio_sink_id = sink;
                            }
                        }
                        ImGui::EndCombo();
                    }

                    if (ImGui::BeginCombo("Device##Output",
                                          Settings::values.audio_device_id.c_str())) {
                        if (ImGui::Selectable("auto")) {
                            Settings::values.audio_device_id = "auto";
                        }

                        for (const auto& device :
                             AudioCore::GetDeviceListForSink(Settings::values.audio_sink_id)) {
                            if (ImGui::Selectable(device.c_str())) {
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
                        if (ImGui::Selectable("Disabled")) {
                            Settings::values.microphone_input_type =
                                Settings::MicrophoneInputType::None;
                        }
                        if (ImGui::Selectable("Real Device")) {
                            Settings::values.microphone_input_type =
                                Settings::MicrophoneInputType::Real;
                        }
                        if (ImGui::Selectable("Static Noise")) {
                            Settings::values.microphone_input_type =
                                Settings::MicrophoneInputType::Static;
                        }
                        ImGui::EndCombo();
                    }

                    if (Settings::values.microphone_input_type ==
                        Settings::MicrophoneInputType::Real) {
                        if (ImGui::BeginCombo("Device##Microphone",
                                              Settings::values.microphone_device.c_str())) {
                            if (ImGui::Selectable("auto")) {
                                Settings::values.microphone_device = "auto";
                            }
#ifdef HAVE_CUBEB
                            for (const auto& device : AudioCore::ListCubebInputDevices()) {
                                if (ImGui::Selectable(device.c_str())) {
                                    Settings::values.microphone_device = device;
                                }
                            }
#endif

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
                        if (ImGui::Selectable("blank")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "blank";
                        }
                        if (ImGui::Selectable("image (parameter: file path or URL)")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "image";
                        }
                        ImGui::EndCombo();
                    }
                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::InnerCamera)] == "image") {
                        GUI_CameraAddBrowse(
                            "...##Inner",
                            static_cast<std::size_t>(Service::CAM::CameraIndex::InnerCamera));
                        ImGui::InputText(
                            "Parameter##Inner",
                            &Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)]);
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
                            if (ImGui::Selectable("None")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::None;
                            }
                            if (ImGui::Selectable("Horizontal")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Horizontal;
                            }
                            if (ImGui::Selectable("Vertical")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Vertical;
                            }
                            if (ImGui::Selectable("Reverse")) {
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
                        if (ImGui::Selectable("blank")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "blank";
                        }
                        if (ImGui::Selectable("image (parameter: file path or URL)")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "image";
                        }
                        ImGui::EndCombo();
                    }
                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::OuterLeftCamera)] == "image") {
                        GUI_CameraAddBrowse(
                            "...##Outer Left",
                            static_cast<std::size_t>(Service::CAM::CameraIndex::OuterLeftCamera));
                        ImGui::InputText(
                            "Parameter##Outer Left",
                            &Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)]);
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
                            if (ImGui::Selectable("None")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::None;
                            }
                            if (ImGui::Selectable("Horizontal")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Horizontal;
                            }
                            if (ImGui::Selectable("Vertical")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Vertical;
                            }
                            if (ImGui::Selectable("Reverse")) {
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
                        if (ImGui::Selectable("blank")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "blank";
                        }
                        if (ImGui::Selectable("image (parameter: file path or URL)")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "image";
                        }
                        ImGui::EndCombo();
                    }
                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::OuterRightCamera)] == "image") {
                        GUI_CameraAddBrowse(
                            "...##Outer Right",
                            static_cast<std::size_t>(Service::CAM::CameraIndex::OuterRightCamera));
                        ImGui::InputText(
                            "Parameter##Outer Right",
                            &Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)]);
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
                            if (ImGui::Selectable("None")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::None;
                            }
                            if (ImGui::Selectable("Horizontal")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Horizontal;
                            }
                            if (ImGui::Selectable("Vertical")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Vertical;
                            }
                            if (ImGui::Selectable("Reverse")) {
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
                        if (ImGui::Selectable("January")) {
                            cfg.SetBirthday(1, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("February")) {
                            cfg.SetBirthday(2, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("March")) {
                            cfg.SetBirthday(3, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("April")) {
                            cfg.SetBirthday(4, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("May")) {
                            cfg.SetBirthday(5, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("June")) {
                            cfg.SetBirthday(6, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("July")) {
                            cfg.SetBirthday(7, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("August")) {
                            cfg.SetBirthday(8, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("September")) {
                            cfg.SetBirthday(9, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("October")) {
                            cfg.SetBirthday(10, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("November")) {
                            cfg.SetBirthday(11, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("December")) {
                            cfg.SetBirthday(12, day);
                            config_savegame_changed = true;
                        }

                        ImGui::EndCombo();
                    }

                    if (ImGui::InputScalar("Birthday Day", ImGuiDataType_U8, &day)) {
                        cfg.SetBirthday(month, day);
                        config_savegame_changed = true;
                    }

                    if (ImGui::BeginCombo("Language", [&] {
                            switch (cfg.GetSystemLanguage()) {
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
                        if (ImGui::Selectable("Japanese")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_JP);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("English")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_EN);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("French")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_FR);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("German")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_DE);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Italian")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_IT);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Spanish")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ES);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Simplified Chinese")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ZH);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Korean")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_KO);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Dutch")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_NL);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Portuguese")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_PT);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Russian")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_RU);
                            config_savegame_changed = true;
                        }

                        if (ImGui::Selectable("Traditional Chinese")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_TW);
                            config_savegame_changed = true;
                        }

                        ImGui::EndCombo();
                    }

                    if (ImGui::BeginCombo("Sound Output Mode", [&] {
                            switch (cfg.GetSoundOutputMode()) {
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
                        if (ImGui::Selectable("Mono")) {
                            cfg.SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_MONO);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Stereo")) {
                            cfg.SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_STEREO);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Surround")) {
                            cfg.SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_SURROUND);
                            config_savegame_changed = true;
                        }
                        ImGui::EndCombo();
                    }

                    if (ImGui::BeginCombo("Country", [&] {
                            switch (cfg.GetCountryCode()) {
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
                        if (ImGui::Selectable("Japan")) {
                            cfg.SetCountry(1);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Anguilla")) {
                            cfg.SetCountry(8);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Antigua and Barbuda")) {
                            cfg.SetCountry(9);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Argentina")) {
                            cfg.SetCountry(10);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Aruba")) {
                            cfg.SetCountry(11);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Bahamas")) {
                            cfg.SetCountry(12);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Barbados")) {
                            cfg.SetCountry(13);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Belize")) {
                            cfg.SetCountry(14);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Bolivia")) {
                            cfg.SetCountry(15);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Brazil")) {
                            cfg.SetCountry(16);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("British Virgin Islands")) {
                            cfg.SetCountry(17);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Canada")) {
                            cfg.SetCountry(18);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Cayman Islands")) {
                            cfg.SetCountry(19);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Chile")) {
                            cfg.SetCountry(20);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Colombia")) {
                            cfg.SetCountry(21);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Costa Rica")) {
                            cfg.SetCountry(22);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Dominica")) {
                            cfg.SetCountry(23);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Dominican Republic")) {
                            cfg.SetCountry(24);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Ecuador")) {
                            cfg.SetCountry(25);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("El Salvador")) {
                            cfg.SetCountry(26);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("French Guiana")) {
                            cfg.SetCountry(27);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Grenada")) {
                            cfg.SetCountry(28);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Guadeloupe")) {
                            cfg.SetCountry(29);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Guatemala")) {
                            cfg.SetCountry(30);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Guyana")) {
                            cfg.SetCountry(31);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Haiti")) {
                            cfg.SetCountry(32);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Honduras")) {
                            cfg.SetCountry(33);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Jamaica")) {
                            cfg.SetCountry(34);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Martinique")) {
                            cfg.SetCountry(35);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Mexico")) {
                            cfg.SetCountry(36);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Montserrat")) {
                            cfg.SetCountry(37);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Netherlands Antilles")) {
                            cfg.SetCountry(38);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Nicaragua")) {
                            cfg.SetCountry(39);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Panama")) {
                            cfg.SetCountry(40);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Paraguay")) {
                            cfg.SetCountry(41);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Peru")) {
                            cfg.SetCountry(42);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Saint Kitts and Nevis")) {
                            cfg.SetCountry(43);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Saint Lucia")) {
                            cfg.SetCountry(44);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Saint Vincent and the Grenadines")) {
                            cfg.SetCountry(45);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Suriname")) {
                            cfg.SetCountry(46);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Trinidad and Tobago")) {
                            cfg.SetCountry(47);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Turks and Caicos Islands")) {
                            cfg.SetCountry(48);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("United States")) {
                            cfg.SetCountry(49);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Uruguay")) {
                            cfg.SetCountry(50);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("US Virgin Islands")) {
                            cfg.SetCountry(51);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Venezuela")) {
                            cfg.SetCountry(52);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Albania")) {
                            cfg.SetCountry(64);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Australia")) {
                            cfg.SetCountry(65);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Austria")) {
                            cfg.SetCountry(66);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Belgium")) {
                            cfg.SetCountry(67);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Bosnia and Herzegovina")) {
                            cfg.SetCountry(68);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Botswana")) {
                            cfg.SetCountry(69);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Bulgaria")) {
                            cfg.SetCountry(70);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Croatia")) {
                            cfg.SetCountry(71);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Cyprus")) {
                            cfg.SetCountry(72);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Czech Republic")) {
                            cfg.SetCountry(73);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Denmark")) {
                            cfg.SetCountry(74);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Estonia")) {
                            cfg.SetCountry(75);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Finland")) {
                            cfg.SetCountry(76);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("France")) {
                            cfg.SetCountry(77);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Germany")) {
                            cfg.SetCountry(78);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Greece")) {
                            cfg.SetCountry(79);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Hungary")) {
                            cfg.SetCountry(80);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Iceland")) {
                            cfg.SetCountry(81);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Ireland")) {
                            cfg.SetCountry(82);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Italy")) {
                            cfg.SetCountry(83);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Latvia")) {
                            cfg.SetCountry(84);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Lesotho")) {
                            cfg.SetCountry(85);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Liechtenstein")) {
                            cfg.SetCountry(86);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Lithuania")) {
                            cfg.SetCountry(87);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Luxembourg")) {
                            cfg.SetCountry(88);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Macedonia")) {
                            cfg.SetCountry(89);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Malta")) {
                            cfg.SetCountry(90);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Montenegro")) {
                            cfg.SetCountry(91);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Mozambique")) {
                            cfg.SetCountry(92);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Namibia")) {
                            cfg.SetCountry(93);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Netherlands")) {
                            cfg.SetCountry(94);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("New Zealand")) {
                            cfg.SetCountry(95);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Norway")) {
                            cfg.SetCountry(96);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Poland")) {
                            cfg.SetCountry(97);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Portugal")) {
                            cfg.SetCountry(98);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Romania")) {
                            cfg.SetCountry(99);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Russia")) {
                            cfg.SetCountry(100);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Serbia")) {
                            cfg.SetCountry(101);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Slovakia")) {
                            cfg.SetCountry(102);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Slovenia")) {
                            cfg.SetCountry(103);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("South Africa")) {
                            cfg.SetCountry(104);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Spain")) {
                            cfg.SetCountry(105);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Swaziland")) {
                            cfg.SetCountry(106);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Sweden")) {
                            cfg.SetCountry(107);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Switzerland")) {
                            cfg.SetCountry(108);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Turkey")) {
                            cfg.SetCountry(109);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("United Kingdom")) {
                            cfg.SetCountry(110);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Zambia")) {
                            cfg.SetCountry(111);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Zimbabwe")) {
                            cfg.SetCountry(112);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Azerbaijan")) {
                            cfg.SetCountry(113);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Mauritania")) {
                            cfg.SetCountry(114);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Mali")) {
                            cfg.SetCountry(115);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Niger")) {
                            cfg.SetCountry(116);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Chad")) {
                            cfg.SetCountry(117);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Sudan")) {
                            cfg.SetCountry(118);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Eritrea")) {
                            cfg.SetCountry(119);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Djibouti")) {
                            cfg.SetCountry(120);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Somalia")) {
                            cfg.SetCountry(121);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Andorra")) {
                            cfg.SetCountry(122);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Gibraltar")) {
                            cfg.SetCountry(123);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Guernsey")) {
                            cfg.SetCountry(124);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Isle of Man")) {
                            cfg.SetCountry(125);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Jersey")) {
                            cfg.SetCountry(126);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Monaco")) {
                            cfg.SetCountry(127);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Taiwan")) {
                            cfg.SetCountry(128);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("South Korea")) {
                            cfg.SetCountry(136);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Hong Kong")) {
                            cfg.SetCountry(144);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Macau")) {
                            cfg.SetCountry(145);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Indonesia")) {
                            cfg.SetCountry(152);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Singapore")) {
                            cfg.SetCountry(153);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Thailand")) {
                            cfg.SetCountry(154);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Philippines")) {
                            cfg.SetCountry(155);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Malaysia")) {
                            cfg.SetCountry(156);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("China")) {
                            cfg.SetCountry(160);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("United Arab Emirates")) {
                            cfg.SetCountry(168);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("India")) {
                            cfg.SetCountry(169);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Egypt")) {
                            cfg.SetCountry(170);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Oman")) {
                            cfg.SetCountry(171);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Qatar")) {
                            cfg.SetCountry(172);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Kuwait")) {
                            cfg.SetCountry(173);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Saudi Arabia")) {
                            cfg.SetCountry(174);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Syria")) {
                            cfg.SetCountry(175);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Bahrain")) {
                            cfg.SetCountry(176);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Jordan")) {
                            cfg.SetCountry(177);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("San Marino")) {
                            cfg.SetCountry(184);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Vatican City")) {
                            cfg.SetCountry(185);
                            config_savegame_changed = true;
                        }
                        if (ImGui::Selectable("Bermuda")) {
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
                            ImGui::Unindent();
                        }
                        ImGui::Unindent();
                    }
                    ImGui::Checkbox("Use Shader JIT", &Settings::values.use_shader_jit);
                    ImGui::Checkbox("Enable VSync", &Settings::values.enable_vsync);
                    ImGui::Checkbox("Dump Textures", &Settings::values.dump_textures);
                    ImGui::Checkbox("Use Custom Textures", &Settings::values.custom_textures);
                    ImGui::Checkbox("Preload Custom Textures", &Settings::values.preload_textures);
                    ImGui::Checkbox("Enable Linear Filtering",
                                    &Settings::values.enable_linear_filtering);
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(
                            "This is required for some shaders to work correctly");
                        ImGui::EndTooltip();
                    }
                    ImGui::Checkbox("Sharper Distant Objects",
                                    &Settings::values.sharper_distant_objects);

                    ImGui::ColorEdit3("Background Color", &Settings::values.background_color_red,
                                      ImGuiColorEditFlags_NoInputs);

                    const u16 min = 0;
                    const u16 max = 10;
                    ImGui::SliderScalar("Resolution", ImGuiDataType_U16,
                                        &Settings::values.resolution, &min, &max,
                                        Settings::values.resolution == 0 ? "Window Size" : "%d");

                    ImGui::InputText("Post Processing\nShader",
                                     &Settings::values.post_processing_shader);
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted("File name without extension and folder");
                        ImGui::EndTooltip();
                    }

                    if (ImGui::BeginCombo("Texture Filter",
                                          Settings::values.texture_filter.c_str())) {
                        const auto& filters = OpenGL::TextureFilterer::GetFilterNames();

                        for (const auto& filter : filters) {
                            if (ImGui::Selectable(std::string(filter).c_str())) {
                                Settings::values.texture_filter = filter;
                            }
                        }

                        ImGui::EndCombo();
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
                            default:
                                break;
                            }

                            return "Invalid value";
                        }())) {
                        if (ImGui::Selectable("Off", Settings::values.render_3d ==
                                                         Settings::StereoRenderOption::Off)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Off;
                            Settings::values.post_processing_shader = "none (builtin)";
                        }

                        if (ImGui::Selectable("Side by Side",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::SideBySide)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::SideBySide;
                            Settings::values.post_processing_shader = "none (builtin)";
                        }

                        if (ImGui::Selectable("Anaglyph",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Anaglyph)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Anaglyph;
                            Settings::values.post_processing_shader = "dubois (builtin)";
                        }

                        if (ImGui::Selectable("Interlaced",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Interlaced)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Interlaced;
                            Settings::values.post_processing_shader = "horizontal (builtin)";
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
                    GUI_AddControlsSettings(is_open, nullptr, plugin_manager, io);

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
                            if (ImGui::Selectable("Default")) {
                                Settings::values.layout = Settings::Layout::Default;
                            }
                            if (ImGui::Selectable("Single Screen")) {
                                Settings::values.layout = Settings::Layout::SingleScreen;
                            }
                            if (ImGui::Selectable("Large Screen")) {
                                Settings::values.layout = Settings::Layout::LargeScreen;
                            }
                            if (ImGui::Selectable("Side by Side")) {
                                Settings::values.layout = Settings::Layout::SideScreen;
                            }
                            if (ImGui::Selectable("Medium Screen")) {
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
                            public_rooms = GetPublicCitraRooms();
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
                        public_rooms = GetPublicCitraRooms();
                    }
                    ImGui::SameLine();
                    ImGui::InputText("Search", &public_rooms_query);

                    if (ImGui::BeginChildFrame(
                            ImGui::GetID("Public Room List"),
                            ImVec2(-1.0f, ImGui::GetContentRegionAvail().y - 40.0f),
                            ImGuiWindowFlags_HorizontalScrollbar)) {
                        for (const auto& room : public_rooms) {
                            std::string room_string =
                                fmt::format("{}\n\nHas Password: {}\nMaximum Members: "
                                            "{}\nPreferred Game: {}\nOwner: {}",
                                            room.name, room.has_password ? "Yes" : "No",
                                            room.max_players, room.game, room.owner);
                            if (!room.description.empty()) {
                                room_string +=
                                    fmt::format("\n\nDescription:\n{}", room.description);
                            }
                            if (!room.members.empty()) {
                                room_string +=
                                    fmt::format("\n\nMembers ({}):", room.members.size());
                                for (const CitraRoom::Member& member : room.members) {
                                    if (member.game.empty()) {
                                        room_string += fmt::format("\n\t{}", member.nickname);
                                    } else {
                                        room_string += fmt::format("\n\t{} is playing {}",
                                                                   member.nickname, member.game);
                                    }
                                }
                            }

                            if (asl::String(room_string.c_str())
                                    .toLowerCase()
                                    .contains(
                                        asl::String(public_rooms_query.c_str()).toLowerCase())) {
                                if (ImGui::Selectable(room_string.c_str())) {
                                    Settings::values.multiplayer_ip = room.ip;
                                    Settings::values.multiplayer_port = room.port;
                                }
                                ImGui::Separator();
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

                if (ImGui::BeginTabItem("Hacks")) {
                    ImGui::Checkbox("Priority Boost", &Settings::values.enable_priority_boost);
                    ImGui::EndTabItem();
                }

                if (!host_multiplayer_room_room_created &&
                    ImGui::BeginTabItem("Host Multiplayer Room")) {
                    ImGui::InputText("IP", &host_multiplayer_room_ip);

                    ImGui::InputScalar("Port", ImGuiDataType_U16, &host_multiplayer_room_port);

                    ImGui::InputScalar("Member Slots", ImGuiDataType_U32,
                                       &host_multiplayer_room_member_slots);

                    ImGui::NewLine();

                    ImGui::PushTextWrapPos();
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

        if (!installed.empty()) {
            ImGui::OpenPopup("Installed");

            ImGui::SetNextWindowPos(ImVec2());
            ImGui::SetNextWindowSize(io.DisplaySize);

            bool open = true;
            if (ImGui::BeginPopupModal("Installed", &open,
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoResize)) {
                ImGui::InputText("Search", &installed_query);

                if (ImGui::BeginChildFrame(ImGui::GetID("Installed"), ImVec2(-1.0f, -1.0f),
                                           ImGuiWindowFlags_HorizontalScrollbar)) {
                    for (const auto& title : installed) {
                        const auto [path, name] = title;

                        if (asl::String(name.c_str())
                                .toLowerCase()
                                .contains(asl::String(installed_query.c_str()).toLowerCase()) &&
                            ImGui::Selectable(name.c_str())) {
                            Settings::values.file_path = path;
                            installed.clear();
                            installed_query.clear();
                            break;
                        }
                    }
                }
                ImGui::EndChildFrame();
                ImGui::EndPopup();
            }
            if (!open) {
                installed.clear();
                installed_query.clear();
            }
        }

        ImGui::End();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }
}
