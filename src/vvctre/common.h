// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <tuple>
#include <vector>
#include "common/common_types.h"

class PluginManager;
struct ImGuiIO;

namespace Core {
class System;
} // namespace Core

extern const u8 vvctre_version_major;
extern const u8 vvctre_version_minor;
extern const u8 vvctre_version_patch;

struct CitraRoom {
    std::string name;
    std::string description;
    std::string owner;
    std::string ip;
    u16 port;
    u32 max_players;
    bool has_password;
    std::string game;

    struct Member {
        std::string nickname;
        std::string game;
    };

    std::vector<Member> members;
};

using CitraRoomList = std::vector<CitraRoom>;

void vvctreShutdown(PluginManager* plugin_manager);

std::vector<std::tuple<std::string, std::string>> GetInstalledList();
CitraRoomList GetPublicCitraRooms();
const std::string GetRoomPopupText(const CitraRoom& room);

bool GUI_CameraAddBrowse(const char* label, std::size_t index);
void GUI_AddControlsSettings(bool& is_open, Core::System* system, PluginManager& plugin_manager,
                             ImGuiIO& io);
