// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <glad/glad.h>
#include "common/common_types.h"
#include "common/math_util.h"
#include "core/frontend/emu_window.h"
#include "core/hw/gpu.h"
#include "video_core/renderer/resource_manager.h"
#include "video_core/renderer/state.h"

namespace Layout {
struct FramebufferLayout;
}

namespace OpenGL {

class RasterizerOpenGL;

/// Structure used for storing information about the textures for each 3DS screen
struct TextureInfo {
    OGLTexture resource;
    GLsizei width;
    GLsizei height;
    GPU::Regs::PixelFormat format;
    GLenum gl_format;
    GLenum gl_type;
};

/// Structure used for storing information about the display target for each 3DS screen
struct ScreenInfo {
    GLuint display_texture;
    Common::Rectangle<float> display_texcoords;
    TextureInfo texture;
};

class Renderer {
public:
    explicit Renderer(Frontend::EmuWindow& window);
    ~Renderer();

    /// Swap buffers (render frame)
    void SwapBuffers();

    void UpdateCurrentFramebufferLayout();

    RasterizerOpenGL* Rasterizer() const {
        return rasterizer.get();
    }

    Frontend::EmuWindow& GetRenderWindow() {
        return render_window;
    }

    const Frontend::EmuWindow& GetRenderWindow() const {
        return render_window;
    }

private:
    void InitOpenGLObjects();
    void ReloadSampler();
    void ReloadShader();
    void ConfigureFramebufferTexture(TextureInfo& texture,
                                     const GPU::Regs::FramebufferConfig& framebuffer);
    void DrawScreens(const Layout::FramebufferLayout& layout);
    void DrawSingleScreenRotated(const ScreenInfo& screen_info, float x, float y, float w, float h);
    void DrawSingleScreen(const ScreenInfo& screen_info, float x, float y, float w, float h);
    void DrawSingleScreenStereoRotated(const ScreenInfo& screen_info_l,
                                       const ScreenInfo& screen_info_r, float x, float y, float w,
                                       float h);
    void DrawSingleScreenStereo(const ScreenInfo& screen_info_l, const ScreenInfo& screen_info_r,
                                float x, float y, float w, float h);

    // Loads framebuffer from emulated memory into the display information structure
    void LoadFBToScreenInfo(const GPU::Regs::FramebufferConfig& framebuffer,
                            ScreenInfo& screen_info, bool right_eye);
    // Fills active OpenGL texture with the given RGB color.
    void LoadColorToActiveGLTexture(u8 color_r, u8 color_g, u8 color_b, const TextureInfo& texture);

    OpenGLState state;
    Frontend::EmuWindow& render_window;
    std::unique_ptr<RasterizerOpenGL> rasterizer;

    // OpenGL object IDs
    OGLVertexArray vertex_array;
    OGLBuffer vertex_buffer;
    OGLProgram shader;
    OGLFramebuffer screenshot_framebuffer;
    OGLSampler filter_sampler;

    /// Display information for top and bottom screens respectively
    std::array<ScreenInfo, 3> screen_infos;

    // Shader uniform location indices
    GLuint uniform_modelview_matrix;
    GLuint uniform_color_texture;
    GLuint uniform_color_texture_r;

    // Shader uniform for Dolphin compatibility
    GLuint uniform_i_resolution;
    GLuint uniform_o_resolution;
    GLuint uniform_layer;

    // Shader attribute input indices
    GLuint attrib_position;
    GLuint attrib_tex_coord;
};

} // namespace OpenGL
