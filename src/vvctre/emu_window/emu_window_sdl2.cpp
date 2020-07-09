// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <string>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <asl/Date.h>
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
#include "common/logging/log.h"
#include "common/string_util.h"
#include "common/texture.h"
#include "core/3ds.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/core.h"
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
#include "network/network.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/texture_filters/texture_filterer.h"
#include "video_core/video_core.h"
#include "vvctre/common.h"
#include "vvctre/emu_window/emu_window_sdl2.h"
#include "vvctre/plugins.h"

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

void EmuWindow_SDL2::ToggleFullscreen() {
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
        SDL_SetWindowFullscreen(window, 0);
    } else {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
}

EmuWindow_SDL2::EmuWindow_SDL2(Core::System& system, PluginManager& plugin_manager,
                               SDL_Window* window)
    : window(window), system(system), plugin_manager(plugin_manager) {
    Network::Init();

    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        multiplayer_on_error =
            room_member->BindOnError([&](const Network::RoomMember::Error& error) {
                pfd::message("vvctre", Network::GetErrorStr(error), pfd::choice::ok,
                             pfd::icon::error);

                multiplayer_message.clear();
                multiplayer_messages.clear();
            });

        multiplayer_on_chat_message =
            room_member->BindOnChatMessageReceived([&](const Network::ChatEntry& entry) {
                if (multiplayer_blocked_nicknames.count(entry.nickname)) {
                    return;
                }

                if (multiplayer_messages.size() == 100) {
                    multiplayer_messages.pop_front();
                }

                asl::Date date = asl::Date::now();
                multiplayer_messages.push_back(fmt::format("[{}:{}] <{}> {}", date.hours(),
                                                           date.minutes(), entry.nickname,
                                                           entry.message));
            });

        multiplayer_on_status_message =
            room_member->BindOnStatusMessageReceived([&](const Network::StatusMessageEntry& entry) {
                if (multiplayer_messages.size() == 100) {
                    multiplayer_messages.pop_front();
                }

                switch (entry.type) {
                case Network::StatusMessageTypes::IdMemberJoin:
                    multiplayer_messages.push_back(fmt::format("{} joined", entry.nickname));
                    break;
                case Network::StatusMessageTypes::IdMemberLeave:
                    multiplayer_messages.push_back(fmt::format("{} left", entry.nickname));
                    break;
                case Network::StatusMessageTypes::IdMemberKicked:
                    multiplayer_messages.push_back(fmt::format("{} was kicked", entry.nickname));
                    break;
                case Network::StatusMessageTypes::IdMemberBanned:
                    multiplayer_messages.push_back(fmt::format("{} was banned", entry.nickname));
                    break;
                case Network::StatusMessageTypes::IdAddressUnbanned:
                    multiplayer_messages.push_back("Someone was unbanned");
                    break;
                }
            });

        ConnectToCitraRoom();
    }

    SDL_SetWindowTitle(window, fmt::format("vvctre {}.{}.{}", vvctre_version_major,
                                           vvctre_version_minor, vvctre_version_patch)
                                   .c_str());

    if (Settings::values.start_in_fullscreen_mode) {
        ToggleFullscreen();
    } else {
        SDL_SetWindowMinimumSize(window, Core::kScreenTopWidth,
                                 Core::kScreenTopHeight + Core::kScreenBottomHeight);
        SDL_RestoreWindow(window);
        SDL_SetWindowSize(window, Core::kScreenTopWidth,
                          Core::kScreenTopHeight + Core::kScreenBottomHeight);
    }

    SDL_GL_SetSwapInterval(Settings::values.enable_vsync ? 1 : 0);

    OnResize();
    SDL_PumpEvents();
    LOG_INFO(Frontend, "Version: {}.{}.{}", vvctre_version_major, vvctre_version_minor,
             vvctre_version_patch);
    LOG_INFO(Frontend, "Movie version: {}", Core::MovieVersion);
}

EmuWindow_SDL2::~EmuWindow_SDL2() {
    SDL_Quit();
    Network::Shutdown();
}

