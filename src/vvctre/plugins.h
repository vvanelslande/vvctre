// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <flags.h>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"
#include "core/frontend/input.h"

namespace Core {
class System;
} // namespace Core

struct SDL_Window;

class PluginManager {
public:
    explicit PluginManager(Core::System& core, SDL_Window* window, const flags::args& args);
    ~PluginManager();

    void InitialSettingsOpening();
    void InitialSettingsOkPressed();
    void BeforeLoading();
    void BeforeLoadingAfterFirstTime();
    void EmulationStarting();
    void EmulationStartingAfterFirstTime();
    void EmulatorClosing();
    void FatalError();
    void BeforeDrawingFPS();
    void AddMenus();
    void AddTabs();
    void AfterSwapWindow();
    void* NewButtonDevice(const char* params);
    void DeleteButtonDevice(void* device);
    void CallScreenshotCallbacks(void* data);

    bool paused = false;
    SDL_Window* window = nullptr;
    void* cfg = nullptr;
    bool show_fatal_error_messages = true;
    bool built_in_logger_enabled = true;

private:
    struct Plugin {
        void* handle = nullptr;
        void (*before_drawing_fps)() = nullptr;
        void (*add_menu)() = nullptr;
        void (*add_tab)() = nullptr;
        void (*after_swap_window)() = nullptr;
        void (*screenshot_callback)(void* data) = nullptr;
    };

    std::vector<Plugin> plugins;
    std::vector<std::unique_ptr<Input::ButtonDevice>> buttons;
    static std::unordered_map<std::string, void*> function_map;
};
