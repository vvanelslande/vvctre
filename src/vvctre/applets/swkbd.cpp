// Copyright 2020 vvctre emulator project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <SDL.h>
#include <portable-file-dialogs.h>
#include "core/settings.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "vvctre/applets/swkbd.h"
#include "vvctre/emu_window/emu_window_sdl2.h"

namespace Frontend {

SDL2_SoftwareKeyboard::SDL2_SoftwareKeyboard(EmuWindow_SDL2& emu_window) : emu_window(emu_window) {}

void SDL2_SoftwareKeyboard::Execute(const KeyboardConfig& config) {
    SoftwareKeyboard::Execute(config);

    EmuWindow_SDL2::KeyboardData data{config, 0, ""};
    emu_window.keyboard_data = &data;

    SDL_GL_SetSwapInterval(1);

    while (emu_window.IsOpen() && emu_window.keyboard_data != nullptr) {
        VideoCore::g_renderer->SwapBuffers();
    }

    SDL_GL_SetSwapInterval(Settings::values.enable_vsync ? 1 : 0);
    Finalize(data.text, data.code);
}

void SDL2_SoftwareKeyboard::ShowError(const std::string& error) {
    pfd::message("vvctre", error, pfd::choice::ok, pfd::icon::error);
}

} // namespace Frontend
