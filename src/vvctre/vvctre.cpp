// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iostream>
#include <memory>
#include <string>

#ifdef _WIN32
// windows.h needs to be included before shellapi.h
#include <windows.h>

#include <shellapi.h>
#else
#include <sys/stat.h>
#endif

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <asl/File.h>
#include <asl/Http.h>
#include <asl/JSON.h>
#include <fmt/format.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <portable-file-dialogs.h>
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/param_package.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/loader/loader.h"
#include "core/movie.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "network/network.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
#include "vvctre/applets/mii_selector.h"
#include "vvctre/applets/swkbd.h"
#include "vvctre/camera/image.h"
#include "vvctre/common.h"
#include "vvctre/emu_window/emu_window_sdl2.h"
#include "vvctre/initial_settings.h"
#include "vvctre/plugins.h"

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef _WIN32
extern "C" {
// Tells Nvidia drivers to use the dedicated GPU by default on laptops with switchable graphics
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}
#endif

static std::function<void()> play_movie_loop_callback;

static void InitializeLogging() {
    Log::Filter log_filter(Log::Level::Debug);
    log_filter.ParseFilterString(Settings::values.log_filter);
    Log::SetGlobalFilter(log_filter);

    Log::AddBackend(std::make_unique<Log::ColorConsoleBackend>());
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        std::cerr << "Failed to initialize SDL2! Exiting..." << std::endl;
        std::exit(1);
    }

    Network::Init();
    InputCommon::Init();

    SDL_SetMainReady();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

    SDL_Window* window =
        SDL_CreateWindow(fmt::format("vvctre {}.{}.{} - Initial Settings", vvctre_version_major,
                                     vvctre_version_minor, vvctre_version_patch)
                             .c_str(),
                         SDL_WINDOWPOS_UNDEFINED, // x position
                         SDL_WINDOWPOS_UNDEFINED, // y position
                         640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        pfd::message("vvctre", fmt::format("Failed to create window: {}", SDL_GetError()),
                     pfd::choice::ok, pfd::icon::error);
        vvctreShutdown(nullptr);
        std::exit(1);
    }
    SDL_SetWindowMinimumSize(window, 640, 480);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == nullptr) {
        pfd::message("vvctre", fmt::format("Failed to create OpenGL context: {}", SDL_GetError()),
                     pfd::choice::ok, pfd::icon::error);
        vvctreShutdown(nullptr);
        std::exit(1);
    }
    if (!gladLoadGLLoader(static_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        pfd::message("vvctre", fmt::format("Failed to initialize OpenGL: {}", SDL_GetError()),
                     pfd::choice::ok, pfd::icon::error);
        vvctreShutdown(nullptr);
        std::exit(1);
    }
    SDL_GL_SetSwapInterval(1);
    SDL_PumpEvents();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetStyle().WindowRounding = 0.0f;
    ImGui::GetStyle().ChildRounding = 0.0f;
    ImGui::GetStyle().FrameRounding = 0.0f;
    ImGui::GetStyle().GrabRounding = 0.0f;
    ImGui::GetStyle().PopupRounding = 0.0f;
    ImGui::GetStyle().ScrollbarRounding = 0.0f;
    ImGui_ImplSDL2_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    Core::System& system = Core::System::GetInstance();
    PluginManager plugin_manager(system, window);

    std::shared_ptr<Service::CFG::Module> cfg = std::make_shared<Service::CFG::Module>();
    plugin_manager.cfg = cfg.get();
    plugin_manager.InitialSettingsOpening();
    bool ok_multiplayer = false;
    if (argc < 2) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
        }

        if (!ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT)) {
            std::thread([&] {
                const std::string user_agent =
                    fmt::format("vvctre/{}.{}.{}", vvctre_version_major, vvctre_version_minor,
                                vvctre_version_patch);
                asl::HttpResponse r = asl::Http::get(
                    "https://api.github.com/repos/vvanelslande/vvctre/releases/latest",
                    asl::Dic<>("User-Agent", user_agent.c_str()));
                if (r.ok()) {
                    asl::Var json = r.json();
                    if (json["assets"].length() == 2) {
                        const std::string running_version =
                            fmt::format("{}.{}.{}", vvctre_version_major, vvctre_version_minor,
                                        vvctre_version_patch);
                        const std::string latest_version = *json["tag_name"];
                        if (running_version != latest_version) {
                            if (pfd::message(
                                    "vvctre",
                                    fmt::format(
                                        "You have a old version.\nRunning: {}\nLatest: "
                                        "{}\nOpen the latest version download page and exit?",
                                        running_version, latest_version),
                                    pfd::choice::yes_no, pfd::icon::question)
                                    .result() == pfd::button::yes) {
#ifdef _WIN32
                                [[maybe_unused]] const int code = std::system(
                                    "start https://github.com/vvanelslande/vvctre/releases/latest");
#else
                                [[maybe_unused]] const int code = std::system(
                                    "xdg-open "
                                    "https://github.com/vvanelslande/vvctre/releases/latest");
#endif

                                vvctreShutdown(&plugin_manager);
                                std::exit(0);
                            }
                        }
                    }
                }
            }).detach();
        }

        InitialSettings(plugin_manager, window, *cfg, ok_multiplayer);
        if (Settings::values.file_path.empty()) {
            vvctreShutdown(&plugin_manager);
            return 0;
        }
    } else {
        Settings::values.file_path = std::string(argv[1]);
        Settings::Apply();
    }
    plugin_manager.InitialSettingsOkPressed();

    InitializeLogging();

    if (!Settings::values.record_movie.empty()) {
        Core::Movie::GetInstance().PrepareForRecording();
    }

    if (!Settings::values.play_movie.empty()) {
        Core::Movie::GetInstance().PrepareForPlayback(Settings::values.play_movie);
    }

    std::unique_ptr<EmuWindow_SDL2> emu_window =
        std::make_unique<EmuWindow_SDL2>(system, plugin_manager, window, ok_multiplayer);

    // Register frontend applets
    system.RegisterSoftwareKeyboard(std::make_shared<Frontend::SDL2_SoftwareKeyboard>(*emu_window));
    system.RegisterMiiSelector(std::make_shared<Frontend::SDL2_MiiSelector>(*emu_window));

    // Register camera implementations
    Camera::RegisterFactory("image", std::make_unique<Camera::ImageCameraFactory>());

    plugin_manager.BeforeLoading();
    cfg.reset();
    plugin_manager.cfg = nullptr;

    const Core::System::ResultStatus load_result =
        system.Load(*emu_window, Settings::values.file_path);

    plugin_manager.EmulationStarting();

    switch (load_result) {
    case Core::System::ResultStatus::ErrorNotInitialized:
        vvctreShutdown(&plugin_manager);
        pfd::message("vvctre", "Not initialized", pfd::choice::ok, pfd::icon::error);
        return 1;
    case Core::System::ResultStatus::ErrorSystemMode:
        vvctreShutdown(&plugin_manager);
        pfd::message("vvctre", "Failed to determine system mode", pfd::choice::ok,
                     pfd::icon::error);
        return 1;
    case Core::System::ResultStatus::ErrorLoader_ErrorEncrypted:
        vvctreShutdown(&plugin_manager);
        pfd::message("vvctre", "Encrypted file", pfd::choice::ok, pfd::icon::error);
        return 1;
    case Core::System::ResultStatus::ErrorLoader_ErrorUnsupportedFormat:
        vvctreShutdown(&plugin_manager);
        pfd::message("vvctre", "Unsupported file format", pfd::choice::ok, pfd::icon::error);
        return 1;
    case Core::System::ResultStatus::ErrorFileNotFound:
        vvctreShutdown(&plugin_manager);
        pfd::message("vvctre", "File not found", pfd::choice::ok, pfd::icon::error);
        return 1;
    default:
        break;
    }

    if (!Settings::values.play_movie.empty()) {
        Core::Movie& movie = Core::Movie::GetInstance();

        if (asl::File(Settings::values.play_movie.c_str()).name().contains("loop")) {
            play_movie_loop_callback = [&movie] {
                movie.StartPlayback(Settings::values.play_movie, play_movie_loop_callback);
            };
            play_movie_loop_callback();
        } else {
            movie.StartPlayback(Settings::values.play_movie, [&] {
                pfd::message("vvctre", "Playback finished", pfd::choice::ok);
            });
        }
    }

    if (!Settings::values.record_movie.empty()) {
        Core::Movie::GetInstance().StartRecording(Settings::values.record_movie);
    }

    while (emu_window->IsOpen()) {
        if (emu_window->paused || plugin_manager.paused) {
            while (emu_window->IsOpen() && (emu_window->paused || plugin_manager.paused)) {
                VideoCore::g_renderer->SwapBuffers();
                SDL_GL_SetSwapInterval(1);
            }
            SDL_GL_SetSwapInterval(Settings::values.enable_vsync ? 1 : 0);
        }

        switch (system.RunLoop()) {
        case Core::System::ResultStatus::Success: {
            break;
        }
        case Core::System::ResultStatus::FatalError: {
            pfd::message("vvctre", "Fatal error.\nCheck the console window for more details.",
                         pfd::choice::ok, pfd::icon::error);
            plugin_manager.FatalError();
            system.SetStatus(Core::System::ResultStatus::Success);
            break;
        }
        case Core::System::ResultStatus::ShutdownRequested: {
            emu_window->Close();
            break;
        }
        default: {
            break;
        }
        }
    }

    vvctreShutdown(&plugin_manager);

    return 0;
}
