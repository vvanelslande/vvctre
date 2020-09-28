// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <curl/curl.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <imgui_stdlib.h>
#include <mbedtls/ssl.h>
#include <nlohmann/json.hpp>
#include <portable-file-dialogs.h>
#include <whereami.h>
#include "common/common_funcs.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/loader/loader.h"
#include "core/loader/smdh.h"
#include "core/movie.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "vvctre/common.h"
#include "vvctre/plugins.h"

const u8 vvctre_version_major = 38;
const u8 vvctre_version_minor = 0;
const u8 vvctre_version_patch = 3;

void vvctreShutdown(PluginManager* plugin_manager) {
    if (plugin_manager != nullptr) {
        plugin_manager->EmulatorClosing();
    }
    Core::Movie::GetInstance().Shutdown();
    Core::System::GetInstance().Shutdown();
    InputCommon::Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_Quit();
    curl_global_cleanup();
}

std::vector<std::tuple<std::string, std::string>> GetInstalledList() {
    std::vector<std::tuple<std::string, std::string>> all;
    std::vector<std::tuple<std::string, std::string>> bootable;
    std::vector<std::tuple<std::string, std::string>> updates;
    std::vector<std::tuple<std::string, std::string>> dlc;
    std::vector<std::tuple<std::string, std::string>> system;
    std::vector<std::tuple<std::string, std::string>> download_play;

    const auto AddTitlesForMediaType = [&](Service::FS::MediaType media_type) {
        FileUtil::FSTEntry entries;
        FileUtil::ScanDirectoryTree(Service::AM::GetMediaTitlePath(media_type), entries, 1);
        for (const FileUtil::FSTEntry& tid_high : entries.children) {
            for (const FileUtil::FSTEntry& tid_low : tid_high.children) {
                std::string tid_string = tid_high.virtual_name + tid_low.virtual_name;

                if (tid_string.length() == Service::AM::TITLE_ID_VALID_LENGTH) {
                    const u64 tid = std::stoull(tid_string, nullptr, 16);

                    const std::string path = Service::AM::GetTitleContentPath(media_type, tid);
                    std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(path);
                    if (loader != nullptr) {
                        std::string title;
                        loader->ReadTitle(title);

                        switch (std::stoull(tid_high.virtual_name, nullptr, 16)) {
                        case 0x00040000: {
                            bootable.push_back(std::make_tuple(
                                path, fmt::format("[Bootable] {} ({:016X})",
                                                  title.empty() ? "Unknown" : title, tid)));
                            break;
                        }
                        case 0x0004000e: {
                            updates.push_back(std::make_tuple(
                                path, fmt::format("[Update] {} ({:016X})",
                                                  title.empty() ? "Unknown" : title, tid)));
                            break;
                        }
                        case 0x0004008c: {
                            dlc.push_back(std::make_tuple(
                                path, fmt::format("[DLC] {} ({:016X})",
                                                  title.empty() ? "Unknown" : title, tid)));
                            break;
                        }
                        case 0x00040001: {
                            download_play.push_back(std::make_tuple(
                                path, fmt::format("[Download Play] {} ({:016X})",
                                                  title.empty() ? "Unknown" : title, tid)));
                            break;
                        }
                        default: {
                            system.push_back(std::make_tuple(
                                path, fmt::format("[System] {} ({:016X})",
                                                  title.empty() ? "Unknown" : title, tid)));
                            break;
                        }
                        }
                    }
                }
            }
        }
    };

    AddTitlesForMediaType(Service::FS::MediaType::NAND);
    AddTitlesForMediaType(Service::FS::MediaType::SDMC);

    all.insert(all.end(), bootable.begin(), bootable.end());
    all.insert(all.end(), updates.begin(), updates.end());
    all.insert(all.end(), dlc.begin(), dlc.end());
    all.insert(all.end(), system.begin(), system.end());
    all.insert(all.end(), download_play.begin(), download_play.end());

    return all;
}

