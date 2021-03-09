// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/renderer/rasterizer_cache.h"
#include "video_core/renderer/texture_filters/bicubic.h"
#include "video_core/renderer/texture_filters/tex_coord.h"

namespace OpenGL {

constexpr std::string_view fragment_shader = R"(#version 330 core

in vec2 tex_coord;

out vec4 frag_color;

uniform sampler2D input_texture;

// from http://www.java-gaming.org/index.php?topic=35123.0
vec4 cubic(float v) {
    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
    vec4 s = n * n * n;
    float x = s.x;
    float y = s.y - 4.0 * s.x;
    float z = s.z - 4.0 * s.y + 6.0 * s.x;
    float w = 6.0 - x - y - z;
    return vec4(x, y, z, w) * (1.0 / 6.0);
}

vec4 textureBicubic(sampler2D sampler, vec2 texCoords) {

    vec2 texSize = textureSize(sampler, 0);
    vec2 invTexSize = 1.0 / texSize;

    texCoords = texCoords * texSize - 0.5;

    vec2 fxy = fract(texCoords);
    texCoords -= fxy;

    vec4 xcubic = cubic(fxy.x);
    vec4 ycubic = cubic(fxy.y);

    vec4 c = texCoords.xxyy + vec2(-0.5, +1.5).xyxy;

    vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
    vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;

    offset *= invTexSize.xxyy;

    vec4 sample0 = texture(sampler, offset.xz);
    vec4 sample1 = texture(sampler, offset.yz);
    vec4 sample2 = texture(sampler, offset.xw);
    vec4 sample3 = texture(sampler, offset.yw);

    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);

    return mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
}

void main() {
    frag_color = textureBicubic(input_texture, tex_coord);
})";

Bicubic::Bicubic(u16 scale_factor) : TextureFilterBase(scale_factor) {
    program.Create(tex_coord.data(), fragment_shader.data());
    vao.Create();
    src_sampler.Create();

    state.draw.shader_program = program.handle;
    state.draw.vertex_array = vao.handle;
    state.draw.shader_program = program.handle;
    state.texture_units[0].sampler = src_sampler.handle;

    glSamplerParameteri(src_sampler.handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(src_sampler.handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(src_sampler.handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(src_sampler.handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Bicubic::Filter(GLuint src_tex, const Common::Rectangle<u32>& src_rect, GLuint dst_tex,
                     const Common::Rectangle<u32>& dst_rect, GLuint read_fb_handle,
                     GLuint draw_fb_handle) {
    const OpenGLState cur_state = OpenGLState::GetCurState();
    state.texture_units[0].texture_2d = src_tex;
    state.draw.draw_framebuffer = draw_fb_handle;
    state.viewport = {static_cast<GLint>(dst_rect.left), static_cast<GLint>(dst_rect.bottom),
                      static_cast<GLsizei>(dst_rect.GetWidth()),
                      static_cast<GLsizei>(dst_rect.GetHeight())};
    state.Apply();

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_tex, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    cur_state.Apply();
}

} // namespace OpenGL