void EmuWindow_SDL2::SwapBuffers() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO();

    plugin_manager.BeforeDrawingFPS();

    if (ImGui::Begin("FPS and Menu", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::SetWindowPos(ImVec2(), ImGuiCond_Once);
        ImGui::TextColored(fps_color, "%d FPS", static_cast<int>(io.Framerate));
        if (ImGui::BeginPopupContextItem("##menu", ImGuiMouseButton_Right)) {
            if (ImGui::IsWindowAppearing() && !ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT)) {
                paused = true;
            }

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

                    if (!files.empty()) {
                        auto am = Service::AM::GetModule(system);

                        for (const auto& file : files) {
                            const Service::AM::InstallStatus status = Service::AM::InstallCIA(
                                file, [&](std::size_t current, std::size_t total) {
                                    // Poll events
                                    SDL_Event event;
                                    while (SDL_PollEvent(&event)) {
                                        ImGui_ImplSDL2_ProcessEvent(&event);

                                        if (event.type == SDL_QUIT ||
                                            (event.type == SDL_WINDOWEVENT &&
                                             event.window.event == SDL_WINDOWEVENT_CLOSE)) {
                                            std::exit(1);
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

                        return;
                    }
                }

                if (ImGui::BeginMenu("Amiibo")) {
                    if (ImGui::MenuItem("Load")) {
                        const std::vector<std::string> result =
                            pfd::open_file("Load Amiibo", *asl::Process::myDir(),
                                           {"Amiibo Files", "*.bin *.BIN", "Anything", "*"})
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

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Audio")) {
                    ImGui::TextUnformatted("Volume:");
                    ImGui::SameLine();
                    ImGui::SliderFloat("##volume", &Settings::values.audio_volume, 0.0f, 1.0f);

                    ImGui::TextUnformatted("Sink:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##sink", Settings::values.audio_sink_id.c_str())) {
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

                    ImGui::TextUnformatted("Device:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##device", Settings::values.audio_device_id.c_str())) {
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
                        ImGui::TextUnformatted("Microphone Device:");
                        ImGui::SameLine();

                        if (ImGui::BeginCombo("##microphonedevice",
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

                    ImGui::TextUnformatted("Resolution:");
                    ImGui::SameLine();
                    const u16 min = 0;
                    const u16 max = 10;
                    ImGui::SliderScalar("##resolution", ImGuiDataType_U16,
                                        &Settings::values.resolution, &min, &max,
                                        Settings::values.resolution == 0 ? "Window Size" : "%d");

                    ImGui::TextUnformatted("Background Color:");
                    ImGui::SameLine();
                    if (ImGui::ColorEdit3("##backgroundcolor",
                                          &Settings::values.background_color_red,
                                          ImGuiColorEditFlags_NoInputs)) {
                        VideoCore::g_renderer_background_color_update_requested = true;
                    }

                    ImGui::TextUnformatted("Post Processing Shader:");
                    ImGui::SameLine();
                    ImGui::InputText("##postprocessingshader",
                                     &Settings::values.post_processing_shader);
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        Settings::Apply();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted("File name without extension and folder");
                        ImGui::EndTooltip();
                    }

                    ImGui::TextUnformatted("Texture Filter:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##texturefilter",
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

                    ImGui::Checkbox("Dump Textures", &Settings::values.dump_textures);
                    ImGui::Checkbox("Use Custom Textures", &Settings::values.custom_textures);
                    ImGui::Checkbox("Preload Custom Textures", &Settings::values.preload_textures);

                    ImGui::TextUnformatted("3D:");
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

                    ImGui::SameLine();

                    u8 factor_3d = Settings::values.factor_3d;
                    const u8 factor_3d_min = 0;
                    const u8 factor_3d_max = 100;
                    ImGui::PushItemWidth(135);
                    if (ImGui::SliderScalar("##factor_3d", ImGuiDataType_U8, &factor_3d,
                                            &factor_3d_min, &factor_3d_max, "%d%%")) {
                        Settings::values.factor_3d = factor_3d;
                    }
                    ImGui::PopItemWidth();

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Camera")) {
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
                        ImGui::TextUnformatted("Inner Camera Parameter:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(316);
                        if (ImGui::InputText(
                                "##innercameraparameter",
                                &Settings::values.camera_parameter[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)])) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        ImGui::PopItemWidth();
                        if (GUI_CameraAddBrowse(
                                "Browse...##innercamera",
                                static_cast<std::size_t>(Service::CAM::CameraIndex::InnerCamera))) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
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
                        ImGui::TextUnformatted("Outer Left Camera Parameter:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(316);
                        if (ImGui::InputText(
                                "##outerleftcameraparameter",
                                &Settings::values.camera_parameter[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)])) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        ImGui::PopItemWidth();
                        if (GUI_CameraAddBrowse("Browse...##outerleftcamera",
                                                static_cast<std::size_t>(
                                                    Service::CAM::CameraIndex::OuterLeftCamera))) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                    }

                    ImGui::TextUnformatted("Outer Right Camera Engine:");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("##outerrightengine",
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
                        ImGui::TextUnformatted("Outer Right Camera Parameter:");
                        ImGui::SameLine();
                        ImGui::PushItemWidth(316);
                        if (ImGui::InputText(
                                "##outerrightcameraparameter",
                                &Settings::values.camera_parameter[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)])) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        ImGui::PopItemWidth();
                        if (GUI_CameraAddBrowse("Browse...##outerrightcamera",
                                                static_cast<std::size_t>(
                                                    Service::CAM::CameraIndex::OuterRightCamera))) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("System")) {
                    auto cfg = Service::CFG::GetModule(system);

                    if (cfg != nullptr) {
                        ImGui::TextUnformatted("Will Restart");
                        ImGui::Indent();

                        ImGui::TextUnformatted("Username:");
                        ImGui::SameLine();

                        std::string username = Common::UTF16ToUTF8(cfg->GetUsername());
                        if (ImGui::InputText("##username", &username)) {
                            cfg->SetUsername(Common::UTF8ToUTF16(username));
                            cfg->UpdateConfigNANDSavegame();
                            system.RequestReset();
                        }

                        ImGui::TextUnformatted("Birthday:");
                        ImGui::SameLine();

                        auto [month, day] = cfg->GetBirthday();

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
                                cfg->SetBirthday(1, day);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("February")) {
                                cfg->SetBirthday(2, day);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("March")) {
                                cfg->SetBirthday(3, day);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("April")) {
                                cfg->SetBirthday(4, day);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("May")) {
                                cfg->SetBirthday(5, day);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("June")) {
                                cfg->SetBirthday(6, day);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("July")) {
                                cfg->SetBirthday(7, day);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("August")) {
                                cfg->SetBirthday(8, day);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("September")) {
                                cfg->SetBirthday(9, day);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("October")) {
                                cfg->SetBirthday(10, day);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("November")) {
                                cfg->SetBirthday(11, day);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("December")) {
                                cfg->SetBirthday(12, day);
                                update_config_savegame = true;
                            }

                            ImGui::EndCombo();
                        }

                        ImGui::SameLine();

                        if (ImGui::InputScalar("##birthday_day", ImGuiDataType_U8, &day)) {
                            cfg->SetBirthday(month, day);
                            cfg->UpdateConfigNANDSavegame();
                            system.RequestReset();
                        }

                        ImGui::TextUnformatted("Language:");
                        ImGui::SameLine();

                        if (ImGui::BeginCombo("##language", [&] {
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
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_JP);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("English")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_EN);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("French")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_FR);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("German")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_DE);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("Italian")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_IT);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("Spanish")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ES);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("Simplified Chinese")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ZH);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("Korean")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_KO);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("Dutch")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_NL);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("Portugese")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_PT);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("Russian")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_RU);
                                update_config_savegame = true;
                            }

                            if (ImGui::Selectable("Traditional Chinese")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_TW);
                                update_config_savegame = true;
                            }

                            ImGui::EndCombo();
                        }

                        ImGui::TextUnformatted("Sound output mode:");
                        ImGui::SameLine();
                        if (ImGui::BeginCombo("##soundoutputmode", [&] {
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
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Stereo")) {
                                cfg->SetSoundOutputMode(
                                    Service::CFG::SoundOutputMode::SOUND_STEREO);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Surround")) {
                                cfg->SetSoundOutputMode(
                                    Service::CFG::SoundOutputMode::SOUND_SURROUND);
                                update_config_savegame = true;
                            }
                            ImGui::EndCombo();
                        }

                        ImGui::TextUnformatted("Country:");
                        ImGui::SameLine();
                        if (ImGui::BeginCombo("##country", [&] {
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
                                cfg->SetCountryCode(1);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Anguilla")) {
                                cfg->SetCountryCode(8);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Antigua and Barbuda")) {
                                cfg->SetCountryCode(9);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Argentina")) {
                                cfg->SetCountryCode(10);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Aruba")) {
                                cfg->SetCountryCode(11);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Bahamas")) {
                                cfg->SetCountryCode(12);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Barbados")) {
                                cfg->SetCountryCode(13);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Belize")) {
                                cfg->SetCountryCode(14);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Bolivia")) {
                                cfg->SetCountryCode(15);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Brazil")) {
                                cfg->SetCountryCode(16);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("British Virgin Islands")) {
                                cfg->SetCountryCode(17);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Canada")) {
                                cfg->SetCountryCode(18);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Cayman Islands")) {
                                cfg->SetCountryCode(19);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Chile")) {
                                cfg->SetCountryCode(20);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Colombia")) {
                                cfg->SetCountryCode(21);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Costa Rica")) {
                                cfg->SetCountryCode(22);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Dominica")) {
                                cfg->SetCountryCode(23);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Dominican Republic")) {
                                cfg->SetCountryCode(24);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Ecuador")) {
                                cfg->SetCountryCode(25);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("El Salvador")) {
                                cfg->SetCountryCode(26);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("French Guiana")) {
                                cfg->SetCountryCode(27);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Grenada")) {
                                cfg->SetCountryCode(28);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Guadeloupe")) {
                                cfg->SetCountryCode(29);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Guatemala")) {
                                cfg->SetCountryCode(30);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Guyana")) {
                                cfg->SetCountryCode(31);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Haiti")) {
                                cfg->SetCountryCode(32);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Honduras")) {
                                cfg->SetCountryCode(33);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Jamaica")) {
                                cfg->SetCountryCode(34);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Martinique")) {
                                cfg->SetCountryCode(35);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Mexico")) {
                                cfg->SetCountryCode(36);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Montserrat")) {
                                cfg->SetCountryCode(37);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Netherlands Antilles")) {
                                cfg->SetCountryCode(38);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Nicaragua")) {
                                cfg->SetCountryCode(39);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Panama")) {
                                cfg->SetCountryCode(40);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Paraguay")) {
                                cfg->SetCountryCode(41);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Peru")) {
                                cfg->SetCountryCode(42);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Saint Kitts and Nevis")) {
                                cfg->SetCountryCode(43);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Saint Lucia")) {
                                cfg->SetCountryCode(44);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Saint Vincent and the Grenadines")) {
                                cfg->SetCountryCode(45);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Suriname")) {
                                cfg->SetCountryCode(46);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Trinidad and Tobago")) {
                                cfg->SetCountryCode(47);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Turks and Caicos Islands")) {
                                cfg->SetCountryCode(48);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("United States")) {
                                cfg->SetCountryCode(49);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Uruguay")) {
                                cfg->SetCountryCode(50);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("US Virgin Islands")) {
                                cfg->SetCountryCode(51);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Venezuela")) {
                                cfg->SetCountryCode(52);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Albania")) {
                                cfg->SetCountryCode(64);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Australia")) {
                                cfg->SetCountryCode(65);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Austria")) {
                                cfg->SetCountryCode(66);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Belgium")) {
                                cfg->SetCountryCode(67);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Bosnia and Herzegovina")) {
                                cfg->SetCountryCode(68);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Botswana")) {
                                cfg->SetCountryCode(69);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Bulgaria")) {
                                cfg->SetCountryCode(70);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Croatia")) {
                                cfg->SetCountryCode(71);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Cyprus")) {
                                cfg->SetCountryCode(72);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Czech Republic")) {
                                cfg->SetCountryCode(73);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Denmark")) {
                                cfg->SetCountryCode(74);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Estonia")) {
                                cfg->SetCountryCode(75);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Finland")) {
                                cfg->SetCountryCode(76);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("France")) {
                                cfg->SetCountryCode(77);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Germany")) {
                                cfg->SetCountryCode(78);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Greece")) {
                                cfg->SetCountryCode(79);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Hungary")) {
                                cfg->SetCountryCode(80);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Iceland")) {
                                cfg->SetCountryCode(81);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Ireland")) {
                                cfg->SetCountryCode(82);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Italy")) {
                                cfg->SetCountryCode(83);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Latvia")) {
                                cfg->SetCountryCode(84);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Lesotho")) {
                                cfg->SetCountryCode(85);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Liechtenstein")) {
                                cfg->SetCountryCode(86);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Lithuania")) {
                                cfg->SetCountryCode(87);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Luxembourg")) {
                                cfg->SetCountryCode(88);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Macedonia")) {
                                cfg->SetCountryCode(89);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Malta")) {
                                cfg->SetCountryCode(90);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Montenegro")) {
                                cfg->SetCountryCode(91);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Mozambique")) {
                                cfg->SetCountryCode(92);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Namibia")) {
                                cfg->SetCountryCode(93);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Netherlands")) {
                                cfg->SetCountryCode(94);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("New Zealand")) {
                                cfg->SetCountryCode(95);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Norway")) {
                                cfg->SetCountryCode(96);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Poland")) {
                                cfg->SetCountryCode(97);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Portugal")) {
                                cfg->SetCountryCode(98);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Romania")) {
                                cfg->SetCountryCode(99);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Russia")) {
                                cfg->SetCountryCode(100);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Serbia")) {
                                cfg->SetCountryCode(101);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Slovakia")) {
                                cfg->SetCountryCode(102);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Slovenia")) {
                                cfg->SetCountryCode(103);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("South Africa")) {
                                cfg->SetCountryCode(104);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Spain")) {
                                cfg->SetCountryCode(105);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Swaziland")) {
                                cfg->SetCountryCode(106);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Sweden")) {
                                cfg->SetCountryCode(107);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Switzerland")) {
                                cfg->SetCountryCode(108);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Turkey")) {
                                cfg->SetCountryCode(109);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("United Kingdom")) {
                                cfg->SetCountryCode(110);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Zambia")) {
                                cfg->SetCountryCode(111);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Zimbabwe")) {
                                cfg->SetCountryCode(112);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Azerbaijan")) {
                                cfg->SetCountryCode(113);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Mauritania")) {
                                cfg->SetCountryCode(114);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Mali")) {
                                cfg->SetCountryCode(115);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Niger")) {
                                cfg->SetCountryCode(116);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Chad")) {
                                cfg->SetCountryCode(117);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Sudan")) {
                                cfg->SetCountryCode(118);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Eritrea")) {
                                cfg->SetCountryCode(119);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Djibouti")) {
                                cfg->SetCountryCode(120);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Somalia")) {
                                cfg->SetCountryCode(121);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Andorra")) {
                                cfg->SetCountryCode(122);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Gibraltar")) {
                                cfg->SetCountryCode(123);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Guernsey")) {
                                cfg->SetCountryCode(124);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Isle of Man")) {
                                cfg->SetCountryCode(125);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Jersey")) {
                                cfg->SetCountryCode(126);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Monaco")) {
                                cfg->SetCountryCode(127);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Taiwan")) {
                                cfg->SetCountryCode(128);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("South Korea")) {
                                cfg->SetCountryCode(136);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Hong Kong")) {
                                cfg->SetCountryCode(144);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Macau")) {
                                cfg->SetCountryCode(145);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Indonesia")) {
                                cfg->SetCountryCode(152);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Singapore")) {
                                cfg->SetCountryCode(153);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Thailand")) {
                                cfg->SetCountryCode(154);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Philippines")) {
                                cfg->SetCountryCode(155);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Malaysia")) {
                                cfg->SetCountryCode(156);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("China")) {
                                cfg->SetCountryCode(160);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("United Arab Emirates")) {
                                cfg->SetCountryCode(168);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("India")) {
                                cfg->SetCountryCode(169);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Egypt")) {
                                cfg->SetCountryCode(170);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Oman")) {
                                cfg->SetCountryCode(171);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Qatar")) {
                                cfg->SetCountryCode(172);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Kuwait")) {
                                cfg->SetCountryCode(173);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Saudi Arabia")) {
                                cfg->SetCountryCode(174);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Syria")) {
                                cfg->SetCountryCode(175);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Bahrain")) {
                                cfg->SetCountryCode(176);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Jordan")) {
                                cfg->SetCountryCode(177);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("San Marino")) {
                                cfg->SetCountryCode(184);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Vatican City")) {
                                cfg->SetCountryCode(185);
                                update_config_savegame = true;
                            }
                            if (ImGui::Selectable("Bermuda")) {
                                cfg->SetCountryCode(186);
                                update_config_savegame = true;
                            }
                            ImGui::EndCombo();
                        }

                        ImGui::Unindent();
                        ImGui::NewLine();
                    }

                    ImGui::TextUnformatted("Restart Recommended");
                    ImGui::Indent();
                    ImGui::TextUnformatted("Play Coins:");
                    ImGui::SameLine();
                    const u16 min = 0;
                    const u16 max = 300;
                    if (ImGui::IsWindowAppearing()) {
                        play_coins = Service::PTM::Module::GetPlayCoins();
                    }
                    if (ImGui::SliderScalar("##playcoins", ImGuiDataType_U16, &play_coins, &min,
                                            &max)) {
                        play_coins_changed = true;
                    }
                    ImGui::Unindent();

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("GUI")) {
                    ImGui::TextUnformatted("FPS Color:");
                    ImGui::SameLine();
                    ImGui::ColorPicker4("##fps_color", (float*)&fps_color);

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Hacks")) {
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

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                if (ImGui::BeginMenu("Layout")) {
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
                                Settings::Apply();
                            }
                            if (ImGui::Selectable("Single Screen")) {
                                Settings::values.layout = Settings::Layout::SingleScreen;
                                Settings::Apply();
                            }
                            if (ImGui::Selectable("Large Screen")) {
                                Settings::values.layout = Settings::Layout::LargeScreen;
                                Settings::Apply();
                            }
                            if (ImGui::Selectable("Side by Side")) {
                                Settings::values.layout = Settings::Layout::SideScreen;
                                Settings::Apply();
                            }
                            if (ImGui::Selectable("Medium Screen")) {
                                Settings::values.layout = Settings::Layout::MediumScreen;
                                Settings::Apply();
                            }
                            ImGui::EndCombo();
                        }
                    } else {
                        ImGui::TextUnformatted("Top Left");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##topleft", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_top_left)) {
                            Settings::Apply();
                        }
                        ImGui::TextUnformatted("Top Top");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##toptop", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_top_top)) {
                            Settings::Apply();
                        }
                        ImGui::TextUnformatted("Top Right");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##topright", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_top_right)) {
                            Settings::Apply();
                        }
                        ImGui::TextUnformatted("Top Bottom");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##topbottom", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_top_bottom)) {
                            Settings::Apply();
                        }
                        ImGui::TextUnformatted("Bottom Left");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##bottomleft", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_bottom_left)) {
                            Settings::Apply();
                        }
                        ImGui::TextUnformatted("Bottom Top");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##bottomtop", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_bottom_top)) {
                            Settings::Apply();
                        }
                        ImGui::TextUnformatted("Bottom Right");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##bottomright", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_bottom_right)) {
                            Settings::Apply();
                        }
                        ImGui::TextUnformatted("Bottom Bottom");
                        ImGui::SameLine();
                        if (ImGui::InputScalar("##bottombottom", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_bottom_bottom)) {
                            Settings::Apply();
                        }
                    }

                    ImGui::Separator();

                    if (ImGui::Checkbox("Use Custom Layout", &Settings::values.use_custom_layout)) {
                        Settings::Apply();
                    }

                    if (ImGui::Checkbox("Swap Screens", &Settings::values.swap_screens)) {
                        Settings::Apply();
                    }

                    if (ImGui::Checkbox("Upright Screens", &Settings::values.upright_screens)) {
                        Settings::Apply();
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Debugging")) {
                    if (ImGui::Checkbox("IPC Recorder", &show_ipc_recorder_window)) {
                        ipc_records.clear();

                        if (!show_ipc_recorder_window) {
                            IPCDebugger::Recorder& r = system.Kernel().GetIPCRecorder();

                            r.SetEnabled(false);
                            r.UnbindCallback(ipc_recorder_callback);

                            ipc_recorder_callback = nullptr;
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::Checkbox("Cheats", &show_cheats_window);

                bool fullscreen = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                if (ImGui::Checkbox("Fullscreen", &fullscreen)) {
                    ToggleFullscreen();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Emulation")) {
                if (ImGui::MenuItem("Restart")) {
                    system.RequestReset();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tools")) {
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

                                    stbi_write_png(filename.c_str(), layout.width, layout.height, 4,
                                                   v.data(), layout.width * 4);
                                }
                            },
                            layout)) {
                        delete[] data;
                    }
                }

                if (ImGui::MenuItem("Copy Screenshot")) {
                    CopyScreenshot();
                }

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

                if (ImGui::BeginMenu("Movie")) {
                    auto& movie = Core::Movie::GetInstance();

                    if (ImGui::MenuItem("Play", nullptr, nullptr,
                                        !movie.IsPlayingInput() && !movie.IsRecordingInput())) {
                        const auto filename = pfd::open_file("Play Movie", *asl::Process::myDir(),
                                                             {"VvCtre Movie", "*.vcm"})
                                                  .result();
                        if (!filename.empty()) {
                            const auto movie_result = movie.ValidateMovie(filename[0]);
                            switch (movie_result) {
                            case Core::Movie::ValidationResult::OK:
                                movie.StartPlayback(filename[0], [&] {
                                    pfd::message("vvctre", "Playback finished", pfd::choice::ok);
                                });
                                break;
                            case Core::Movie::ValidationResult::GameDismatch:
                                pfd::message(
                                    "vvctre",
                                    "Movie was recorded using a ROM with a different program ID",
                                    pfd::choice::ok, pfd::icon::warning);
                                movie.StartPlayback(filename[0], [&] {
                                    pfd::message("vvctre", "Playback finished", pfd::choice::ok,
                                                 pfd::icon::info);
                                });
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
                            pfd::save_file("Play Movie", "movie.vcm", {"VvCtre Movie", "*.vcm"})
                                .result();
                        if (!filename.empty()) {
                            movie.StartRecording(filename);
                        }
                    }

                    if (ImGui::MenuItem("Stop Playback/Recording", nullptr, nullptr,
                                        movie.IsPlayingInput() || movie.IsRecordingInput())) {
                        movie.Shutdown();
                        pfd::message("vvctre", "Movie saved", pfd::choice::ok);
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (std::shared_ptr<Network::RoomMember> room_member =
                    Network::GetRoomMember().lock()) {
                if (room_member->GetState() == Network::RoomMember::State::Idle) {
                    if (ImGui::BeginMenu("Multiplayer")) {
                        if (ImGui::MenuItem("Connect To Citra Room")) {
                            if (!ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT)) {
                                public_rooms = GetPublicCitraRooms();
                            }
                            show_connect_to_citra_room = true;
                        }
                        ImGui::EndMenu();
                    }
                }
            }

            plugin_manager.AddMenus();

            ImGui::Separator();
            ImGui::MenuItem("Close Menu");

            ImGui::EndPopup();
        } else {
            if (play_coins_changed) {
                Service::PTM::Module::SetPlayCoins(play_coins);
                play_coins_changed = false;
            }
            if (update_config_savegame) {
                Service::CFG::GetModule(system)->UpdateConfigNANDSavegame();
                system.RequestReset();
                update_config_savegame = false;
            }
            paused = false;
        }
    }
    ImGui::End();

    if (swkbd_config != nullptr && swkbd_code != nullptr && swkbd_text != nullptr) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Keyboard", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputTextWithHint("", swkbd_config->hint_text.c_str(), swkbd_text,
                                     swkbd_config->multiline_mode ? ImGuiInputTextFlags_Multiline
                                                                  : 0);

            switch (swkbd_config->button_config) {
            case Frontend::ButtonConfig::None:
            case Frontend::ButtonConfig::Single: {
                if (ImGui::Button((swkbd_config->button_text[2].empty()
                                       ? Frontend::SWKBD_BUTTON_OKAY
                                       : swkbd_config->button_text[2])
                                      .c_str())) {
                    swkbd_config = nullptr;
                    swkbd_code = nullptr;
                    swkbd_text = nullptr;
                }
                break;
            }

            case Frontend::ButtonConfig::Dual: {
                const std::string cancel = swkbd_config->button_text[0].empty()
                                               ? Frontend::SWKBD_BUTTON_CANCEL
                                               : swkbd_config->button_text[0];
                const std::string ok = swkbd_config->button_text[2].empty()
                                           ? Frontend::SWKBD_BUTTON_OKAY
                                           : swkbd_config->button_text[2];
                if (ImGui::Button(cancel.c_str())) {
                    swkbd_config = nullptr;
                    swkbd_code = nullptr;
                    swkbd_text = nullptr;
                    break;
                }
                if (Frontend::SoftwareKeyboard::ValidateInput(*swkbd_text, *swkbd_config) ==
                    Frontend::ValidationError::None) {
                    ImGui::SameLine();
                    if (ImGui::Button(ok.c_str())) {
                        *swkbd_code = 1;
                        swkbd_config = nullptr;
                        swkbd_code = nullptr;
                        swkbd_text = nullptr;
                    }
                }
                break;
            }

            case Frontend::ButtonConfig::Triple: {
                const std::string cancel = swkbd_config->button_text[0].empty()
                                               ? Frontend::SWKBD_BUTTON_CANCEL
                                               : swkbd_config->button_text[0];
                const std::string forgot = swkbd_config->button_text[1].empty()
                                               ? Frontend::SWKBD_BUTTON_FORGOT
                                               : swkbd_config->button_text[1];
                const std::string ok = swkbd_config->button_text[2].empty()
                                           ? Frontend::SWKBD_BUTTON_OKAY
                                           : swkbd_config->button_text[2];
                if (ImGui::Button(cancel.c_str())) {
                    swkbd_config = nullptr;
                    swkbd_code = nullptr;
                    swkbd_text = nullptr;
                    break;
                }
                ImGui::SameLine();
                if (ImGui::Button(forgot.c_str())) {
                    *swkbd_code = 1;
                    swkbd_config = nullptr;
                    swkbd_code = nullptr;
                    swkbd_text = nullptr;
                    break;
                }
                if (Frontend::SoftwareKeyboard::ValidateInput(*swkbd_text, *swkbd_config) ==
                    Frontend::ValidationError::None) {
                    ImGui::SameLine();
                    if (ImGui::Button(ok.c_str())) {
                        *swkbd_code = 2;
                        swkbd_config = nullptr;
                        swkbd_code = nullptr;
                        swkbd_text = nullptr;
                    }
                }
                break;
            }
            }
        }
        ImGui::End();
    }

    if (mii_selector_config != nullptr && mii_selector_miis != nullptr &&
        mii_selector_code != nullptr && mii_selector_selected_mii != nullptr) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin(
                (mii_selector_config->title.empty() ? "Mii Selector" : mii_selector_config->title)
                    .c_str(),
                nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::ListBoxHeader("##miis")) {
                for (std::size_t index = 0; index < mii_selector_miis->size(); ++index) {
                    if (ImGui::Selectable(
                            Common::UTF16BufferToUTF8(mii_selector_miis->at(index).mii_name)
                                .c_str())) {
                        *mii_selector_code = 0;
                        *mii_selector_selected_mii = mii_selector_miis->at(index);
                        mii_selector_config = nullptr;
                        mii_selector_miis = nullptr;
                        mii_selector_code = nullptr;
                        mii_selector_selected_mii = nullptr;
                        break;
                    }
                }
                ImGui::ListBoxFooter();
            }
            if (mii_selector_config && mii_selector_config->enable_cancel_button &&
                ImGui::Button("Cancel")) {
                mii_selector_config = nullptr;
                mii_selector_miis = nullptr;
                mii_selector_code = nullptr;
                mii_selector_selected_mii = nullptr;
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
                            ipc_records[record.id] = record;
                        });
                } else {
                    r.UnbindCallback(ipc_recorder_callback);
                    ipc_recorder_callback = nullptr;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                ipc_records.clear();
            }
            ImGui::SameLine();
            static std::string filter;
            ImGui::InputTextWithHint("##filter", "Filter", &filter);
            if (ImGui::ListBoxHeader("##records", ImVec2(-1.0f, -1.0f))) {
                for (const auto& record : ipc_records) {
                    std::string service_name;
                    std::string function_name = "Unknown";
                    if (record.second.client_port.id != -1) {
                        service_name = system.ServiceManager().GetServiceNameByPortId(
                            static_cast<u32>(record.second.client_port.id));
                    }
                    if (service_name.empty()) {
                        service_name = record.second.server_session.name;
                        service_name = Common::ReplaceAll(service_name, "_Server", "");
                        service_name = Common::ReplaceAll(service_name, "_Client", "");
                    }
                    const std::string label = fmt::format(
                        "#{} - {} - {} (0x{:08X}) - {} - {}", record.first, service_name,
                        record.second.function_name.empty() ? "Unknown"
                                                            : record.second.function_name,
                        record.second.untranslated_request_cmdbuf.empty()
                            ? 0xFFFFFFFF
                            : record.second.untranslated_request_cmdbuf[0],
                        record.second.is_hle ? "HLE" : "LLE",
                        IPC_Recorder_GetStatusString(record.second.status));
                    if (label.find(filter) != std::string::npos) {
                        ImGui::Selectable(label.c_str());
                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted(
                                fmt::format(
                                    "id: {}\n"
                                    "status: {}\n"
                                    "client_process.type: {}\n"
                                    "client_process.name: {}\n"
                                    "client_process.id: {}\n"
                                    "client_thread.type: {}\n"
                                    "client_thread.name: {}\n"
                                    "client_thread.id: {}\n"
                                    "client_session.type: {}\n"
                                    "client_session.name: {}\n"
                                    "client_session.id: {}\n"
                                    "client_port.type: {}\n"
                                    "client_port.name: {}\n"
                                    "client_port.id: {}\n"
                                    "server_process.type: {}\n"
                                    "server_process.name: {}\n"
                                    "server_process.id: {}\n"
                                    "server_thread.type: {}\n"
                                    "server_thread.name: {}\n"
                                    "server_thread.id: {}\n"
                                    "server_session.type: {}\n"
                                    "server_session.name: {}\n"
                                    "server_session.id: {}\n"
                                    "function_name: {}\n"
                                    "is_hle: {}\n"
                                    "untranslated_request_cmdbuf: 0x{:08X}\n"
                                    "translated_request_cmdbuf: 0x{:08X}\n"
                                    "untranslated_reply_cmdbuf: 0x{:08X}\n"
                                    "translated_reply_cmdbuf: 0x{:08X}",
                                    record.first, static_cast<int>(record.second.status),
                                    record.second.client_process.type,
                                    record.second.client_process.name,
                                    record.second.client_process.id,
                                    record.second.client_thread.type,
                                    record.second.client_thread.name,
                                    record.second.client_thread.id,
                                    record.second.client_session.type,
                                    record.second.client_session.name,
                                    record.second.client_session.id, record.second.client_port.type,
                                    record.second.client_port.name, record.second.client_port.id,
                                    record.second.server_process.type,
                                    record.second.server_process.name,
                                    record.second.server_process.id,
                                    record.second.server_thread.type,
                                    record.second.server_thread.name,
                                    record.second.server_thread.id,
                                    record.second.server_session.type,
                                    record.second.server_session.name,
                                    record.second.server_session.id, record.second.function_name,
                                    record.second.is_hle,
                                    fmt::join(record.second.untranslated_request_cmdbuf, ", 0x"),
                                    fmt::join(record.second.translated_request_cmdbuf, ", 0x"),
                                    fmt::join(record.second.untranslated_reply_cmdbuf, ", 0x"),
                                    fmt::join(record.second.translated_reply_cmdbuf, ", 0x"))
                                    .c_str());
                            ImGui::EndTooltip();
                        }
                    }
                }
                ImGui::ListBoxFooter();
            }
        }
        if (!show_ipc_recorder_window) {
            ipc_records.clear();

            IPCDebugger::Recorder& r = system.Kernel().GetIPCRecorder();

            r.SetEnabled(false);
            r.UnbindCallback(ipc_recorder_callback);

            ipc_recorder_callback = nullptr;
        }
        ImGui::End();
    }

    if (show_cheats_window) {
        ImGui::SetNextWindowSize(ImVec2(480, 640), ImGuiCond_Appearing);
        if (ImGui::Begin("Cheats", &show_cheats_window, ImGuiWindowFlags_NoSavedSettings)) {
            if (ImGui::Button("Edit File")) {
                const std::string filepath = fmt::format(
                    "{}{:016X}.txt", FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir),
                    system.Kernel().GetCurrentProcess()->codeset->program_id);

                FileUtil::CreateFullPath(filepath);

                if (!FileUtil::Exists(filepath)) {
                    FileUtil::CreateEmptyFile(filepath);
                }

#ifdef _WIN32
                const int code = std::system(fmt::format("start {}", filepath).c_str());
#else
                const int code = std::system(fmt::format("xdg-open {}", filepath).c_str());
#endif
                LOG_INFO(Frontend, "Opened cheats file in text editor, exit code: {}", code);
            }

            ImGui::SameLine();

            if (ImGui::Button("Reload File")) {
                system.CheatEngine().LoadCheatFile();
            }

            ImGui::SameLine();

            if (ImGui::Button("Save File")) {
                system.CheatEngine().SaveCheatFile();
            }

            if (ImGui::ListBoxHeader("##cheats", ImVec2(-1.0f, -1.0f))) {
                for (const auto& cheat : system.CheatEngine().GetCheats()) {
                    bool enabled = cheat->IsEnabled();
                    if (ImGui::Checkbox(cheat->GetName().c_str(), &enabled)) {
                        cheat->SetEnabled(enabled);
                    }
                }
                ImGui::ListBoxFooter();
            }
        }

        ImGui::End();
    }

    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        if (room_member->GetState() == Network::RoomMember::State::Joined) {
            ImGui::SetNextWindowSize(ImVec2(640.f, 480.0f), ImGuiCond_Appearing);

            bool open = true;
            const Network::RoomInformation room_information = room_member->GetRoomInformation();
            const Network::RoomMember::MemberList& members = room_member->GetMemberInformation();

            if (ImGui::Begin(fmt::format("{} ({}/{})###room", room_information.name, members.size(),
                                         room_information.member_slots)
                                 .c_str(),
                             &open, ImGuiWindowFlags_NoSavedSettings)) {
                ImGui::PushTextWrapPos();
                ImGui::TextUnformatted(room_information.description.c_str());
                ImGui::PopTextWrapPos();

                if (ImGui::ListBoxHeader("##members", ImVec2(ImGui::GetWindowWidth() / 2.0f,
                                                             ImGui::GetWindowHeight() / 1.23f))) {
                    for (const auto& member : members) {
                        ImGui::PushTextWrapPos();
                        if (member.game_info.name.empty()) {
                            ImGui::TextUnformatted(member.nickname.c_str());
                        } else {
                            ImGui::Text("%s is playing %s", member.nickname.c_str(),
                                        member.game_info.name.c_str());
                        }
                        ImGui::PopTextWrapPos();
                        if (member.nickname != room_member->GetNickname()) {
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
                    ImGui::ListBoxFooter();
                }

                ImGui::SameLine();

                if (ImGui::ListBoxHeader("##messages", ImVec2(ImGui::GetWindowWidth() / 2.0f,
                                                              ImGui::GetWindowHeight() / 1.23f))) {
                    for (const std::string& message : multiplayer_messages) {
                        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() +
                                               ImGui::GetContentRegionAvail().x);
                        ImGui::TextUnformatted(message.c_str());
                        ImGui::PopTextWrapPos();
                    }
                    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                        ImGui::SetScrollHereY(1.0f);
                    }
                    ImGui::ListBoxFooter();
                }

                ImGui::PushItemWidth(ImGui::GetWindowWidth());
                if (ImGui::InputTextWithHint("##message", "Send Chat Message", &multiplayer_message,
                                             ImGuiInputTextFlags_EnterReturnsTrue)) {
                    room_member->SendChatMessage(multiplayer_message);
                    if (multiplayer_messages.size() == 100) {
                        multiplayer_messages.pop_front();
                    }
                    asl::Date date = asl::Date::now();
                    multiplayer_messages.push_back(
                        fmt::format("[{}:{}] <{}> {}", date.hours(), date.minutes(),
                                    room_member->GetNickname(), multiplayer_message));
                    multiplayer_message.clear();
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::PopItemWidth();

                ImGui::EndPopup();
            }
            if (!open) {
                multiplayer_message.clear();
                multiplayer_messages.clear();
                room_member->Leave();
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
                        system.SetResetFilePath(path);
                        system.RequestReset();
                        installed.clear();
                        installed_query.clear();
                        return;
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

    if (show_connect_to_citra_room) {
        ImGui::OpenPopup("Connect To Citra Room");

        ImGui::SetNextWindowSize(io.DisplaySize);

        if (ImGui::BeginPopupModal("Connect To Citra Room", &show_connect_to_citra_room,
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoResize)) {
            ImGui::TextUnformatted("IP:");
            ImGui::SameLine();
            ImGui::InputText("##ip", &Settings::values.multiplayer_ip);

            ImGui::TextUnformatted("Port:");
            ImGui::SameLine();
            ImGui::InputScalar("##port", ImGuiDataType_U16, &Settings::values.multiplayer_port);

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

            if (ImGui::BeginChildFrame(ImGui::GetID("publicrooms"),
                                       ImVec2(-1.0f, ImGui::GetContentRegionAvail().y - 40.0f),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                for (const auto& room : public_rooms) {
                    const std::string room_string =
                        fmt::format("{}\n\nHas Password: {}\nMembers: "
                                    "{}/{}\nPreferred Game: {}\nOwner: "
                                    "{}{}",
                                    room.name, room.has_password ? "Yes" : "No",
                                    room.members.size(), room.max_players, room.game, room.owner,
                                    room.description.empty()
                                        ? ""
                                        : fmt::format("\n\nDescription:\n{}", room.description));

                    if (asl::String(room_string.c_str())
                            .toLowerCase()
                            .contains(asl::String(public_rooms_query.c_str()).toLowerCase())) {
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
                public_rooms_query.clear();
            }

            ImGui::EndPopup();
        }
        if (!show_connect_to_citra_room) {
            public_rooms.clear();
            public_rooms_query.clear();
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
            case SDL_WINDOWEVENT_CLOSE:
                is_open = false;
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
            is_open = false;
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
        if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
            room_member->Join(
                Settings::values.multiplayer_nickname, Service::CFG::GetConsoleIdHash(system),
                Settings::values.multiplayer_ip.c_str(), Settings::values.multiplayer_port,
                Network::NoPreferredMac, Settings::values.multiplayer_password);
        }
    }
}
