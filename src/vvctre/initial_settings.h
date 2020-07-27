// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>

class PluginManager;
struct SDL_Window;

namespace Service::CFG {
class Module;
} // namespace Service::CFG

class InitialSettings {
public:
    explicit InitialSettings(PluginManager& plugin_manager, SDL_Window* window,
                             Service::CFG::Module& cfg, std::atomic<bool>& update_found,
                             bool& ok_multiplayer);
};