CitraRoomList GetPublicCitraRooms() {
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        LOG_ERROR(Frontend, "curl_easy_init failed");
        return {};
    }

    CURLcode error = curl_easy_setopt(curl, CURLOPT_URL, "https://api.citra-emu.org/lobby");
    if (error != CURLE_OK) {
        LOG_ERROR(Frontend, "{}", curl_easy_strerror(error));
        curl_easy_cleanup(curl);
        return {};
    }

    error = curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    if (error != CURLE_OK) {
        LOG_ERROR(Frontend, "{}", curl_easy_strerror(error));
        curl_easy_cleanup(curl);
        return {};
    }

    std::string body;

    error = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    if (error != CURLE_OK) {
        LOG_ERROR(Frontend, "{}", curl_easy_strerror(error));
        curl_easy_cleanup(curl);
        return {};
    }

    error =
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                         static_cast<curl_write_callback>(
                             [](char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
                                 const std::size_t realsize = size * nmemb;
                                 static_cast<std::string*>(userdata)->append(ptr, realsize);
                                 return realsize;
                             }));
    if (error != CURLE_OK) {
        LOG_ERROR(Frontend, "{}", curl_easy_strerror(error));
        curl_easy_cleanup(curl);
        return {};
    }

    error = curl_easy_setopt(
        curl, CURLOPT_SSL_CTX_FUNCTION,
        static_cast<CURLcode (*)(CURL * curl, void* ssl_ctx, void* userptr)>(
            [](CURL* curl, void* ssl_ctx, void* userptr) {
                void* chain = Common::CreateCertificateChainWithSystemCertificates();
                if (chain != nullptr) {
                    mbedtls_ssl_conf_ca_chain(static_cast<mbedtls_ssl_config*>(ssl_ctx),
                                              static_cast<mbedtls_x509_crt*>(chain), NULL);
                } else {
                    return curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
                }
                return CURLE_OK;
            }));
    if (error != CURLE_OK) {
        LOG_ERROR(Frontend, "{}", curl_easy_strerror(error));
        curl_easy_cleanup(curl);
        return {};
    }

    error = curl_easy_perform(curl);
    if (error != CURLE_OK) {
        LOG_ERROR(Frontend, "{}", curl_easy_strerror(error));
        curl_easy_cleanup(curl);
        return {};
    }

    long status_code;
    error = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    if (error != CURLE_OK) {
        LOG_ERROR(Frontend, "{}", curl_easy_strerror(error));
        curl_easy_cleanup(curl);
        return {};
    }

    curl_easy_cleanup(curl);

    if (status_code != 200) {
        LOG_ERROR(Frontend, "status code: {}, body: {}", status_code, body);
        return {};
    }

    const nlohmann::json json_rooms = nlohmann::json::parse(body)["rooms"];

    CitraRoomList rooms;

    for (int i = 0; i < json_rooms.size(); ++i) {
        const nlohmann::json& json_room = json_rooms[i];

        CitraRoom room;
        room.name = json_room["name"].get<std::string>();
        if (json_room.count("description")) {
            room.description = json_room["description"].get<std::string>();
        }
        room.ip = json_room["address"].get<std::string>();
        room.port = json_room["port"].get<u16>();
        room.owner = json_room["owner"].get<std::string>();
        room.max_players = json_room["maxPlayers"].get<u32>();
        room.has_password = json_room["hasPassword"].get<bool>();
        room.game = json_room["preferredGameName"].get<std::string>();
        const nlohmann::json& members = json_room["players"];
        for (const nlohmann::json& json_member : members) {
            CitraRoom::Member member;
            member.nickname = json_member["nickname"].get<std::string>();
            member.game = json_member["gameName"].get<std::string>();
            room.members.push_back(member);
        }
        rooms.push_back(std::move(room));
    }

    return rooms;
}

const std::string GetRoomPopupText(const CitraRoom& room) {
    std::string text =
        fmt::format("Has Password: {}\nMaximum Members: "
                    "{}\nPreferred Game: {}\nOwner: {}",
                    room.has_password ? "Yes" : "No", room.max_players, room.game, room.owner);
    if (!room.description.empty()) {
        text += fmt::format("\n\nDescription:\n{}", room.description);
    }
    if (!room.members.empty()) {
        text += fmt::format("\n\nMembers ({}):", room.members.size());
        for (const CitraRoom::Member& member : room.members) {
            if (member.game.empty()) {
                text += fmt::format("\n\t{}", member.nickname);
            } else {
                text += fmt::format("\n\t{} is playing {}", member.nickname, member.game);
            }
        }
    }
    return text;
}

bool GUI_CameraAddBrowse(const char* label, std::size_t index) {
    if (ImGui::Button(label)) {
        int length = wai_getExecutablePath(nullptr, 0, nullptr);
        std::string vvctre_folder(length, '\0');
        int dirname_length = 0;
        wai_getExecutablePath(&vvctre_folder[0], length, &dirname_length);
        vvctre_folder = vvctre_folder.substr(0, dirname_length);

        const std::vector<std::string> result =
            pfd::open_file("Browse", vvctre_folder,
                           {"All supported files",
                            "*.jpg *.JPG *.jpeg *.JPEG *.jfif *.JFIF *.png *.PNG "
                            "*.bmp "
                            "*.BMP *.psd *.PSD *.tga *.TGA *.gif *.GIF "
                            "*.hdr *.HDR *.pic *.PIC *.ppm *.PPM *.pgm *.PGM",
                            "JPEG",
                            "*.jpg *.JPG *.jpeg *.JPEG *.jfif *.JFIF",
                            "PNG",
                            "*.png *.PNG",
                            "BMP",
                            "*.bmp *.BMP",
                            "PSD",
                            "*.psd *.PSD",
                            "TGA",
                            "*.tga *.TGA",
                            "GIF",
                            "*.gif *.GIF",
                            "HDR",
                            "*.hdr *.HDR",
                            "PIC",
                            "*.pic *.PIC",
                            "PNM",
                            "*.ppm *.PPM *.pgm *.PGM"})
                .result();

        if (!result.empty()) {
            Settings::values.camera_parameter[index] = result[0];
            return true;
        }
    }

    ImGui::SameLine();

    return false;
}

