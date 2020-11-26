// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include "core/frontend/emu_window.h"

namespace Frontend {
class EmuWindow;
} // namespace Frontend

class RendererBase;

namespace Memory {
class MemorySystem;
} // namespace Memory

namespace VideoCore {

extern std::unique_ptr<RendererBase> g_renderer;
extern std::atomic<bool> g_hardware_renderer_enabled;
extern std::atomic<bool> g_shader_jit_enabled;
extern std::atomic<bool> g_hardware_shader_enabled;
extern std::atomic<bool> g_hardware_shader_accurate_multiplication;
extern std::atomic<bool> g_renderer_background_color_update_requested;
extern std::atomic<bool> g_renderer_sampler_update_requested;
extern std::atomic<bool> g_renderer_shader_update_requested;
extern std::atomic<bool> g_texture_filter_update_requested;
extern std::atomic<bool> g_renderer_screenshot_requested;
extern void* g_screenshot_bits;
extern std::function<void()> g_screenshot_complete_callback;
extern Layout::FramebufferLayout g_screenshot_framebuffer_layout;
extern Memory::MemorySystem* g_memory;

void Init(Frontend::EmuWindow& emu_window, Memory::MemorySystem& memory);

void Shutdown();

/// Request a screenshot of the next frame
/// Returns whether another screenshot is pending
bool RequestScreenshot(void* data, std::function<void()> callback,
                       const Layout::FramebufferLayout& layout);

u16 GetResolutionScaleFactor();

} // namespace VideoCore
