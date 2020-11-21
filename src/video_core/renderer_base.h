// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/video_core.h"

namespace Frontend {
class EmuWindow;
} // namespace Frontend

class RendererBase : NonCopyable {
public:
    explicit RendererBase(Frontend::EmuWindow& window);
    virtual ~RendererBase();

    /// Swap buffers (render frame)
    virtual void SwapBuffers() = 0;

    /// Updates the framebuffer layout of the contained render window handle.
    void UpdateCurrentFramebufferLayout();

    VideoCore::RasterizerInterface* Rasterizer() const {
        return rasterizer.get();
    }

    Frontend::EmuWindow& GetRenderWindow() {
        return render_window;
    }

    const Frontend::EmuWindow& GetRenderWindow() const {
        return render_window;
    }

    bool RefreshRasterizerSetting();

protected:
    Frontend::EmuWindow& render_window; ///< Reference to the render window handle.
    std::unique_ptr<VideoCore::RasterizerInterface> rasterizer;

private:
    bool opengl_rasterizer_active = false;
};
