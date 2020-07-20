// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
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
                                 Service::CFG::Module& cfg, bool& ok_multiplayer) {
    signal(SIGINT, [](int) { is_open = false; });
    signal(SIGTERM, [](int) { is_open = false; });

    SDL_Event event;
    CitraRoomList public_rooms;
    bool first_time_in_multiplayer = true;
    std::string public_rooms_query;
    u16 play_coins = 0xDEAD;
    bool play_coins_changed = false;

    while (is_open) {
        // Poll events
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) {
                if (pfd::message("vvctre", "Would you like to exit now?", pfd::choice::yes_no,
                                 pfd::icon::question)
                        .result() == pfd::button::yes) {
                    std::exit(1);
                }
            }
        }

        // Draw window
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::Begin("Initial Settings", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGui::SetWindowPos(ImVec2(), ImGuiCond_Once);
            ImGui::SetWindowSize(io.DisplaySize);
            if (ImGui::BeginTabBar("##tabBar")) {
                if (ImGui::BeginTabItem("Start")) {
                    ImGui::TextUnformatted("File:");
                    ImGui::SameLine();
                    ImGui::PushItemWidth(250);
                    ImGui::InputText("##file", &Settings::values.file_path);
                    ImGui::PopItemWidth();
                    ImGui::SameLine();
                    if (ImGui::Button("Browse...##file")) {
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

                    ImGui::SameLine();
                    if (ImGui::Button("Install CIA")) {
                        const std::vector<std::string> files =
                            pfd::open_file("Install CIA", *asl::Process::myDir(),
                                           {"CTR Importable Archive", "*.cia *.CIA"},
                                           pfd::opt::multiselect)
                                .result();

                        for (const auto& file : files) {
                            const Service::AM::InstallStatus status = Service::AM::InstallCIA(
                                file, [&](std::size_t current, std::size_t total) {
                                    // Poll events
                                    while (SDL_PollEvent(&event)) {
                                        ImGui_ImplSDL2_ProcessEvent(&event);

                                        if (event.type == SDL_QUIT) {
                                            if (pfd::message(
                                                    "vvctre", "Would you like to exit now?",
                                                    pfd::choice::yes_no, pfd::icon::question)
                                                    .result() == pfd::button::yes) {
                                                std::exit(1);
                                            }
                                        }
                                    }

                                    // Draw window
                                    ImGui_ImplOpenGL3_NewFrame();
                                    ImGui_ImplSDL2_NewFrame(window);
                                    ImGui::NewFrame();

                                    ImGui::OpenPopup("Installing CIA");

                                    if (ImGui::BeginPopupModal(
                                            "Installing CIA", nullptr,
                                            ImGuiWindowFlags_NoSavedSettings |
                                                ImGuiWindowFlags_AlwaysAutoResize |
                                                ImGuiWindowFlags_NoMove)) {
                                        ImGui::Text("Installing %s", file.c_str());
                                        ImGui::ProgressBar(static_cast<float>(current) /
                                                           static_cast<float>(total));
                                        ImGui::EndPopup();
                                    }

                                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                                    glClear(GL_COLOR_BUFFER_BIT);
                                    ImGui::Render();
                                    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                                    SDL_GL_SwapWindow(window);
                                });

                            switch (status) {
                            case Service::AM::InstallStatus::Success:
                                break;
                            case Service::AM::InstallStatus::ErrorFailedToOpenFile:
                                pfd::message("vvctre", fmt::format("Failed to open {}", file),
                                             pfd::choice::ok, pfd::icon::error);
                                break;
                            case Service::AM::InstallStatus::ErrorFileNotFound:
                                pfd::message("vvctre", fmt::format("{} not found", file),
                                             pfd::choice::ok, pfd::icon::error);
                                break;
                            case Service::AM::InstallStatus::ErrorAborted:
                                pfd::message("vvctre", fmt::format("{} installation aborted", file),
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

                        continue;
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Installed")) {
                        installed = GetInstalledList();
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("HOME Menu")) {
                        if (Settings::values.region_value == Settings::REGION_VALUE_AUTO_SELECT) {
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
                                pfd::message("vvctre", "HOME Menu not installed", pfd::choice::ok,
                                             pfd::icon::error);
                            }
                        }
                    }

                    if (Settings::values.record_movie.empty()) {
                        ImGui::TextUnformatted("Play Movie:");
                        ImGui::SameLine();
                        ImGui::InputText("##playmovie", &Settings::values.play_movie);
                        ImGui::SameLine();
                        if (ImGui::Button("Browse...##playmovie")) {
                            const std::vector<std::string> result =
                                pfd::open_file("Play Movie", *asl::Process::myDir(),
                                               {"VvCtre Movie", "*.vcm"})
                                    .result();
                            if (!result.empty()) {
                                Settings::values.play_movie = result[0];
                            }
                        }
                    }

                    if (Settings::values.play_movie.empty()) {
                        ImGui::TextUnformatted("Record Movie:");
                        ImGui::SameLine();
                        ImGui::InputText("##recordmovie", &Settings::values.record_movie);
                        ImGui::SameLine();
                        if (ImGui::Button("Browse...##recordmovie")) {
                            const std::string record_movie =
                                pfd::save_file("Record Movie", "movie.vcm",
                                               {"VvCtre Movie", "*.vcm"})
                                    .result();
                            if (!record_movie.empty()) {
                                Settings::values.record_movie = record_movie;
                            }
                        }
                    }

                    ImGui::TextUnformatted("Region:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##region", [&] {
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

                    ImGui::TextUnformatted("Log Filter:");
                    ImGui::SameLine();
                    ImGui::InputText("##logfilter", &Settings::values.log_filter);

                    ImGui::TextUnformatted("Initial Time:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##initial_clock", [] {
                            switch (Settings::values.initial_clock) {
                            case Settings::InitialClock::SystemTime:
                                return "System";
                            case Settings::InitialClock::FixedTime:
                                return "Unix Timestamp";
                            default:
                                break;
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("System")) {
                            Settings::values.initial_clock = Settings::InitialClock::SystemTime;
                        }

                        if (ImGui::Selectable("Unix Timestamp")) {
                            Settings::values.initial_clock = Settings::InitialClock::FixedTime;
                        }

                        ImGui::EndCombo();
                    }
                    if (Settings::values.initial_clock == Settings::InitialClock::FixedTime) {
                        ImGui::SameLine();
                        ImGui::InputScalar("##unix_timestamp", ImGuiDataType_U64,
                                           &Settings::values.unix_timestamp);
                    }

                    ImGui::Checkbox("Use Virtual SD Card", &Settings::values.use_virtual_sd);

                    ImGui::Checkbox("Start in Fullscreen Mode",
                                    &Settings::values.start_in_fullscreen_mode);

                    ImGui::Checkbox("Record Frame Times", &Settings::values.record_frame_times);

                    ImGui::Checkbox("Enable GDB Stub", &Settings::values.use_gdbstub);
                    if (Settings::values.use_gdbstub) {
                        ImGui::SameLine();
                        ImGui::Spacing();
                        ImGui::SameLine();
                        ImGui::TextUnformatted("Port:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(45);
                        ImGui::InputScalar("##gdbstubport", ImGuiDataType_U16,
                                           &Settings::values.gdbstub_port);
                        ImGui::PopItemWidth();
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("General")) {
                    ImGui::Checkbox("Use CPU JIT", &Settings::values.use_cpu_jit);
                    ImGui::Checkbox("Limit Speed", &Settings::values.limit_speed);

                    if (Settings::values.limit_speed) {
                        ImGui::SameLine();
                        ImGui::TextUnformatted("To");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(45);
                        ImGui::InputScalar("##speedlimit", ImGuiDataType_U16,
                                           &Settings::values.speed_limit);
                        ImGui::PopItemWidth();
                        ImGui::SameLine();
                        ImGui::TextUnformatted("%");
                    }

                    if (Settings::values.use_custom_cpu_ticks) {
                        ImGui::Checkbox("Custom CPU Ticks:",
                                        &Settings::values.use_custom_cpu_ticks);
                        ImGui::SameLine();
                        ImGui::InputScalar("##customcputicks", ImGuiDataType_U64,
                                           &Settings::values.custom_cpu_ticks);
                    } else {
                        ImGui::Checkbox("Custom CPU Ticks", &Settings::values.use_custom_cpu_ticks);
                    }

                    ImGui::TextUnformatted("CPU Clock Percentage:");
                    ImGui::SameLine();
                    u32 min = 5;
                    u32 max = 400;
                    ImGui::SliderScalar("##cpu_clock_percentage", ImGuiDataType_U32,
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

                    ImGui::TextUnformatted("Volume:");
                    ImGui::SameLine();
                    ImGui::SliderFloat("##volume", &Settings::values.audio_volume, 0.0f, 1.0f);

                    ImGui::TextUnformatted("Sink:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##sink", Settings::values.audio_sink_id.c_str())) {
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

                    ImGui::TextUnformatted("Device:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##device", Settings::values.audio_device_id.c_str())) {
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

                    ImGui::TextUnformatted("Microphone Input Type:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##microphone_input_type", [] {
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
                        ImGui::TextUnformatted("Microphone Device");
                        ImGui::SameLine();

                        if (ImGui::BeginCombo("##microphonedevice",
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
                    ImGui::TextUnformatted("Inner Camera Engine:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##innercameraengine",
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
                        ImGui::TextUnformatted("Inner Camera Parameter:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(316);
                        ImGui::InputText(
                            "##innercameraparameter",
                            &Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)]);
                        ImGui::PopItemWidth();
                        GUI_CameraAddBrowse(
                            "Browse...##innercamera",
                            static_cast<std::size_t>(Service::CAM::CameraIndex::InnerCamera));
                    }

                    ImGui::TextUnformatted("Outer Left Camera Engine:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##outerleftcameraengine",
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
                        ImGui::TextUnformatted("Outer Left Camera Parameter:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(316);
                        ImGui::InputText(
                            "##outerleftcameraparameter",
                            &Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)]);
                        ImGui::PopItemWidth();
                        GUI_CameraAddBrowse(
                            "Browse...##outerleftcamera",
                            static_cast<std::size_t>(Service::CAM::CameraIndex::OuterLeftCamera));
                    }

                    ImGui::TextUnformatted("Outer Right Camera Engine");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##outerrightcameraengine",
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
                        ImGui::TextUnformatted("Outer Right Camera Parameter:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(316);
                        ImGui::InputText(
                            "##outerrightcameraparameter",
                            &Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)]);
                        ImGui::PopItemWidth();
                        GUI_CameraAddBrowse(
                            "Browse...##outerrightcamera",
                            static_cast<std::size_t>(Service::CAM::CameraIndex::OuterRightCamera));
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("System")) {
                    ImGui::TextUnformatted("Username:");
                    ImGui::SameLine();

                    std::string username = Common::UTF16ToUTF8(cfg.GetUsername());
                    if (ImGui::InputText("##username", &username)) {
                        cfg.SetUsername(Common::UTF8ToUTF16(username));
                        update_config_savegame = true;
                    }

                    ImGui::TextUnformatted("Birthday:");
                    ImGui::SameLine();

                    auto [month, day] = cfg.GetBirthday();

                    if (ImGui::BeginCombo("##birthday_month", [&] {
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
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("February")) {
                            cfg.SetBirthday(2, day);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("March")) {
                            cfg.SetBirthday(3, day);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("April")) {
                            cfg.SetBirthday(4, day);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("May")) {
                            cfg.SetBirthday(5, day);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("June")) {
                            cfg.SetBirthday(6, day);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("July")) {
                            cfg.SetBirthday(7, day);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("August")) {
                            cfg.SetBirthday(8, day);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("September")) {
                            cfg.SetBirthday(9, day);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("October")) {
                            cfg.SetBirthday(10, day);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("November")) {
                            cfg.SetBirthday(11, day);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("December")) {
                            cfg.SetBirthday(12, day);
                            update_config_savegame = true;
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::SameLine();

                    if (ImGui::InputScalar("##birthday_day", ImGuiDataType_U8, &day)) {
                        cfg.SetBirthday(month, day);
                        update_config_savegame = true;
                    }

                    ImGui::TextUnformatted("Language:");
                    ImGui::SameLine();

                    if (ImGui::BeginCombo("##language", [&] {
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
                                return "Portugese";
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
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("English")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_EN);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("French")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_FR);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("German")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_DE);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Italian")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_IT);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Spanish")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ES);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Simplified Chinese")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ZH);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Korean")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_KO);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Dutch")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_NL);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Portugese")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_PT);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Russian")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_RU);
                            update_config_savegame = true;
                        }

                        if (ImGui::Selectable("Traditional Chinese")) {
                            cfg.SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_TW);
                            update_config_savegame = true;
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::TextUnformatted("Sound output mode:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##soundoutputmode", [&] {
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
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Stereo")) {
                            cfg.SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_STEREO);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Surround")) {
                            cfg.SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_SURROUND);
                            update_config_savegame = true;
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::TextUnformatted("Country:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##country", [&] {
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
                            cfg.SetCountryCode(1);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Anguilla")) {
                            cfg.SetCountryCode(8);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Antigua and Barbuda")) {
                            cfg.SetCountryCode(9);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Argentina")) {
                            cfg.SetCountryCode(10);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Aruba")) {
                            cfg.SetCountryCode(11);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Bahamas")) {
                            cfg.SetCountryCode(12);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Barbados")) {
                            cfg.SetCountryCode(13);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Belize")) {
                            cfg.SetCountryCode(14);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Bolivia")) {
                            cfg.SetCountryCode(15);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Brazil")) {
                            cfg.SetCountryCode(16);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("British Virgin Islands")) {
                            cfg.SetCountryCode(17);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Canada")) {
                            cfg.SetCountryCode(18);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Cayman Islands")) {
                            cfg.SetCountryCode(19);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Chile")) {
                            cfg.SetCountryCode(20);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Colombia")) {
                            cfg.SetCountryCode(21);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Costa Rica")) {
                            cfg.SetCountryCode(22);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Dominica")) {
                            cfg.SetCountryCode(23);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Dominican Republic")) {
                            cfg.SetCountryCode(24);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Ecuador")) {
                            cfg.SetCountryCode(25);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("El Salvador")) {
                            cfg.SetCountryCode(26);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("French Guiana")) {
                            cfg.SetCountryCode(27);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Grenada")) {
                            cfg.SetCountryCode(28);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Guadeloupe")) {
                            cfg.SetCountryCode(29);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Guatemala")) {
                            cfg.SetCountryCode(30);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Guyana")) {
                            cfg.SetCountryCode(31);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Haiti")) {
                            cfg.SetCountryCode(32);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Honduras")) {
                            cfg.SetCountryCode(33);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Jamaica")) {
                            cfg.SetCountryCode(34);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Martinique")) {
                            cfg.SetCountryCode(35);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Mexico")) {
                            cfg.SetCountryCode(36);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Montserrat")) {
                            cfg.SetCountryCode(37);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Netherlands Antilles")) {
                            cfg.SetCountryCode(38);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Nicaragua")) {
                            cfg.SetCountryCode(39);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Panama")) {
                            cfg.SetCountryCode(40);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Paraguay")) {
                            cfg.SetCountryCode(41);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Peru")) {
                            cfg.SetCountryCode(42);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Saint Kitts and Nevis")) {
                            cfg.SetCountryCode(43);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Saint Lucia")) {
                            cfg.SetCountryCode(44);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Saint Vincent and the Grenadines")) {
                            cfg.SetCountryCode(45);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Suriname")) {
                            cfg.SetCountryCode(46);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Trinidad and Tobago")) {
                            cfg.SetCountryCode(47);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Turks and Caicos Islands")) {
                            cfg.SetCountryCode(48);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("United States")) {
                            cfg.SetCountryCode(49);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Uruguay")) {
                            cfg.SetCountryCode(50);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("US Virgin Islands")) {
                            cfg.SetCountryCode(51);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Venezuela")) {
                            cfg.SetCountryCode(52);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Albania")) {
                            cfg.SetCountryCode(64);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Australia")) {
                            cfg.SetCountryCode(65);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Austria")) {
                            cfg.SetCountryCode(66);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Belgium")) {
                            cfg.SetCountryCode(67);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Bosnia and Herzegovina")) {
                            cfg.SetCountryCode(68);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Botswana")) {
                            cfg.SetCountryCode(69);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Bulgaria")) {
                            cfg.SetCountryCode(70);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Croatia")) {
                            cfg.SetCountryCode(71);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Cyprus")) {
                            cfg.SetCountryCode(72);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Czech Republic")) {
                            cfg.SetCountryCode(73);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Denmark")) {
                            cfg.SetCountryCode(74);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Estonia")) {
                            cfg.SetCountryCode(75);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Finland")) {
                            cfg.SetCountryCode(76);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("France")) {
                            cfg.SetCountryCode(77);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Germany")) {
                            cfg.SetCountryCode(78);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Greece")) {
                            cfg.SetCountryCode(79);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Hungary")) {
                            cfg.SetCountryCode(80);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Iceland")) {
                            cfg.SetCountryCode(81);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Ireland")) {
                            cfg.SetCountryCode(82);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Italy")) {
                            cfg.SetCountryCode(83);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Latvia")) {
                            cfg.SetCountryCode(84);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Lesotho")) {
                            cfg.SetCountryCode(85);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Liechtenstein")) {
                            cfg.SetCountryCode(86);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Lithuania")) {
                            cfg.SetCountryCode(87);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Luxembourg")) {
                            cfg.SetCountryCode(88);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Macedonia")) {
                            cfg.SetCountryCode(89);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Malta")) {
                            cfg.SetCountryCode(90);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Montenegro")) {
                            cfg.SetCountryCode(91);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Mozambique")) {
                            cfg.SetCountryCode(92);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Namibia")) {
                            cfg.SetCountryCode(93);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Netherlands")) {
                            cfg.SetCountryCode(94);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("New Zealand")) {
                            cfg.SetCountryCode(95);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Norway")) {
                            cfg.SetCountryCode(96);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Poland")) {
                            cfg.SetCountryCode(97);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Portugal")) {
                            cfg.SetCountryCode(98);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Romania")) {
                            cfg.SetCountryCode(99);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Russia")) {
                            cfg.SetCountryCode(100);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Serbia")) {
                            cfg.SetCountryCode(101);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Slovakia")) {
                            cfg.SetCountryCode(102);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Slovenia")) {
                            cfg.SetCountryCode(103);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("South Africa")) {
                            cfg.SetCountryCode(104);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Spain")) {
                            cfg.SetCountryCode(105);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Swaziland")) {
                            cfg.SetCountryCode(106);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Sweden")) {
                            cfg.SetCountryCode(107);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Switzerland")) {
                            cfg.SetCountryCode(108);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Turkey")) {
                            cfg.SetCountryCode(109);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("United Kingdom")) {
                            cfg.SetCountryCode(110);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Zambia")) {
                            cfg.SetCountryCode(111);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Zimbabwe")) {
                            cfg.SetCountryCode(112);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Azerbaijan")) {
                            cfg.SetCountryCode(113);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Mauritania")) {
                            cfg.SetCountryCode(114);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Mali")) {
                            cfg.SetCountryCode(115);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Niger")) {
                            cfg.SetCountryCode(116);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Chad")) {
                            cfg.SetCountryCode(117);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Sudan")) {
                            cfg.SetCountryCode(118);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Eritrea")) {
                            cfg.SetCountryCode(119);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Djibouti")) {
                            cfg.SetCountryCode(120);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Somalia")) {
                            cfg.SetCountryCode(121);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Andorra")) {
                            cfg.SetCountryCode(122);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Gibraltar")) {
                            cfg.SetCountryCode(123);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Guernsey")) {
                            cfg.SetCountryCode(124);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Isle of Man")) {
                            cfg.SetCountryCode(125);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Jersey")) {
                            cfg.SetCountryCode(126);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Monaco")) {
                            cfg.SetCountryCode(127);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Taiwan")) {
                            cfg.SetCountryCode(128);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("South Korea")) {
                            cfg.SetCountryCode(136);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Hong Kong")) {
                            cfg.SetCountryCode(144);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Macau")) {
                            cfg.SetCountryCode(145);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Indonesia")) {
                            cfg.SetCountryCode(152);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Singapore")) {
                            cfg.SetCountryCode(153);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Thailand")) {
                            cfg.SetCountryCode(154);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Philippines")) {
                            cfg.SetCountryCode(155);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Malaysia")) {
                            cfg.SetCountryCode(156);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("China")) {
                            cfg.SetCountryCode(160);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("United Arab Emirates")) {
                            cfg.SetCountryCode(168);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("India")) {
                            cfg.SetCountryCode(169);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Egypt")) {
                            cfg.SetCountryCode(170);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Oman")) {
                            cfg.SetCountryCode(171);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Qatar")) {
                            cfg.SetCountryCode(172);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Kuwait")) {
                            cfg.SetCountryCode(173);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Saudi Arabia")) {
                            cfg.SetCountryCode(174);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Syria")) {
                            cfg.SetCountryCode(175);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Bahrain")) {
                            cfg.SetCountryCode(176);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Jordan")) {
                            cfg.SetCountryCode(177);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("San Marino")) {
                            cfg.SetCountryCode(184);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Vatican City")) {
                            cfg.SetCountryCode(185);
                            update_config_savegame = true;
                        }
                        if (ImGui::Selectable("Bermuda")) {
                            cfg.SetCountryCode(186);
                            update_config_savegame = true;
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::TextUnformatted("Play Coins:");
                    ImGui::SameLine();

                    const u16 min = 0;
                    const u16 max = 300;

                    if (play_coins == 0xDEAD) {
                        play_coins = Service::PTM::Module::GetPlayCoins();
                    }

                    if (ImGui::SliderScalar("##playcoins", ImGuiDataType_U16, &play_coins, &min,
                                            &max)) {
                        play_coins_changed = true;
                    }

                    if (ImGui::Button("Regenerate Console ID")) {
                        u32 random_number;
                        u64 console_id;
                        cfg.GenerateConsoleUniqueId(random_number, console_id);
                        cfg.SetConsoleUniqueId(random_number, console_id);
                        update_config_savegame = true;
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

                    ImGui::TextUnformatted("Resolution");
                    ImGui::SameLine();
                    const u16 min = 0;
                    const u16 max = 10;
                    ImGui::SliderScalar("##resolution", ImGuiDataType_U16,
                                        &Settings::values.resolution, &min, &max,
                                        Settings::values.resolution == 0 ? "Window Size" : "%d");

                    ImGui::TextUnformatted("Background Color");
                    ImGui::SameLine();
                    ImGui::ColorEdit3("##backgroundcolor", &Settings::values.background_color_red,
                                      ImGuiColorEditFlags_NoInputs);

                    ImGui::TextUnformatted("Post Processing Shader");
                    ImGui::SameLine();
                    ImGui::InputText("##postprocessingshader",
                                     &Settings::values.post_processing_shader);
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted("File name without extension and folder");
                        ImGui::EndTooltip();
                    }

                    ImGui::TextUnformatted("Texture Filter");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##texturefilter",
                                          Settings::values.texture_filter.c_str())) {
                        const auto& filters = OpenGL::TextureFilterer::GetFilterNames();

                        for (const auto& filter : filters) {
                            if (ImGui::Selectable(std::string(filter).c_str())) {
                                Settings::values.texture_filter = filter;
                            }
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::TextUnformatted("3D");
                    ImGui::SameLine();

                    if (ImGui::BeginCombo("##render_3d", [] {
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

                    ImGui::SameLine();

                    u8 factor_3d = Settings::values.factor_3d;
                    const u8 factor_3d_min = 0;
                    const u8 factor_3d_max = 100;
                    ImGui::PushItemWidth(100);
                    if (ImGui::SliderScalar("##factor_3d", ImGuiDataType_U8, &factor_3d,
                                            &factor_3d_min, &factor_3d_max, "%d%%")) {
                        Settings::values.factor_3d = factor_3d;
                    }
                    ImGui::PopItemWidth();

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Controls")) {
                    if (ImGui::Button("Load File")) {
                        const std::vector<std::string> path =
                            pfd::open_file("Load File", *asl::Process::myDir(),
                                           {"JSON Files", "*.json"})
                                .result();
                        if (!path.empty()) {
                            asl::Var json = asl::Json::read(path[0].c_str());
                            asl::Array buttons = json["buttons"].array();
                            asl::Array analogs = json["analogs"].array();
                            for (int i = 0; i < buttons.length(); ++i) {
                                Settings::values.buttons[static_cast<std::size_t>(i)] = *buttons[i];
                            }
                            for (int i = 0; i < analogs.length(); ++i) {
                                Settings::values.analogs[static_cast<std::size_t>(i)] = *analogs[i];
                            }
                            Settings::values.motion_device = *json["motion_device"];
                            Settings::values.touch_device = *json["touch_device"];
                            Settings::values.cemuhookudp_address = *json["cemuhookudp_address"];
                            Settings::values.cemuhookudp_port =
                                static_cast<u16>(static_cast<int>(json["cemuhookudp_port"]));
                            Settings::values.cemuhookudp_pad_index =
                                static_cast<u8>(static_cast<int>(json["cemuhookudp_pad_index"]));
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Save File")) {
                        const std::string path =
                            pfd::save_file("Save File", "controls.json", {"JSON Files", "*.json"})
                                .result();

                        if (!path.empty()) {
                            asl::Array<asl::String> buttons;
                            asl::Array<asl::String> analogs;
                            for (const std::string& button : Settings::values.buttons) {
                                buttons << asl::String(button.c_str());
                            }
                            for (const std::string& analog : Settings::values.analogs) {
                                analogs << asl::String(analog.c_str());
                            }

                            asl::Var json;
                            json["buttons"] = buttons;
                            json["analogs"] = analogs;
                            json["motion_device"] = Settings::values.motion_device.c_str();
                            json["touch_device"] = Settings::values.touch_device.c_str();
                            json["cemuhookudp_address"] =
                                Settings::values.cemuhookudp_address.c_str();
                            json["cemuhookudp_port"] =
                                static_cast<int>(Settings::values.cemuhookudp_port);
                            json["cemuhookudp_pad_index"] =
                                static_cast<int>(Settings::values.cemuhookudp_pad_index);

                            asl::Json::write(path.c_str(), json, false);
                        }
                    }
                    ImGui::NewLine();

                    const auto GetInput =
                        [&](InputCommon::Polling::DeviceType device_type) -> std::string {
                        switch (device_type) {
                        case InputCommon::Polling::DeviceType::Button: {
                            auto pollers = InputCommon::Polling::GetPollers(device_type);

                            for (auto& poller : pollers) {
                                poller->Start();
                            }

                            for (;;) {
                                for (auto& poller : pollers) {
                                    const Common::ParamPackage params = poller->GetNextInput();
                                    if (params.Has("engine")) {
                                        for (auto& poller : pollers) {
                                            poller->Stop();
                                        }
                                        return params.Serialize();
                                    }
                                }

                                while (SDL_PollEvent(&event)) {
                                    if (event.type == SDL_KEYUP) {
                                        for (auto& poller : pollers) {
                                            poller->Stop();
                                        }
                                        return InputCommon::GenerateKeyboardParam(
                                            event.key.keysym.scancode);
                                    }
                                }
                            }

                            break;
                        }
                        case InputCommon::Polling::DeviceType::Analog: {
                            auto pollers = InputCommon::Polling::GetPollers(device_type);

                            for (auto& poller : pollers) {
                                poller->Start();
                            }

                            std::vector<int> keyboard_scancodes;

                            for (;;) {
                                for (auto& poller : pollers) {
                                    const Common::ParamPackage params = poller->GetNextInput();
                                    if (params.Has("engine")) {
                                        for (auto& poller : pollers) {
                                            poller->Stop();
                                        }
                                        return params.Serialize();
                                    }
                                }

                                while (SDL_PollEvent(&event)) {
                                    if (event.type == SDL_KEYUP) {
                                        pollers.clear();
                                        keyboard_scancodes.push_back(event.key.keysym.scancode);
                                        if (keyboard_scancodes.size() == 5) {
                                            for (auto& poller : pollers) {
                                                poller->Stop();
                                            }
                                            return InputCommon::GenerateAnalogParamFromKeys(
                                                keyboard_scancodes[0], keyboard_scancodes[1],
                                                keyboard_scancodes[2], keyboard_scancodes[3],
                                                keyboard_scancodes[4], 0.5f);
                                        }
                                    }
                                }
                            }

                            break;
                        }
                        default: {
                            return "engine:null";
                        }
                        }
                    };

                    // Buttons
                    ImGui::BeginGroup();
                    ImGui::TextUnformatted("Buttons");

                    ImGui::TextUnformatted("A:");
                    ImGui::SameLine();
                    if (ImGui::Button((InputCommon::ButtonToText(
                                           Settings::values.buttons[Settings::NativeButton::A]) +
                                       "##ButtonA")
                                          .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::A] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::TextUnformatted("B:");
                    ImGui::SameLine();
                    if (ImGui::Button((InputCommon::ButtonToText(
                                           Settings::values.buttons[Settings::NativeButton::B]) +
                                       "##ButtonB")
                                          .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::B] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::TextUnformatted("X:");
                    ImGui::SameLine();
                    if (ImGui::Button((InputCommon::ButtonToText(
                                           Settings::values.buttons[Settings::NativeButton::X]) +
                                       "##ButtonX")
                                          .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::X] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::TextUnformatted("Y:");
                    ImGui::SameLine();
                    if (ImGui::Button((InputCommon::ButtonToText(
                                           Settings::values.buttons[Settings::NativeButton::Y]) +
                                       "##ButtonY")
                                          .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::Y] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::TextUnformatted("L:");
                    ImGui::SameLine();
                    if (ImGui::Button((InputCommon::ButtonToText(
                                           Settings::values.buttons[Settings::NativeButton::L]) +
                                       "##ButtonL")
                                          .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::L] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::TextUnformatted("R:");
                    ImGui::SameLine();
                    if (ImGui::Button((InputCommon::ButtonToText(
                                           Settings::values.buttons[Settings::NativeButton::R]) +
                                       "##ButtonR")
                                          .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::R] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::TextUnformatted("ZL:");
                    ImGui::SameLine();
                    if (ImGui::Button((InputCommon::ButtonToText(
                                           Settings::values.buttons[Settings::NativeButton::ZL]) +
                                       "##ButtonZL")
                                          .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::ZL] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted("If it says -, make sure it says +");
                        ImGui::EndTooltip();
                    }

                    ImGui::TextUnformatted("ZR:");
                    ImGui::SameLine();
                    if (ImGui::Button((InputCommon::ButtonToText(
                                           Settings::values.buttons[Settings::NativeButton::ZR]) +
                                       "##ButtonZR")
                                          .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::ZR] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted("If it says -, make sure it says +");
                        ImGui::EndTooltip();
                    }

                    ImGui::TextUnformatted("Start:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::ButtonToText(
                                 Settings::values.buttons[Settings::NativeButton::Start]) +
                             "##ButtonStart")
                                .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::Start] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::TextUnformatted("Select:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::ButtonToText(
                                 Settings::values.buttons[Settings::NativeButton::Select]) +
                             "##ButtonSelect")
                                .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::Select] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::TextUnformatted("Debug:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::ButtonToText(
                                 Settings::values.buttons[Settings::NativeButton::Debug]) +
                             "##ButtonDebug")
                                .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::Debug] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::TextUnformatted("GPIO14:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::ButtonToText(
                                 Settings::values.buttons[Settings::NativeButton::Gpio14]) +
                             "##ButtonGPIO14")
                                .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::Gpio14] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }

                    ImGui::TextUnformatted("HOME:");
                    ImGui::SameLine();
                    if (ImGui::Button((InputCommon::ButtonToText(
                                           Settings::values.buttons[Settings::NativeButton::Home]) +
                                       "##ButtonHome")
                                          .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::Home] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine();

                    // Circle Pad
                    ImGui::BeginGroup();
                    ImGui::TextUnformatted("Circle Pad");
                    ImGui::TextUnformatted("Up:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::AnalogToText(
                                 Settings::values.analogs[Settings::NativeAnalog::CirclePad],
                                 "up") +
                             "##CirclePadUp")
                                .c_str())) {
                        Common::ParamPackage params = Common::ParamPackage(
                            Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
                        if (params.Get("engine", "") != "analog_from_button") {
                            Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                                InputCommon::GenerateAnalogParamFromKeys(
                                    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT,
                                    SDL_SCANCODE_RIGHT, SDL_SCANCODE_D, 0.5f);
                            params = Common::ParamPackage(
                                Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
                        }
                        params.Set("up", GetInput(InputCommon::Polling::DeviceType::Button));
                        Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                            params.Serialize();
                    }
                    ImGui::TextUnformatted("Down:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::AnalogToText(
                                 Settings::values.analogs[Settings::NativeAnalog::CirclePad],
                                 "down") +
                             "##CirclePadDown")
                                .c_str())) {
                        Common::ParamPackage params = Common::ParamPackage(
                            Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
                        if (params.Get("engine", "") != "analog_from_button") {
                            Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                                InputCommon::GenerateAnalogParamFromKeys(
                                    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT,
                                    SDL_SCANCODE_RIGHT, SDL_SCANCODE_D, 0.5f);
                            params = Common::ParamPackage(
                                Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
                        }
                        params.Set("down", GetInput(InputCommon::Polling::DeviceType::Button));
                        Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                            params.Serialize();
                    }
                    ImGui::TextUnformatted("Left:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::AnalogToText(
                                 Settings::values.analogs[Settings::NativeAnalog::CirclePad],
                                 "left") +
                             "##CirclePadLeft")
                                .c_str())) {
                        Common::ParamPackage params = Common::ParamPackage(
                            Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
                        if (params.Get("engine", "") != "analog_from_button") {
                            Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                                InputCommon::GenerateAnalogParamFromKeys(
                                    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT,
                                    SDL_SCANCODE_RIGHT, SDL_SCANCODE_D, 0.5f);
                            params = Common::ParamPackage(
                                Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
                        }
                        params.Set("left", GetInput(InputCommon::Polling::DeviceType::Button));
                        Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                            params.Serialize();
                    }
                    ImGui::TextUnformatted("Right:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::AnalogToText(
                                 Settings::values.analogs[Settings::NativeAnalog::CirclePad],
                                 "right") +
                             "##CirclePadRight")
                                .c_str())) {
                        Common::ParamPackage params = Common::ParamPackage(
                            Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
                        if (params.Get("engine", "") != "analog_from_button") {
                            Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                                InputCommon::GenerateAnalogParamFromKeys(
                                    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT,
                                    SDL_SCANCODE_RIGHT, SDL_SCANCODE_D, 0.5f);
                            params = Common::ParamPackage(
                                Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
                        }
                        params.Set("right", GetInput(InputCommon::Polling::DeviceType::Button));
                        Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                            params.Serialize();
                    }
                    ImGui::TextUnformatted("Modifier:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::AnalogToText(
                                 Settings::values.analogs[Settings::NativeAnalog::CirclePad],
                                 "modifier") +
                             "##CirclePadModifier")
                                .c_str())) {
                        Common::ParamPackage params = Common::ParamPackage(
                            Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
                        if (params.Get("engine", "") != "analog_from_button") {
                            Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                                InputCommon::GenerateAnalogParamFromKeys(
                                    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT,
                                    SDL_SCANCODE_RIGHT, SDL_SCANCODE_D, 0.5f);
                            params = Common::ParamPackage(
                                Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
                        }
                        params.Set("modifier", GetInput(InputCommon::Polling::DeviceType::Button));
                        Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                            params.Serialize();
                    }
                    {
                        Common::ParamPackage params(
                            Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
                        if (params.Get("engine", "") == "sdl") {
                            float deadzone = params.Get("deadzone", 0.0f);
                            ImGui::TextUnformatted("Deadzone:");
                            ImGui::SameLine();
                            ImGui::PushItemWidth(100);
                            if (ImGui::SliderFloat("##deadzoneCirclePad", &deadzone, 0.0f, 1.0f)) {
                                params.Set("deadzone", deadzone);
                                Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                                    params.Serialize();
                            }
                            ImGui::PopItemWidth();
                        } else if (params.Get("engine", "") == "analog_from_button") {
                            float modifier_scale = params.Get("modifier_scale", 0.5f);
                            ImGui::SameLine();
                            ImGui::PushItemWidth(100);
                            if (ImGui::InputFloat("##modifierScaleCirclePad", &modifier_scale)) {
                                params.Set("modifier_scale", modifier_scale);
                                Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                                    params.Serialize();
                            }
                            ImGui::PopItemWidth();
                        }
                    }
                    if (ImGui::Button("Set All##circlepad")) {
                        Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                            GetInput(InputCommon::Polling::DeviceType::Analog);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted("Keyboard: Press the keys to use for Up, Down, "
                                               "Left, Right, and Modifier.\nReal stick: first move "
                                               "the stick to the right, and then to the bottom.");
                        ImGui::EndTooltip();
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine();

                    // Circle Pad Pro
                    ImGui::BeginGroup();
                    ImGui::TextUnformatted("Circle Pad Pro");
                    ImGui::TextUnformatted("Up:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::AnalogToText(
                                 Settings::values.analogs[Settings::NativeAnalog::CirclePadPro],
                                 "up") +
                             "##CirclePadProUp")
                                .c_str())) {
                        Common::ParamPackage params = Common::ParamPackage(
                            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
                        if (params.Get("engine", "") != "analog_from_button") {
                            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                                InputCommon::GenerateAnalogParamFromKeys(
                                    SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L,
                                    SDL_SCANCODE_D, 0.5f);
                            params = Common::ParamPackage(
                                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
                        }
                        params.Set("up", GetInput(InputCommon::Polling::DeviceType::Button));
                        Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                            params.Serialize();
                    }
                    ImGui::TextUnformatted("Down:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::AnalogToText(
                                 Settings::values.analogs[Settings::NativeAnalog::CirclePadPro],
                                 "down") +
                             "##CirclePadProDown")
                                .c_str())) {
                        Common::ParamPackage params = Common::ParamPackage(
                            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
                        if (params.Get("engine", "") != "analog_from_button") {
                            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                                InputCommon::GenerateAnalogParamFromKeys(
                                    SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L,
                                    SDL_SCANCODE_D, 0.5f);
                            params = Common::ParamPackage(
                                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
                        }
                        params.Set("down", GetInput(InputCommon::Polling::DeviceType::Button));
                        Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                            params.Serialize();
                    }
                    ImGui::TextUnformatted("Left:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::AnalogToText(
                                 Settings::values.analogs[Settings::NativeAnalog::CirclePadPro],
                                 "left") +
                             "##CirclePadProLeft")
                                .c_str())) {
                        Common::ParamPackage params = Common::ParamPackage(
                            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
                        if (params.Get("engine", "") != "analog_from_button") {
                            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                                InputCommon::GenerateAnalogParamFromKeys(
                                    SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L,
                                    SDL_SCANCODE_D, 0.5f);
                            params = Common::ParamPackage(
                                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
                        }
                        params.Set("left", GetInput(InputCommon::Polling::DeviceType::Button));
                        Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                            params.Serialize();
                    }
                    ImGui::TextUnformatted("Right:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::AnalogToText(
                                 Settings::values.analogs[Settings::NativeAnalog::CirclePadPro],
                                 "right") +
                             "##CirclePadProRight")
                                .c_str())) {
                        Common::ParamPackage params = Common::ParamPackage(
                            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
                        if (params.Get("engine", "") != "analog_from_button") {
                            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                                InputCommon::GenerateAnalogParamFromKeys(
                                    SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L,
                                    SDL_SCANCODE_D, 0.5f);
                            params = Common::ParamPackage(
                                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
                        }
                        params.Set("right", GetInput(InputCommon::Polling::DeviceType::Button));
                        Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                            params.Serialize();
                    }
                    ImGui::TextUnformatted("Modifier:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::AnalogToText(
                                 Settings::values.analogs[Settings::NativeAnalog::CirclePadPro],
                                 "modifier") +
                             "##CirclePadProModifier")
                                .c_str())) {
                        Common::ParamPackage params = Common::ParamPackage(
                            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
                        if (params.Get("engine", "") != "analog_from_button") {
                            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                                InputCommon::GenerateAnalogParamFromKeys(
                                    SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L,
                                    SDL_SCANCODE_D, 0.5f);
                            params = Common::ParamPackage(
                                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
                        }
                        params.Set("modifier", GetInput(InputCommon::Polling::DeviceType::Button));
                        Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                            params.Serialize();
                    }
                    {
                        Common::ParamPackage params(
                            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
                        if (params.Get("engine", "") == "sdl") {
                            float deadzone = params.Get("deadzone", 0.0f);
                            ImGui::TextUnformatted("Deadzone:");
                            ImGui::SameLine();
                            ImGui::PushItemWidth(100);
                            if (ImGui::SliderFloat("##deadzoneCirclePadPro", &deadzone, 0.0f,
                                                   1.0f)) {
                                params.Set("deadzone", deadzone);
                                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                                    params.Serialize();
                            }
                            ImGui::PopItemWidth();
                        } else if (params.Get("engine", "") == "analog_from_button") {
                            float modifier_scale = params.Get("modifier_scale", 0.5f);
                            ImGui::SameLine();
                            ImGui::PushItemWidth(100);
                            if (ImGui::InputFloat("##modifierScaleCirclePadPro", &modifier_scale)) {
                                params.Set("modifier_scale", modifier_scale);
                                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                                    params.Serialize();
                            }
                            ImGui::PopItemWidth();
                        }
                    }
                    if (ImGui::Button("Set All##circlepadpro")) {
                        Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                            GetInput(InputCommon::Polling::DeviceType::Analog);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted("Keyboard: Press the keys to use for Up, Down, "
                                               "Left, Right, and Modifier.\nReal stick: first move "
                                               "the stick to the right, and then to the bottom.");
                        ImGui::EndTooltip();
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine();

                    // D-Pad
                    ImGui::BeginGroup();
                    ImGui::TextUnformatted("D-Pad");
                    ImGui::TextUnformatted("Up:");
                    ImGui::SameLine();
                    if (ImGui::Button((InputCommon::ButtonToText(
                                           Settings::values.buttons[Settings::NativeButton::Up]) +
                                       "##DPadUp")
                                          .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::Up] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }
                    ImGui::TextUnformatted("Down:");
                    ImGui::SameLine();
                    if (ImGui::Button((InputCommon::ButtonToText(
                                           Settings::values.buttons[Settings::NativeButton::Down]) +
                                       "##DPadDown")
                                          .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::Down] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }
                    ImGui::TextUnformatted("Left:");
                    ImGui::SameLine();
                    if (ImGui::Button((InputCommon::ButtonToText(
                                           Settings::values.buttons[Settings::NativeButton::Left]) +
                                       "##DPadLeft")
                                          .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::Left] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }
                    ImGui::TextUnformatted("Right:");
                    ImGui::SameLine();
                    if (ImGui::Button(
                            (InputCommon::ButtonToText(
                                 Settings::values.buttons[Settings::NativeButton::Right]) +
                             "##DPadRight")
                                .c_str())) {
                        Settings::values.buttons[Settings::NativeButton::Right] =
                            GetInput(InputCommon::Polling::DeviceType::Button);
                    }
                    ImGui::EndGroup();

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Motion:");
                    ImGui::SameLine();

                    ImGui::PushItemWidth(100);
                    if (ImGui::BeginCombo("##motion_device", [] {
                            const std::string engine =
                                Common::ParamPackage(Settings::values.motion_device)
                                    .Get("engine", "");

                            if (engine == "motion_emu") {
                                return "Right Click";
                            } else if (engine == "cemuhookudp") {
                                return "CemuhookUDP";
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Right Click")) {
                            Settings::values.motion_device = "engine:motion_emu";
                        }

                        if (ImGui::Selectable("CemuhookUDP")) {
                            Settings::values.motion_device = "engine:cemuhookudp";
                        }

                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();

                    Common::ParamPackage motion_device(Settings::values.motion_device);

                    if (motion_device.Get("engine", "") == "motion_emu") {
                        int update_period = motion_device.Get("update_period", 100);
                        float sensitivity = motion_device.Get("sensitivity", 0.01f);
                        float clamp = motion_device.Get("tilt_clamp", 90.0f);

                        ImGui::SameLine();
                        ImGui::Spacing();
                        ImGui::SameLine();
                        ImGui::TextUnformatted("Update Period:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(30);
                        if (ImGui::InputInt("##update_period", &update_period, 0)) {
                            motion_device.Set("update_period", update_period);
                            Settings::values.motion_device = motion_device.Serialize();
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::TextUnformatted("Sensitivity:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(40);
                        if (ImGui::InputFloat("##sensitivity", &sensitivity)) {
                            motion_device.Set("sensitivity", sensitivity);
                            Settings::values.motion_device = motion_device.Serialize();
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::TextUnformatted("Clamp:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(50);
                        if (ImGui::InputFloat("##clamp", &clamp)) {
                            motion_device.Set("tilt_clamp", clamp);
                            Settings::values.motion_device = motion_device.Serialize();
                        }
                        ImGui::PopItemWidth();
                    }

                    ImGui::TextUnformatted("Touch:");
                    ImGui::SameLine();
                    ImGui::PushItemWidth(100);
                    if (ImGui::BeginCombo("##touch_device", [] {
                            const std::string engine =
                                Common::ParamPackage(Settings::values.touch_device)
                                    .Get("engine", "");

                            if (engine == "emu_window") {
                                return "Mouse";
                            } else if (engine == "cemuhookudp") {
                                return "CemuhookUDP";
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Mouse")) {
                            Settings::values.touch_device = "engine:emu_window";
                        }

                        if (ImGui::Selectable("CemuhookUDP")) {
                            Settings::values.touch_device = "engine:cemuhookudp";
                        }

                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();

                    Common::ParamPackage touch_device(Settings::values.touch_device);

                    if (touch_device.Get("engine", "") == "cemuhookudp") {
                        int min_x = touch_device.Get("min_x", 100);
                        int min_y = touch_device.Get("min_y", 50);
                        int max_x = touch_device.Get("max_x", 1800);
                        int max_y = touch_device.Get("max_y", 850);

                        ImGui::SameLine();
                        ImGui::Spacing();
                        ImGui::SameLine();
                        ImGui::TextUnformatted("Min X:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(45);
                        if (ImGui::InputInt("##min_x", &min_x, 0)) {
                            touch_device.Set("min_x", min_x);
                            Settings::values.touch_device = touch_device.Serialize();
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::TextUnformatted("Min Y:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(45);
                        if (ImGui::InputInt("##min_y", &min_y, 0)) {
                            touch_device.Set("min_y", min_y);
                            Settings::values.touch_device = touch_device.Serialize();
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::TextUnformatted("Max X:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(45);
                        if (ImGui::InputInt("##max_x", &max_x, 0)) {
                            touch_device.Set("max_x", max_x);
                            Settings::values.touch_device = touch_device.Serialize();
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::TextUnformatted("Max Y:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(45);
                        if (ImGui::InputInt("##max_y", &max_y, 0)) {
                            touch_device.Set("max_y", max_y);
                            Settings::values.touch_device = touch_device.Serialize();
                        }
                        ImGui::PopItemWidth();
                    }

                    if (motion_device.Get("engine", "") == "cemuhookudp" ||
                        touch_device.Get("engine", "") == "cemuhookudp") {
                        ImGui::TextUnformatted("CemuhookUDP:");

                        ImGui::SameLine();
                        ImGui::PushItemWidth(110);
                        ImGui::InputText("##cemuhookudp_address",
                                         &Settings::values.cemuhookudp_address);
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::TextUnformatted(":");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(45);
                        ImGui::InputScalar("##cemuhookudp_port", ImGuiDataType_U16,
                                           &Settings::values.cemuhookudp_port);
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::TextUnformatted("Pad");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(15);
                        ImGui::InputScalar("##cemuhookudp_pad_index", ImGuiDataType_U8,
                                           &Settings::values.cemuhookudp_pad_index);
                        ImGui::PopItemWidth();
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Layout")) {
                    if (!Settings::values.use_custom_layout) {
                        ImGui::TextUnformatted("Layout:");
                        ImGui::SameLine();
                        if (ImGui::BeginCombo("##layout", [] {
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
                        ImGui::TextUnformatted("Top Left:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##topleft", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_top_left);
                        ImGui::TextUnformatted("Top Top:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##toptop", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_top_top);
                        ImGui::TextUnformatted("Top Right:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##topright", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_top_right);
                        ImGui::TextUnformatted("Top Bottom:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##topbottom", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_top_bottom);
                        ImGui::TextUnformatted("Bottom Left:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##bottomleft", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_bottom_left);
                        ImGui::TextUnformatted("Bottom Top:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##bottomtop", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_bottom_top);
                        ImGui::TextUnformatted("Bottom Right:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##bottomright", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_bottom_right);
                        ImGui::TextUnformatted("Bottom Bottom:");
                        ImGui::SameLine();
                        ImGui::InputScalar("##bottombottom", ImGuiDataType_U16,
                                           &Settings::values.custom_layout_bottom_bottom);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("LLE")) {
                    ImGui::TextUnformatted("Modules:");
                    for (auto& module : Settings::values.lle_modules) {
                        ImGui::Checkbox(module.first.c_str(), &module.second);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Hacks")) {
                    ImGui::Checkbox("Priority Boost", &Settings::values.enable_priority_boost);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Multiplayer")) {
                    if (first_time_in_multiplayer) {
                        if (!ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT)) {
                            public_rooms = GetPublicCitraRooms();
                        }
                        first_time_in_multiplayer = false;
                    }

                    ImGui::TextUnformatted("IP:");
                    ImGui::SameLine();
                    ImGui::InputText("##ip", &Settings::values.multiplayer_ip);

                    ImGui::TextUnformatted("Port:");
                    ImGui::SameLine();
                    ImGui::InputScalar("##port", ImGuiDataType_U16,
                                       &Settings::values.multiplayer_port);

                    ImGui::TextUnformatted("Nickname:");
                    ImGui::SameLine();
                    ImGui::InputText("##nickname", &Settings::values.multiplayer_nickname);

                    ImGui::TextUnformatted("Password:");
                    ImGui::SameLine();
                    ImGui::InputText("##password", &Settings::values.multiplayer_password);

                    ImGui::NewLine();
                    ImGui::TextUnformatted("Public Rooms");

                    ImGui::TextUnformatted("Search:");
                    ImGui::SameLine();
                    ImGui::InputText("##search", &public_rooms_query);
                    ImGui::SameLine();
                    if (ImGui::Button("Refresh")) {
                        public_rooms = GetPublicCitraRooms();
                    }
                    if (ImGui::BeginChildFrame(
                            ImGui::GetID("publicrooms"),
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

                if (!host_multiplayer_room_room_created &&
                    ImGui::BeginTabItem("Host Multiplayer Room")) {
                    ImGui::TextUnformatted("IP:");
                    ImGui::SameLine();
                    ImGui::InputText("##ip", &host_multiplayer_room_ip);

                    ImGui::TextUnformatted("Port:");
                    ImGui::SameLine();
                    ImGui::InputScalar("##port", ImGuiDataType_U16, &host_multiplayer_room_port);

                    ImGui::TextUnformatted("Member Slots:");
                    ImGui::SameLine();
                    ImGui::InputScalar("##memberslots", ImGuiDataType_U32,
                                       &host_multiplayer_room_member_slots);

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Read:");
                    ImGui::Bullet();
                    ImGui::TextUnformatted("Name is vvctre and can't be changed");
                    ImGui::Bullet();
                    ImGui::TextUnformatted("No description");
                    ImGui::Bullet();
                    ImGui::TextUnformatted("No preferred game");
                    ImGui::Bullet();
                    ImGui::TextUnformatted("No password");
                    ImGui::Bullet();
                    ImGui::TextUnformatted("No collision checks");
                    ImGui::Bullet();
                    ImGui::TextUnformatted("No message length limit");
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

            // OK
            if (!Settings::values.file_path.empty()) {
                ImGui::NewLine();

                ImGui::Dummy(
                    ImVec2(0.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFontSize() - 10.0f));

                if (ImGui::Button("OK")) {
                    Settings::Apply();
                    if (update_config_savegame) {
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
                    if (update_config_savegame) {
                        cfg.UpdateConfigNANDSavegame();
                    }
                    if (play_coins_changed) {
                        Service::PTM::Module::SetPlayCoins(play_coins);
                    }
                    ok_multiplayer = true;
                    return;
                }
            }
        }

        if (!installed.empty()) {
            ImGui::OpenPopup("Installed");

            ImGui::SetNextWindowSize(io.DisplaySize);

            bool open = true;
            if (ImGui::BeginPopupModal("Installed", &open,
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoResize)) {
                ImGui::TextUnformatted("Search:");
                ImGui::SameLine();
                ImGui::InputText("##search", &installed_query);

                if (ImGui::ListBoxHeader("##installed", ImVec2(-1.0f, -1.0f))) {
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
                    ImGui::ListBoxFooter();
                }
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