void GUI_AddControlsSettings(bool& is_open, Core::System* system, PluginManager& plugin_manager,
                             ImGuiIO& io) {
    SDL_Event event;

    const auto GetInput = [&](InputCommon::Polling::DeviceType device_type) -> std::string {
        switch (device_type) {
        case InputCommon::Polling::DeviceType::Button: {
            auto pollers = InputCommon::Polling::GetPollers(device_type);

            for (auto& poller : pollers) {
                poller->Start();
            }

            while (is_open) {
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
                        return InputCommon::GenerateKeyboardParam(event.key.keysym.scancode);
                    } else if (event.type == SDL_QUIT) {
                        if (pfd::message("vvctre", "Would you like to exit now?",
                                         pfd::choice::yes_no, pfd::icon::question)
                                .result() == pfd::button::yes) {
                            vvctreShutdown(&plugin_manager);
                            std::exit(0);
                        }
                    }
                }
            }

            if (!is_open) {
                vvctreShutdown(&plugin_manager);
                std::exit(0);
            }

            return "engine:null";
        }
        case InputCommon::Polling::DeviceType::Analog: {
            auto pollers = InputCommon::Polling::GetPollers(device_type);

            for (auto& poller : pollers) {
                poller->Start();
            }

            std::vector<int> keyboard_scancodes;

            while (is_open) {
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
                                keyboard_scancodes[0], keyboard_scancodes[1], keyboard_scancodes[2],
                                keyboard_scancodes[3], keyboard_scancodes[4], 0.5f);
                        }
                    } else if (event.type == SDL_QUIT) {
                        if (pfd::message("vvctre", "Would you like to exit now?",
                                         pfd::choice::yes_no, pfd::icon::question)
                                .result() == pfd::button::yes) {
                            vvctreShutdown(&plugin_manager);
                            std::exit(0);
                        }
                    }
                }
            }

            if (!is_open) {
                vvctreShutdown(&plugin_manager);
                std::exit(0);
            }
            return "engine:null";
        }
        default: {
            return "engine:null";
        }
        }
    };

    if (ImGui::Button("Load File")) {
        int length = wai_getExecutablePath(nullptr, 0, nullptr);
        std::string vvctre_folder(length, '\0');
        int dirname_length = 0;
        wai_getExecutablePath(&vvctre_folder[0], length, &dirname_length);
        vvctre_folder = vvctre_folder.substr(0, dirname_length);

        const std::vector<std::string> path =
            pfd::open_file("Load File", vvctre_folder, {"JSON Files", "*.json"}).result();
        if (!path.empty()) {
            std::string json_string;
            FileUtil::ReadFileToString(true, path[0], json_string);

            try {
                const nlohmann::json json = nlohmann::json::parse(json_string);
                const nlohmann::json buttons = json["buttons"];
                const nlohmann::json analogs = json["analogs"];
                for (int i = 0; i < buttons.size(); ++i) {
                    Settings::values.buttons[static_cast<std::size_t>(i)] =
                        buttons[i].get<std::string>();
                }
                for (int i = 0; i < analogs.size(); ++i) {
                    Settings::values.analogs[static_cast<std::size_t>(i)] =
                        analogs[i].get<std::string>();
                }
                Settings::values.motion_device = json["motion_device"].get<std::string>();
                Settings::values.touch_device = json["touch_device"].get<std::string>();
                Settings::values.cemuhookudp_address =
                    json["cemuhookudp_address"].get<std::string>();
                Settings::values.cemuhookudp_port = json["cemuhookudp_port"].get<u16>();
                Settings::values.cemuhookudp_pad_index = json["cemuhookudp_pad_index"].get<u8>();

                if (system != nullptr) {
                    std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                    if (hid != nullptr) {
                        hid->ReloadInputDevices();
                    }

                    Service::SM::ServiceManager& sm = system->ServiceManager();
                    std::shared_ptr<Service::IR::IR_USER> ir_user =
                        sm.GetService<Service::IR::IR_USER>("ir:USER");
                    if (ir_user != nullptr) {
                        ir_user->ReloadInputDevices();
                    }
                }

                InputCommon::ReloadInputDevices();
            } catch (const nlohmann::json::exception& e) {
                LOG_ERROR(Frontend, "{}", e.what());
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save File")) {
        const std::string path =
            pfd::save_file("Save File", "controls.json", {"JSON Files", "*.json"}).result();

        if (!path.empty()) {
            const nlohmann::json json = {
                {"buttons", Settings::values.buttons},
                {"analogs", Settings::values.analogs},
                {"motion_device", Settings::values.motion_device},
                {"touch_device", Settings::values.touch_device},
                {"cemuhookudp_address", Settings::values.cemuhookudp_address},
                {"cemuhookudp_port", Settings::values.cemuhookudp_port},
                {"cemuhookudp_pad_index", Settings::values.cemuhookudp_pad_index},
            };

            FileUtil::WriteStringToFile(true, path, json.dump());
        }
    }
    ImGui::SameLine();
    ImGui::Button("Copy Params");
    if (ImGui::BeginPopupContextItem("Copy Params", ImGuiPopupFlags_MouseButtonLeft)) {
        ImGuiStyle& style = ImGui::GetStyle();

        ImGui::TextUnformatted("Buttons");
        ImGui::Separator();

        if (ImGui::Selectable("A##Buttons")) {
            ImGui::SetClipboardText(Settings::values.buttons[Settings::NativeButton::A].c_str());
        }

        if (ImGui::Selectable("B##Buttons")) {
            ImGui::SetClipboardText(Settings::values.buttons[Settings::NativeButton::B].c_str());
        }

        if (ImGui::Selectable("X##Buttons")) {
            ImGui::SetClipboardText(Settings::values.buttons[Settings::NativeButton::X].c_str());
        }

        if (ImGui::Selectable("Y##Buttons")) {
            ImGui::SetClipboardText(Settings::values.buttons[Settings::NativeButton::Y].c_str());
        }

        if (ImGui::Selectable("L##Buttons")) {
            ImGui::SetClipboardText(Settings::values.buttons[Settings::NativeButton::L].c_str());
        }

        if (ImGui::Selectable("R##Buttons")) {
            ImGui::SetClipboardText(Settings::values.buttons[Settings::NativeButton::R].c_str());
        }

        if (ImGui::Selectable("ZL##Buttons")) {
            ImGui::SetClipboardText(Settings::values.buttons[Settings::NativeButton::ZL].c_str());
        }

        if (ImGui::Selectable("ZR##Buttons")) {
            ImGui::SetClipboardText(Settings::values.buttons[Settings::NativeButton::ZR].c_str());
        }

        if (ImGui::Selectable("Start##Buttons")) {
            ImGui::SetClipboardText(
                Settings::values.buttons[Settings::NativeButton::Start].c_str());
        }

        if (ImGui::Selectable("Select##Buttons")) {
            ImGui::SetClipboardText(
                Settings::values.buttons[Settings::NativeButton::Select].c_str());
        }

        if (ImGui::Selectable("Debug##Buttons")) {
            ImGui::SetClipboardText(
                Settings::values.buttons[Settings::NativeButton::Debug].c_str());
        }

        if (ImGui::Selectable("GPIO14##Buttons")) {
            ImGui::SetClipboardText(
                Settings::values.buttons[Settings::NativeButton::Gpio14].c_str());
        }

        if (ImGui::Selectable("HOME##Buttons")) {
            ImGui::SetClipboardText(Settings::values.buttons[Settings::NativeButton::Home].c_str());
        }

        ImGui::NewLine();

        ImGui::TextUnformatted("Circle Pad");
        ImGui::Separator();

        if (ImGui::Selectable("Up##Circle Pad")) {
            ImGui::SetClipboardText(
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad])
                    .Get("up", "")
                    .c_str());
        }

        if (ImGui::Selectable("Down##Circle Pad")) {
            ImGui::SetClipboardText(
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad])
                    .Get("down", "")
                    .c_str());
        }

        if (ImGui::Selectable("Left##Circle Pad")) {
            ImGui::SetClipboardText(
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad])
                    .Get("left", "")
                    .c_str());
        }

        if (ImGui::Selectable("Right##Circle Pad")) {
            ImGui::SetClipboardText(
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad])
                    .Get("right", "")
                    .c_str());
        }

        if (ImGui::Selectable("Modifier##Circle Pad")) {
            ImGui::SetClipboardText(
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad])
                    .Get("modifier", "")
                    .c_str());
        }

        if (ImGui::Selectable("All##Circle Pad")) {
            ImGui::SetClipboardText(
                Settings::values.analogs[Settings::NativeAnalog::CirclePad].c_str());
        }

        ImGui::NewLine();

        ImGui::TextUnformatted("Circle Pad Pro");
        ImGui::Separator();

        if (ImGui::Selectable("Up##Circle Pad Pro")) {
            ImGui::SetClipboardText(
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePadPro])
                    .Get("up", "")
                    .c_str());
        }

        if (ImGui::Selectable("Down##Circle Pad Pro")) {
            ImGui::SetClipboardText(
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePadPro])
                    .Get("down", "")
                    .c_str());
        }

        if (ImGui::Selectable("Left##Circle Pad Pro")) {
            ImGui::SetClipboardText(
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePadPro])
                    .Get("left", "")
                    .c_str());
        }

        if (ImGui::Selectable("Right##Circle Pad Pro")) {
            ImGui::SetClipboardText(
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePadPro])
                    .Get("right", "")
                    .c_str());
        }

        if (ImGui::Selectable("Modifier##Circle Pad Pro")) {
            ImGui::SetClipboardText(
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePadPro])
                    .Get("modifier", "")
                    .c_str());
        }

        if (ImGui::Selectable("All##Circle Pad Pro")) {
            ImGui::SetClipboardText(
                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro].c_str());
        }

        ImGui::NewLine();

        ImGui::TextUnformatted("D-Pad");
        ImGui::Separator();

        if (ImGui::Selectable("Up##D-Pad")) {
            ImGui::SetClipboardText(Settings::values.buttons[Settings::NativeButton::Up].c_str());
        }

        if (ImGui::Selectable("Down##D-Pad")) {
            ImGui::SetClipboardText(Settings::values.buttons[Settings::NativeButton::Down].c_str());
        }

        if (ImGui::Selectable("Left##D-Pad")) {
            ImGui::SetClipboardText(Settings::values.buttons[Settings::NativeButton::Left].c_str());
        }

        if (ImGui::Selectable("Right##D-Pad")) {
            ImGui::SetClipboardText(
                Settings::values.buttons[Settings::NativeButton::Right].c_str());
        }

        ImGui::NewLine();

        ImGui::TextUnformatted("Motion & Touch");
        ImGui::Separator();

        if (ImGui::Selectable("Motion##Motion & Touch")) {
            ImGui::SetClipboardText(Settings::values.motion_device.c_str());
        }

        if (ImGui::Selectable("Touch##Motion & Touch")) {
            ImGui::SetClipboardText(Settings::values.touch_device.c_str());
        }

        ImGui::EndPopup();
    }
    if (ImGui::Button("Set All (Circle Pad)")) {
        Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
            GetInput(InputCommon::Polling::DeviceType::Analog);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(io.DisplaySize.x * 0.5f);
        ImGui::TextUnformatted("Keyboard: Press the keys to use for Up, Down, "
                               "Left, Right, and Modifier.\n\nReal stick: first move "
                               "the stick to the right, and then to the bottom.");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    ImGui::SameLine();
    if (ImGui::Button("Set All (Circle Pad Pro)")) {
        Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
            GetInput(InputCommon::Polling::DeviceType::Analog);

        if (system != nullptr) {
            Service::SM::ServiceManager& sm = system->ServiceManager();
            std::shared_ptr<Service::IR::IR_USER> ir_user =
                sm.GetService<Service::IR::IR_USER>("ir:USER");
            if (ir_user != nullptr) {
                ir_user->ReloadInputDevices();
            }
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(io.DisplaySize.x * 0.5f);
        ImGui::TextUnformatted("Keyboard: Press the keys to use for Up, Down, "
                               "Left, Right, and Modifier.\n\nReal stick: first move "
                               "the stick to the right, and then to the bottom.");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::NewLine();

    ImGui::TextUnformatted("Buttons");
    ImGui::Separator();

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::A]) + "##A")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::A] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("A");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::B]) + "##B")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::B] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("B");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::X]) + "##X")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::X] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("X");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::Y]) + "##Y")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::Y] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Y");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::L]) + "##L")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::L] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("L");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::R]) + "##R")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::R] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("R");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::ZL]) +
             "##ZL")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::ZL] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            Service::SM::ServiceManager& sm = system->ServiceManager();
            std::shared_ptr<Service::IR::IR_USER> ir_user =
                sm.GetService<Service::IR::IR_USER>("ir:USER");
            if (ir_user != nullptr) {
                ir_user->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("ZL");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::ZR]) +
             "##ZR")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::ZR] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            Service::SM::ServiceManager& sm = system->ServiceManager();
            std::shared_ptr<Service::IR::IR_USER> ir_user =
                sm.GetService<Service::IR::IR_USER>("ir:USER");
            if (ir_user != nullptr) {
                ir_user->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("ZR");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::Start]) +
             "##Start")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::Start] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Start");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::Select]) +
             "##Select")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::Select] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Select");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::Debug]) +
             "##Debug")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::Debug] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Debug");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::Gpio14]) +
             "##GPIO14")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::Gpio14] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("GPIO14");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::Home]) +
             "##HOME")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::Home] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("HOME");

    ImGui::NewLine();

    ImGui::TextUnformatted("Circle Pad");
    ImGui::Separator();

    if (ImGui::Button((InputCommon::AnalogToText(
                           Settings::values.analogs[Settings::NativeAnalog::CirclePad], "up") +
                       "##Circle Pad Up")
                          .c_str())) {
        Common::ParamPackage params =
            Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
        if (params.Get("engine", "") != "analog_from_button") {
            Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
                                                         SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
                                                         SDL_SCANCODE_D, 0.5f);
            params =
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
        }
        params.Set("up", GetInput(InputCommon::Polling::DeviceType::Button));
        Settings::values.analogs[Settings::NativeAnalog::CirclePad] = params.Serialize();

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Up");

    if (ImGui::Button((InputCommon::AnalogToText(
                           Settings::values.analogs[Settings::NativeAnalog::CirclePad], "down") +
                       "##Circle Pad Down")
                          .c_str())) {
        Common::ParamPackage params =
            Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
        if (params.Get("engine", "") != "analog_from_button") {
            Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
                                                         SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
                                                         SDL_SCANCODE_D, 0.5f);
            params =
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
        }
        params.Set("down", GetInput(InputCommon::Polling::DeviceType::Button));
        Settings::values.analogs[Settings::NativeAnalog::CirclePad] = params.Serialize();

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Down");

    if (ImGui::Button((InputCommon::AnalogToText(
                           Settings::values.analogs[Settings::NativeAnalog::CirclePad], "left") +
                       "##Circle Pad Left")
                          .c_str())) {
        Common::ParamPackage params =
            Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
        if (params.Get("engine", "") != "analog_from_button") {
            Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
                                                         SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
                                                         SDL_SCANCODE_D, 0.5f);
            params =
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
        }
        params.Set("left", GetInput(InputCommon::Polling::DeviceType::Button));
        Settings::values.analogs[Settings::NativeAnalog::CirclePad] = params.Serialize();

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Left");

    if (ImGui::Button((InputCommon::AnalogToText(
                           Settings::values.analogs[Settings::NativeAnalog::CirclePad], "right") +
                       "##Circle Pad Right")
                          .c_str())) {
        Common::ParamPackage params =
            Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
        if (params.Get("engine", "") != "analog_from_button") {
            Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
                                                         SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
                                                         SDL_SCANCODE_D, 0.5f);
            params =
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
        }
        params.Set("right", GetInput(InputCommon::Polling::DeviceType::Button));
        Settings::values.analogs[Settings::NativeAnalog::CirclePad] = params.Serialize();

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Right");

    if (ImGui::Button(
            (InputCommon::AnalogToText(Settings::values.analogs[Settings::NativeAnalog::CirclePad],
                                       "modifier") +
             "##Circle Pad Modifier")
                .c_str())) {
        Common::ParamPackage params =
            Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
        if (params.Get("engine", "") != "analog_from_button") {
            Settings::values.analogs[Settings::NativeAnalog::CirclePad] =
                InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
                                                         SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
                                                         SDL_SCANCODE_D, 0.5f);
            params =
                Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
        }
        params.Set("modifier", GetInput(InputCommon::Polling::DeviceType::Button));
        Settings::values.analogs[Settings::NativeAnalog::CirclePad] = params.Serialize();

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Modifier");

    {
        Common::ParamPackage params(Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
        if (params.Get("engine", "") == "sdl") {
            float deadzone = params.Get("deadzone", 0.0f);
            ImGui::SliderFloat("Deadzone##Circle Pad", &deadzone, 0.0f, 1.0f);

            if (ImGui::IsItemDeactivatedAfterEdit()) {
                params.Set("deadzone", deadzone);
                Settings::values.analogs[Settings::NativeAnalog::CirclePad] = params.Serialize();

                if (system != nullptr) {
                    std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                    if (hid != nullptr) {
                        hid->ReloadInputDevices();
                    }
                }
            }
        } else if (params.Get("engine", "") == "analog_from_button") {
            float modifier_scale = params.Get("modifier_scale", 0.5f);
            ImGui::InputFloat("Modifier Scale##Circle Pad", &modifier_scale);

            if (ImGui::IsItemDeactivatedAfterEdit()) {
                params.Set("modifier_scale", modifier_scale);
                Settings::values.analogs[Settings::NativeAnalog::CirclePad] = params.Serialize();

                if (system != nullptr) {
                    std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                    if (hid != nullptr) {
                        hid->ReloadInputDevices();
                    }
                }
            }
        }
    }

    ImGui::NewLine();

    ImGui::TextUnformatted("Circle Pad Pro");
    ImGui::Separator();

    if (ImGui::Button((InputCommon::AnalogToText(
                           Settings::values.analogs[Settings::NativeAnalog::CirclePadPro], "up") +
                       "##Circle Pad Pro Up")
                          .c_str())) {
        Common::ParamPackage params =
            Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
        if (params.Get("engine", "") != "analog_from_button") {
            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_I, SDL_SCANCODE_K,
                                                         SDL_SCANCODE_J, SDL_SCANCODE_L,
                                                         SDL_SCANCODE_D, 0.5f);
            params = Common::ParamPackage(
                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
        }
        params.Set("up", GetInput(InputCommon::Polling::DeviceType::Button));
        Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] = params.Serialize();

        if (system != nullptr) {
            Service::SM::ServiceManager& sm = system->ServiceManager();
            std::shared_ptr<Service::IR::IR_USER> ir_user =
                sm.GetService<Service::IR::IR_USER>("ir:USER");
            if (ir_user != nullptr) {
                ir_user->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Up");

    if (ImGui::Button((InputCommon::AnalogToText(
                           Settings::values.analogs[Settings::NativeAnalog::CirclePadPro], "down") +
                       "##Circle Pad Pro Down")
                          .c_str())) {
        Common::ParamPackage params =
            Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
        if (params.Get("engine", "") != "analog_from_button") {
            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_I, SDL_SCANCODE_K,
                                                         SDL_SCANCODE_J, SDL_SCANCODE_L,
                                                         SDL_SCANCODE_D, 0.5f);
            params = Common::ParamPackage(
                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
        }
        params.Set("down", GetInput(InputCommon::Polling::DeviceType::Button));
        Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] = params.Serialize();

        if (system != nullptr) {
            Service::SM::ServiceManager& sm = system->ServiceManager();
            std::shared_ptr<Service::IR::IR_USER> ir_user =
                sm.GetService<Service::IR::IR_USER>("ir:USER");
            if (ir_user != nullptr) {
                ir_user->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Down");

    if (ImGui::Button((InputCommon::AnalogToText(
                           Settings::values.analogs[Settings::NativeAnalog::CirclePadPro], "left") +
                       "##Circle Pad Pro Left")
                          .c_str())) {
        Common::ParamPackage params =
            Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
        if (params.Get("engine", "") != "analog_from_button") {
            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_I, SDL_SCANCODE_K,
                                                         SDL_SCANCODE_J, SDL_SCANCODE_L,
                                                         SDL_SCANCODE_D, 0.5f);
            params = Common::ParamPackage(
                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
        }
        params.Set("left", GetInput(InputCommon::Polling::DeviceType::Button));
        Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] = params.Serialize();

        if (system != nullptr) {
            Service::SM::ServiceManager& sm = system->ServiceManager();
            std::shared_ptr<Service::IR::IR_USER> ir_user =
                sm.GetService<Service::IR::IR_USER>("ir:USER");
            if (ir_user != nullptr) {
                ir_user->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Left");

    if (ImGui::Button(
            (InputCommon::AnalogToText(
                 Settings::values.analogs[Settings::NativeAnalog::CirclePadPro], "right") +
             "##Circle Pad Pro Right")
                .c_str())) {
        Common::ParamPackage params =
            Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
        if (params.Get("engine", "") != "analog_from_button") {
            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_I, SDL_SCANCODE_K,
                                                         SDL_SCANCODE_J, SDL_SCANCODE_L,
                                                         SDL_SCANCODE_D, 0.5f);
            params = Common::ParamPackage(
                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
        }
        params.Set("right", GetInput(InputCommon::Polling::DeviceType::Button));
        Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] = params.Serialize();

        if (system != nullptr) {
            Service::SM::ServiceManager& sm = system->ServiceManager();
            std::shared_ptr<Service::IR::IR_USER> ir_user =
                sm.GetService<Service::IR::IR_USER>("ir:USER");
            if (ir_user != nullptr) {
                ir_user->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Right");

    if (ImGui::Button(
            (InputCommon::AnalogToText(
                 Settings::values.analogs[Settings::NativeAnalog::CirclePadPro], "modifier") +
             "##Circle Pad Pro Modifier")
                .c_str())) {
        Common::ParamPackage params =
            Common::ParamPackage(Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
        if (params.Get("engine", "") != "analog_from_button") {
            Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] =
                InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_I, SDL_SCANCODE_K,
                                                         SDL_SCANCODE_J, SDL_SCANCODE_L,
                                                         SDL_SCANCODE_D, 0.5f);
            params = Common::ParamPackage(
                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
        }
        params.Set("modifier", GetInput(InputCommon::Polling::DeviceType::Button));
        Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] = params.Serialize();

        if (system != nullptr) {
            Service::SM::ServiceManager& sm = system->ServiceManager();
            std::shared_ptr<Service::IR::IR_USER> ir_user =
                sm.GetService<Service::IR::IR_USER>("ir:USER");
            if (ir_user != nullptr) {
                ir_user->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Modifier");

    {
        Common::ParamPackage params(Settings::values.analogs[Settings::NativeAnalog::CirclePadPro]);
        if (params.Get("engine", "") == "sdl") {
            float deadzone = params.Get("deadzone", 0.0f);
            ImGui::SliderFloat("Deadzone##Circle Pad Pro", &deadzone, 0.0f, 1.0f);

            if (ImGui::IsItemDeactivatedAfterEdit()) {
                params.Set("deadzone", deadzone);
                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] = params.Serialize();

                if (system != nullptr) {
                    Service::SM::ServiceManager& sm = system->ServiceManager();
                    std::shared_ptr<Service::IR::IR_USER> ir_user =
                        sm.GetService<Service::IR::IR_USER>("ir:USER");
                    if (ir_user != nullptr) {
                        ir_user->ReloadInputDevices();
                    }
                }
            }
        } else if (params.Get("engine", "") == "analog_from_button") {
            float modifier_scale = params.Get("modifier_scale", 0.5f);
            ImGui::InputFloat("Modifier Scale##Circle Pad Pro", &modifier_scale);

            if (ImGui::IsItemDeactivatedAfterEdit()) {
                params.Set("modifier_scale", modifier_scale);
                Settings::values.analogs[Settings::NativeAnalog::CirclePadPro] = params.Serialize();

                if (system != nullptr) {
                    Service::SM::ServiceManager& sm = system->ServiceManager();
                    std::shared_ptr<Service::IR::IR_USER> ir_user =
                        sm.GetService<Service::IR::IR_USER>("ir:USER");
                    if (ir_user != nullptr) {
                        ir_user->ReloadInputDevices();
                    }
                }
            }
        }
    }

    ImGui::NewLine();

    ImGui::TextUnformatted("D-Pad");
    ImGui::Separator();

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::Up]) +
             "##D-Pad Up")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::Up] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Up");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::Down]) +
             "##D-Pad Down")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::Down] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Down");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::Left]) +
             "##D-Pad Left")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::Left] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Left");

    if (ImGui::Button(
            (InputCommon::ButtonToText(Settings::values.buttons[Settings::NativeButton::Right]) +
             "##D-Pad Right")
                .c_str())) {
        Settings::values.buttons[Settings::NativeButton::Right] =
            GetInput(InputCommon::Polling::DeviceType::Button);

        if (system != nullptr) {
            std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
            if (hid != nullptr) {
                hid->ReloadInputDevices();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Right");

    ImGui::NewLine();

    ImGui::TextUnformatted("Motion");
    ImGui::Separator();

    if (ImGui::BeginCombo("Device##Motion", [] {
            const std::string engine =
                Common::ParamPackage(Settings::values.motion_device).Get("engine", "");

            if (engine == "motion_emu") {
                return "Right Click";
            } else if (engine == "cemuhookudp") {
                return "CemuhookUDP";
            }

            return "Invalid";
        }())) {
        if (ImGui::Selectable("Right Click")) {
            Settings::values.motion_device = "engine:motion_emu";

            if (system != nullptr) {
                std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                if (hid != nullptr) {
                    hid->ReloadInputDevices();
                }
            }
        }

        if (ImGui::Selectable("CemuhookUDP")) {
            Settings::values.motion_device = "engine:cemuhookudp";

            if (system != nullptr) {
                std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                if (hid != nullptr) {
                    hid->ReloadInputDevices();
                }
            }
        }

        ImGui::EndCombo();
    }

    Common::ParamPackage motion_device(Settings::values.motion_device);

    if (motion_device.Get("engine", "") == "motion_emu") {
        int update_period = motion_device.Get("update_period", 100);
        float sensitivity = motion_device.Get("sensitivity", 0.01f);
        float clamp = motion_device.Get("tilt_clamp", 90.0f);

        ImGui::InputInt("Update Period##Motion", &update_period, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            motion_device.Set("update_period", update_period);
            Settings::values.motion_device = motion_device.Serialize();

            if (system != nullptr) {
                std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                if (hid != nullptr) {
                    hid->ReloadInputDevices();
                }
            }
        }

        ImGui::InputFloat("Sensitivity##Motion", &sensitivity);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            motion_device.Set("sensitivity", sensitivity);
            Settings::values.motion_device = motion_device.Serialize();

            if (system != nullptr) {
                std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                if (hid != nullptr) {
                    hid->ReloadInputDevices();
                }
            }
        }

        ImGui::InputFloat("Clamp##Motion", &clamp);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            motion_device.Set("tilt_clamp", clamp);
            Settings::values.motion_device = motion_device.Serialize();

            if (system != nullptr) {
                std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                if (hid != nullptr) {
                    hid->ReloadInputDevices();
                }
            }
        }
    }

    ImGui::NewLine();

    ImGui::TextUnformatted("Touch");
    ImGui::Separator();

    if (ImGui::BeginCombo("Device##Touch", [] {
            const std::string engine =
                Common::ParamPackage(Settings::values.touch_device).Get("engine", "");

            if (engine == "emu_window") {
                return "Mouse";
            } else if (engine == "cemuhookudp") {
                return "CemuhookUDP";
            }

            return "Invalid";
        }())) {
        if (ImGui::Selectable("Mouse")) {
            Settings::values.touch_device = "engine:emu_window";

            if (system != nullptr) {
                std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                if (hid != nullptr) {
                    hid->ReloadInputDevices();
                }
            }
        }

        if (ImGui::Selectable("CemuhookUDP")) {
            Settings::values.touch_device = "engine:cemuhookudp";

            if (system != nullptr) {
                std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                if (hid != nullptr) {
                    hid->ReloadInputDevices();
                }
            }
        }

        ImGui::EndCombo();
    }

    Common::ParamPackage touch_device(Settings::values.touch_device);

    if (touch_device.Get("engine", "") == "cemuhookudp") {
        int min_x = touch_device.Get("min_x", 100);
        int min_y = touch_device.Get("min_y", 50);
        int max_x = touch_device.Get("max_x", 1800);
        int max_y = touch_device.Get("max_y", 850);

        ImGui::InputInt("Minimum X##Touch", &min_x, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            touch_device.Set("min_x", min_x);
            Settings::values.touch_device = touch_device.Serialize();

            if (system != nullptr) {
                std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                if (hid != nullptr) {
                    hid->ReloadInputDevices();
                }
            }
        }

        ImGui::InputInt("Minimum Y##Touch", &min_y, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            touch_device.Set("min_y", min_y);
            Settings::values.touch_device = touch_device.Serialize();

            if (system != nullptr) {
                std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                if (hid != nullptr) {
                    hid->ReloadInputDevices();
                }
            }
        }

        ImGui::InputInt("Maximum X##Touch", &max_x, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            touch_device.Set("max_x", max_x);
            Settings::values.touch_device = touch_device.Serialize();

            if (system != nullptr) {
                std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                if (hid != nullptr) {
                    hid->ReloadInputDevices();
                }
            }
        }

        ImGui::InputInt("Maximum Y##Touch", &max_y, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            touch_device.Set("max_y", max_y);
            Settings::values.touch_device = touch_device.Serialize();

            if (system != nullptr) {
                std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(*system);
                if (hid != nullptr) {
                    hid->ReloadInputDevices();
                }
            }
        }
    }

    if (motion_device.Get("engine", "") == "cemuhookudp" ||
        touch_device.Get("engine", "") == "cemuhookudp") {
        ImGui::NewLine();

        ImGui::TextUnformatted("CemuhookUDP");
        ImGui::Separator();

        ImGui::InputText("Address##CemuhookUDP", &Settings::values.cemuhookudp_address);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            InputCommon::ReloadInputDevices();
        }

        ImGui::InputScalar("Port##CemuhookUDP", ImGuiDataType_U16,
                           &Settings::values.cemuhookudp_port);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            InputCommon::ReloadInputDevices();
        }

        ImGui::InputScalar("Pad##CemuhookUDP", ImGuiDataType_U8,
                           &Settings::values.cemuhookudp_pad_index);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            InputCommon::ReloadInputDevices();
        }
    }
}
