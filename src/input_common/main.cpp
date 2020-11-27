// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <SDL.h>
#include <memory>
#include <thread>
#include "common/param_package.h"
#include "input_common/analog_from_button.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_emu.h"
#include "input_common/sdl/sdl.h"
#include "input_common/udp/udp.h"

namespace InputCommon {

static std::shared_ptr<Keyboard> keyboard;
static std::shared_ptr<MotionEmu> motion_emu;
static std::unique_ptr<CemuhookUDP::State> udp;
static std::unique_ptr<SDL::State> sdl;

void Init() {
    keyboard = std::make_shared<Keyboard>();
    Input::RegisterFactory<Input::ButtonDevice>("keyboard", keyboard);
    Input::RegisterFactory<Input::AnalogDevice>("analog_from_button",
                                                std::make_shared<AnalogFromButton>());
    motion_emu = std::make_shared<MotionEmu>();
    Input::RegisterFactory<Input::MotionDevice>("motion_emu", motion_emu);

    sdl = SDL::Init();
    udp = std::make_unique<CemuhookUDP::State>();
}

void Shutdown() {
    Input::UnregisterFactory<Input::ButtonDevice>("keyboard");
    keyboard.reset();
    Input::UnregisterFactory<Input::AnalogDevice>("analog_from_button");
    Input::UnregisterFactory<Input::MotionDevice>("motion_emu");
    motion_emu.reset();
    sdl.reset();
    udp.reset();
}

Keyboard* GetKeyboard() {
    return keyboard.get();
}

MotionEmu* GetMotionEmu() {
    return motion_emu.get();
}

std::string GenerateKeyboardParam(int key_code) {
    Common::ParamPackage param{
        {"engine", "keyboard"},
        {"code", std::to_string(key_code)},
    };
    return param.Serialize();
}

std::string GenerateAnalogParamFromKeys(int key_up, int key_down, int key_left, int key_right,
                                        int key_modifier, float modifier_scale) {
    Common::ParamPackage circle_pad_param{
        {"engine", "analog_from_button"},
        {"up", GenerateKeyboardParam(key_up)},
        {"down", GenerateKeyboardParam(key_down)},
        {"left", GenerateKeyboardParam(key_left)},
        {"right", GenerateKeyboardParam(key_right)},
        {"modifier", GenerateKeyboardParam(key_modifier)},
        {"modifier_scale", std::to_string(modifier_scale)},
    };
    return circle_pad_param.Serialize();
}

void ReloadInputDevices() {
    if (udp) {
        udp->ReloadUDPClient();
    }
}

std::string ButtonToText(const std::string& params_string) {
    const Common::ParamPackage params(params_string);

    if (params.Get("engine", "") == "keyboard") {
        return std::string(
            SDL_GetScancodeName(static_cast<SDL_Scancode>(params.Get("code", SDL_SCANCODE_A))));
    }

    if (params.Get("engine", "") == "sdl") {
        if (params.Has("hat")) {
            return fmt::format("Hat {} {}", params.Get("hat", ""), params.Get("direction", ""));
        }

        if (params.Has("axis")) {
            return fmt::format("Axis {}{}", params.Get("axis", ""), params.Get("direction", ""));
        }

        if (params.Has("button")) {
            return fmt::format("Button {}", params.Get("button", ""));
        }
    }

    return std::string("[unknown]");
}

std::string AnalogToText(const std::string& params_string, const std::string& dir) {
    const Common::ParamPackage params(params_string);

    if (params.Get("engine", "") == "analog_from_button") {
        return ButtonToText(params.Get(dir, ""));
    }

    if (params.Get("engine", "") == "sdl") {
        if (dir == "modifier") {
            return std::string("[unused]");
        }

        if (dir == "left" || dir == "right") {
            return fmt::format("Axis {}", params.Get("axis_x", ""));
        }

        if (dir == "up" || dir == "down") {
            return fmt::format("Axis {}", params.Get("axis_y", ""));
        }
    }

    return std::string("[unknown]");
}

namespace Polling {

std::vector<std::unique_ptr<DevicePoller>> GetPollers(DeviceType type) {
    std::vector<std::unique_ptr<DevicePoller>> pollers = sdl->GetPollers(type);
    return pollers;
}

} // namespace Polling
} // namespace InputCommon
