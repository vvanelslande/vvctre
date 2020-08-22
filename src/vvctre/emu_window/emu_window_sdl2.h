// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <unordered_set>
#include <utility>
#include <imgui.h>
#include "core/frontend/applets/mii_selector.h"
#include "core/frontend/applets/swkbd.h"
#include "core/frontend/emu_window.h"
#include "core/hle/kernel/ipc_debugger/recorder.h"
#include "network/room_member.h"
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
    std::map<int, IPCDebugger::RequestRecord> ipc_records;
    std::string ipc_recorder_filter;
    bool show_ipc_recorder_window = false;

    // Installed
    std::string installed_query;
    std::vector<std::tuple<std::string, std::string>> installed;

    // Cheats
    bool show_cheats_window = false;
    bool show_cheats_text_editor = false;
    std::string cheats_file_content;

    // Play coins
    u16 play_coins = 0;
    bool play_coins_changed = false;

    // Multiplayer
    bool show_connect_to_citra_room = false;
    CitraRoomList public_rooms;
    std::string public_rooms_query;
    std::string multiplayer_message;
    std::deque<std::string> multiplayer_messages;
    Network::RoomMember::CallbackHandle<Network::RoomMember::Error> multiplayer_on_error;
    Network::RoomMember::CallbackHandle<Network::ChatEntry> multiplayer_on_chat_message;
    Network::RoomMember::CallbackHandle<Network::StatusMessageEntry> multiplayer_on_status_message;
    std::unordered_set<std::string> multiplayer_blocked_nicknames;

    // Plugins
    PluginManager& plugin_manager;
};
