// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "common/logging/log.h"
#include "core/settings.h"
#include "video_core/pica.h"
#include "video_core/renderer/renderer.h"
#include "video_core/video_core.h"

namespace VideoCore {

std::unique_ptr<OpenGL::Renderer> g_renderer;
std::atomic<bool> g_hardware_shader_enabled;
std::atomic<bool> g_hardware_shader_accurate_multiplication;
std::atomic<bool> g_renderer_background_color_update_requested;
std::atomic<bool> g_renderer_sampler_update_requested;
std::atomic<bool> g_renderer_shader_update_requested;
std::atomic<bool> g_texture_filter_update_requested;
std::atomic<bool> g_renderer_screenshot_requested;
void* g_screenshot_bits;
std::function<void()> g_screenshot_complete_callback;
Layout::FramebufferLayout g_screenshot_framebuffer_layout;
Memory::MemorySystem* g_memory;

void Init(Frontend::EmuWindow& emu_window, Memory::MemorySystem& memory) {
    g_memory = &memory;
    Pica::Init();
    g_renderer = std::make_unique<OpenGL::Renderer>(emu_window);
}

void Shutdown() {
    Pica::Shutdown();
    g_renderer.reset();
}

bool RequestScreenshot(void* data, std::function<void()> callback,
                       const Layout::FramebufferLayout& layout) {
    if (g_renderer_screenshot_requested) {
        return true;
    }

    g_screenshot_bits = data;
    g_screenshot_complete_callback = std::move(callback);
    g_screenshot_framebuffer_layout = layout;
    g_renderer_screenshot_requested = true;

    return false;
}

u16 GetResolutionScaleFactor() {
    return Settings::values.resolution
               ? Settings::values.resolution
               : g_renderer->GetRenderWindow().GetFramebufferLayout().GetScalingRatio();
}

} // namespace VideoCore
