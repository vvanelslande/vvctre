// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <clip.h>
#include <cryptopp/osrng.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <imgui_stdlib.h>
#include <portable-file-dialogs.h>
#include <stb_image_write.h>
#include <whereami.h>
#ifdef HAVE_CUBEB
#include "audio_core/cubeb_input.h"
#endif
#include "audio_core/dsp_interface.h"
#include "audio_core/sdl2_input.h"
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
#include "core/hle/service/mic_u.h"
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

static std::string IPC_Recorder_GetStatusString(IPC::RequestStatus status) {
    switch (status) {
    case IPC::RequestStatus::Sent:
        return "Sent";
    case IPC::RequestStatus::Handling:
        return "Handling";
    case IPC::RequestStatus::Handled:
        return "Handled";
    case IPC::RequestStatus::HLEUnimplemented:
        return "HLEUnimplemented";
    default:
        break;
    }

    return "Invalid";
}

std::pair<unsigned, unsigned> EmuWindow_SDL2::TouchToPixelPos(float touch_x, float touch_y) const {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    touch_x *= w;
    touch_y *= h;

    return {static_cast<unsigned>(std::max(std::round(touch_x), 0.0f)),
            static_cast<unsigned>(std::max(std::round(touch_y), 0.0f))};
}

bool EmuWindow_SDL2::IsOpen() const {
    return is_open;
}

void EmuWindow_SDL2::Close() {
    is_open = false;
}

