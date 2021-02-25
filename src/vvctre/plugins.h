// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <flags.h>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"
#include "core/frontend/input.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Core {
class System;
} // namespace Core

struct SDL_Window;

struct Plugin {
    std::string name;
    void (*initial_settings_opening)() = nullptr;
    void (*initial_settings_ok_pressed)() = nullptr;
    void (*before_loading)() = nullptr;
    void (*before_loading_after_first_time)() = nullptr;
    void (*emulation_starting)() = nullptr;
    void (*emulation_starting_after_first_time)() = nullptr;
    void (*emulator_closing)() = nullptr;
    void (*fatal_error)() = nullptr;
    void (*before_drawing_fps)() = nullptr;
    void (*add_menu)() = nullptr;
    void (*add_tab)() = nullptr;
    void (*after_swap_window)() = nullptr;
    void (*screenshot_callback)(void* data) = nullptr;

#ifdef _WIN32
    HMODULE handle;
#else
    void* handle;
#endif
};

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
    void CallScreenshotCallbacks(void* data);
    void* NewButtonDevice(const char* params);
    void DeleteButtonDevice(void* device);

    bool paused = false;
    SDL_Window* window = nullptr;
    void* cfg = nullptr;
    bool show_fatal_error_messages = true;
    bool built_in_logger_enabled = true;
    const flags::args& args;
    std::vector<Plugin> plugins;

private:
    std::vector<std::unique_ptr<Input::ButtonDevice>> buttons;
    static std::unordered_map<std::string, void*> function_map;
};
