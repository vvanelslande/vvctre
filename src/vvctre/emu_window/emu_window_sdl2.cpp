// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <sstream>
#include <string>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <asl/Date.h>
#include <asl/File.h>
#include <asl/Process.h>
#include <asl/String.h>
#include <clip.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <imgui_stdlib.h>
#include <portable-file-dialogs.h>
#ifdef HAVE_CUBEB
#include "audio_core/cubeb_input.h"
#endif
#include <stb_image_write.h>
#include "audio_core/sink.h"
#include "audio_core/sink_details.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "common/texture.h"
#include "core/3ds.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/core.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/archive_source_sd_savedata.h"
#include "core/file_sys/ncch_container.h"
#include "core/hle/applets/mii_selector.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/movie.h"
#include "core/settings.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_emu.h"
#include "input_common/sdl/sdl.h"
#include "network/room_member.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/texture_filters/texture_filterer.h"
#include "video_core/video_core.h"
#include "vvctre/common.h"
#include "vvctre/emu_window/emu_window_sdl2.h"
#include "vvctre/plugins.h"

static bool is_open = true;

static std::string IPC_Recorder_GetStatusString(IPCDebugger::RequestStatus status) {
    switch (status) {
    case IPCDebugger::RequestStatus::Sent:
        return "Sent";
    case IPCDebugger::RequestStatus::Handling:
        return "Handling";
    case IPCDebugger::RequestStatus::Handled:
        return "Handled";
    case IPCDebugger::RequestStatus::HLEUnimplemented:
        return "HLEUnimplemented";
    default:
        break;
    }

    return "Invalid";
}

