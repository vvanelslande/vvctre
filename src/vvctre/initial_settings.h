// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <tuple>
#include <vector>
#include "common/common_types.h"
#include "network/common.h"
#include "vvctre/common.h"

class PluginManager;
struct SDL_Window;

namespace Service::CFG {
class Module;
} // namespace Service::CFG

class InitialSettings {
public:
    explicit InitialSettings(PluginManager& plugin_manager, SDL_Window* window,
                             Service::CFG::Module& cfg);

private:
    // System
    bool update_config_savegame = false;

    // Installed
    std::vector<std::tuple<std::string, std::string>> installed;
    std::string installed_query;

    // Host Multiplayer Room
    std::string host_multiplayer_room_ip = "0.0.0.0";
    u16 host_multiplayer_room_port = Network::DEFAULT_PORT;
    u32 host_multiplayer_room_member_slots = Network::DEFAULT_MEMBER_SLOTS;
    bool host_multiplayer_room_room_created = false;
};
