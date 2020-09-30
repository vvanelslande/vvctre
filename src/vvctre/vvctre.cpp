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
#include <curl/curl.h>
#include <fmt/format.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <mbedtls/ssl.h>
#include <nlohmann/json.hpp>
#include <portable-file-dialogs.h>
#include "common/common_funcs.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/param_package.h"
#include "common/scope_exit.h"
#include "core/3ds.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/loader/loader.h"
#include "core/movie.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "network/room_member.h"
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

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        pfd::message("vvctre", fmt::format("Failed to initialize SDL2: {}", SDL_GetError()),
                     pfd::choice::ok, pfd::icon::error);
        return 1;
    }

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
                         Core::kScreenTopWidth, Core::kScreenTopHeight + Core::kScreenBottomHeight,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        pfd::message("vvctre", fmt::format("Failed to create window: {}", SDL_GetError()),
                     pfd::choice::ok, pfd::icon::error);
        vvctreShutdown(nullptr);
        std::exit(1);
    }
    SDL_SetWindowMinimumSize(window, Core::kScreenTopWidth,
                             Core::kScreenTopHeight + Core::kScreenBottomHeight);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == nullptr) {
        pfd::message("vvctre", fmt::format("Failed to create OpenGL context: {}", SDL_GetError()),
                     pfd::choice::ok, pfd::icon::error);
        vvctreShutdown(nullptr);
        std::exit(1);
    }
    if (!gladLoadGLLoader(static_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        pfd::message("vvctre", fmt::format("Failed to load OpenGL functions: {}", SDL_GetError()),
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

    curl_global_init(CURL_GLOBAL_DEFAULT);

    Core::System& system = Core::System::GetInstance();
    PluginManager plugin_manager(system, window);
    system.SetEmulationStartingAfterFirstTime(
        [&plugin_manager] { plugin_manager.EmulationStartingAfterFirstTime(); });
    if (!system.IsOnLoadFailedSet()) {
        system.SetOnLoadFailed([&plugin_manager](Core::System::ResultStatus result) {
            switch (result) {
            case Core::System::ResultStatus::ErrorNotInitialized:
                vvctreShutdown(&plugin_manager);
                pfd::message("vvctre", "Not initialized", pfd::choice::ok, pfd::icon::error);
                std::exit(1);
            case Core::System::ResultStatus::ErrorSystemMode:
                vvctreShutdown(&plugin_manager);
                pfd::message("vvctre", "Failed to determine system mode", pfd::choice::ok,
                             pfd::icon::error);
                std::exit(1);
            case Core::System::ResultStatus::ErrorLoader_ErrorEncrypted:
                vvctreShutdown(&plugin_manager);
                pfd::message("vvctre", "Encrypted file", pfd::choice::ok, pfd::icon::error);
                std::exit(1);
            case Core::System::ResultStatus::ErrorLoader_ErrorUnsupportedFormat:
                vvctreShutdown(&plugin_manager);
                pfd::message("vvctre", "Unsupported file format", pfd::choice::ok,
                             pfd::icon::error);
                std::exit(1);
            case Core::System::ResultStatus::ErrorFileNotFound:
                vvctreShutdown(&plugin_manager);
                pfd::message("vvctre", "File not found", pfd::choice::ok, pfd::icon::error);
                std::exit(1);
            default:
                break;
            }
        });
    }

    std::shared_ptr<Service::CFG::Module> cfg = std::make_shared<Service::CFG::Module>();
    plugin_manager.cfg = cfg.get();
    plugin_manager.InitialSettingsOpening();
    std::atomic<bool> update_found{false};
    bool ok_multiplayer = false;
    if (argc < 2) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
        }

        if (!ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT)) {
            std::thread([&] {
                CURL* curl = curl_easy_init();
                if (curl == nullptr) {
                    return;
                }

                CURLcode error = curl_easy_setopt(
                    curl, CURLOPT_URL,
                    "https://api.github.com/repos/vvanelslande/vvctre/releases/latest");
                if (error != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    return;
                }

                error = curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
                if (error != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    return;
                }

                error = curl_easy_setopt(curl, CURLOPT_USERAGENT,
                                         fmt::format("vvctre/{}.{}.{}", vvctre_version_major,
                                                     vvctre_version_minor, vvctre_version_patch)
                                             .c_str());
                if (error != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    return;
                }

                std::string body;

                error = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
                if (error != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    return;
                }

                error = curl_easy_setopt(
                    curl, CURLOPT_WRITEFUNCTION,
                    static_cast<curl_write_callback>(
                        [](char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
                            const std::size_t realsize = size * nmemb;
                            static_cast<std::string*>(userdata)->append(ptr, realsize);
                            return realsize;
                        }));
                if (error != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    return;
                }

                error = curl_easy_setopt(
                    curl, CURLOPT_SSL_CTX_FUNCTION,
                    static_cast<CURLcode (*)(CURL * curl, void* ssl_ctx, void* userptr)>(
                        [](CURL* curl, void* ssl_ctx, void* userptr) {
                            void* chain = Common::CreateCertificateChainWithSystemCertificates();
                            if (chain != nullptr) {
                                mbedtls_ssl_conf_ca_chain(static_cast<mbedtls_ssl_config*>(ssl_ctx),
                                                          static_cast<mbedtls_x509_crt*>(chain),
                                                          NULL);
                            } else {
                                return curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
                            }
                            return CURLE_OK;
                        }));
                if (error != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    return;
                }

                error = curl_easy_perform(curl);
                if (error != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    return;
                }

                long status_code;
                error = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
                if (error != CURLE_OK) {
                    curl_easy_cleanup(curl);
                    return;
                }

                curl_easy_cleanup(curl);

                if (status_code != 200) {
                    return;
                }

                const nlohmann::json json = nlohmann::json::parse(body);
                if (json["assets"].size() == 2) {
                    if (fmt::format("{}.{}.{}", vvctre_version_major, vvctre_version_minor,
                                    vvctre_version_patch) != json["tag_name"].get<std::string>()) {
                        update_found = true;
                    }
                }
            }).detach();
        }

        InitialSettings(plugin_manager, window, *cfg, update_found, ok_multiplayer);
        if (Settings::values.file_path.empty()) {
            vvctreShutdown(&plugin_manager);
            return 0;
        }
    } else {
        Settings::values.file_path = std::string(argv[1]);
        Settings::Apply();
    }
    plugin_manager.InitialSettingsOkPressed();

    Log::Filter log_filter(Log::Level::Debug);
    log_filter.ParseFilterString(Settings::values.log_filter);
    Log::SetGlobalFilter(log_filter);
    Log::AddBackend(std::make_unique<Log::ColorConsoleBackend>());

    if (!Settings::values.record_movie.empty()) {
        Core::Movie::GetInstance().PrepareForRecording();
    }

    if (!Settings::values.play_movie.empty()) {
        Core::Movie::GetInstance().PrepareForPlayback(Settings::values.play_movie);
    }

    std::unique_ptr<EmuWindow_SDL2> emu_window =
        std::make_unique<EmuWindow_SDL2>(system, plugin_manager, window, ok_multiplayer);

    system.SetBeforeLoadingAfterFirstTime([&plugin_manager, &emu_window] {
        emu_window.BeforeLoadingAfterFirstTime();
        plugin_manager.BeforeLoadingAfterFirstTime();
    });

    system.RegisterSoftwareKeyboard(std::make_shared<Frontend::SDL2_SoftwareKeyboard>(*emu_window));
    system.RegisterMiiSelector(std::make_shared<Frontend::SDL2_MiiSelector>(*emu_window));
    Camera::RegisterFactory("image", std::make_unique<Camera::ImageCameraFactory>());

    plugin_manager.BeforeLoading();
    cfg.reset();
    plugin_manager.cfg = nullptr;

    const Core::System::ResultStatus load_result =
        system.Load(*emu_window, Settings::values.file_path);

    plugin_manager.EmulationStarting();

    if (!Settings::values.play_movie.empty()) {
        Core::Movie& movie = Core::Movie::GetInstance();

        const Core::Movie::ValidationResult movie_result =
            movie.ValidateMovie(Settings::values.play_movie);
        switch (movie_result) {
        case Core::Movie::ValidationResult::OK:
            if (FileUtil::GetFilename(Settings::values.play_movie).find("loop") !=
                std::string::npos) {
                play_movie_loop_callback = [&movie] {
                    movie.StartPlayback(Settings::values.play_movie, play_movie_loop_callback);
                };

                play_movie_loop_callback();
            } else {
                movie.StartPlayback(Settings::values.play_movie, [&] {
                    pfd::message("vvctre", "Playback finished", pfd::choice::ok);
                });
            }
            break;
        case Core::Movie::ValidationResult::GameDismatch:
            pfd::message("vvctre", "Movie was recorded using a ROM with a different program ID",
                         pfd::choice::ok, pfd::icon::warning);
            if (FileUtil::GetFilename(Settings::values.play_movie).find("loop") !=
                std::string::npos) {
                play_movie_loop_callback = [&movie] {
                    movie.StartPlayback(Settings::values.play_movie, play_movie_loop_callback);
                };

                play_movie_loop_callback();
            } else {
                movie.StartPlayback(Settings::values.play_movie, [&] {
                    pfd::message("vvctre", "Playback finished", pfd::choice::ok);
                });
            }
            break;
        case Core::Movie::ValidationResult::Invalid:
            pfd::message("vvctre", "Movie file doesn't have a valid header", pfd::choice::ok,
                         pfd::icon::info);
            break;
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

        switch (system.Run()) {
        case Core::System::ResultStatus::Success: {
            break;
        }
        case Core::System::ResultStatus::FatalError: {
            if (plugin_manager.show_fatal_error_messages) {
                pfd::message("vvctre", "Fatal error.\nCheck the console window for more details.",
                             pfd::choice::ok, pfd::icon::error);
            }
            plugin_manager.FatalError();
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
