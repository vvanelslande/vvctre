// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>
#include <imgui.h>
#include "core/frontend/applets/mii_selector.h"
#include "core/frontend/applets/swkbd.h"
#include "core/frontend/emu_window.h"
#include "core/hle/kernel/ipc_debugger/recorder.h"
#include "vvctre/common.h"

class PluginManager;
struct SDL_Window;

namespace Core {
class System;
} // namespace Core

class EmuWindow_SDL2 : public Frontend::EmuWindow {
public:
    explicit EmuWindow_SDL2(Core::System& system, PluginManager& plugin_manager, SDL_Window* window,
                            bool& ok_multiplayer);
    ~EmuWindow_SDL2();

    /// Swap buffers to display the next frame
    void SwapBuffers() override;

    /// Polls window events
    void PollEvents() override;

    /// Whether the window is still open, and a close request hasn't yet been sent
    bool IsOpen() const;

    void Close();

    struct KeyboardData {
        const Frontend::KeyboardConfig& config;
        u8 code;
        std::string text;
    };
    KeyboardData* keyboard_data = nullptr;

    struct MiiSelectorData {
        const Frontend::MiiSelectorConfig& config;
        const std::vector<HLE::Applets::MiiData>& miis;
        u32 code;
        HLE::Applets::MiiData selected_mii;
    };
    MiiSelectorData* mii_selector_data = nullptr;

    bool paused = false;

private:
    /// Called by PollEvents when a key is pressed or released.
    void OnKeyEvent(int key, u8 state);

    /// Called by PollEvents when the mouse moves.
    void OnMouseMotion(s32 x, s32 y);

    /// Called by PollEvents when a mouse button is pressed or released
    void OnMouseButton(u32 button, u8 state, s32 x, s32 y);

    /// Translates pixel position (0..1) to pixel positions
    std::pair<unsigned, unsigned> TouchToPixelPos(float touch_x, float touch_y) const;

    /// Called by PollEvents when a finger starts touching the touchscreen
    void OnFingerDown(float x, float y);

    /// Called by PollEvents when a finger moves while touching the touchscreen
    void OnFingerMotion(float x, float y);

    /// Called by PollEvents when a finger stops touching the touchscreen
    void OnFingerUp();

    /// Called by PollEvents when any event that may cause the window to be resized occurs
    void OnResize();

    /// Called when Tools -> Copy Screenshot is clicked
    void CopyScreenshot();

    void ConnectToCitraRoom();

    std::function<void()> play_movie_loop_callback;
    bool request_reset = false;
    bool menu_open = false;

    // Window
    SDL_Window* window = nullptr;

    // System
    Core::System& system;
    bool config_savegame_changed = false;

    // FPS color
    // Default: Green
    ImVec4 fps_color{0.0f, 1.0f, 0.0f, 1.0f};

    // IPC recorder
    IPCDebugger::CallbackHandle ipc_recorder_callback;
    std::vector<IPCDebugger::RequestRecord> all_ipc_records;
    std::vector<IPCDebugger::RequestRecord> ipc_recorder_search_results;
    int ipc_recorder_id_offset = 1;
    std::string ipc_recorder_search_text;
    std::string ipc_recorder_search_text_;
    bool show_ipc_recorder_window = false;

    // Installed
    std::vector<std::tuple<std::string, std::string>> all_installed;
    std::vector<std::tuple<std::string, std::string>> installed_search_results;
    std::string installed_search_text;
    std::string installed_search_text_;

    // Amiibo
    std::vector<std::tuple<u64, std::string>> amiibo_generate_and_load_search_results;
    std::string amiibo_generate_and_load_search_text;
    std::string amiibo_generate_and_load_search_text_;
    std::string amiibo_generate_and_load_custom_id;

    // Cheats
    bool show_cheats_window = false;
    bool show_cheats_text_editor = false;
    std::string cheats_text_editor_text;

    // Play coins
    u16 play_coins = 0;
    bool play_coins_changed = false;

    // Multiplayer
    bool show_connect_to_citra_room = false;
    CitraRoomList all_public_rooms;
    CitraRoomList public_rooms_search_results;
    std::string public_rooms_search_text;
    std::string public_rooms_search_text_;
    std::string multiplayer_message;
    std::vector<std::string> multiplayer_messages;
    std::unordered_set<std::string> multiplayer_blocked_nicknames;

    // Plugins
    PluginManager& plugin_manager;
};
