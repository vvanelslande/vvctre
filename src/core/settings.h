// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>
#include <SDL.h>
#include "common/common_types.h"
#include "core/hle/service/cam/cam.h"
#include "input_common/main.h"
#include "input_common/udp/client.h"
#include "network/common.h"

namespace Settings {

enum class InitialClock {
    SystemTime = 0,
    FixedTime = 1,
};

enum class Layout {
    Default,
    SingleScreen,
    LargeScreen,
    SideScreen,
    MediumScreen,
};

enum class MicrophoneInputType {
    None,
    Real,
    Static,
};

enum class StereoRenderOption {
    Off,
    SideBySide,
    Anaglyph,
    Interlaced,
};

namespace NativeButton {

enum Values {
    A,
    B,
    X,
    Y,
    Up,
    Down,
    Left,
    Right,
    L,
    R,
    Start,
    Select,
    Debug,
    Gpio14,

    ZL,
    ZR,

    Home,

    NumButtons,
};

constexpr int BUTTON_HID_BEGIN = A;
constexpr int BUTTON_IR_BEGIN = ZL;
constexpr int BUTTON_NS_BEGIN = Home;

constexpr int BUTTON_HID_END = BUTTON_IR_BEGIN;
constexpr int BUTTON_IR_END = BUTTON_NS_BEGIN;
constexpr int BUTTON_NS_END = NumButtons;

constexpr int NUM_BUTTONS_HID = BUTTON_HID_END - BUTTON_HID_BEGIN;
constexpr int NUM_BUTTONS_IR = BUTTON_IR_END - BUTTON_IR_BEGIN;
constexpr int NUM_BUTTONS_NS = BUTTON_NS_END - BUTTON_NS_BEGIN;

} // namespace NativeButton

namespace NativeAnalog {

enum Values {
    CirclePad,
    CirclePadPro,

    NumAnalogs,
};

} // namespace NativeAnalog

// A special value for Values::region_value indicating that vvctre will automatically select a
// region value to fit the region lockout info of the game
static constexpr int REGION_VALUE_AUTO_SELECT = -1;

struct Values {
    // Start
    std::string file_path;
    std::string play_movie;
    std::string record_movie;
    int region_value = REGION_VALUE_AUTO_SELECT;
    std::string log_filter = "*:Info";
    InitialClock initial_clock = InitialClock::SystemTime;
    u64 unix_timestamp = 0;
    bool use_virtual_sd = true;
    bool start_in_fullscreen_mode = false;
    bool record_frame_times = false;
    bool use_gdbstub = false;
    u16 gdbstub_port = 24689;

    // General
    bool use_cpu_jit = true;
    bool limit_speed = true;
    u16 speed_limit = 100;
    bool use_custom_cpu_ticks = false;
    u64 custom_cpu_ticks = 77;
    u32 cpu_clock_percentage = 100;

    // Audio
    bool enable_dsp_lle = false;
    bool enable_dsp_lle_multithread = false;
    float audio_volume = 1.0f;
    std::string audio_sink_id = "auto";
    std::string audio_device_id = "auto";
    MicrophoneInputType microphone_input_type = MicrophoneInputType::None;
    std::string microphone_device;

    // Camera
    std::array<std::string, Service::CAM::NumCameras> camera_engine{
        "blank",
        "blank",
        "blank",
    };
    std::array<std::string, Service::CAM::NumCameras> camera_parameter;
    std::array<Service::CAM::Flip, Service::CAM::NumCameras> camera_flip;

    // Graphics
    bool use_hardware_renderer = true;
    bool use_hardware_shader = true;
    bool hardware_shader_accurate_multiplication = false;
    bool use_shader_jit = true;
    bool enable_vsync = false;
    bool dump_textures = false;
    bool custom_textures = false;
    bool preload_textures = false;
    bool enable_linear_filtering = true;
    bool sharper_distant_objects = false;
    u16 resolution = 1;
    float background_color_red = 0.0f;
    float background_color_green = 0.0f;
    float background_color_blue = 0.0f;
    std::string post_processing_shader = "none (builtin)";
    std::string texture_filter = "none";
    StereoRenderOption render_3d = StereoRenderOption::Off;
    std::atomic<u8> factor_3d{0};

    // Controls
    std::array<std::string, NativeButton::NumButtons> buttons = {
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_A),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_S),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_Z),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_X),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_T),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_G),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_F),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_H),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_Q),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_W),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_M),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_N),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_0),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_P),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_1),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_2),
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_B),
    };

    std::array<std::string, NativeAnalog::NumAnalogs> analogs = {
        InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
                                                 SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
                                                 SDL_SCANCODE_D, 0.5f),
        InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J,
                                                 SDL_SCANCODE_L, SDL_SCANCODE_D, 0.5f),
    };

    std::string motion_device =
        "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0";
    std::string touch_device = "engine:emu_window";
    std::string cemuhookudp_address = InputCommon::CemuhookUDP::DEFAULT_ADDR;
    u16 cemuhookudp_port = InputCommon::CemuhookUDP::DEFAULT_PORT;
    u8 cemuhookudp_pad_index = 0;

    // Layout
    Layout layout = Layout::Default;
    bool swap_screens = false;
    bool upright_screens = false;
    bool use_custom_layout = false;
    u16 custom_layout_top_left = 0;
    u16 custom_layout_top_top = 0;
    u16 custom_layout_top_right = 400;
    u16 custom_layout_top_bottom = 240;
    u16 custom_layout_bottom_left = 40;
    u16 custom_layout_bottom_top = 240;
    u16 custom_layout_bottom_right = 360;
    u16 custom_layout_bottom_bottom = 480;

    // LLE Modules
    std::unordered_map<std::string, bool> lle_modules = {
        {"FS", false},
        {"PM", false},
        {"LDR", false},
        {"PXI", false},
        {"ERR", false},
        {"AC", false},
        {"ACT", false},
        {"AM", false},
        {"BOSS", false},
        {"CAM", false},
        {"CECD", false},
        {"CFG", false},
        {"DLP", false},
        {"DSP", false},
        {"FRD", false},
        {"GSP", false},
        {"HID", false},
        {"IR", false},
        {"MIC", false},
        {"NDM", false},
        {"NEWS", false},
        {"NFC", false},
        {"NIM", false},
        {"NS", false},
        {"NWM", false},
        {"PTM", false},
        {"CSND", false},
        {"HTTP", false},
        {"SOC", false},
        {"SSL", false},
        {"PS", false},
        {"MCU", false},
        // No HLE implementation
        {"CDC", false},
        {"GPIO", false},
        {"I2C", false},
        {"MP", false},
        {"PDN", false},
        {"SPI", false},
    };

    // Hacks
    bool enable_priority_boost = true;

    // Multiplayer
    std::string multiplayer_ip = "127.0.0.1";
    u16 multiplayer_port = Network::DEFAULT_PORT;
    std::string multiplayer_nickname = "vvctre";
    std::string multiplayer_password;
} extern values;

void Apply();

} // namespace Settings