void EmuWindow_SDL2::OnMouseMotion(s32 x, s32 y) {
    TouchMoved((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
    InputCommon::GetMotionEmu()->Tilt(x, y);
}

void EmuWindow_SDL2::OnMouseButton(u32 button, u8 state, s32 x, s32 y) {
    if (button == SDL_BUTTON_LEFT) {
        if (state == SDL_PRESSED) {
            TouchPressed((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
        } else {
            TouchReleased();
        }
    } else if (button == SDL_BUTTON_RIGHT) {
        if (state == SDL_PRESSED) {
            InputCommon::GetMotionEmu()->BeginTilt(x, y);
        } else {
            InputCommon::GetMotionEmu()->EndTilt();
        }
    }
}

std::pair<unsigned, unsigned> EmuWindow_SDL2::TouchToPixelPos(float touch_x, float touch_y) const {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    touch_x *= w;
    touch_y *= h;

    return {static_cast<unsigned>(std::max(std::round(touch_x), 0.0f)),
            static_cast<unsigned>(std::max(std::round(touch_y), 0.0f))};
}

void EmuWindow_SDL2::OnFingerDown(float x, float y) {
    // TODO(NeatNit): keep track of multitouch using the fingerID and a dictionary of some kind
    // This isn't critical because the best we can do when we have that is to average them, like the
    // 3DS does

    const auto [px, py] = TouchToPixelPos(x, y);
    TouchPressed(px, py);
}

void EmuWindow_SDL2::OnFingerMotion(float x, float y) {
    const auto [px, py] = TouchToPixelPos(x, y);
    TouchMoved(px, py);
}

void EmuWindow_SDL2::OnFingerUp() {
    TouchReleased();
}

void EmuWindow_SDL2::OnKeyEvent(int key, u8 state) {
    if (state == SDL_PRESSED) {
        InputCommon::GetKeyboard()->PressKey(key);
    } else if (state == SDL_RELEASED) {
        InputCommon::GetKeyboard()->ReleaseKey(key);
    }
}

bool EmuWindow_SDL2::IsOpen() const {
    return is_open;
}

void EmuWindow_SDL2::Close() {
    is_open = false;
}

void EmuWindow_SDL2::OnResize() {
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    UpdateCurrentFramebufferLayout(width, height);
}

EmuWindow_SDL2::EmuWindow_SDL2(Core::System& system, PluginManager& plugin_manager,
                               SDL_Window* window, bool& ok_multiplayer)
    : window(window), system(system), plugin_manager(plugin_manager) {
    signal(SIGINT, [](int) { is_open = false; });
    signal(SIGTERM, [](int) { is_open = false; });

    Network::RoomMember& room_member = system.RoomMember();

    room_member.BindOnStateChanged([&](const Network::RoomMember::State& state) {
        if (state == Network::RoomMember::State::Idle) {
            multiplayer_message.clear();
            multiplayer_messages.clear();
            multiplayer_blocked_nicknames.clear();
        }
    });

    room_member.BindOnError([&](const Network::RoomMember::Error& error) {
        pfd::message("vvctre", Network::GetErrorStr(error), pfd::choice::ok, pfd::icon::error);
    });

    room_member.BindOnChatMessageReceived([&](const Network::ChatEntry& entry) {
        if (multiplayer_blocked_nicknames.count(entry.nickname)) {
            return;
        }

        asl::Date date = asl::Date::now();
        multiplayer_messages.push_back(fmt::format("[{:02}:{:02}] <{}> {}", date.hours(),
                                                   date.minutes(), entry.nickname, entry.message));
    });

    room_member.BindOnStatusMessageReceived([&](const Network::StatusMessageEntry& entry) {
        if (multiplayer_blocked_nicknames.count(entry.nickname)) {
            return;
        }

        asl::Date date = asl::Date::now();

        switch (entry.type) {
        case Network::StatusMessageTypes::IdMemberJoin:
            multiplayer_messages.push_back(fmt::format("[{:02}:{:02}] {} joined", date.hours(),
                                                       date.minutes(), entry.nickname));
            break;
        case Network::StatusMessageTypes::IdMemberLeave:
            multiplayer_messages.push_back(
                fmt::format("[{:02}:{:02}] {} left", date.hours(), date.minutes(), entry.nickname));
            break;
        case Network::StatusMessageTypes::IdMemberKicked:
            multiplayer_messages.push_back(fmt::format("[{:02}:{:02}] {} was kicked", date.hours(),
                                                       date.minutes(), entry.nickname));
            break;
        case Network::StatusMessageTypes::IdMemberBanned:
            multiplayer_messages.push_back(fmt::format("[{:02}:{:02}] {} was banned", date.hours(),
                                                       date.minutes(), entry.nickname));
            break;
        case Network::StatusMessageTypes::IdAddressUnbanned:
            multiplayer_messages.push_back(
                fmt::format("[{:02}:{:02}] Someone was unbanned", date.hours(), date.minutes()));
            break;
        }
    });

    if (ok_multiplayer) {
        ConnectToCitraRoom();
    }

    SDL_SetWindowTitle(window, fmt::format("vvctre {}.{}.{}", vvctre_version_major,
                                           vvctre_version_minor, vvctre_version_patch)
                                   .c_str());

    SDL_GL_SetSwapInterval(Settings::values.enable_vsync ? 1 : 0);

    OnResize();
    SDL_PumpEvents();
    LOG_INFO(Frontend, "Version: {}.{}.{}", vvctre_version_major, vvctre_version_minor,
             vvctre_version_patch);
    LOG_INFO(Frontend, "Movie version: {}", Core::MovieVersion);
}

EmuWindow_SDL2::~EmuWindow_SDL2() = default;

void EmuWindow_SDL2::SwapBuffers() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    plugin_manager.BeforeDrawingFPS();

    ImGui::SetNextWindowPos(ImVec2(), ImGuiCond_Once);
    if (ImGui::Begin("FPS and Menu", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::TextColored(fps_color, "%d FPS", static_cast<int>(io.Framerate));
        if (ImGui::BeginPopupContextItem("Menu", ImGuiMouseButton_Right)) {
            if (ImGui::IsWindowAppearing() && !ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT)) {
                paused = true;
            }

            menu_open = true;

            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load File")) {
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
                        system.SetResetFilePath(result[0]);
                        system.RequestReset();
                    }
                }

                if (ImGui::MenuItem("Load Installed")) {
                    installed = GetInstalledList();
                }

                if (ImGui::MenuItem("Install CIA")) {
                    const std::vector<std::string> files =
                        pfd::open_file("Install CIA", *asl::Process::myDir(),
                                       {"CTR Importable Archive", "*.cia *.CIA"},
                                       pfd::opt::multiselect)
                            .result();

                    if (files.empty()) {
                        return;
                    }

                    std::shared_ptr<Service::AM::Module> am = Service::AM::GetModule(system);

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

                            const Service::AM::InstallStatus status = Service::AM::InstallCIA(
                                file, [&](std::size_t current, std::size_t total) {
                                    std::lock_guard<std::mutex> lock(mutex);
                                    current_file_current = current;
                                    current_file_total = total;
                                });

                            switch (status) {
                            case Service::AM::InstallStatus::Success:
                                if (am != nullptr) {
                                    am->ScanForAllTitles();
                                }
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

                        installing = false;
                    }).detach();

                    SDL_Event event;

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

                    return;
                }

                if (ImGui::BeginMenu("Amiibo")) {
                    if (ImGui::MenuItem("Load")) {
                        const std::vector<std::string> result =
                            pfd::open_file("Load Amiibo", *asl::Process::myDir(),
                                           {"Amiibo Files", "*.bin *.BIN"})
                                .result();

                        if (!result.empty()) {
                            FileUtil::IOFile file(result[0], "rb");
                            Service::NFC::AmiiboData data;
                            if (file.ReadArray(&data, 1) == 1) {
                                std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                    system.ServiceManager()
                                        .GetService<Service::NFC::Module::Interface>("nfc:u");
                                if (nfc != nullptr) {
                                    nfc->LoadAmiibo(data);
                                }
                            } else {
                                pfd::message("vvctre", "Failed to load the amiibo file",
                                             pfd::choice::ok, pfd::icon::error);
                            }
                        }
                    }

                    if (ImGui::MenuItem("Remove")) {
                        std::shared_ptr<Service::NFC::Module::Interface> nfc =
                            system.ServiceManager().GetService<Service::NFC::Module::Interface>(
                                "nfc:u");
                        if (nfc != nullptr) {
                            nfc->RemoveAmiibo();
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Settings")) {
                if (ImGui::BeginMenu("General")) {
                    if (ImGui::Checkbox("Use CPU JIT", &Settings::values.use_cpu_jit)) {
                        request_reset = true;
                    }

                    ImGui::PushTextWrapPos();
                    ImGui::TextUnformatted("If you enable or disable the CPU JIT, emulation will "
                                           "restart when the menu is closed.");
                    ImGui::PopTextWrapPos();

                    ImGui::NewLine();

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

                    u32 min = 5;
                    u32 max = 400;
                    ImGui::SliderScalar("CPU Clock Percentage", ImGuiDataType_U32,
                                        &Settings::values.cpu_clock_percentage, &min, &max, "%d%%");

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Audio")) {
                    ImGui::TextUnformatted("DSP LLE");
                    ImGui::Separator();

                    if (ImGui::Checkbox("Enable", &Settings::values.enable_dsp_lle)) {
                        request_reset = true;
                    }

                    if (Settings::values.enable_dsp_lle) {
                        if (ImGui::Checkbox("Use Multiple Threads",
                                            &Settings::values.enable_dsp_lle_multithread)) {
                            request_reset = true;
                        }
                    }

                    ImGui::NewLine();

                    ImGui::PushTextWrapPos();
                    ImGui::TextUnformatted("If you change anything here, emulation will "
                                           "restart when the menu is closed.");
                    ImGui::PopTextWrapPos();

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Output");
                    ImGui::Separator();

                    if (ImGui::Checkbox("Enable Stretching##Output",
                                        &Settings::values.enable_audio_stretching)) {
                        Settings::Apply();
                    }

                    ImGui::SliderFloat("Volume##Output", &Settings::values.audio_volume, 0.0f,
                                       1.0f);

                    if (ImGui::BeginCombo("Sink##Output", Settings::values.audio_sink_id.c_str())) {
                        if (ImGui::Selectable("auto")) {
                            Settings::values.audio_sink_id = "auto";
                            Settings::Apply();
                        }
                        for (const auto& sink : AudioCore::GetSinkIDs()) {
                            if (ImGui::Selectable(sink)) {
                                Settings::values.audio_sink_id = sink;
                                Settings::Apply();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    if (ImGui::BeginCombo("Device##Output",
                                          Settings::values.audio_device_id.c_str())) {
                        if (ImGui::Selectable("auto")) {
                            Settings::values.audio_device_id = "auto";
                            Settings::Apply();
                        }

                        for (const auto& device :
                             AudioCore::GetDeviceListForSink(Settings::values.audio_sink_id)) {
                            if (ImGui::Selectable(device.c_str())) {
                                Settings::values.audio_device_id = device;
                                Settings::Apply();
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
                            Settings::Apply();
                        }
                        if (ImGui::Selectable("Real Device")) {
                            Settings::values.microphone_input_type =
                                Settings::MicrophoneInputType::Real;
                            Settings::Apply();
                        }
                        if (ImGui::Selectable("Static Noise")) {
                            Settings::values.microphone_input_type =
                                Settings::MicrophoneInputType::Static;
                            Settings::Apply();
                        }
                        ImGui::EndCombo();
                    }

                    if (Settings::values.microphone_input_type ==
                        Settings::MicrophoneInputType::Real) {
                        if (ImGui::BeginCombo("Device##Microphone",
                                              Settings::values.microphone_device.c_str())) {
                            if (ImGui::Selectable("auto")) {
                                Settings::values.microphone_device = "auto";
                                Settings::Apply();
                            }
#ifdef HAVE_CUBEB
                            for (const auto& device : AudioCore::ListCubebInputDevices()) {
                                if (ImGui::Selectable(device.c_str())) {
                                    Settings::values.microphone_device = device;
                                    Settings::Apply();
                                }
                            }
#endif

                            ImGui::EndCombo();
                        }
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Camera")) {
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
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::Selectable("image (parameter: file path or URL)")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "image";
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::InnerCamera)] == "image") {
                        if (GUI_CameraAddBrowse(
                                "...##Inner",
                                static_cast<std::size_t>(Service::CAM::CameraIndex::InnerCamera))) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::InputText(
                                "Parameter##Inner",
                                &Settings::values.camera_parameter[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)])) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
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

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Horizontal")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Horizontal;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Vertical")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Vertical;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Reverse")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Reverse;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
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
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::Selectable("image (parameter: file path or URL)")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "image";
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::OuterLeftCamera)] == "image") {
                        if (GUI_CameraAddBrowse("...##Outer Left",
                                                static_cast<std::size_t>(
                                                    Service::CAM::CameraIndex::OuterLeftCamera))) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::InputText(
                                "Parameter##Outer Left",
                                &Settings::values.camera_parameter[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)])) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
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

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Horizontal")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Horizontal;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Vertical")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Vertical;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Reverse")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Reverse;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
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
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::Selectable("image (parameter: file path or URL)")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "image";
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::OuterRightCamera)] == "image") {
                        if (GUI_CameraAddBrowse("...##Outer Right",
                                                static_cast<std::size_t>(
                                                    Service::CAM::CameraIndex::OuterRightCamera))) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::InputText(
                                "Parameter##Outer Right",
                                &Settings::values.camera_parameter[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)])) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
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

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Horizontal")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Horizontal;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Vertical")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Vertical;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Reverse")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Reverse;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }
                    ImGui::Unindent();

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("System")) {
                    std::shared_ptr<Service::CFG::Module> cfg = Service::CFG::GetModule(system);

                    if (cfg != nullptr) {
                        ImGui::PushTextWrapPos();
                        ImGui::TextUnformatted("If you change anything here, emulation will "
                                               "restart when the menu is closed.");
                        ImGui::PopTextWrapPos();
                        ImGui::NewLine();

                        ImGui::TextUnformatted("Config Savegame");
                        ImGui::Separator();

                        std::string username = Common::UTF16ToUTF8(cfg->GetUsername());
                        if (ImGui::InputText("Username", &username)) {
                            cfg->SetUsername(Common::UTF8ToUTF16(username));
                            config_savegame_changed = true;
                        }

                        auto [month, day] = cfg->GetBirthday();

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
                                cfg->SetBirthday(1, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("February")) {
                                cfg->SetBirthday(2, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("March")) {
                                cfg->SetBirthday(3, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("April")) {
                                cfg->SetBirthday(4, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("May")) {
                                cfg->SetBirthday(5, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("June")) {
                                cfg->SetBirthday(6, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("July")) {
                                cfg->SetBirthday(7, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("August")) {
                                cfg->SetBirthday(8, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("September")) {
                                cfg->SetBirthday(9, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("October")) {
                                cfg->SetBirthday(10, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("November")) {
                                cfg->SetBirthday(11, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("December")) {
                                cfg->SetBirthday(12, day);
                                config_savegame_changed = true;
                            }

                            ImGui::EndCombo();
                        }

                        if (ImGui::InputScalar("Birthday Day", ImGuiDataType_U8, &day)) {
                            cfg->SetBirthday(month, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::BeginCombo("Language", [&] {
                                switch (cfg->GetSystemLanguage()) {
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
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_JP);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("English")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_EN);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("French")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_FR);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("German")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_DE);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Italian")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_IT);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Spanish")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ES);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Simplified Chinese")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ZH);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Korean")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_KO);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Dutch")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_NL);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Portuguese")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_PT);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Russian")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_RU);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Traditional Chinese")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_TW);
                                config_savegame_changed = true;
                            }

                            ImGui::EndCombo();
                        }

                        if (ImGui::BeginCombo("Sound Output Mode", [&] {
                                switch (cfg->GetSoundOutputMode()) {
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
                                cfg->SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_MONO);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Stereo")) {
                                cfg->SetSoundOutputMode(
                                    Service::CFG::SoundOutputMode::SOUND_STEREO);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Surround")) {
                                cfg->SetSoundOutputMode(
                                    Service::CFG::SoundOutputMode::SOUND_SURROUND);
                                config_savegame_changed = true;
                            }
                            ImGui::EndCombo();
                        }

                        if (ImGui::BeginCombo("Country", [&] {
                                switch (cfg->GetCountryCode()) {
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
                                cfg->SetCountry(1);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Anguilla")) {
                                cfg->SetCountry(8);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Antigua and Barbuda")) {
                                cfg->SetCountry(9);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Argentina")) {
                                cfg->SetCountry(10);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Aruba")) {
                                cfg->SetCountry(11);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Bahamas")) {
                                cfg->SetCountry(12);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Barbados")) {
                                cfg->SetCountry(13);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Belize")) {
                                cfg->SetCountry(14);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Bolivia")) {
                                cfg->SetCountry(15);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Brazil")) {
                                cfg->SetCountry(16);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("British Virgin Islands")) {
                                cfg->SetCountry(17);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Canada")) {
                                cfg->SetCountry(18);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Cayman Islands")) {
                                cfg->SetCountry(19);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Chile")) {
                                cfg->SetCountry(20);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Colombia")) {
                                cfg->SetCountry(21);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Costa Rica")) {
                                cfg->SetCountry(22);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Dominica")) {
                                cfg->SetCountry(23);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Dominican Republic")) {
                                cfg->SetCountry(24);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Ecuador")) {
                                cfg->SetCountry(25);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("El Salvador")) {
                                cfg->SetCountry(26);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("French Guiana")) {
                                cfg->SetCountry(27);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Grenada")) {
                                cfg->SetCountry(28);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Guadeloupe")) {
                                cfg->SetCountry(29);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Guatemala")) {
                                cfg->SetCountry(30);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Guyana")) {
                                cfg->SetCountry(31);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Haiti")) {
                                cfg->SetCountry(32);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Honduras")) {
                                cfg->SetCountry(33);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Jamaica")) {
                                cfg->SetCountry(34);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Martinique")) {
                                cfg->SetCountry(35);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Mexico")) {
                                cfg->SetCountry(36);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Montserrat")) {
                                cfg->SetCountry(37);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Netherlands Antilles")) {
                                cfg->SetCountry(38);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Nicaragua")) {
                                cfg->SetCountry(39);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Panama")) {
                                cfg->SetCountry(40);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Paraguay")) {
                                cfg->SetCountry(41);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Peru")) {
                                cfg->SetCountry(42);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Saint Kitts and Nevis")) {
                                cfg->SetCountry(43);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Saint Lucia")) {
                                cfg->SetCountry(44);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Saint Vincent and the Grenadines")) {
                                cfg->SetCountry(45);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Suriname")) {
                                cfg->SetCountry(46);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Trinidad and Tobago")) {
                                cfg->SetCountry(47);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Turks and Caicos Islands")) {
                                cfg->SetCountry(48);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("United States")) {
                                cfg->SetCountry(49);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Uruguay")) {
                                cfg->SetCountry(50);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("US Virgin Islands")) {
                                cfg->SetCountry(51);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Venezuela")) {
                                cfg->SetCountry(52);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Albania")) {
                                cfg->SetCountry(64);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Australia")) {
                                cfg->SetCountry(65);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Austria")) {
                                cfg->SetCountry(66);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Belgium")) {
                                cfg->SetCountry(67);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Bosnia and Herzegovina")) {
                                cfg->SetCountry(68);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Botswana")) {
                                cfg->SetCountry(69);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Bulgaria")) {
                                cfg->SetCountry(70);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Croatia")) {
                                cfg->SetCountry(71);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Cyprus")) {
                                cfg->SetCountry(72);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Czech Republic")) {
                                cfg->SetCountry(73);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Denmark")) {
                                cfg->SetCountry(74);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Estonia")) {
                                cfg->SetCountry(75);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Finland")) {
                                cfg->SetCountry(76);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("France")) {
                                cfg->SetCountry(77);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Germany")) {
                                cfg->SetCountry(78);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Greece")) {
                                cfg->SetCountry(79);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Hungary")) {
                                cfg->SetCountry(80);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Iceland")) {
                                cfg->SetCountry(81);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Ireland")) {
                                cfg->SetCountry(82);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Italy")) {
                                cfg->SetCountry(83);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Latvia")) {
                                cfg->SetCountry(84);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Lesotho")) {
                                cfg->SetCountry(85);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Liechtenstein")) {
                                cfg->SetCountry(86);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Lithuania")) {
                                cfg->SetCountry(87);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Luxembourg")) {
                                cfg->SetCountry(88);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Macedonia")) {
                                cfg->SetCountry(89);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Malta")) {
                                cfg->SetCountry(90);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Montenegro")) {
                                cfg->SetCountry(91);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Mozambique")) {
                                cfg->SetCountry(92);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Namibia")) {
                                cfg->SetCountry(93);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Netherlands")) {
                                cfg->SetCountry(94);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("New Zealand")) {
                                cfg->SetCountry(95);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Norway")) {
                                cfg->SetCountry(96);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Poland")) {
                                cfg->SetCountry(97);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Portugal")) {
                                cfg->SetCountry(98);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Romania")) {
                                cfg->SetCountry(99);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Russia")) {
                                cfg->SetCountry(100);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Serbia")) {
                                cfg->SetCountry(101);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Slovakia")) {
                                cfg->SetCountry(102);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Slovenia")) {
                                cfg->SetCountry(103);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("South Africa")) {
                                cfg->SetCountry(104);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Spain")) {
                                cfg->SetCountry(105);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Swaziland")) {
                                cfg->SetCountry(106);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Sweden")) {
                                cfg->SetCountry(107);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Switzerland")) {
                                cfg->SetCountry(108);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Turkey")) {
                                cfg->SetCountry(109);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("United Kingdom")) {
                                cfg->SetCountry(110);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Zambia")) {
                                cfg->SetCountry(111);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Zimbabwe")) {
                                cfg->SetCountry(112);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Azerbaijan")) {
                                cfg->SetCountry(113);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Mauritania")) {
                                cfg->SetCountry(114);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Mali")) {
                                cfg->SetCountry(115);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Niger")) {
                                cfg->SetCountry(116);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Chad")) {
                                cfg->SetCountry(117);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Sudan")) {
                                cfg->SetCountry(118);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Eritrea")) {
                                cfg->SetCountry(119);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Djibouti")) {
                                cfg->SetCountry(120);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Somalia")) {
                                cfg->SetCountry(121);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Andorra")) {
                                cfg->SetCountry(122);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Gibraltar")) {
                                cfg->SetCountry(123);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Guernsey")) {
                                cfg->SetCountry(124);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Isle of Man")) {
                                cfg->SetCountry(125);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Jersey")) {
                                cfg->SetCountry(126);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Monaco")) {
                                cfg->SetCountry(127);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Taiwan")) {
                                cfg->SetCountry(128);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("South Korea")) {
                                cfg->SetCountry(136);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Hong Kong")) {
                                cfg->SetCountry(144);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Macau")) {
                                cfg->SetCountry(145);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Indonesia")) {
                                cfg->SetCountry(152);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Singapore")) {
                                cfg->SetCountry(153);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Thailand")) {
                                cfg->SetCountry(154);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Philippines")) {
                                cfg->SetCountry(155);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Malaysia")) {
                                cfg->SetCountry(156);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("China")) {
                                cfg->SetCountry(160);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("United Arab Emirates")) {
                                cfg->SetCountry(168);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("India")) {
                                cfg->SetCountry(169);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Egypt")) {
                                cfg->SetCountry(170);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Oman")) {
                                cfg->SetCountry(171);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Qatar")) {
                                cfg->SetCountry(172);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Kuwait")) {
                                cfg->SetCountry(173);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Saudi Arabia")) {
                                cfg->SetCountry(174);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Syria")) {
                                cfg->SetCountry(175);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Bahrain")) {
                                cfg->SetCountry(176);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Jordan")) {
                                cfg->SetCountry(177);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("San Marino")) {
                                cfg->SetCountry(184);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Vatican City")) {
                                cfg->SetCountry(185);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Bermuda")) {
                                cfg->SetCountry(186);
                                config_savegame_changed = true;
                            }
                            ImGui::EndCombo();
                        }

                        if (ImGui::Button("Regenerate Console ID")) {
                            u32 random_number;
                            u64 console_id;
                            cfg->GenerateConsoleUniqueId(random_number, console_id);
                            cfg->SetConsoleUniqueId(random_number, console_id);
                            config_savegame_changed = true;
                        }

                        if (ImGui::BeginPopupContextItem("Console ID",
                                                         ImGuiPopupFlags_MouseButtonRight)) {
                            std::string console_id =
                                fmt::format("0x{:016X}", cfg->GetConsoleUniqueId());
                            ImGui::InputText("##Console ID", &console_id[0], 18,
                                             ImGuiInputTextFlags_ReadOnly);
                            ImGui::EndPopup();
                        }

                        ImGui::NewLine();
                    }

                    ImGui::TextUnformatted("Play Coins");
                    ImGui::Separator();
                    const u16 min = 0;
                    const u16 max = 300;
                    if (ImGui::IsWindowAppearing()) {
                        play_coins = Service::PTM::Module::GetPlayCoins();
                    }
                    if (ImGui::SliderScalar("Play Coins", ImGuiDataType_U16, &play_coins, &min,
                                            &max)) {
                        play_coins_changed = true;
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Graphics")) {
                    if (ImGui::Checkbox("Use Hardware Renderer",
                                        &Settings::values.use_hardware_renderer)) {
                        Settings::Apply();
                    }

                    if (Settings::values.use_hardware_renderer) {
                        ImGui::Indent();

                        if (ImGui::Checkbox("Use Hardware Shader",
                                            &Settings::values.use_hardware_shader)) {
                            Settings::Apply();
                        }

                        if (Settings::values.use_hardware_shader) {
                            ImGui::Indent();

                            if (ImGui::Checkbox(
                                    "Accurate Multiplication",
                                    &Settings::values.hardware_shader_accurate_multiplication)) {
                                Settings::Apply();
                            }

                            ImGui::Unindent();
                        }

                        ImGui::Unindent();
                    }

                    ImGui::Checkbox("Use Shader JIT", &Settings::values.use_shader_jit);
                    ImGui::Checkbox("Enable VSync", &Settings::values.enable_vsync);

                    if (ImGui::Checkbox("Enable Linear Filtering",
                                        &Settings::values.enable_linear_filtering)) {
                        Settings::Apply();
                    }

                    if (ImGui::Checkbox("Sharper Distant Objects",
                                        &Settings::values.sharper_distant_objects)) {
                        request_reset = true;
                    }

                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::PushTextWrapPos(io.DisplaySize.x * 0.5f);
                        ImGui::TextUnformatted(
                            "If you change this, emulation will restart when the menu is closed");
                        ImGui::PopTextWrapPos();
                        ImGui::EndTooltip();
                    }

                    ImGui::Checkbox("Dump Textures", &Settings::values.dump_textures);
                    ImGui::Checkbox("Use Custom Textures", &Settings::values.custom_textures);
                    ImGui::Checkbox("Preload Custom Textures", &Settings::values.preload_textures);

                    if (ImGui::ColorEdit3("Background Color",
                                          &Settings::values.background_color_red,
                                          ImGuiColorEditFlags_NoInputs)) {
                        VideoCore::g_renderer_background_color_update_requested = true;
                    }

                    const u16 min = 0;
                    const u16 max = 10;
                    ImGui::SliderScalar("Resolution", ImGuiDataType_U16,
                                        &Settings::values.resolution, &min, &max,
                                        Settings::values.resolution == 0 ? "Window Size" : "%d");

                    ImGui::InputText("Post Processing Shader",
                                     &Settings::values.post_processing_shader);
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        VideoCore::g_renderer_shader_update_requested = true;
                    }
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
                                Settings::Apply();
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
                            Settings::Apply();
                        }

                        if (ImGui::Selectable("Side by Side",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::SideBySide)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::SideBySide;
                            Settings::values.post_processing_shader = "none (builtin)";
                            Settings::Apply();
                        }

                        if (ImGui::Selectable("Anaglyph",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Anaglyph)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Anaglyph;
                            Settings::values.post_processing_shader = "dubois (builtin)";
                            Settings::Apply();
                        }

                        if (ImGui::Selectable("Interlaced",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Interlaced)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Interlaced;
                            Settings::values.post_processing_shader = "horizontal (builtin)";
                            Settings::Apply();
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

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Controls")) {
                    GUI_AddControlsSettings(is_open, &system, plugin_manager, io);

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("LLE Modules")) {
                    ImGui::PushTextWrapPos(io.DisplaySize.x * 0.5f);
                    ImGui::TextUnformatted("If you enable or disable a LLE module, emulation will "
                                           "restart when the menu is closed.");
                    ImGui::PopTextWrapPos();

                    for (auto& module : Settings::values.lle_modules) {
                        if (ImGui::Checkbox(module.first.c_str(), &module.second)) {
                            request_reset = true;
                        }
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Hacks")) {
                    if (ImGui::Checkbox("Priority Boost",
                                        &Settings::values.enable_priority_boost)) {
                        request_reset = true;
                    }

                    ImGui::PushTextWrapPos(io.DisplaySize.x * 0.5f);
                    ImGui::TextUnformatted(
                        "If you enable or disable Priority Boost, emulation will "
                        "restart when the menu is closed.");
                    ImGui::PopTextWrapPos();

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("GUI")) {
                    ImGui::ColorEdit4("FPS Color", &fps_color.x,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
                                          ImGuiColorEditFlags_AlphaPreview);

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                if (ImGui::BeginMenu("Layout")) {
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
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }
                            if (ImGui::Selectable("Single Screen")) {
                                Settings::values.layout = Settings::Layout::SingleScreen;
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }
                            if (ImGui::Selectable("Large Screen")) {
                                Settings::values.layout = Settings::Layout::LargeScreen;
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }
                            if (ImGui::Selectable("Side by Side")) {
                                Settings::values.layout = Settings::Layout::SideScreen;
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }
                            if (ImGui::Selectable("Medium Screen")) {
                                Settings::values.layout = Settings::Layout::MediumScreen;
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }
                            ImGui::EndCombo();
                        }
                    }

                    if (ImGui::Checkbox("Use Custom Layout", &Settings::values.use_custom_layout)) {
                        VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                    }

                    if (ImGui::Checkbox("Swap Screens", &Settings::values.swap_screens)) {
                        VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                    }

                    if (ImGui::Checkbox("Upright Screens", &Settings::values.upright_screens)) {
                        VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                    }

                    if (Settings::values.use_custom_layout) {
                        ImGui::NewLine();

                        ImGui::TextUnformatted("Top Screen");
                        ImGui::Separator();

                        if (ImGui::InputScalar("Left##Top Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_top_left)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        if (ImGui::InputScalar("Top##Top Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_top_top)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        if (ImGui::InputScalar("Right##Top Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_top_right)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        if (ImGui::InputScalar("Bottom##Top Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_top_bottom)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        ImGui::NewLine();

                        ImGui::TextUnformatted("Bottom Screen");
                        ImGui::Separator();

                        if (ImGui::InputScalar("Left##Bottom Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_bottom_left)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        if (ImGui::InputScalar("Top##Bottom Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_bottom_top)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        if (ImGui::InputScalar("Right##Bottom Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_bottom_right)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        if (ImGui::InputScalar("Bottom##Bottom Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_bottom_bottom)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Debugging")) {
                    if (ImGui::Checkbox("IPC Recorder", &show_ipc_recorder_window)) {
                        if (!show_ipc_recorder_window) {
                            IPCDebugger::Recorder& r = system.Kernel().GetIPCRecorder();

                            r.SetEnabled(false);
                            r.UnbindCallback(ipc_recorder_callback);

                            all_ipc_records.clear();
                            ipc_recorder_search_results.clear();
                            ipc_recorder_search_text.clear();
                            ipc_recorder_callback = nullptr;
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::Checkbox("Cheats", &show_cheats_window);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Emulation")) {
                if (ImGui::MenuItem("Restart")) {
                    request_reset = true;
                }

                if (ImGui::MenuItem("Restart With Different Log Filter")) {
                    SDL_Event event;
                    std::string new_log_filter = Settings::values.log_filter;
                    bool new_log_filter_window_open = true;

                    while (is_open) {
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

                        ImGui::OpenPopup("New Log Filter");
                        ImGui::SetNextWindowPos(
                            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                        if (ImGui::BeginPopupModal("New Log Filter", &new_log_filter_window_open,
                                                   ImGuiWindowFlags_NoSavedSettings |
                                                       ImGuiWindowFlags_NoMove |
                                                       ImGuiWindowFlags_AlwaysAutoResize)) {
                            if (ImGui::InputText("##New Log Filter", &new_log_filter,
                                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                                Settings::values.log_filter = new_log_filter;
                                Log::Filter log_filter(Log::Level::Debug);
                                log_filter.ParseFilterString(Settings::values.log_filter);
                                Log::SetGlobalFilter(log_filter);
                                request_reset = true;
                                return;
                            }
                            ImGui::EndPopup();
                        }

                        if (!new_log_filter_window_open) {
                            return;
                        }

                        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                        glClear(GL_COLOR_BUFFER_BIT);
                        ImGui::Render();
                        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                        SDL_GL_SwapWindow(window);
                    }
                }

                if (ImGui::BeginMenu("Restart With Different Region")) {
                    if (Settings::values.region_value != Settings::REGION_VALUE_AUTO_SELECT &&
                        ImGui::MenuItem("Auto-select")) {
                        Settings::values.region_value = Settings::REGION_VALUE_AUTO_SELECT;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 0 && ImGui::MenuItem("Japan")) {
                        Settings::values.region_value = 0;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 1 && ImGui::MenuItem("USA")) {
                        Settings::values.region_value = 1;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 2 && ImGui::MenuItem("Europe")) {
                        Settings::values.region_value = 2;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 3 && ImGui::MenuItem("Australia")) {
                        Settings::values.region_value = 3;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 4 && ImGui::MenuItem("China")) {
                        Settings::values.region_value = 4;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 5 && ImGui::MenuItem("Korea")) {
                        Settings::values.region_value = 5;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 6 && ImGui::MenuItem("Taiwan")) {
                        Settings::values.region_value = 6;
                        request_reset = true;
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tools")) {
                if (ImGui::MenuItem("Dump RomFS")) {
                    const std::string folder = pfd::select_folder("Dump RomFS").result();

                    if (!folder.empty()) {
                        Loader::AppLoader& loader = system.GetAppLoader();

                        if (loader.DumpRomFS(folder) == Loader::ResultStatus::Success) {
                            loader.DumpUpdateRomFS(folder);
                            pfd::message("vvctre", "RomFS dumped", pfd::choice::ok);
                        } else {
                            pfd::message("vvctre", "Failed to dump RomFS", pfd::choice::ok,
                                         pfd::icon::error);
                        }
                    }
                }

                if (ImGui::BeginMenu("Files")) {
                    if (ImGui::MenuItem("Copy Cheats File Path")) {
                        const u64 program_id =
                            system.Kernel().GetCurrentProcess()->codeset->program_id;
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                fmt::format("{}{:016X}.txt",
                                            FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir),
                                            program_id),
                                FileUtil::DirectorySeparator::PlatformDefault)
                                .c_str());
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Folders")) {
                    if (ImGui::MenuItem("Copy Save Data Folder Path")) {
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                FileSys::ArchiveSource_SDSaveData::GetSaveDataPathFor(
                                    FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                                    system.Kernel().GetCurrentProcess()->codeset->program_id),
                                FileUtil::DirectorySeparator::PlatformDefault)
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Extra Data Folder Path")) {
                        u64 extdata_id = 0;
                        system.GetAppLoader().ReadExtdataId(extdata_id);
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                FileSys::GetExtDataPathFromId(
                                    FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), extdata_id),
                                FileUtil::DirectorySeparator::PlatformDefault)
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Title Folder Path")) {
                        const u64 program_id =
                            system.Kernel().GetCurrentProcess()->codeset->program_id;
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                Service::AM::GetTitlePath(
                                    Service::AM::GetTitleMediaType(program_id), program_id))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Update Folder Path")) {
                        const u64 program_id =
                            system.Kernel().GetCurrentProcess()->codeset->program_id;
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                Service::AM::GetTitlePath(Service::FS::MediaType::SDMC,
                                                          0x0004000e00000000 |
                                                              static_cast<u32>(program_id)))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Mod Folder Path")) {
                        const u64 program_id =
                            system.Kernel().GetCurrentProcess()->codeset->program_id;
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                fmt::format("{}luma/titles/{:016X}",
                                            FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                                            FileSys::GetModId(program_id)))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Cheats Folder Path")) {
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy SysData Folder Path")) {
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Custom Textures Folder Path")) {
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                fmt::format(
                                    "{}textures/{:016X}",
                                    FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                                    system.Kernel().GetCurrentProcess()->codeset->program_id))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Dumped Textures Folder Path")) {
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                fmt::format(
                                    "{}textures/{:016X}",
                                    FileUtil::GetUserPath(FileUtil::UserPath::DumpDir),
                                    system.Kernel().GetCurrentProcess()->codeset->program_id))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Post Processing Shaders Folder Path")) {
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir))
                                .c_str());
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Screenshot")) {
                    if (ImGui::MenuItem("Save Screenshot")) {
                        const auto& layout = GetFramebufferLayout();
                        u8* data = new u8[layout.width * layout.height * 4];
                        if (VideoCore::RequestScreenshot(
                                data,
                                [=] {
                                    const auto filename =
                                        pfd::save_file("Save Screenshot", "screenshot.png",
                                                       {"Portable Network Graphics", "*.png"})
                                            .result();
                                    if (!filename.empty()) {
                                        std::vector<u8> v(layout.width * layout.height * 4);
                                        std::memcpy(v.data(), data, v.size());
                                        delete[] data;

                                        const auto convert_bgra_to_rgba =
                                            [](const std::vector<u8>& input,
                                               const Layout::FramebufferLayout& layout) {
                                                int offset = 0;
                                                std::vector<u8> output(input.size());

                                                for (u32 y = 0; y < layout.height; ++y) {
                                                    for (u32 x = 0; x < layout.width; ++x) {
                                                        output[offset] = input[offset + 2];
                                                        output[offset + 1] = input[offset + 1];
                                                        output[offset + 2] = input[offset];
                                                        output[offset + 3] = input[offset + 3];

                                                        offset += 4;
                                                    }
                                                }

                                                return output;
                                            };

                                        v = convert_bgra_to_rgba(v, layout);
                                        Common::FlipRGBA8Texture(v, static_cast<u64>(layout.width),
                                                                 static_cast<u64>(layout.height));

                                        stbi_write_png(filename.c_str(), layout.width,
                                                       layout.height, 4, v.data(),
                                                       layout.width * 4);
                                    }
                                },
                                layout)) {
                            delete[] data;
                        }
                    }

                    if (ImGui::MenuItem("Copy Screenshot")) {
                        CopyScreenshot();
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Movie")) {
                    auto& movie = Core::Movie::GetInstance();

                    if (ImGui::MenuItem("Play", nullptr, nullptr,
                                        !movie.IsPlayingInput() && !movie.IsRecordingInput())) {
                        const auto filename = pfd::open_file("Play Movie", *asl::Process::myDir(),
                                                             {"VvCtre Movie", "*.vcm"})
                                                  .result();
                        if (!filename.empty()) {
                            const Core::Movie::ValidationResult movie_result =
                                movie.ValidateMovie(filename[0]);
                            switch (movie_result) {
                            case Core::Movie::ValidationResult::OK:
                                if (asl::File(filename[0].c_str()).name().contains("loop")) {
                                    play_movie_loop_callback = [this, &movie,
                                                                filename = filename[0]] {
                                        movie.StartPlayback(filename, play_movie_loop_callback);
                                    };

                                    play_movie_loop_callback();
                                } else {
                                    movie.StartPlayback(filename[0], [&] {
                                        pfd::message("vvctre", "Playback finished",
                                                     pfd::choice::ok);
                                    });
                                }
                                break;
                            case Core::Movie::ValidationResult::GameDismatch:
                                pfd::message(
                                    "vvctre",
                                    "Movie was recorded using a ROM with a different program ID",
                                    pfd::choice::ok, pfd::icon::warning);
                                if (asl::File(filename[0].c_str()).name().contains("loop")) {
                                    play_movie_loop_callback = [this, &movie,
                                                                filename = filename[0]] {
                                        movie.StartPlayback(filename, play_movie_loop_callback);
                                    };

                                    play_movie_loop_callback();
                                } else {
                                    movie.StartPlayback(filename[0], [&] {
                                        pfd::message("vvctre", "Playback finished",
                                                     pfd::choice::ok);
                                    });
                                }
                                break;
                            case Core::Movie::ValidationResult::Invalid:
                                pfd::message("vvctre", "Movie file doesn't have a valid header",
                                             pfd::choice::ok, pfd::icon::info);
                                break;
                            }
                        }
                    }

                    if (ImGui::MenuItem("Record", nullptr, nullptr,
                                        !movie.IsPlayingInput() && !movie.IsRecordingInput())) {
                        const std::string filename =
                            pfd::save_file("Record Movie", "movie.vcm", {"VvCtre Movie", "*.vcm"})
                                .result();
                        if (!filename.empty()) {
                            movie.StartRecording(filename);
                        }
                    }

                    if (ImGui::MenuItem("Stop Playback/Recording", nullptr, nullptr,
                                        movie.IsPlayingInput() || movie.IsRecordingInput())) {
                        movie.Shutdown();
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Multiplayer")) {
                if (ImGui::MenuItem("Connect To Citra Room")) {
                    if (!ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT)) {
                        public_rooms = GetPublicCitraRooms();
                    }
                    show_connect_to_citra_room = true;
                }
                ImGui::EndMenu();
            }

            plugin_manager.AddMenus();

            ImGui::EndPopup();
        } else if (menu_open) {
            if (play_coins_changed) {
                Service::PTM::Module::SetPlayCoins(play_coins);
                request_reset = true;
                play_coins_changed = false;
            }
            if (config_savegame_changed) {
                Service::CFG::GetModule(system)->UpdateConfigNANDSavegame();
                request_reset = true;
                config_savegame_changed = false;
            }
            if (request_reset) {
                system.RequestReset();
                request_reset = false;
            }
            paused = false;
            menu_open = false;
        }
    }
    ImGui::End();

    if (keyboard_data != nullptr) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Keyboard", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
            if (!keyboard_data->config.hint_text.empty()) {
                ImGui::TextUnformatted(keyboard_data->config.hint_text.c_str());
            }

            if (keyboard_data->config.multiline_mode) {
                ImGui::InputTextMultiline("##text_multiline", &keyboard_data->text);
            } else {
                ImGui::InputText("##text_one_line", &keyboard_data->text);
            }

            switch (keyboard_data->config.button_config) {
            case Frontend::ButtonConfig::None:
            case Frontend::ButtonConfig::Single: {
                if (ImGui::Button((keyboard_data->config.button_text[2].empty()
                                       ? Frontend::SWKBD_BUTTON_OKAY
                                       : keyboard_data->config.button_text[2])
                                      .c_str())) {
                    keyboard_data = nullptr;
                }
                break;
            }

            case Frontend::ButtonConfig::Dual: {
                const std::string cancel = keyboard_data->config.button_text[0].empty()
                                               ? Frontend::SWKBD_BUTTON_CANCEL
                                               : keyboard_data->config.button_text[0];
                const std::string ok = keyboard_data->config.button_text[2].empty()
                                           ? Frontend::SWKBD_BUTTON_OKAY
                                           : keyboard_data->config.button_text[2];
                if (ImGui::Button(cancel.c_str())) {
                    keyboard_data = nullptr;
                    break;
                }
                if (Frontend::SoftwareKeyboard::ValidateInput(keyboard_data->text,
                                                              keyboard_data->config) ==
                    Frontend::ValidationError::None) {
                    ImGui::SameLine();
                    if (ImGui::Button(ok.c_str())) {
                        keyboard_data->code = 1;
                        keyboard_data = nullptr;
                    }
                }
                break;
            }

            case Frontend::ButtonConfig::Triple: {
                const std::string cancel = keyboard_data->config.button_text[0].empty()
                                               ? Frontend::SWKBD_BUTTON_CANCEL
                                               : keyboard_data->config.button_text[0];
                const std::string forgot = keyboard_data->config.button_text[1].empty()
                                               ? Frontend::SWKBD_BUTTON_FORGOT
                                               : keyboard_data->config.button_text[1];
                const std::string ok = keyboard_data->config.button_text[2].empty()
                                           ? Frontend::SWKBD_BUTTON_OKAY
                                           : keyboard_data->config.button_text[2];
                if (ImGui::Button(cancel.c_str())) {
                    keyboard_data = nullptr;
                    break;
                }
                ImGui::SameLine();
                if (ImGui::Button(forgot.c_str())) {
                    keyboard_data->code = 1;
                    keyboard_data = nullptr;
                    break;
                }
                if (Frontend::SoftwareKeyboard::ValidateInput(keyboard_data->text,
                                                              keyboard_data->config) ==
                    Frontend::ValidationError::None) {
                    ImGui::SameLine();
                    if (ImGui::Button(ok.c_str())) {
                        keyboard_data->code = 2;
                        keyboard_data = nullptr;
                    }
                }
                break;
            }
            }
        }
        ImGui::End();
    }

    if (mii_selector_data != nullptr) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin((mii_selector_data->config.title.empty() ? "Mii Selector"
                                                                  : mii_selector_data->config.title)
                             .c_str(),
                         nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::ListBoxHeader("##miis")) {
                ImGui::TextUnformatted("Standard Mii");
                ImGui::Separator();

                if (ImGui::Selectable("vvctre")) {
                    mii_selector_data->code = 0;
                    mii_selector_data->selected_mii =
                        HLE::Applets::MiiSelector::GetStandardMiiResult().selected_mii_data;
                    mii_selector_data = nullptr;
                }

                if (mii_selector_data != nullptr && !mii_selector_data->miis.empty()) {
                    ImGui::NewLine();

                    ImGui::TextUnformatted("Your Miis");
                    ImGui::Separator();

                    for (std::size_t index = 0; index < mii_selector_data->miis.size(); ++index) {
                        const HLE::Applets::MiiData& mii = mii_selector_data->miis[index];
                        if (ImGui::Selectable((Common::UTF16BufferToUTF8(mii.mii_name) +
                                               fmt::format("##{}", static_cast<u32>(mii.mii_id)))
                                                  .c_str())) {
                            mii_selector_data->code = 0;
                            mii_selector_data->selected_mii = mii;
                            mii_selector_data = nullptr;
                            break;
                        }
                    }
                }
                ImGui::ListBoxFooter();
            }

            if (mii_selector_data != nullptr && mii_selector_data->config.enable_cancel_button &&
                ImGui::Button("Cancel")) {
                mii_selector_data = nullptr;
            }
        }
        ImGui::End();
    }

    if (show_ipc_recorder_window) {
        ImGui::SetNextWindowSize(ImVec2(480, 640), ImGuiCond_Appearing);
        if (ImGui::Begin("IPC Recorder", &show_ipc_recorder_window,
                         ImGuiWindowFlags_NoSavedSettings)) {
            IPCDebugger::Recorder& r = system.Kernel().GetIPCRecorder();
            bool enabled = r.IsEnabled();

            if (ImGui::Checkbox("Enabled", &enabled)) {
                r.SetEnabled(enabled);

                if (enabled) {
                    ipc_recorder_callback =
                        r.BindCallback([&](const IPCDebugger::RequestRecord& record) {
                            const int index = record.id - ipc_recorder_id_offset;
                            if (all_ipc_records.size() > index) {
                                all_ipc_records[index] = record;
                            } else {
                                all_ipc_records.emplace_back(record);
                            }
                            if (!ipc_recorder_search_text.empty()) {
                                std::string service_name;
                                std::string function_name = "Unknown";
                                if (record.client_port.id != -1) {
                                    service_name = system.ServiceManager().GetServiceNameByPortId(
                                        static_cast<u32>(record.client_port.id));
                                }
                                if (service_name.empty()) {
                                    service_name = record.server_session.name;
                                    service_name = Common::ReplaceAll(service_name, "_Server", "");
                                    service_name = Common::ReplaceAll(service_name, "_Client", "");
                                }
                                const std::string label = fmt::format(
                                    "#{} - {} - {} (0x{:08X}) - {} - {}", record.id, service_name,
                                    record.function_name.empty() ? "Unknown" : record.function_name,
                                    record.untranslated_request_cmdbuf.empty()
                                        ? 0xFFFFFFFF
                                        : record.untranslated_request_cmdbuf[0],
                                    record.is_hle ? "HLE" : "LLE",
                                    IPC_Recorder_GetStatusString(record.status));

                                if (label.find(ipc_recorder_search_text) != std::string::npos) {
                                    ipc_recorder_search_results.push_back(record);
                                }
                            }
                        });
                } else {
                    r.UnbindCallback(ipc_recorder_callback);
                    ipc_recorder_callback = nullptr;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                ipc_recorder_id_offset += all_ipc_records.size();
                all_ipc_records.clear();
                ipc_recorder_search_results.clear();
            }
            ImGui::SameLine();
            if (ImGui::InputTextWithHint("##search", "Search", &ipc_recorder_search_text_,
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                ipc_recorder_search_text = ipc_recorder_search_text_;
                ipc_recorder_search_results.clear();

                if (!ipc_recorder_search_text.empty()) {
                    for (const IPCDebugger::RequestRecord& record : all_ipc_records) {
                        std::string service_name;
                        std::string function_name = "Unknown";
                        if (record.client_port.id != -1) {
                            service_name = system.ServiceManager().GetServiceNameByPortId(
                                static_cast<u32>(record.client_port.id));
                        }
                        if (service_name.empty()) {
                            service_name = record.server_session.name;
                            service_name = Common::ReplaceAll(service_name, "_Server", "");
                            service_name = Common::ReplaceAll(service_name, "_Client", "");
                        }
                        const std::string label = fmt::format(
                            "#{} - {} - {} (0x{:08X}) - {} - {}", record.id, service_name,
                            record.function_name.empty() ? "Unknown" : record.function_name,
                            record.untranslated_request_cmdbuf.empty()
                                ? 0xFFFFFFFF
                                : record.untranslated_request_cmdbuf[0],
                            record.is_hle ? "HLE" : "LLE",
                            IPC_Recorder_GetStatusString(record.status));

                        if (label.find(ipc_recorder_search_text) != std::string::npos) {
                            ipc_recorder_search_results.push_back(record);
                        }
                    }
                }
            }
            const float width = ImGui::GetWindowWidth();
            if (ImGui::BeginChildFrame(ImGui::GetID("Records"), ImVec2(-1.0f, -1.0f),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                const std::vector<IPCDebugger::RequestRecord>& records =
                    ipc_recorder_search_text.empty() ? all_ipc_records
                                                     : ipc_recorder_search_results;
                ImGuiListClipper clipper(records.size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        const IPCDebugger::RequestRecord& record = records[i];

                        std::string service_name;
                        std::string function_name = "Unknown";
                        if (record.client_port.id != -1) {
                            service_name = system.ServiceManager().GetServiceNameByPortId(
                                static_cast<u32>(record.client_port.id));
                        }
                        if (service_name.empty()) {
                            service_name = record.server_session.name;
                            service_name = Common::ReplaceAll(service_name, "_Server", "");
                            service_name = Common::ReplaceAll(service_name, "_Client", "");
                        }
                        const std::string label = fmt::format(
                            "#{} - {} - {} (0x{:08X}) - {} - {}", record.id, service_name,
                            record.function_name.empty() ? "Unknown" : record.function_name,
                            record.untranslated_request_cmdbuf.empty()
                                ? 0xFFFFFFFF
                                : record.untranslated_request_cmdbuf[0],
                            record.is_hle ? "HLE" : "LLE",
                            IPC_Recorder_GetStatusString(record.status));
                        ImGui::Selectable(label.c_str());
                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();

                            ImGui::PushTextWrapPos(width * 0.7f);

                            ImGui::TextUnformatted(
                                fmt::format(
                                    "ID: {}\n"
                                    "Status: {} ({})\n"
                                    "HLE: {}\n"
                                    "Function: {}\n"
                                    "Client Process: {} ({})\n"
                                    "Client Thread: {} ({})\n"
                                    "Client Session: {} ({})\n"
                                    "Client Port: {} ({})\n"
                                    "Server Process: {} ({})\n"
                                    "Server Thread: {} ({})\n"
                                    "Server Session: {} ({})\n"
                                    "Untranslated Request Command Buffer: 0x{:08X}\n"
                                    "Translated Request Command Buffer: 0x{:08X}\n"
                                    "Untranslated Reply Command Buffer: 0x{:08X}\n"
                                    "Translated Reply Command Buffer: 0x{:08X}",
                                    record.id, IPC_Recorder_GetStatusString(record.status),
                                    static_cast<int>(record.status), record.is_hle ? "Yes" : "No",
                                    record.function_name.empty() ? "Unknown" : record.function_name,
                                    record.client_process.name, record.client_process.id,
                                    record.client_thread.name, record.client_thread.id,
                                    record.client_session.name, record.client_session.id,
                                    record.client_port.name, record.client_port.id,
                                    record.server_process.name, record.server_process.id,
                                    record.server_thread.name, record.server_thread.id,
                                    record.server_session.name, record.server_session.id,
                                    fmt::join(record.untranslated_request_cmdbuf, ", 0x"),
                                    fmt::join(record.translated_request_cmdbuf, ", 0x"),
                                    fmt::join(record.untranslated_reply_cmdbuf, ", 0x"),
                                    fmt::join(record.translated_reply_cmdbuf, ", 0x"))
                                    .c_str());

                            ImGui::PopTextWrapPos();

                            ImGui::EndTooltip();
                        }
                    }
                }
            }
            ImGui::EndChildFrame();
        }
        if (!show_ipc_recorder_window) {
            IPCDebugger::Recorder& r = system.Kernel().GetIPCRecorder();

            r.SetEnabled(false);
            r.UnbindCallback(ipc_recorder_callback);

            all_ipc_records.clear();
            ipc_recorder_search_results.clear();
            ipc_recorder_search_text.clear();
            ipc_recorder_callback = nullptr;
        }
        ImGui::End();
    }

    if (show_cheats_window) {
        ImGui::SetNextWindowSize(ImVec2(480.0f, 640.0f), ImGuiCond_Appearing);

        if (ImGui::Begin("Cheats", &show_cheats_window, ImGuiWindowFlags_NoSavedSettings)) {
            if (ImGui::Button("Edit")) {
                std::ostringstream oss;
                system.CheatEngine().SaveCheatsToStream(oss);
                cheats_text_editor_text = oss.str();
                show_cheats_text_editor = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("Reload File")) {
                system.CheatEngine().LoadCheatsFromFile();

                if (show_cheats_text_editor) {
                    std::ostringstream oss;
                    system.CheatEngine().SaveCheatsToStream(oss);
                    cheats_text_editor_text = oss.str();
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Save File")) {
                system.CheatEngine().SaveCheatsToFile();

                if (show_cheats_text_editor) {
                    const std::string filepath = fmt::format(
                        "{}{:016X}.txt", FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir),
                        system.Kernel().GetCurrentProcess()->codeset->program_id);

                    FileUtil::ReadFileToString(true, filepath, cheats_text_editor_text);
                }
            }

            if (ImGui::BeginChildFrame(ImGui::GetID("Cheats"), ImVec2(-1.0f, -1.0f),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                const std::vector<std::shared_ptr<Cheats::CheatBase>>& cheats =
                    system.CheatEngine().GetCheats();

                ImGuiListClipper clipper(cheats.size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        const std::shared_ptr<Cheats::CheatBase>& cheat = cheats[i];
                        bool enabled = cheat->IsEnabled();
                        if (ImGui::Checkbox(cheat->GetName().c_str(), &enabled)) {
                            cheat->SetEnabled(enabled);
                        }
                    }
                }
            }
            ImGui::EndChildFrame();
        }

        if (!show_cheats_window) {
            show_cheats_text_editor = false;
            cheats_text_editor_text.clear();
        }

        ImGui::End();

        if (show_cheats_text_editor) {
            ImGui::SetNextWindowSize(ImVec2(640.0f, 480.0f), ImGuiCond_Appearing);

            if (ImGui::Begin("Cheats Text Editor", &show_cheats_text_editor,
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar)) {
                if (ImGui::BeginMenuBar()) {
                    if (ImGui::BeginMenu("File")) {
                        if (ImGui::MenuItem("Save")) {
                            const std::string filepath = fmt::format(
                                "{}{:016X}.txt",
                                FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir),
                                system.Kernel().GetCurrentProcess()->codeset->program_id);

                            FileUtil::WriteStringToFile(true, filepath, cheats_text_editor_text);

                            system.CheatEngine().LoadCheatsFromFile();
                        }

                        if (ImGui::MenuItem("Load Cheats From Text")) {
                            std::istringstream iss(cheats_text_editor_text);
                            system.CheatEngine().LoadCheatsFromStream(iss);
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::EndMenuBar();
                }

                ImGui::InputTextMultiline("##cheats_text_editor_text", &cheats_text_editor_text,
                                          ImVec2(-1.0f, -1.0f));
            }

            if (!show_cheats_text_editor) {
                cheats_text_editor_text.clear();
            }
        }
    }

    Network::RoomMember& room_member = system.RoomMember();
    if (room_member.GetState() == Network::RoomMember::State::Joined) {
        ImGui::SetNextWindowSize(ImVec2(640.f, 480.0f), ImGuiCond_Appearing);

        bool open = true;
        const Network::RoomInformation& room_information = room_member.GetRoomInformation();
        const Network::RoomMember::MemberList& members = room_member.GetMemberInformation();

        if (ImGui::Begin(fmt::format("{} ({}/{})###Room", room_information.name, members.size(),
                                     room_information.member_slots)
                             .c_str(),
                         &open, ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::PushTextWrapPos();
            ImGui::TextUnformatted(room_information.description.c_str());
            ImGui::PopTextWrapPos();

            ImGui::Columns(2);

            if (ImGui::BeginChildFrame(ImGui::GetID("Members"), ImVec2(-1.0f, -1.0f),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                ImGuiListClipper clipper(members.size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        const Network::RoomMember::MemberInformation& member = members[i];

                        if (member.game_info.name.empty()) {
                            ImGui::TextUnformatted(member.nickname.c_str());
                        } else {
                            ImGui::Text("%s is playing %s", member.nickname.c_str(),
                                        member.game_info.name.c_str());
                        }
                        if (member.nickname != room_member.GetNickname()) {
                            if (ImGui::BeginPopupContextItem(member.nickname.c_str(),
                                                             ImGuiMouseButton_Right)) {
                                if (multiplayer_blocked_nicknames.count(member.nickname)) {
                                    if (ImGui::MenuItem("Unblock")) {
                                        multiplayer_blocked_nicknames.erase(member.nickname);
                                    }
                                } else {
                                    if (ImGui::MenuItem("Block")) {
                                        multiplayer_blocked_nicknames.insert(member.nickname);
                                    }
                                }

                                ImGui::EndPopup();
                            }
                        }
                    }
                }
            }
            ImGui::EndChildFrame();

            ImGui::NextColumn();

            const ImVec2 spacing = style.ItemSpacing;

            if (ImGui::BeginChildFrame(ImGui::GetID("Messages"),
                                       ImVec2(-1.0f, -ImGui::GetFrameHeightWithSpacing()),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

                ImGuiListClipper clipper(multiplayer_messages.size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        ImGui::TextUnformatted(multiplayer_messages[i].c_str());

                        if (ImGui::BeginPopupContextItem("Message Menu")) {
                            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, spacing);

                            if (ImGui::MenuItem("Delete")) {
                                multiplayer_messages.erase(multiplayer_messages.begin() + i);
                                clipper.ItemsCount = 0;
                                ImGui::EndPopup();
                                break;
                            }

                            if (ImGui::MenuItem("Clear")) {
                                multiplayer_messages.clear();
                                clipper.ItemsCount = 0;
                                ImGui::EndPopup();
                                break;
                            }

                            if (ImGui::MenuItem("Copy")) {
                                ImGui::SetClipboardText(multiplayer_messages[i].c_str());
                            }

                            if (ImGui::MenuItem("Copy All")) {
                                std::string all;
                                for (std::size_t j = 0; j < multiplayer_messages.size(); ++j) {
                                    if (j > 0) {
                                        all += '\n';
                                    }
                                    all += multiplayer_messages[j];
                                }
                                ImGui::SetClipboardText(all.c_str());
                            }

                            ImGui::PopStyleVar();

                            ImGui::EndPopup();
                        }
                    }
                }

                ImGui::PopStyleVar();

                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChildFrame();

            ImGui::PushItemWidth(-1.0f);
            if (ImGui::InputTextWithHint("##message", "Send Chat Message", &multiplayer_message,
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                room_member.SendChatMessage(multiplayer_message);
                multiplayer_message.clear();
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::PopItemWidth();

            ImGui::Columns();

            ImGui::EndPopup();
        }
        if (!open) {
            room_member.Leave();
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
            ImGui::InputText("Search", &installed_search_text);

            if (ImGui::BeginChildFrame(ImGui::GetID("Installed"), ImVec2(-1.0f, -1.0f),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                for (const auto& title : installed) {
                    const auto [path, name] = title;

                    if (asl::String(name.c_str())
                            .toLowerCase()
                            .contains(asl::String(installed_search_text.c_str()).toLowerCase()) &&
                        ImGui::Selectable(name.c_str())) {
                        system.SetResetFilePath(path);
                        system.RequestReset();
                        installed.clear();
                        installed_search_text.clear();
                        return;
                    }
                }
            }
            ImGui::EndChildFrame();
            ImGui::EndPopup();
        }
        if (!open) {
            installed.clear();
            installed_search_text.clear();
        }
    }

    if (show_connect_to_citra_room) {
        ImGui::OpenPopup("Connect To Citra Room");

        ImGui::SetNextWindowPos(ImVec2());
        ImGui::SetNextWindowSize(io.DisplaySize);

        if (ImGui::BeginPopupModal("Connect To Citra Room", &show_connect_to_citra_room,
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoResize)) {
            ImGui::InputText("IP", &Settings::values.multiplayer_ip);
            ImGui::InputScalar("Port", ImGuiDataType_U16, &Settings::values.multiplayer_port);
            ImGui::InputText("Nickname", &Settings::values.multiplayer_nickname);
            ImGui::InputText("Password", &Settings::values.multiplayer_password);

            ImGui::NewLine();
            ImGui::TextUnformatted("Public Rooms");

            if (ImGui::Button("Refresh")) {
                public_rooms = GetPublicCitraRooms();
            }
            ImGui::SameLine();
            ImGui::InputText("Search", &public_rooms_search_text);

            if (ImGui::BeginChildFrame(ImGui::GetID("Public Room List"),
                                       ImVec2(-1.0f, ImGui::GetContentRegionAvail().y - 40.0f),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                for (const auto& room : public_rooms) {
                    std::string room_string =
                        fmt::format("{}\n\nHas Password: {}\nMaximum Members: "
                                    "{}\nPreferred Game: {}\nOwner: {}",
                                    room.name, room.has_password ? "Yes" : "No", room.max_players,
                                    room.game, room.owner);
                    if (!room.description.empty()) {
                        room_string += fmt::format("\n\nDescription:\n{}", room.description);
                    }
                    if (!room.members.empty()) {
                        room_string += fmt::format("\n\nMembers ({}):", room.members.size());
                        for (const CitraRoom::Member& member : room.members) {
                            if (member.game.empty()) {
                                room_string += fmt::format("\n\t{}", member.nickname);
                            } else {
                                room_string += fmt::format("\n\t{} is playing {}", member.nickname,
                                                           member.game);
                            }
                        }
                    }

                    if (asl::String(room_string.c_str())
                            .toLowerCase()
                            .contains(
                                asl::String(public_rooms_search_text.c_str()).toLowerCase())) {
                        if (ImGui::Selectable(room_string.c_str())) {
                            Settings::values.multiplayer_ip = room.ip;
                            Settings::values.multiplayer_port = room.port;
                        }
                        ImGui::Separator();
                    }
                }
            }
            ImGui::EndChildFrame();

            ImGui::NewLine();

            if (ImGui::Button("Connect")) {
                ConnectToCitraRoom();
                show_connect_to_citra_room = false;
                public_rooms.clear();
                public_rooms_search_text.clear();
            }

            ImGui::EndPopup();
        }
        if (!show_connect_to_citra_room) {
            public_rooms.clear();
            public_rooms_search_text.clear();
        }
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);

    plugin_manager.AfterSwapWindow();
}

void EmuWindow_SDL2::PollEvents() {
    SDL_Event event;

    // SDL_PollEvent returns 0 when there are no more events in the event queue
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type) {
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_MAXIMIZED:
            case SDL_WINDOWEVENT_RESTORED:
            case SDL_WINDOWEVENT_MINIMIZED:
                OnResize();
                break;
            default:
                break;
            }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureKeyboard) {
                OnKeyEvent(static_cast<int>(event.key.keysym.scancode), event.key.state);
            }

            break;
        case SDL_MOUSEMOTION:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse && event.button.which != SDL_TOUCH_MOUSEID) {
                OnMouseMotion(event.motion.x, event.motion.y);
            }

            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse && event.button.which != SDL_TOUCH_MOUSEID) {
                OnMouseButton(event.button.button, event.button.state, event.button.x,
                              event.button.y);
            }

            break;
        case SDL_FINGERDOWN:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse) {
                OnFingerDown(event.tfinger.x, event.tfinger.y);
            }

            break;
        case SDL_FINGERMOTION:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse) {
                OnFingerMotion(event.tfinger.x, event.tfinger.y);
            }

            break;
        case SDL_FINGERUP:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse) {
                OnFingerUp();
            }

            break;
        case SDL_QUIT:
            if (pfd::message("vvctre", "Would you like to exit now?", pfd::choice::yes_no,
                             pfd::icon::question)
                    .result() == pfd::button::yes) {
                is_open = false;
            }

            break;
        default:
            break;
        }
    }
}

