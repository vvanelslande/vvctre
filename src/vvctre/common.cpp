
// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <asl/Http.h>
#include <asl/Process.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <portable-file-dialogs.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/loader/loader.h"
#include "core/loader/smdh.h"
#include "core/movie.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "vvctre/common.h"
#include "vvctre/plugins.h"

const u8 vvctre_version_major = 36;
const u8 vvctre_version_minor = 5;
const u8 vvctre_version_patch = 1;

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
                std::string tid_string = tid_high.virtualName + tid_low.virtualName;

                if (tid_string.length() == Service::AM::TITLE_ID_VALID_LENGTH) {
                    const u64 tid = std::stoull(tid_string, nullptr, 16);

                    const std::string path = Service::AM::GetTitleContentPath(media_type, tid);
                    std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(path);
                    if (loader != nullptr) {
                        std::string title;
                        loader->ReadTitle(title);

                        switch (std::stoull(tid_high.virtualName, nullptr, 16)) {
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
    asl::HttpResponse r = asl::Http::get("https://api.citra-emu.org/lobby");

    if (!r.ok()) {
        return {};
    }

    asl::Var json_rooms = r.json()("rooms");

    CitraRoomList rooms;

    for (int i = 0; i < json_rooms.length(); ++i) {
        asl::Var json_room = json_rooms[i];

        CitraRoom room;
        room.name = *json_room("name");
        if (json_room.has("description")) {
            room.description = *json_room("description");
        }
        room.ip = *json_room("address");
        room.port = static_cast<u16>(static_cast<int>(json_room("port")));
        room.owner = *json_room("owner");
        room.max_players = json_room("maxPlayers");
        room.has_password = json_room("hasPassword");
        room.game = *json_room("preferredGameName");
        asl::Array<asl::Var> members = json_room("players").array();
        foreach (asl::Var& json_member, members) {
            CitraRoom::Member member;
            member.nickname = *json_member("nickname");
            member.game = *json_member("gameName");
            room.members.push_back(member);
        }
        rooms.push_back(std::move(room));
    }

    return rooms;
}

bool GUI_CameraAddBrowse(const char* label, std::size_t index) {
    if (ImGui::Button(label)) {
        const std::vector<std::string> result =
            pfd::open_file("Browse", *asl::Process::myDir(),
                           {"All supported files",
                            "*.jpg *.JPG *.jpeg *.JPEG *.jfif *.JFIF *.png *.PNG "
                            "*.bmp "
                            "*.BMP *.psd *.PSD *.tga *.TGA *.gif *.GIF "
                            "*.hdr *.HDR *.pic *.PIC *.ppm *.PPM *.pgm *.PGM",
                            "JPEG", "*.jpg *.JPG *.jpeg *.JPEG *.jfif *.JFIF", "PNG", "*.png *.PNG",
                            "BMP",
                            "*.bmp *.BMP"
                            "PSD",
                            "*.psd *.PSD"
                            "TGA",
                            "*.tga *.TGA"
                            "GIF",
                            "*.gif *.GIF"
                            "HDR",
                            "*.hdr *.HDR"
                            "PIC",
                            "*.pic *.PIC"
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