void EmuWindow_SDL2::BeforeLoadingAfterFirstTime() {
    show_cheats_text_editor = false;
    cheats_text_editor_text.clear();
    all_ipc_records.clear();
    ipc_recorder_search_results.clear();
    ipc_recorder_search_text.clear();
    ipc_recorder_search_text_.clear();
    ipc_recorder_callback = nullptr;
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
        switch (error) {
        case Network::RoomMember::Error::LostConnection:
            pfd::message("Error", "Connection to room lost.", pfd::choice::ok, pfd::icon::error);
            break;
        case Network::RoomMember::Error::HostKicked:
            pfd::message("Error", "You have been kicked by the room host.", pfd::choice::ok,
                         pfd::icon::error);
            break;
        case Network::RoomMember::Error::UnknownError:
            pfd::message("Error", "room_member_impl->server is nullptr", pfd::choice::ok,
                         pfd::icon::error);
            break;
        case Network::RoomMember::Error::NicknameCollisionOrNicknameInvalid:
            pfd::message("Error", "Nickname is already in use or not valid.", pfd::choice::ok,
                         pfd::icon::error);
            break;
        case Network::RoomMember::Error::MacAddressCollision:
            pfd::message("Error", "MAC address is already in use. Try to reconnect.",
                         pfd::choice::ok, pfd::icon::error);
            break;
        case Network::RoomMember::Error::ConsoleIdCollision:
            pfd::message("Error", "Your console ID conflicted with someone else's in the room.",
                         pfd::choice::ok, pfd::icon::error);
            break;
        case Network::RoomMember::Error::WrongVersion:
            pfd::message("Error", "Wrong version", pfd::choice::ok, pfd::icon::error);
            break;
        case Network::RoomMember::Error::WrongPassword:
            pfd::message("Error", "Wrong password", pfd::choice::ok, pfd::icon::error);
            break;
        case Network::RoomMember::Error::CouldNotConnect:
            pfd::message("Error", "Could not connect", pfd::choice::ok, pfd::icon::error);
            break;
        case Network::RoomMember::Error::RoomIsFull:
            pfd::message("Error", "Room is full", pfd::choice::ok, pfd::icon::error);
            break;
        case Network::RoomMember::Error::HostBanned:
            pfd::message("Error", "The host of the room has banned you.", pfd::choice::ok,
                         pfd::icon::error);
            break;
        default:
            break;
        }
    });

    room_member.BindOnChatMessageReceived([&](const Network::ChatEntry& entry) {
        if (multiplayer_blocked_nicknames.count(entry.nickname)) {
            return;
        }

        multiplayer_messages.push_back(fmt::format(
            "[{:%H:%M}] <{}> {}", fmt::localtime(std::time(NULL)), entry.nickname, entry.message));
    });

    room_member.BindOnStatusMessageReceived([&](const Network::StatusMessageEntry& entry) {
        if (multiplayer_blocked_nicknames.count(entry.nickname)) {
            return;
        }

        switch (entry.type) {
        case Network::StatusMessageTypes::IdMemberJoin:
            multiplayer_messages.push_back(fmt::format(
                "[{:%H:%M}] {} joined", fmt::localtime(std::time(NULL)), entry.nickname));
            break;
        case Network::StatusMessageTypes::IdMemberLeave:
            multiplayer_messages.push_back(
                fmt::format("[{:%H:%M}] {} left", fmt::localtime(std::time(NULL)), entry.nickname));
            break;
        case Network::StatusMessageTypes::IdMemberKicked:
            multiplayer_messages.push_back(fmt::format(
                "[{:%H:%M}] {} was kicked", fmt::localtime(std::time(NULL)), entry.nickname));
            break;
        case Network::StatusMessageTypes::IdMemberBanned:
            multiplayer_messages.push_back(fmt::format(
                "[{:%H:%M}] {} was banned", fmt::localtime(std::time(NULL)), entry.nickname));
            break;
        case Network::StatusMessageTypes::IdAddressUnbanned:
            multiplayer_messages.push_back(
                fmt::format("[{:%H:%M}] Someone was unbanned", fmt::localtime(std::time(NULL))));
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

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            ImGui::OpenPopup("Menu");
            menu_open = true;
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("Menu");
            paused = true;
            menu_open = true;
        }

        if (ImGui::BeginPopup("Menu")) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load File")) {
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
                        system.SetResetFilePath(result[0]);
                        system.RequestReset();
                    }
                }

                if (ImGui::BeginMenu("Load Installed")) {
                    if (!installed_menu_opened) {
                        all_installed = GetInstalledList();
                        installed_menu_opened = true;
                    }

                    if (ImGui::InputTextWithHint("##search", "Search", &installed_search_text_,
                                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                        installed_search_text = installed_search_text_;
                        installed_search_results.clear();

                        if (!installed_search_text.empty()) {
                            for (const auto& title : all_installed) {
                                const auto [path, name] = title;

                                if (Common::ToLower(name).find(Common::ToLower(
                                        installed_search_text)) != std::string::npos) {
                                    installed_search_results.push_back(title);
                                }
                            }
                        }
                    }

                    const auto& v =
                        installed_search_text.empty() ? all_installed : installed_search_results;

                    ImGuiListClipper clipper;
                    clipper.Begin(v.size());

                    while (clipper.Step()) {
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            const auto [path, name] = v[i];

                            if (ImGui::MenuItem(name.c_str())) {
                                system.SetResetFilePath(path);
                                system.RequestReset();
                                all_installed.clear();
                                installed_search_results.clear();
                                installed_search_text.clear();
                                installed_search_text_.clear();
                                break;
                            }
                        }
                    }

                    ImGui::EndMenu();
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

                        glClearColor(Settings::values.background_color_red,
                                     Settings::values.background_color_green,
                                     Settings::values.background_color_blue, 0.0f);
                        glClear(GL_COLOR_BUFFER_BIT);
                        ImGui::Render();
                        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                        SDL_GL_SwapWindow(window);
                    }

                    return;
                }

                if (ImGui::BeginMenu("Amiibo")) {
                    if (ImGui::MenuItem("Load File (Encrypted)")) {
                        int length = wai_getExecutablePath(nullptr, 0, nullptr);
                        std::string vvctre_folder(length, '\0');
                        int dirname_length = 0;
                        wai_getExecutablePath(&vvctre_folder[0], length, &dirname_length);
                        vvctre_folder = vvctre_folder.substr(0, dirname_length);

                        const std::vector<std::string> result =
                            pfd::open_file("Load Amiibo", vvctre_folder,
                                           {"Amiibo Files", "*.bin *.BIN"})
                                .result();

                        if (!result.empty()) {
                            FileUtil::IOFile file(result[0], "rb");
                            Service::NFC::AmiiboData data;

                            if (file.ReadArray(&data, 1) == 1) {
                                if (std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                        system.ServiceManager()
                                            .GetService<Service::NFC::Module::Interface>("nfc:u")) {
                                    nfc->LoadAmiibo(data);
                                }
                            } else {
                                pfd::message("vvctre", "Failed to load the amiibo file",
                                             pfd::choice::ok, pfd::icon::error);
                            }
                        }
                    }

                    if (ImGui::MenuItem("Load File (Decrypted)")) {
                        int length = wai_getExecutablePath(nullptr, 0, nullptr);
                        std::string vvctre_folder(length, '\0');
                        int dirname_length = 0;
                        wai_getExecutablePath(&vvctre_folder[0], length, &dirname_length);
                        vvctre_folder = vvctre_folder.substr(0, dirname_length);

                        const std::vector<std::string> result =
                            pfd::open_file("Load Amiibo", vvctre_folder,
                                           {"Amiibo Files", "*.bin *.BIN"})
                                .result();

                        if (!result.empty()) {
                            FileUtil::IOFile file(result[0], "rb");
                            std::array<u8, 540> array;

                            if (file.ReadBytes(array.data(), 540) == 540) {
                                if (std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                        system.ServiceManager()
                                            .GetService<Service::NFC::Module::Interface>("nfc:u")) {
                                    Service::NFC::AmiiboData data{};
                                    std::memcpy(data.uuid.data(), &array[0x1D4], data.uuid.size());
                                    std::memcpy(&data.char_id, &array[0x1DC], 8);
                                    nfc->LoadAmiibo(data);
                                }
                            } else {
                                pfd::message("vvctre", "Failed to load the amiibo file",
                                             pfd::choice::ok, pfd::icon::error);
                            }
                        }
                    }

                    if (ImGui::MenuItem("Remove")) {
                        if (std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                system.ServiceManager().GetService<Service::NFC::Module::Interface>(
                                    "nfc:u")) {
                            nfc->RemoveAmiibo();
                        }
                    }

                    if (ImGui::MenuItem("Save (Encrypted)")) {
                        const std::string path = pfd::save_file("Save Amiibo", "amiibo.bin",
                                                                {"BIN Files", "*.bin *.BIN"})
                                                     .result();

                        if (!path.empty()) {
                            std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                system.ServiceManager().GetService<Service::NFC::Module::Interface>(
                                    "nfc:u");

                            if (nfc != nullptr) {
                                FileUtil::IOFile file(path, "wb");
                                file.WriteObject(nfc->GetAmiiboData());
                            }
                        }
                    }

                    if (ImGui::MenuItem("Save (Decrypted)")) {
                        const std::string path = pfd::save_file("Save Amiibo", "amiibo.bin",
                                                                {"BIN Files", "*.bin *.BIN"})
                                                     .result();

                        if (!path.empty()) {
                            if (std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                    system.ServiceManager()
                                        .GetService<Service::NFC::Module::Interface>("nfc:u")) {
                                FileUtil::IOFile file(path, "wb");
                                std::array<u8, 540> array{};
                                array[2] = 0x0F;
                                array[3] = 0xE0;
                                const Service::NFC::AmiiboData data = nfc->GetAmiiboData();
                                std::memcpy(&array[0x1D4], data.uuid.data(), data.uuid.size());
                                std::memcpy(&array[0x1DC], &data.char_id, 8);
                                file.WriteObject(array);
                            }
                        }
                    }

                    if (ImGui::BeginMenu("Generate & Load")) {
                        if (ImGui::InputTextWithHint("##id", "ID", &amiibo_generate_and_load_id,
                                                     ImGuiInputTextFlags_EnterReturnsTrue) &&
                            !amiibo_generate_and_load_id.empty()) {
                            std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                system.ServiceManager().GetService<Service::NFC::Module::Interface>(
                                    "nfc:u");

                            if (nfc != nullptr) {
                                try {
                                    const u64 id = Common::swap64(
                                        std::stoull(amiibo_generate_and_load_id, nullptr, 0));

                                    Service::NFC::AmiiboData data{};
                                    CryptoPP::AutoSeededRandomPool rng;
                                    rng.GenerateBlock(
                                        static_cast<CryptoPP::byte*>(data.uuid.data()),
                                        data.uuid.size());
                                    std::memcpy(&data.char_id, &id, sizeof(id));
                                    nfc->LoadAmiibo(data);

                                    ImGui::CloseCurrentPopup();
                                } catch (const std::invalid_argument&) {
                                }
                            }
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Settings")) {
                if (ImGui::BeginMenu("General")) {
                    if (ImGui::Checkbox("Enable Core 2", &Settings::values.enable_core_2)) {
                        request_reset = true;
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::PushTextWrapPos(io.DisplaySize.x * 0.5f);
                        ImGui::TextUnformatted("This is needed to play some games (including "
                                               "Donkey Kong Country Returns 3D, Sonic Boom: Fire & "
                                               "Ice, and Sonic Boom: Shattered Crystal)");
                        ImGui::PopTextWrapPos();
                        ImGui::EndTooltip();
                    }

                    ImGui::PushTextWrapPos();
                    ImGui::TextUnformatted("If you enable or disable core 2, emulation will "
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
                        system.DSP().EnableStretching(Settings::values.enable_audio_stretching);
                    }

                    ImGui::SliderFloat("Volume##Output", &Settings::values.audio_volume, 0.0f,
                                       1.0f);

                    if (ImGui::BeginCombo("Sink##Output", Settings::values.audio_sink_id.c_str())) {
                        if (ImGui::Selectable("auto", Settings::values.audio_sink_id == "auto")) {
                            Settings::values.audio_sink_id = "auto";

                            system.DSP().SetSink(Settings::values.audio_sink_id,
                                                 Settings::values.audio_device_id);
                        }

                        for (const auto& sink : AudioCore::GetSinkIDs()) {
                            if (ImGui::Selectable(sink, Settings::values.audio_sink_id == sink)) {
                                Settings::values.audio_sink_id = sink;

                                system.DSP().SetSink(Settings::values.audio_sink_id,
                                                     Settings::values.audio_device_id);
                            }
                        }

                        ImGui::EndCombo();
                    }

                    if (ImGui::BeginCombo("Device##Output",
                                          Settings::values.audio_device_id.c_str())) {
                        if (ImGui::Selectable("auto", Settings::values.audio_device_id == "auto")) {
                            Settings::values.audio_device_id = "auto";
                            system.DSP().SetSink(Settings::values.audio_sink_id,
                                                 Settings::values.audio_device_id);
                        }

                        for (const std::string& device :
                             AudioCore::GetDeviceListForSink(Settings::values.audio_sink_id)) {
                            if (ImGui::Selectable(device.c_str(),
                                                  Settings::values.audio_device_id == device)) {
                                Settings::values.audio_device_id = device;
                                system.DSP().SetSink(Settings::values.audio_sink_id,
                                                     Settings::values.audio_device_id);
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
                            Service::MIC::ReloadMic(system);
                        }

                        if (ImGui::Selectable("Real Device",
                                              Settings::values.microphone_input_type ==
                                                  Settings::MicrophoneInputType::Real)) {
                            Settings::values.microphone_input_type =
                                Settings::MicrophoneInputType::Real;
                            Service::MIC::ReloadMic(system);
                        }

                        if (ImGui::Selectable("Static Noise",
                                              Settings::values.microphone_input_type ==
                                                  Settings::MicrophoneInputType::Static)) {
                            Settings::values.microphone_input_type =
                                Settings::MicrophoneInputType::Static;
                            Service::MIC::ReloadMic(system);
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
                                Service::MIC::ReloadMic(system);
                            }
#ifdef HAVE_CUBEB
                            if (ImGui::Selectable(
                                    "Cubeb", Settings::values.microphone_real_device_backend ==
                                                 Settings::MicrophoneRealDeviceBackend::Cubeb)) {
                                Settings::values.microphone_real_device_backend =
                                    Settings::MicrophoneRealDeviceBackend::Cubeb;
                                Service::MIC::ReloadMic(system);
                            }
#endif
                            if (ImGui::Selectable(
                                    "SDL2", Settings::values.microphone_real_device_backend ==
                                                Settings::MicrophoneRealDeviceBackend::SDL2)) {
                                Settings::values.microphone_real_device_backend =
                                    Settings::MicrophoneRealDeviceBackend::SDL2;
                                Service::MIC::ReloadMic(system);
                            }
                            if (ImGui::Selectable(
                                    "Null", Settings::values.microphone_real_device_backend ==
                                                Settings::MicrophoneRealDeviceBackend::Null)) {
                                Settings::values.microphone_real_device_backend =
                                    Settings::MicrophoneRealDeviceBackend::Null;
                                Service::MIC::ReloadMic(system);
                            }
                            ImGui::EndCombo();
                        }

                        if (ImGui::BeginCombo("Device##Microphone",
                                              Settings::values.microphone_device.c_str())) {
                            if (ImGui::Selectable("auto",
                                                  Settings::values.microphone_device == "auto")) {
                                Settings::values.microphone_device = "auto";
                                Service::MIC::ReloadMic(system);
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
                                        Service::MIC::ReloadMic(system);
                                    }
                                }
#else
                                for (const std::string& device :
                                     AudioCore::ListSDL2InputDevices()) {
                                    if (ImGui::Selectable(device.c_str(),
                                                          Settings::values.microphone_device ==
                                                              device)) {
                                        Settings::values.microphone_device = device;
                                        Service::MIC::ReloadMic(system);
                                    }
                                }
#endif

                                break;
                            case Settings::MicrophoneRealDeviceBackend::Cubeb:
#ifdef HAVE_CUBEB
                                for (const std::string& device :
                                     AudioCore::ListCubebInputDevices()) {
                                    if (ImGui::Selectable(device.c_str(),
                                                          Settings::values.microphone_device ==
                                                              device)) {
                                        Settings::values.microphone_device = device;
                                        Service::MIC::ReloadMic(system);
                                    }
                                }
#endif

                                break;
                            case Settings::MicrophoneRealDeviceBackend::SDL2:
                                for (const std::string& device :
                                     AudioCore::ListSDL2InputDevices()) {
                                    if (ImGui::Selectable(device.c_str(),
                                                          Settings::values.microphone_device ==
                                                              device)) {
                                        Settings::values.microphone_device = device;
                                        Service::MIC::ReloadMic(system);
                                    }
                                }

                                break;
                            case Settings::MicrophoneRealDeviceBackend::Null:
                                if (ImGui::Selectable("null", Settings::values.microphone_device ==
                                                                  "null")) {
                                    Settings::values.microphone_device = "null";
                                    Service::MIC::ReloadMic(system);
                                }

                                break;
                            default:
                                break;
                            }

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
                        if (ImGui::Selectable(
                                "blank", Settings::values.camera_engine[static_cast<std::size_t>(
                                             Service::CAM::CameraIndex::InnerCamera)] == "blank")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "blank";

                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }

                        if (ImGui::Selectable(
                                "image", Settings::values.camera_engine[static_cast<std::size_t>(
                                             Service::CAM::CameraIndex::InnerCamera)] == "image")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "image";

                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }

                        if (ImGui::Selectable(
                                "tcp_client_rgb24_640x480",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] ==
                                    "tcp_client_rgb24_640x480")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] =
                                "tcp_client_rgb24_640x480";

                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
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
                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }

                        ImGui::InputText(
                            "File Path/URL##Inner",
                            &Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)]);

                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }
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

                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }

                        if (ImGui::InputScalar("Port##Inner", ImGuiDataType_U16, &port, 0, 0)) {
                            params.Set("port", static_cast<int>(port));

                            Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = params.Serialize();
                        }

                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
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

                                if (std::shared_ptr<Service::CAM::Module> cam =
                                        Service::CAM::GetModule(system)) {
                                    cam->ReloadCameraDevices();
                                }
                            }

                            if (ImGui::Selectable(
                                    "Horizontal",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::InnerCamera)] ==
                                        Service::CAM::Flip::Horizontal)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Horizontal;

                                if (std::shared_ptr<Service::CAM::Module> cam =
                                        Service::CAM::GetModule(system)) {
                                    cam->ReloadCameraDevices();
                                }
                            }

                            if (ImGui::Selectable(
                                    "Vertical",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::InnerCamera)] ==
                                        Service::CAM::Flip::Vertical)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Vertical;

                                if (std::shared_ptr<Service::CAM::Module> cam =
                                        Service::CAM::GetModule(system)) {
                                    cam->ReloadCameraDevices();
                                }
                            }

                            if (ImGui::Selectable(
                                    "Reverse",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::InnerCamera)] ==
                                        Service::CAM::Flip::Reverse)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Reverse;

                                if (std::shared_ptr<Service::CAM::Module> cam =
                                        Service::CAM::GetModule(system)) {
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
                        if (ImGui::Selectable(
                                "blank",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] == "blank")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "blank";

                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }

                        if (ImGui::Selectable(
                                "image",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] == "image")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "image";

                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }

                        if (ImGui::Selectable(
                                "tcp_client_rgb24_640x480",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] ==
                                    "tcp_client_rgb24_640x480")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] =
                                "tcp_client_rgb24_640x480";

                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
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
                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }

                        ImGui::InputText(
                            "File Path/URL##Outer Left",
                            &Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)]);

                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }
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

                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }

                        if (ImGui::InputScalar("Port##Outer Left", ImGuiDataType_U16, &port, 0,
                                               0)) {
                            params.Set("port", static_cast<int>(port));

                            Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = params.Serialize();
                        }

                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
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

                                if (std::shared_ptr<Service::CAM::Module> cam =
                                        Service::CAM::GetModule(system)) {
                                    cam->ReloadCameraDevices();
                                }
                            }

                            if (ImGui::Selectable(
                                    "Horizontal",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::OuterLeftCamera)] ==
                                        Service::CAM::Flip::Horizontal)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Horizontal;

                                if (std::shared_ptr<Service::CAM::Module> cam =
                                        Service::CAM::GetModule(system)) {
                                    cam->ReloadCameraDevices();
                                }
                            }

                            if (ImGui::Selectable(
                                    "Vertical",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::OuterLeftCamera)] ==
                                        Service::CAM::Flip::Vertical)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Vertical;

                                if (std::shared_ptr<Service::CAM::Module> cam =
                                        Service::CAM::GetModule(system)) {
                                    cam->ReloadCameraDevices();
                                }
                            }

                            if (ImGui::Selectable(
                                    "Reverse",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::OuterLeftCamera)] ==
                                        Service::CAM::Flip::Reverse)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Reverse;

                                if (std::shared_ptr<Service::CAM::Module> cam =
                                        Service::CAM::GetModule(system)) {
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
                        if (ImGui::Selectable(
                                "blank",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] == "blank")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "blank";

                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }

                        if (ImGui::Selectable(
                                "image",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] == "image")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "image";

                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }

                        if (ImGui::Selectable(
                                "tcp_client_rgb24_640x480",
                                Settings::values.camera_engine[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] ==
                                    "tcp_client_rgb24_640x480")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] =
                                "tcp_client_rgb24_640x480";

                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
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
                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }

                        ImGui::InputText(
                            "File Path/URL##Outer Right",
                            &Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)]);

                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }
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

                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
                        }

                        if (ImGui::InputScalar("Port##Outer Right", ImGuiDataType_U16, &port, 0,
                                               0)) {
                            params.Set("port", static_cast<int>(port));

                            Settings::values.camera_parameter[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = params.Serialize();
                        }

                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            if (std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system)) {
                                cam->ReloadCameraDevices();
                            }
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

                                if (std::shared_ptr<Service::CAM::Module> cam =
                                        Service::CAM::GetModule(system)) {
                                    cam->ReloadCameraDevices();
                                }
                            }

                            if (ImGui::Selectable(
                                    "Horizontal",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::OuterRightCamera)] ==
                                        Service::CAM::Flip::Horizontal)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Horizontal;

                                if (std::shared_ptr<Service::CAM::Module> cam =
                                        Service::CAM::GetModule(system)) {
                                    cam->ReloadCameraDevices();
                                }
                            }

                            if (ImGui::Selectable(
                                    "Vertical",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::OuterRightCamera)] ==
                                        Service::CAM::Flip::Vertical)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Vertical;

                                if (std::shared_ptr<Service::CAM::Module> cam =
                                        Service::CAM::GetModule(system)) {
                                    cam->ReloadCameraDevices();
                                }
                            }

                            if (ImGui::Selectable(
                                    "Reverse",
                                    Settings::values.camera_flip[static_cast<std::size_t>(
                                        Service::CAM::CameraIndex::OuterRightCamera)] ==
                                        Service::CAM::Flip::Reverse)) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Reverse;

                                if (std::shared_ptr<Service::CAM::Module> cam =
                                        Service::CAM::GetModule(system)) {
                                    cam->ReloadCameraDevices();
                                }
                            }

                            ImGui::EndCombo();
                        }
                    }

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
                            if (ImGui::Selectable("January", month == 1)) {
                                cfg->SetBirthday(1, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("February", month == 2)) {
                                cfg->SetBirthday(2, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("March", month == 3)) {
                                cfg->SetBirthday(3, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("April", month == 4)) {
                                cfg->SetBirthday(4, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("May", month == 5)) {
                                cfg->SetBirthday(5, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("June", month == 6)) {
                                cfg->SetBirthday(6, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("July", month == 7)) {
                                cfg->SetBirthday(7, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("August", month == 8)) {
                                cfg->SetBirthday(8, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("September", month == 9)) {
                                cfg->SetBirthday(9, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("October", month == 10)) {
                                cfg->SetBirthday(10, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("November", month == 11)) {
                                cfg->SetBirthday(11, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("December", month == 12)) {
                                cfg->SetBirthday(12, day);
                                config_savegame_changed = true;
                            }

                            ImGui::EndCombo();
                        }

                        if (ImGui::InputScalar("Birthday Day", ImGuiDataType_U8, &day)) {
                            cfg->SetBirthday(month, day);
                            config_savegame_changed = true;
                        }

                        const Service::CFG::SystemLanguage language = cfg->GetSystemLanguage();

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

                                return "Invalid";
                            }())) {
                            if (ImGui::Selectable("Japanese",
                                                  language ==
                                                      Service::CFG::SystemLanguage::LANGUAGE_JP)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_JP);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("English",
                                                  language ==
                                                      Service::CFG::SystemLanguage::LANGUAGE_EN)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_EN);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("French",
                                                  language ==
                                                      Service::CFG::SystemLanguage::LANGUAGE_FR)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_FR);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("German",
                                                  language ==
                                                      Service::CFG::SystemLanguage::LANGUAGE_DE)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_DE);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Italian",
                                                  language ==
                                                      Service::CFG::SystemLanguage::LANGUAGE_IT)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_IT);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Spanish",
                                                  language ==
                                                      Service::CFG::SystemLanguage::LANGUAGE_ES)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ES);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Simplified Chinese",
                                                  language ==
                                                      Service::CFG::SystemLanguage::LANGUAGE_ZH)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ZH);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Korean",
                                                  language ==
                                                      Service::CFG::SystemLanguage::LANGUAGE_KO)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_KO);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Dutch",
                                                  language ==
                                                      Service::CFG::SystemLanguage::LANGUAGE_NL)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_NL);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Portuguese",
                                                  language ==
                                                      Service::CFG::SystemLanguage::LANGUAGE_PT)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_PT);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Russian",
                                                  language ==
                                                      Service::CFG::SystemLanguage::LANGUAGE_RU)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_RU);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Traditional Chinese",
                                                  language ==
                                                      Service::CFG::SystemLanguage::LANGUAGE_TW)) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_TW);
                                config_savegame_changed = true;
                            }

                            ImGui::EndCombo();
                        }

                        const Service::CFG::SoundOutputMode sound_output_mode =
                            cfg->GetSoundOutputMode();

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
                                cfg->SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_MONO);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable(
                                    "Stereo", sound_output_mode ==
                                                  Service::CFG::SoundOutputMode::SOUND_STEREO)) {
                                cfg->SetSoundOutputMode(
                                    Service::CFG::SoundOutputMode::SOUND_STEREO);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable(
                                    "Surround",
                                    sound_output_mode ==
                                        Service::CFG::SoundOutputMode::SOUND_SURROUND)) {
                                cfg->SetSoundOutputMode(
                                    Service::CFG::SoundOutputMode::SOUND_SURROUND);
                                config_savegame_changed = true;
                            }

                            ImGui::EndCombo();
                        }

                        const u8 country_code = cfg->GetCountryCode();

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
                                cfg->SetCountry(1);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Anguilla", country_code == 8)) {
                                cfg->SetCountry(8);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Antigua and Barbuda", country_code == 9)) {
                                cfg->SetCountry(9);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Argentina", country_code == 10)) {
                                cfg->SetCountry(10);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Aruba", country_code == 11)) {
                                cfg->SetCountry(11);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Bahamas", country_code == 12)) {
                                cfg->SetCountry(12);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Barbados", country_code == 13)) {
                                cfg->SetCountry(13);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Belize", country_code == 14)) {
                                cfg->SetCountry(14);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Bolivia", country_code == 15)) {
                                cfg->SetCountry(15);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Brazil", country_code == 16)) {
                                cfg->SetCountry(16);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("British Virgin Islands", country_code == 17)) {
                                cfg->SetCountry(17);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Canada", country_code == 18)) {
                                cfg->SetCountry(18);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Cayman Islands", country_code == 19)) {
                                cfg->SetCountry(19);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Chile", country_code == 20)) {
                                cfg->SetCountry(20);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Colombia", country_code == 21)) {
                                cfg->SetCountry(21);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Costa Rica", country_code == 22)) {
                                cfg->SetCountry(22);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Dominica", country_code == 23)) {
                                cfg->SetCountry(23);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Dominican Republic", country_code == 24)) {
                                cfg->SetCountry(24);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Ecuador", country_code == 25)) {
                                cfg->SetCountry(25);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("El Salvador", country_code == 26)) {
                                cfg->SetCountry(26);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("French Guiana", country_code == 27)) {
                                cfg->SetCountry(27);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Grenada", country_code == 28)) {
                                cfg->SetCountry(28);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Guadeloupe", country_code == 29)) {
                                cfg->SetCountry(29);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Guatemala", country_code == 30)) {
                                cfg->SetCountry(30);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Guyana", country_code == 31)) {
                                cfg->SetCountry(31);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Haiti", country_code == 32)) {
                                cfg->SetCountry(32);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Honduras", country_code == 33)) {
                                cfg->SetCountry(33);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Jamaica", country_code == 34)) {
                                cfg->SetCountry(34);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Martinique", country_code == 35)) {
                                cfg->SetCountry(35);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Mexico", country_code == 36)) {
                                cfg->SetCountry(36);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Montserrat", country_code == 37)) {
                                cfg->SetCountry(37);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Netherlands Antilles", country_code == 38)) {
                                cfg->SetCountry(38);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Nicaragua", country_code == 39)) {
                                cfg->SetCountry(39);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Panama", country_code == 40)) {
                                cfg->SetCountry(40);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Paraguay", country_code == 41)) {
                                cfg->SetCountry(41);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Peru", country_code == 42)) {
                                cfg->SetCountry(42);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Saint Kitts and Nevis", country_code == 43)) {
                                cfg->SetCountry(43);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Saint Lucia", country_code == 44)) {
                                cfg->SetCountry(44);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Saint Vincent and the Grenadines",
                                                  country_code == 45)) {
                                cfg->SetCountry(45);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Suriname", country_code == 46)) {
                                cfg->SetCountry(46);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Trinidad and Tobago", country_code == 47)) {
                                cfg->SetCountry(47);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Turks and Caicos Islands", country_code == 48)) {
                                cfg->SetCountry(48);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("United States", country_code == 49)) {
                                cfg->SetCountry(49);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Uruguay", country_code == 50)) {
                                cfg->SetCountry(50);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("US Virgin Islands", country_code == 51)) {
                                cfg->SetCountry(51);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Venezuela", country_code == 52)) {
                                cfg->SetCountry(52);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Albania", country_code == 64)) {
                                cfg->SetCountry(64);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Australia", country_code == 65)) {
                                cfg->SetCountry(65);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Austria", country_code == 66)) {
                                cfg->SetCountry(66);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Belgium", country_code == 67)) {
                                cfg->SetCountry(67);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Bosnia and Herzegovina", country_code == 68)) {
                                cfg->SetCountry(68);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Botswana", country_code == 69)) {
                                cfg->SetCountry(69);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Bulgaria", country_code == 70)) {
                                cfg->SetCountry(70);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Croatia", country_code == 71)) {
                                cfg->SetCountry(71);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Cyprus", country_code == 72)) {
                                cfg->SetCountry(72);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Czech Republic", country_code == 73)) {
                                cfg->SetCountry(73);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Denmark", country_code == 74)) {
                                cfg->SetCountry(74);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Estonia", country_code == 75)) {
                                cfg->SetCountry(75);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Finland", country_code == 76)) {
                                cfg->SetCountry(76);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("France", country_code == 77)) {
                                cfg->SetCountry(77);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Germany", country_code == 78)) {
                                cfg->SetCountry(78);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Greece", country_code == 79)) {
                                cfg->SetCountry(79);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Hungary", country_code == 80)) {
                                cfg->SetCountry(80);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Iceland", country_code == 81)) {
                                cfg->SetCountry(81);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Ireland", country_code == 82)) {
                                cfg->SetCountry(82);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Italy", country_code == 83)) {
                                cfg->SetCountry(83);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Latvia", country_code == 84)) {
                                cfg->SetCountry(84);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Lesotho", country_code == 85)) {
                                cfg->SetCountry(85);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Liechtenstein", country_code == 86)) {
                                cfg->SetCountry(86);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Lithuania", country_code == 87)) {
                                cfg->SetCountry(87);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Luxembourg", country_code == 88)) {
                                cfg->SetCountry(88);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Macedonia", country_code == 89)) {
                                cfg->SetCountry(89);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Malta", country_code == 90)) {
                                cfg->SetCountry(90);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Montenegro", country_code == 91)) {
                                cfg->SetCountry(91);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Mozambique", country_code == 92)) {
                                cfg->SetCountry(92);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Namibia", country_code == 93)) {
                                cfg->SetCountry(93);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Netherlands", country_code == 94)) {
                                cfg->SetCountry(94);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("New Zealand", country_code == 95)) {
                                cfg->SetCountry(95);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Norway", country_code == 96)) {
                                cfg->SetCountry(96);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Poland", country_code == 97)) {
                                cfg->SetCountry(97);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Portugal", country_code == 98)) {
                                cfg->SetCountry(98);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Romania", country_code == 99)) {
                                cfg->SetCountry(99);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Russia", country_code == 100)) {
                                cfg->SetCountry(100);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Serbia", country_code == 101)) {
                                cfg->SetCountry(101);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Slovakia", country_code == 102)) {
                                cfg->SetCountry(102);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Slovenia", country_code == 103)) {
                                cfg->SetCountry(103);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("South Africa", country_code == 104)) {
                                cfg->SetCountry(104);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Spain", country_code == 105)) {
                                cfg->SetCountry(105);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Swaziland", country_code == 106)) {
                                cfg->SetCountry(106);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Sweden", country_code == 107)) {
                                cfg->SetCountry(107);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Switzerland", country_code == 108)) {
                                cfg->SetCountry(108);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Turkey", country_code == 109)) {
                                cfg->SetCountry(109);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("United Kingdom", country_code == 110)) {
                                cfg->SetCountry(110);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Zambia", country_code == 111)) {
                                cfg->SetCountry(111);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Zimbabwe", country_code == 112)) {
                                cfg->SetCountry(112);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Azerbaijan", country_code == 113)) {
                                cfg->SetCountry(113);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Mauritania", country_code == 114)) {
                                cfg->SetCountry(114);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Mali", country_code == 115)) {
                                cfg->SetCountry(115);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Niger", country_code == 116)) {
                                cfg->SetCountry(116);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Chad", country_code == 117)) {
                                cfg->SetCountry(117);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Sudan", country_code == 118)) {
                                cfg->SetCountry(118);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Eritrea", country_code == 119)) {
                                cfg->SetCountry(119);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Djibouti", country_code == 120)) {
                                cfg->SetCountry(120);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Somalia", country_code == 121)) {
                                cfg->SetCountry(121);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Andorra", country_code == 122)) {
                                cfg->SetCountry(122);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Gibraltar", country_code == 123)) {
                                cfg->SetCountry(123);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Guernsey", country_code == 124)) {
                                cfg->SetCountry(124);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Isle of Man", country_code == 125)) {
                                cfg->SetCountry(125);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Jersey", country_code == 126)) {
                                cfg->SetCountry(126);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Monaco", country_code == 127)) {
                                cfg->SetCountry(127);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Taiwan", country_code == 128)) {
                                cfg->SetCountry(128);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("South Korea", country_code == 136)) {
                                cfg->SetCountry(136);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Hong Kong", country_code == 144)) {
                                cfg->SetCountry(144);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Macau", country_code == 145)) {
                                cfg->SetCountry(145);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Indonesia", country_code == 152)) {
                                cfg->SetCountry(152);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Singapore", country_code == 153)) {
                                cfg->SetCountry(153);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Thailand", country_code == 154)) {
                                cfg->SetCountry(154);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Philippines", country_code == 155)) {
                                cfg->SetCountry(155);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Malaysia", country_code == 156)) {
                                cfg->SetCountry(156);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("China", country_code == 160)) {
                                cfg->SetCountry(160);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("United Arab Emirates", country_code == 168)) {
                                cfg->SetCountry(168);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("India", country_code == 169)) {
                                cfg->SetCountry(169);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Egypt", country_code == 170)) {
                                cfg->SetCountry(170);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Oman", country_code == 171)) {
                                cfg->SetCountry(171);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Qatar", country_code == 172)) {
                                cfg->SetCountry(172);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Kuwait", country_code == 173)) {
                                cfg->SetCountry(173);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Saudi Arabia", country_code == 174)) {
                                cfg->SetCountry(174);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Syria", country_code == 175)) {
                                cfg->SetCountry(175);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Bahrain", country_code == 176)) {
                                cfg->SetCountry(176);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Jordan", country_code == 177)) {
                                cfg->SetCountry(177);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("San Marino", country_code == 184)) {
                                cfg->SetCountry(184);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Vatican City", country_code == 185)) {
                                cfg->SetCountry(185);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Bermuda", country_code == 186)) {
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
                        VideoCore::g_hardware_renderer_enabled =
                            Settings::values.use_hardware_renderer;
                    }

                    if (Settings::values.use_hardware_renderer) {
                        ImGui::Indent();

                        if (ImGui::Checkbox("Use Hardware Shader",
                                            &Settings::values.use_hardware_shader)) {
                            VideoCore::g_hardware_shader_enabled =
                                Settings::values.use_hardware_shader;
                        }

                        if (Settings::values.use_hardware_shader) {
                            ImGui::Indent();

                            if (ImGui::Checkbox(
                                    "Accurate Multiplication",
                                    &Settings::values.hardware_shader_accurate_multiplication)) {
                                VideoCore::g_hardware_shader_accurate_multiplication =
                                    Settings::values.hardware_shader_accurate_multiplication;
                            }

                            if (ImGui::Checkbox("Enable Disk Shader Cache",
                                                &Settings::values.enable_disk_shader_cache)) {
                                request_reset = true;
                            }

                            if (ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::PushTextWrapPos(io.DisplaySize.x * 0.5f);
                                ImGui::TextUnformatted("If you change this, emulation will restart "
                                                       "when the menu is closed");
                                ImGui::PopTextWrapPos();
                                ImGui::EndTooltip();
                            }

                            ImGui::Unindent();
                        }

                        if (ImGui::Checkbox("Sharper Distant Objects",
                                            &Settings::values.sharper_distant_objects)) {
                            request_reset = true;
                        }

                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            ImGui::PushTextWrapPos(io.DisplaySize.x * 0.5f);
                            ImGui::TextUnformatted("If you change this, emulation will restart "
                                                   "when the menu is closed");
                            ImGui::PopTextWrapPos();
                            ImGui::EndTooltip();
                        }

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

                            for (const auto& filter : filters) {
                                if (ImGui::Selectable(std::string(filter).c_str(),
                                                      Settings::values.texture_filter == filter)) {
                                    Settings::values.texture_filter = filter;
                                    VideoCore::g_texture_filter_update_requested = true;
                                }
                            }

                            ImGui::EndCombo();
                        }

                        ImGui::Unindent();
                    }

                    if (ImGui::Checkbox("Use Shader JIT", &Settings::values.use_shader_jit)) {
                        VideoCore::g_shader_jit_enabled = Settings::values.use_shader_jit;
                    }
                    if (ImGui::Checkbox("Enable VSync", &Settings::values.enable_vsync)) {
                        if (!paused) {
                            SDL_GL_SetSwapInterval(Settings::values.enable_vsync ? 1 : 0);
                        }
                    }

                    if (ImGui::Checkbox("Enable Linear Filtering",
                                        &Settings::values.enable_linear_filtering)) {
                        VideoCore::g_renderer_sampler_update_requested = true;
                    }

                    if (ImGui::ColorEdit3("Background Color",
                                          &Settings::values.background_color_red,
                                          ImGuiColorEditFlags_NoInputs)) {
                        VideoCore::g_renderer_background_color_update_requested = true;
                    }

                    ImGui::InputText("Post Processing Shader",
                                     &Settings::values.post_processing_shader);

                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        VideoCore::g_renderer_shader_update_requested = true;
                    }

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
                            VideoCore::g_renderer_shader_update_requested = true;
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
                            VideoCore::g_renderer_shader_update_requested = true;
                        }

                        if (ImGui::Selectable("Anaglyph",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Anaglyph)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Anaglyph;

                            if (Settings::values.post_processing_shader != "dubois (builtin)") {
                                Settings::values.post_processing_shader = "dubois (builtin)";
                            }

                            VideoCore::g_renderer_shader_update_requested = true;
                        }

                        if (ImGui::Selectable("Interlaced",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Interlaced)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Interlaced;

                            if (Settings::values.post_processing_shader != "horizontal (builtin)") {
                                Settings::values.post_processing_shader = "horizontal (builtin)";
                            }

                            VideoCore::g_renderer_shader_update_requested = true;
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
                            VideoCore::g_renderer_shader_update_requested = true;
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
                    GUI_AddControlsSettings(is_open, &system, plugin_manager, io, this);

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
                            if (ImGui::Selectable("Default", Settings::values.layout ==
                                                                 Settings::Layout::Default)) {
                                Settings::values.layout = Settings::Layout::Default;
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }

                            if (ImGui::Selectable("Single Screen",
                                                  Settings::values.layout ==
                                                      Settings::Layout::SingleScreen)) {
                                Settings::values.layout = Settings::Layout::SingleScreen;
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }

                            if (ImGui::Selectable("Large Screen",
                                                  Settings::values.layout ==
                                                      Settings::Layout::LargeScreen)) {
                                Settings::values.layout = Settings::Layout::LargeScreen;
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }

                            if (ImGui::Selectable("Side by Side",
                                                  Settings::values.layout ==
                                                      Settings::Layout::SideScreen)) {
                                Settings::values.layout = Settings::Layout::SideScreen;
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }

                            if (ImGui::Selectable("Medium Screen",
                                                  Settings::values.layout ==
                                                      Settings::Layout::MediumScreen)) {
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

                ImGui::Checkbox("Cheats", &show_cheats_window);

                if (ImGui::Checkbox("IPC Recorder", &show_ipc_recorder_window)) {
                    if (!show_ipc_recorder_window) {
                        IPC::Recorder& r = system.Kernel().GetIPCRecorder();

                        r.SetEnabled(false);
                        r.UnbindCallback(ipc_recorder_callback);

                        all_ipc_records.clear();
                        ipc_recorder_search_results.clear();
                        ipc_recorder_search_text.clear();
                        ipc_recorder_search_text_.clear();
                        ipc_recorder_callback = nullptr;
                    }
                }

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

                if (Settings::values.initial_clock == Settings::InitialClock::UnixTimestamp) {
                    if (ImGui::MenuItem("Restart Using System Time")) {
                        Settings::values.initial_clock = Settings::InitialClock::System;
                        request_reset = true;
                    }
                }

                if (ImGui::MenuItem("Restart Using Unix Timestamp")) {
                    SDL_Event event;
                    u64 unix_timestamp = Settings::values.unix_timestamp;
                    bool unix_timestamp_window_open = true;

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

                        ImGui::OpenPopup("Unix Timestamp");
                        ImGui::SetNextWindowPos(
                            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                        if (ImGui::BeginPopupModal("Unix Timestamp", &unix_timestamp_window_open,
                                                   ImGuiWindowFlags_NoSavedSettings |
                                                       ImGuiWindowFlags_NoMove |
                                                       ImGuiWindowFlags_AlwaysAutoResize)) {
                            ImGui::InputScalar("##Unix Timestamp", ImGuiDataType_U64,
                                               &unix_timestamp);
                            if (ImGui::Button("OK")) {
                                Settings::values.initial_clock =
                                    Settings::InitialClock::UnixTimestamp;
                                Settings::values.unix_timestamp = unix_timestamp;
                                request_reset = true;
                                return;
                            }
                            ImGui::EndPopup();
                        }

                        if (!unix_timestamp_window_open) {
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
                    if (Settings::values.region_value != Settings::Region::AutoSelect &&
                        ImGui::MenuItem("Auto-select")) {
                        Settings::values.region_value = Settings::Region::AutoSelect;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != Settings::Region::Japan &&
                        ImGui::MenuItem("Japan")) {
                        Settings::values.region_value = Settings::Region::Japan;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != Settings::Region::USA &&
                        ImGui::MenuItem("USA")) {
                        Settings::values.region_value = Settings::Region::USA;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != Settings::Region::Europe &&
                        ImGui::MenuItem("Europe")) {
                        Settings::values.region_value = Settings::Region::Europe;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != Settings::Region::Australia &&
                        ImGui::MenuItem("Australia")) {
                        Settings::values.region_value = Settings::Region::Australia;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != Settings::Region::China &&
                        ImGui::MenuItem("China")) {
                        Settings::values.region_value = Settings::Region::China;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != Settings::Region::Korea &&
                        ImGui::MenuItem("Korea")) {
                        Settings::values.region_value = Settings::Region::Korea;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != Settings::Region::Taiwan &&
                        ImGui::MenuItem("Taiwan")) {
                        Settings::values.region_value = Settings::Region::Taiwan;
                        request_reset = true;
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem("Reset HLE Wireless Reboot Information")) {
                    if (std::shared_ptr<Service::APT::Module> apt =
                            Service::APT::GetModule(system)) {
                        apt->SetWirelessRebootInfo(std::vector<u8>{});
                    }
                }

                if (ImGui::MenuItem("Reset HLE Delivery Argument")) {
                    if (std::shared_ptr<Service::APT::Module> apt =
                            Service::APT::GetModule(system)) {
                        apt->GetAppletManager()->SetDeliverArg(std::nullopt);
                    }
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
                        const auto& layout = GetFramebufferLayout();
                        u8* data = new u8[layout.width * layout.height * 4];

                        if (VideoCore::RequestScreenshot(
                                data,
                                [=] {
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

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Movie")) {
                    auto& movie = Core::Movie::GetInstance();

                    if (ImGui::MenuItem("Play", nullptr, nullptr,
                                        !movie.IsPlayingInput() && !movie.IsRecordingInput())) {
                        int length = wai_getExecutablePath(nullptr, 0, nullptr);
                        std::string vvctre_folder(length, '\0');
                        int dirname_length = 0;
                        wai_getExecutablePath(&vvctre_folder[0], length, &dirname_length);
                        vvctre_folder = vvctre_folder.substr(0, dirname_length);

                        const auto filename =
                            pfd::open_file("Play Movie", vvctre_folder, {"VvCtre Movie", "*.vcm"})
                                .result();
                        if (!filename.empty()) {
                            const Core::Movie::ValidationResult movie_result =
                                movie.ValidateMovie(filename[0]);
                            switch (movie_result) {
                            case Core::Movie::ValidationResult::OK:
                                if (FileUtil::GetFilename(filename[0]).find("loop") !=
                                    std::string::npos) {
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
                            case Core::Movie::ValidationResult::DifferentProgramID:
                                pfd::message("vvctre",
                                             "Movie was recorded using a ROM with a different "
                                             "program ID",
                                             pfd::choice::ok, pfd::icon::warning);
                                if (FileUtil::GetFilename(filename[0]).find("loop") !=
                                    std::string::npos) {
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

            if (ImGui::BeginMenu("Paths")) {
                const u64 program_id = system.Kernel().GetCurrentProcess()->codeset->program_id;

                if (ImGui::CollapsingHeader("Files")) {
                    // Cheats
                    std::string cheats_file = FileUtil::SanitizePath(
                        fmt::format("{}{:016X}.txt",
                                    FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir),
                                    program_id),
                        FileUtil::DirectorySeparator::PlatformDefault);
                    ImGui::InputText("Cheats##Files", &cheats_file[0], cheats_file.length(),
                                     ImGuiInputTextFlags_ReadOnly);

                    // Luma3DS Mod exheader.bin
                    std::string luma3ds_mod_exheader_bin_file = FileUtil::SanitizePath(
                        fmt::format("{}luma/titles/{:016X}/exheader.bin",
                                    FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                                    FileSys::GetModId(program_id)));
                    ImGui::InputText(
                        "Luma3DS Mod exheader.bin##Files", &luma3ds_mod_exheader_bin_file[0],
                        luma3ds_mod_exheader_bin_file.length(), ImGuiInputTextFlags_ReadOnly);

                    // Luma3DS Mod code.bin
                    std::string luma3ds_mod_code_bin_file = FileUtil::SanitizePath(
                        fmt::format("{}luma/titles/{:016X}/code.bin",
                                    FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                                    FileSys::GetModId(program_id)));
                    ImGui::InputText("Luma3DS Mod code.bin##Files", &luma3ds_mod_code_bin_file[0],
                                     luma3ds_mod_code_bin_file.length(),
                                     ImGuiInputTextFlags_ReadOnly);

                    // Luma3DS Mod code.ips
                    std::string luma3ds_mod_code_ips_file = FileUtil::SanitizePath(
                        fmt::format("{}luma/titles/{:016X}/code.ips",
                                    FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                                    FileSys::GetModId(program_id)));
                    ImGui::InputText("Luma3DS Mod code.ips##Files", &luma3ds_mod_code_ips_file[0],
                                     luma3ds_mod_code_ips_file.length(),
                                     ImGuiInputTextFlags_ReadOnly);

                    // Luma3DS Mod code.bps
                    std::string luma3ds_mod_code_bps_file = FileUtil::SanitizePath(
                        fmt::format("{}luma/titles/{:016X}/code.bin",
                                    FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                                    FileSys::GetModId(program_id)));
                    ImGui::InputText("Luma3DS Mod code.bps##Files", &luma3ds_mod_code_bps_file[0],
                                     luma3ds_mod_code_bps_file.length(),
                                     ImGuiInputTextFlags_ReadOnly);

                    // Disk Shader Cache
                    std::string disk_shader_cache_file = FileUtil::SanitizePath(fmt::format(
                        "{}/{:016X}.vsc", FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir),
                        program_id));
                    ImGui::InputText("Disk Shader Cache##Files", &disk_shader_cache_file[0],
                                     disk_shader_cache_file.length(), ImGuiInputTextFlags_ReadOnly);
                }

                if (ImGui::CollapsingHeader("Folders")) {
                    // Save Data
                    std::string save_data_folder = FileUtil::SanitizePath(
                        FileSys::ArchiveSource_SDSaveData::GetSaveDataPathFor(
                            FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                            system.Kernel().GetCurrentProcess()->codeset->program_id),
                        FileUtil::DirectorySeparator::PlatformDefault);
                    ImGui::InputText("Save Data##Folders", &save_data_folder[0],
                                     save_data_folder.length(), ImGuiInputTextFlags_ReadOnly);

                    // Extra Data
                    u64 extdata_id = 0;
                    system.GetAppLoader().ReadExtdataId(extdata_id);
                    std::string extra_data_folder = FileUtil::SanitizePath(
                        FileSys::GetExtDataPathFromId(
                            FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), extdata_id),
                        FileUtil::DirectorySeparator::PlatformDefault);
                    ImGui::InputText("Extra Data##Folders", &extra_data_folder[0],
                                     extra_data_folder.length(), ImGuiInputTextFlags_ReadOnly);

                    // Title
                    std::string title_folder = FileUtil::SanitizePath(Service::AM::GetTitlePath(
                        Service::AM::GetTitleMediaType(program_id), program_id));
                    ImGui::InputText("Title##Folders", &title_folder[0], title_folder.length(),
                                     ImGuiInputTextFlags_ReadOnly);

                    // Update
                    const u16 category = static_cast<u16>((program_id >> 32) & 0xFFFF);
                    if (!(category & Service::AM::CATEGORY_DLP)) {
                        std::string update_folder =
                            FileUtil::SanitizePath(Service::AM::GetTitlePath(
                                Service::FS::MediaType::SDMC,
                                0x0004000e00000000 | static_cast<u32>(program_id)));
                        ImGui::InputText("Update##Folders", &update_folder[0],
                                         update_folder.length(), ImGuiInputTextFlags_ReadOnly);
                    }

                    // Luma3DS Mod
                    std::string luma3ds_mod_folder = FileUtil::SanitizePath(fmt::format(
                        "{}luma/titles/{:016X}", FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                        FileSys::GetModId(program_id)));
                    ImGui::InputText("Luma3DS Mod##Folders", &luma3ds_mod_folder[0],
                                     luma3ds_mod_folder.length(), ImGuiInputTextFlags_ReadOnly);

                    // Luma3DS Mod RomFS
                    std::string luma3ds_mod_romfs_folder = FileUtil::SanitizePath(
                        fmt::format("{}luma/titles/{:016X}/romfs",
                                    FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                                    FileSys::GetModId(program_id)));
                    ImGui::InputText("Luma3DS Mod RomFS##Folders", &luma3ds_mod_romfs_folder[0],
                                     luma3ds_mod_romfs_folder.length(),
                                     ImGuiInputTextFlags_ReadOnly);

                    // Luma3DS Mod RomFS Patches & Stubs
                    std::string luma3ds_mod_romfs_patches_and_stubs_folder = FileUtil::SanitizePath(
                        fmt::format("{}luma/titles/{:016X}/romfs_ext",
                                    FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                                    FileSys::GetModId(program_id)));
                    ImGui::InputText("Luma3DS Mod RomFS Patches & Stubs##Folders",
                                     &luma3ds_mod_romfs_patches_and_stubs_folder[0],
                                     luma3ds_mod_romfs_patches_and_stubs_folder.length(),
                                     ImGuiInputTextFlags_ReadOnly);

                    // Cheats
                    std::string cheats_folder = FileUtil::SanitizePath(
                        FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir));
                    ImGui::InputText("Cheats##Folders", &cheats_folder[0], cheats_folder.length(),
                                     ImGuiInputTextFlags_ReadOnly);

                    // System Data
                    std::string system_data_folder = FileUtil::SanitizePath(
                        FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir));
                    ImGui::InputText("System Data##Folders", &system_data_folder[0],
                                     system_data_folder.length(), ImGuiInputTextFlags_ReadOnly);

                    // Custom Textures
                    std::string custom_textures_folder = FileUtil::SanitizePath(fmt::format(
                        "{}textures/{:016X}", FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                        program_id));
                    ImGui::InputText("Custom Textures##Folders", &custom_textures_folder[0],
                                     custom_textures_folder.length(), ImGuiInputTextFlags_ReadOnly);

                    // Dumped Textures
                    std::string dumped_textures_folder = FileUtil::SanitizePath(fmt::format(
                        "{}textures/{:016X}", FileUtil::GetUserPath(FileUtil::UserPath::DumpDir),
                        program_id));
                    ImGui::InputText("Dumped Textures##Folders", &dumped_textures_folder[0],
                                     dumped_textures_folder.length(), ImGuiInputTextFlags_ReadOnly);

                    // Post Processing Shaders And Disk Shader Caches
                    std::string post_processing_shaders_and_disk_shader_caches_folder =
                        FileUtil::SanitizePath(
                            FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir));
                    ImGui::InputText("Post Processing Shaders And Disk Shader Caches##Folders",
                                     &post_processing_shaders_and_disk_shader_caches_folder[0],
                                     post_processing_shaders_and_disk_shader_caches_folder.length(),
                                     ImGuiInputTextFlags_ReadOnly);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Multiplayer")) {
                if (!multiplayer_menu_opened) {
                    if (!ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT)) {
                        all_public_rooms = GetPublicCitraRooms();
                    }

                    multiplayer_menu_opened = true;
                }

                ImGui::InputText("IP", &Settings::values.multiplayer_ip);
                ImGui::InputScalar("Port", ImGuiDataType_U16, &Settings::values.multiplayer_port);
                ImGui::InputText("Nickname", &Settings::values.multiplayer_nickname);
                ImGui::InputText("Password", &Settings::values.multiplayer_password);

                if (ImGui::Button("Connect")) {
                    ConnectToCitraRoom();
                }

                ImGui::NewLine();
                ImGui::TextUnformatted("Public Rooms");

                if (ImGui::Button("Refresh")) {
                    all_public_rooms = GetPublicCitraRooms();

                    public_rooms_search_text = public_rooms_search_text_;
                    public_rooms_search_results.clear();

                    const std::string lower_case_text = Common::ToLower(public_rooms_search_text);

                    if (!public_rooms_search_text.empty()) {
                        for (const CitraRoom& room : all_public_rooms) {
                            if ((Common::ToLower(room.name).find(lower_case_text) !=
                                 std::string::npos) ||
                                (Common::ToLower(GetRoomPopupText(room)).find(lower_case_text) !=
                                 std::string::npos)) {
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

                    const std::string lower_case_text = Common::ToLower(public_rooms_search_text);

                    if (!public_rooms_search_text.empty()) {
                        for (const CitraRoom& room : all_public_rooms) {
                            if ((Common::ToLower(room.name).find(lower_case_text) !=
                                 std::string::npos) ||
                                (Common::ToLower(GetRoomPopupText(room)).find(lower_case_text) !=
                                 std::string::npos)) {
                                public_rooms_search_results.push_back(room);
                            }
                        }
                    }
                }

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
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                            ImGui::Selectable(id.c_str());
                            ImGui::PopStyleColor();
                        } else {
                            ImGui::Selectable(id.c_str());
                        }

                        if (ImGui::IsItemClicked()) {
                            ImGui::OpenPopup(id.c_str());
                        }

                        if (ImGui::BeginPopup(id.c_str(), ImGuiWindowFlags_HorizontalScrollbar)) {
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

            installed_menu_opened = false;
            all_installed.clear();
            installed_search_results.clear();
            installed_search_text.clear();
            installed_search_text_.clear();
            multiplayer_menu_opened = false;
            all_public_rooms.clear();
            public_rooms_search_results.clear();
            public_rooms_search_text.clear();
            public_rooms_search_text_.clear();
            amiibo_generate_and_load_id.clear();
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
            if (ImGui::BeginListBox("##miis")) {
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

                ImGui::EndListBox();
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
            IPC::Recorder& r = system.Kernel().GetIPCRecorder();
            bool enabled = r.IsEnabled();

            if (ImGui::Checkbox("Enabled", &enabled)) {
                r.SetEnabled(enabled);

                if (enabled) {
                    ipc_recorder_callback = r.BindCallback([&](const IPC::RequestRecord& record) {
                        const int index = record.id - ipc_recorder_id_offset;
                        if (all_ipc_records.size() > index) {
                            all_ipc_records[index] = record;
                        } else {
                            all_ipc_records.emplace_back(record);
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
                ipc_recorder_search_text.clear();
                ipc_recorder_search_text_.clear();
            }

            ImGui::SameLine();

            if (ImGui::InputTextWithHint("##search", "Search", &ipc_recorder_search_text_,
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                ipc_recorder_search_text = ipc_recorder_search_text_;
                ipc_recorder_search_results.clear();

                if (!ipc_recorder_search_text.empty()) {
                    for (const IPC::RequestRecord& record : all_ipc_records) {
                        std::string service_name;

                        if (record.client_port.id != -1) {
                            service_name = system.ServiceManager().GetServiceNameByPortId(
                                static_cast<u32>(record.client_port.id));
                        }

                        if (service_name.empty()) {
                            service_name = record.server_session.name;
                            service_name = Common::ReplaceAll(service_name, "_Server", "");
                            service_name = Common::ReplaceAll(service_name, "_Client", "");
                        }

                        std::string label = fmt::format("#{} - {} - {}", record.id,
                                                        IPC_Recorder_GetStatusString(record.status),
                                                        record.is_hle ? "HLE" : "LLE");

                        if (!service_name.empty()) {
                            label += fmt::format(" - {}", service_name);
                        }

                        if (!record.function_name.empty()) {
                            label += fmt::format(" - {}", record.function_name);
                        }

                        if (!record.untranslated_request_cmdbuf.empty()) {
                            label +=
                                fmt::format(" - 0x{:08X}", record.untranslated_request_cmdbuf[0]);
                        }

                        if (label.find(ipc_recorder_search_text) != std::string::npos) {
                            ipc_recorder_search_results.push_back(record);
                        }
                    }
                }
            }

            const float width = ImGui::GetWindowWidth();

            if (ImGui::BeginChildFrame(ImGui::GetID("Records"), ImVec2(-1.0f, -1.0f),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                std::vector<IPC::RequestRecord>& records = ipc_recorder_search_text.empty()
                                                               ? all_ipc_records
                                                               : ipc_recorder_search_results;

                ImGuiListClipper clipper;
                clipper.Begin(records.size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        IPC::RequestRecord& record = records[i];
                        std::string service_name;

                        if (record.client_port.id != -1) {
                            service_name = system.ServiceManager().GetServiceNameByPortId(
                                static_cast<u32>(record.client_port.id));
                        }

                        if (service_name.empty()) {
                            service_name = record.server_session.name;
                            service_name = Common::ReplaceAll(service_name, "_Server", "");
                            service_name = Common::ReplaceAll(service_name, "_Client", "");
                        }

                        std::string label = fmt::format("#{} - {} - {}", record.id,
                                                        IPC_Recorder_GetStatusString(record.status),
                                                        record.is_hle ? "HLE" : "LLE");

                        if (!service_name.empty()) {
                            label += fmt::format(" - {}", service_name);
                        }

                        if (!record.function_name.empty()) {
                            label += fmt::format(" - {}", record.function_name);
                        }

                        if (!record.untranslated_request_cmdbuf.empty()) {
                            label +=
                                fmt::format(" - 0x{:08X}", record.untranslated_request_cmdbuf[0]);
                        }

                        if (ImGui::Selectable(label.c_str())) {
                            ImGui::OpenPopup(label.c_str());
                        }

                        if (ImGui::BeginPopup(label.c_str(),
                                              ImGuiWindowFlags_HorizontalScrollbar)) {
                            ImGui::InputInt("ID", &record.id, 0, 0, ImGuiInputTextFlags_ReadOnly);

                            std::string status_string = IPC_Recorder_GetStatusString(record.status);
                            ImGui::InputText("Status", &status_string,
                                             ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputText("Function Name", &record.function_name,
                                             ImGuiInputTextFlags_ReadOnly);

                            if (!record.untranslated_request_cmdbuf.empty()) {
                                std::string function_header_code_string =
                                    fmt::format("0x{:08X}", record.untranslated_request_cmdbuf[0]);
                                ImGui::InputText("Function Header Code",
                                                 &function_header_code_string,
                                                 ImGuiInputTextFlags_ReadOnly);
                            }

                            ImGui::InputText("Service Name", &service_name,
                                             ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputText("Client Process Name", &record.client_process.name,
                                             ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputInt("Client Process ID", &record.client_process.id, 0, 0,
                                            ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputText("Client Thread Name", &record.client_thread.name,
                                             ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputInt("Client Thread ID", &record.client_thread.id, 0, 0,
                                            ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputText("Client Session Name", &record.client_session.name,
                                             ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputInt("Client Session ID", &record.client_session.id, 0, 0,
                                            ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputText("Client Port Name", &record.client_port.name,
                                             ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputInt("Client Port ID", &record.client_port.id, 0, 0,
                                            ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputText("Server Process Name", &record.server_process.name,
                                             ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputInt("Server Process ID", &record.server_process.id, 0, 0,
                                            ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputText("Server Thread Name", &record.server_thread.name,
                                             ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputInt("Server Thread ID", &record.server_thread.id, 0, 0,
                                            ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputText("Server Session Name", &record.server_session.name,
                                             ImGuiInputTextFlags_ReadOnly);

                            ImGui::InputInt("Server Session ID", &record.server_session.id, 0, 0,
                                            ImGuiInputTextFlags_ReadOnly);

                            if (!record.untranslated_request_cmdbuf.empty()) {
                                if (ImGui::CollapsingHeader(
                                        "Untranslated Request Command Buffer")) {
                                    for (std::size_t i = 0;
                                         i < record.untranslated_request_cmdbuf.size(); ++i) {
                                        std::string string = fmt::format(
                                            "0x{:08X}", record.untranslated_request_cmdbuf[i]);
                                        ImGui::InputText(
                                            fmt::format("{}##Untranslated Request Command Buffer",
                                                        i)
                                                .c_str(),
                                            &string, ImGuiInputTextFlags_ReadOnly);
                                    }
                                }
                            }

                            if (!record.translated_request_cmdbuf.empty()) {
                                if (ImGui::CollapsingHeader("Translated Request Command Buffer")) {
                                    for (std::size_t i = 0;
                                         i < record.translated_request_cmdbuf.size(); ++i) {
                                        std::string string = fmt::format(
                                            "0x{:08X}", record.translated_request_cmdbuf[i]);
                                        ImGui::InputText(
                                            fmt::format("{}##Translated Request Command Buffer", i)
                                                .c_str(),
                                            &string, ImGuiInputTextFlags_ReadOnly);
                                    }
                                }
                            }

                            if (!record.untranslated_reply_cmdbuf.empty()) {
                                if (ImGui::CollapsingHeader("Untranslated Reply Command Buffer")) {
                                    for (std::size_t i = 0;
                                         i < record.untranslated_reply_cmdbuf.size(); ++i) {
                                        std::string string = fmt::format(
                                            "0x{:08X}", record.untranslated_reply_cmdbuf[i]);
                                        ImGui::InputText(
                                            fmt::format("{}##Untranslated Reply Command Buffer", i)
                                                .c_str(),
                                            &string, ImGuiInputTextFlags_ReadOnly);
                                    }
                                }
                            }

                            if (!record.translated_reply_cmdbuf.empty()) {
                                if (ImGui::CollapsingHeader("Translated Reply Command Buffer")) {
                                    for (std::size_t i = 0;
                                         i < record.translated_reply_cmdbuf.size(); ++i) {
                                        std::string string = fmt::format(
                                            "0x{:08X}", record.translated_reply_cmdbuf[i]);
                                        ImGui::InputText(
                                            fmt::format("{}##Translated Reply Command Buffer", i)
                                                .c_str(),
                                            &string, ImGuiInputTextFlags_ReadOnly);
                                    }
                                }
                            }

                            ImGui::EndPopup();
                        }
                    }
                }
            }

            ImGui::EndChildFrame();
        }

        if (!show_ipc_recorder_window) {
            IPC::Recorder& r = system.Kernel().GetIPCRecorder();

            r.SetEnabled(false);
            r.UnbindCallback(ipc_recorder_callback);

            all_ipc_records.clear();
            ipc_recorder_search_results.clear();
            ipc_recorder_search_text.clear();
            ipc_recorder_search_text_.clear();
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

                ImGuiListClipper clipper;
                clipper.Begin(cheats.size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        const std::shared_ptr<Cheats::CheatBase>& cheat = cheats[i];
                        bool enabled = cheat->IsEnabled();

                        if (ImGui::Checkbox(cheat->GetName().c_str(), &enabled)) {
                            cheat->SetEnabled(enabled);

                            if (show_cheats_text_editor) {
                                std::ostringstream oss;
                                system.CheatEngine().SaveCheatsToStream(oss);
                                cheats_text_editor_text = oss.str();
                            }
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
                             ImGuiWindowFlags_NoSavedSettings)) {
                if (ImGui::Button("Save")) {
                    const std::string filepath = fmt::format(
                        "{}{:016X}.txt", FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir),
                        system.Kernel().GetCurrentProcess()->codeset->program_id);

                    FileUtil::WriteStringToFile(true, filepath, cheats_text_editor_text);

                    system.CheatEngine().LoadCheatsFromFile();
                }

                ImGui::SameLine();

                if (ImGui::Button("Load Cheats From Text")) {
                    std::istringstream iss(cheats_text_editor_text);
                    system.CheatEngine().LoadCheatsFromStream(iss);
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
                ImGuiListClipper clipper;
                clipper.Begin(members.size());

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

                ImGuiListClipper clipper;
                clipper.Begin(multiplayer_messages.size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        ImGui::TextUnformatted(multiplayer_messages[i].c_str());

                        if (ImGui::BeginPopupContextItem("Message Menu")) {
                            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, spacing);

                            if (ImGui::MenuItem("Delete")) {
                                multiplayer_messages.erase(multiplayer_messages.begin() + i);
                                clipper.ItemsCount = 0;
                                ImGui::PopStyleVar();
                                ImGui::EndPopup();
                                break;
                            }

                            if (ImGui::MenuItem("Clear")) {
                                multiplayer_messages.clear();
                                clipper.ItemsCount = 0;
                                ImGui::PopStyleVar();
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
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureKeyboard) {
                InputCommon::GetKeyboard()->PressKey(static_cast<int>(event.key.keysym.scancode));
            }

            break;
        case SDL_KEYUP:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureKeyboard) {
                InputCommon::GetKeyboard()->ReleaseKey(static_cast<int>(event.key.keysym.scancode));
            }

            break;
        case SDL_MOUSEMOTION:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse && event.button.which != SDL_TOUCH_MOUSEID) {
                TouchMoved((unsigned)std::max(event.motion.x, 0),
                           (unsigned)std::max(event.motion.y, 0));
                InputCommon::GetMotionEmu()->Tilt(event.motion.x, event.motion.y);
            }

            break;
        case SDL_MOUSEBUTTONDOWN:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse && event.button.which != SDL_TOUCH_MOUSEID) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    TouchPressed((unsigned)std::max(event.button.x, 0),
                                 (unsigned)std::max(event.button.y, 0));
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    InputCommon::GetMotionEmu()->BeginTilt(event.button.x, event.button.y);
                }
            }

            break;
        case SDL_MOUSEBUTTONUP:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse && event.button.which != SDL_TOUCH_MOUSEID) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    TouchReleased();
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    InputCommon::GetMotionEmu()->EndTilt();
                }
            }

            break;
        case SDL_FINGERDOWN:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse) {
                const auto [px, py] = TouchToPixelPos(event.tfinger.x, event.tfinger.y);
                TouchPressed(px, py);
            }

            break;
        case SDL_FINGERMOTION:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse) {
                const auto [px, py] = TouchToPixelPos(event.tfinger.x, event.tfinger.y);
                TouchMoved(px, py);
            }

            break;
        case SDL_FINGERUP:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse) {
                TouchReleased();
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

void EmuWindow_SDL2::ConnectToCitraRoom() {
    if (!Settings::values.multiplayer_ip.empty() && Settings::values.multiplayer_port != 0 &&
        !Settings::values.multiplayer_nickname.empty()) {
        system.RoomMember().Join(
            Settings::values.multiplayer_nickname, Service::CFG::GetConsoleIdHash(system),
            Settings::values.multiplayer_ip.c_str(), Settings::values.multiplayer_port,
            Network::NO_PREFERRED_MAC_ADDRESS, Settings::values.multiplayer_password);
    }
}