void EmuWindow_SDL2::CopyScreenshot() {
    const auto& layout = GetFramebufferLayout();
    u8* data = new u8[layout.width * layout.height * 4];

    if (VideoCore::RequestScreenshot(
            data,
            [=] {
                std::vector<u8> v(layout.width * layout.height * 4);
                std::memcpy(v.data(), data, v.size());
                delete[] data;

                const auto convert_bgra_to_rgba = [](const std::vector<u8>& input,
                                                     const Layout::FramebufferLayout& layout) {
                    int offset = 0;
                    std::vector<u8> output(input.size());

                    for (u32 y = 0; y < layout.height; ++y) {
                        for (u32 x = 0; x < layout.width; ++x) {
                            output[offset] = input[offset + 2];
                            output[offset + 1] = input[offset + 1];
                            output[offset + 2] = input[offset];
                            output[offset + 3] = input[offset + 3];

                            offset += 4;
                        }
                    }

                    return output;
                };

                v = convert_bgra_to_rgba(v, layout);
                Common::FlipRGBA8Texture(v, static_cast<u64>(layout.width),
                                         static_cast<u64>(layout.height));

                clip::image_spec spec;
                spec.width = layout.width;
                spec.height = layout.height;
                spec.bits_per_pixel = 32;
                spec.bytes_per_row = spec.width * 4;
                spec.red_mask = 0xff;
                spec.green_mask = 0xff00;
                spec.blue_mask = 0xff0000;
                spec.alpha_mask = 0xff000000;
                spec.red_shift = 0;
                spec.green_shift = 8;
                spec.blue_shift = 16;
                spec.alpha_shift = 24;

                clip::set_image(clip::image(v.data(), spec));
            },
            layout)) {
        delete[] data;
    }
}

void EmuWindow_SDL2::ConnectToCitraRoom() {
    if (!Settings::values.multiplayer_ip.empty() && Settings::values.multiplayer_port != 0 &&
        !Settings::values.multiplayer_nickname.empty()) {
        system.RoomMember().Join(
            Settings::values.multiplayer_nickname, Service::CFG::GetConsoleIdHash(system),
            Settings::values.multiplayer_ip.c_str(), Settings::values.multiplayer_port,
            Network::NO_PREFERRED_MAC_ADDRESS, Settings::values.multiplayer_password);
    }
}
