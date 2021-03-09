// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// modified from
// https://github.com/bloc97/Anime4K/blob/533cee5f7018d0e57ad2a26d76d43f13b9d8782a/glsl/Anime4K_Adaptive_v1.0RC2_UltraFast.glsl

// MIT License
//
// Copyright(c) 2019 bloc97
//
// Permission is hereby granted,
// free of charge,
// to any person obtaining a copy of this software and associated documentation
// files(the "Software"),
// to deal in the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software,
// and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all copies
// or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS",
// WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "video_core/renderer/rasterizer_cache.h"
#include "video_core/renderer/texture_filters/anime4k_ultrafast.h"
#include "video_core/renderer/texture_filters/tex_coord.h"

namespace OpenGL {

constexpr std::string_view refine_fragment = R"(#version 330 core

in vec2 tex_coord;
in vec2 input_max;

out vec4 frag_color;

uniform sampler2D HOOKED;
uniform sampler2DRect LUMAD;
uniform sampler2DRect LUMAG;

uniform float final_scale;

const float LINE_DETECT_THRESHOLD = 0.4;
const float STRENGTH = 0.6;

// the original shader used the alpha channel for luminance,
// which doesn't work for our use case
struct RGBAL {
    vec4 c;
    float l;
};

vec4 getAverage(vec4 cc, vec4 a, vec4 b, vec4 c) {
    return cc * (1 - STRENGTH) + ((a + b + c) / 3) * STRENGTH;
}

#define GetRGBAL(offset)                                                                           \
    RGBAL(textureOffset(HOOKED, tex_coord, offset),                                                \
          texture(LUMAD, clamp((gl_FragCoord.xy + offset) * final_scale, vec2(0.0), input_max)).x)

float min3v(float a, float b, float c) {
    return min(min(a, b), c);
}

float max3v(float a, float b, float c) {
    return max(max(a, b), c);
}

vec4 Compute() {
    RGBAL cc = GetRGBAL(ivec2(0));

    if (cc.l > LINE_DETECT_THRESHOLD) {
        return cc.c;
    }

    RGBAL tl = GetRGBAL(ivec2(-1, -1));
    RGBAL t = GetRGBAL(ivec2(0, -1));
    RGBAL tr = GetRGBAL(ivec2(1, -1));

    RGBAL l = GetRGBAL(ivec2(-1, 0));

    RGBAL r = GetRGBAL(ivec2(1, 0));

    RGBAL bl = GetRGBAL(ivec2(-1, 1));
    RGBAL b = GetRGBAL(ivec2(0, 1));
    RGBAL br = GetRGBAL(ivec2(1, 1));

    // Kernel 0 and 4
    float maxDark = max3v(br.l, b.l, bl.l);
    float minLight = min3v(tl.l, t.l, tr.l);

    if (minLight > cc.l && minLight > maxDark) {
        return getAverage(cc.c, tl.c, t.c, tr.c);
    } else {
        maxDark = max3v(tl.l, t.l, tr.l);
        minLight = min3v(br.l, b.l, bl.l);
        if (minLight > cc.l && minLight > maxDark) {
            return getAverage(cc.c, br.c, b.c, bl.c);
        }
    }

    // Kernel 1 and 5
    maxDark = max3v(cc.l, l.l, b.l);
    minLight = min3v(r.l, t.l, tr.l);

    if (minLight > maxDark) {
        return getAverage(cc.c, r.c, t.c, tr.c);
    } else {
        maxDark = max3v(cc.l, r.l, t.l);
        minLight = min3v(bl.l, l.l, b.l);
        if (minLight > maxDark) {
            return getAverage(cc.c, bl.c, l.c, b.c);
        }
    }

    // Kernel 2 and 6
    maxDark = max3v(l.l, tl.l, bl.l);
    minLight = min3v(r.l, br.l, tr.l);

    if (minLight > cc.l && minLight > maxDark) {
        return getAverage(cc.c, r.c, br.c, tr.c);
    } else {
        maxDark = max3v(r.l, br.l, tr.l);
        minLight = min3v(l.l, tl.l, bl.l);
        if (minLight > cc.l && minLight > maxDark) {
            return getAverage(cc.c, l.c, tl.c, bl.c);
        }
    }

    // Kernel 3 and 7
    maxDark = max3v(cc.l, l.l, t.l);
    minLight = min3v(r.l, br.l, b.l);

    if (minLight > maxDark) {
        return getAverage(cc.c, r.c, br.c, b.c);
    } else {
        maxDark = max3v(cc.l, r.l, b.l);
        minLight = min3v(t.l, l.l, tl.l);
        if (minLight > maxDark) {
            return getAverage(cc.c, t.c, l.c, tl.c);
        }
    }

    return cc.c;
}

void main() {
    frag_color = Compute();
})";

constexpr std::string_view refine_vertex = R"(#version 330 core

out vec2 tex_coord;
out vec2 input_max;

uniform sampler2D HOOKED;

const vec2 vertices[4] =
    vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));

void main() {
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    tex_coord = (vertices[gl_VertexID] + 1.0) / 2.0;
    input_max = textureSize(HOOKED, 0) * 2.0 - 1.0;
})";

constexpr std::string_view x_gradient_fragment = R"(#version 330 core

in vec2 tex_coord;

out vec2 frag_color;

uniform sampler2D tex_input;

const vec3 K = vec3(0.2627, 0.6780, 0.0593);
// TODO: improve handling of alpha channel
#define GetLum(xoffset) dot(K, textureOffset(tex_input, tex_coord, ivec2(xoffset, 0)).rgb)

void main() {
    float l = GetLum(-1);
    float c = GetLum(0);
    float r = GetLum(1);

    frag_color = vec2(r - l, l + 2.0 * c + r);
})";

constexpr std::string_view y_gradient_fragment = R"(#version 330 core

in vec2 input_max;

out float frag_color;

uniform sampler2DRect tex_input;

void main() {
    vec2 t = texture(tex_input, min(gl_FragCoord.xy + vec2(0.0, 1.0), input_max)).xy;
    vec2 c = texture(tex_input, gl_FragCoord.xy).xy;
    vec2 b = texture(tex_input, max(gl_FragCoord.xy - vec2(0.0, 1.0), vec2(0.0))).xy;

    vec2 grad = vec2(t.x + 2 * c.x + b.x, b.y - t.y);

    frag_color = 1 - length(grad);
})";

constexpr std::string_view y_gradient_vertex = R"(#version 330 core

out vec2 input_max;

uniform sampler2D tex_size;

const vec2 vertices[4] =
    vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));

void main() {
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    input_max = textureSize(tex_size, 0) * 2 - 1;
})";

Anime4kUltrafast::Anime4kUltrafast(u16 scale_factor) : TextureFilterBase(scale_factor) {
    const OpenGLState cur_state = OpenGLState::GetCurState();
    const auto setup_temp_tex = [this](TempTex& texture, GLint internal_format, GLint format) {
        texture.fbo.Create();
        texture.tex.Create();
        state.draw.draw_framebuffer = texture.fbo.handle;
        state.Apply();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_RECTANGLE, texture.tex.handle);
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, internal_format, 1024 * internal_scale_factor,
                     1024 * internal_scale_factor, 0, format, GL_HALF_FLOAT, nullptr);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE,
                               texture.tex.handle, 0);
    };
    setup_temp_tex(LUMAD, GL_R16F, GL_RED);
    setup_temp_tex(XY, GL_RG16F, GL_RG);

    vao.Create();

    for (std::size_t idx = 0; idx < samplers.size(); ++idx) {
        samplers[idx].Create();
        state.texture_units[idx].sampler = samplers[idx].handle;
        glSamplerParameteri(samplers[idx].handle, GL_TEXTURE_MIN_FILTER,
                            idx == 0 ? GL_LINEAR : GL_NEAREST);
        glSamplerParameteri(samplers[idx].handle, GL_TEXTURE_MAG_FILTER,
                            idx == 0 ? GL_LINEAR : GL_NEAREST);
        glSamplerParameteri(samplers[idx].handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(samplers[idx].handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    state.draw.vertex_array = vao.handle;

    gradient_x_program.Create(tex_coord.data(), x_gradient_fragment.data());
    gradient_y_program.Create(y_gradient_vertex.data(), y_gradient_fragment.data());
    refine_program.Create(refine_vertex.data(), refine_fragment.data());

    state.draw.shader_program = gradient_y_program.handle;
    state.Apply();
    glUniform1i(glGetUniformLocation(gradient_y_program.handle, "tex_input"), 2);

    state.draw.shader_program = refine_program.handle;
    state.Apply();
    glUniform1i(glGetUniformLocation(refine_program.handle, "LUMAD"), 1);
    glUniform1f(glGetUniformLocation(refine_program.handle, "final_scale"),
                static_cast<GLfloat>(internal_scale_factor) / scale_factor);

    cur_state.Apply();
}

void Anime4kUltrafast::Filter(GLuint src_tex, const Common::Rectangle<u32>& src_rect,
                              GLuint dst_tex, const Common::Rectangle<u32>& dst_rect,
                              GLuint read_fb_handle, GLuint draw_fb_handle) {
    const OpenGLState cur_state = OpenGLState::GetCurState();

    state.viewport = {static_cast<GLint>(src_rect.left * internal_scale_factor),
                      static_cast<GLint>(src_rect.bottom * internal_scale_factor),
                      static_cast<GLsizei>(src_rect.GetWidth() * internal_scale_factor),
                      static_cast<GLsizei>(src_rect.GetHeight() * internal_scale_factor)};
    state.texture_units[0].texture_2d = src_tex;
    state.draw.draw_framebuffer = XY.fbo.handle;
    state.draw.shader_program = gradient_x_program.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, LUMAD.tex.handle);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_RECTANGLE, XY.tex.handle);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // gradient y pass
    state.draw.draw_framebuffer = LUMAD.fbo.handle;
    state.draw.shader_program = gradient_y_program.handle;
    state.Apply();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // refine pass
    state.viewport = {static_cast<GLint>(dst_rect.left), static_cast<GLint>(dst_rect.bottom),
                      static_cast<GLsizei>(dst_rect.GetWidth()),
                      static_cast<GLsizei>(dst_rect.GetHeight())};
    state.draw.draw_framebuffer = draw_fb_handle;
    state.draw.shader_program = refine_program.handle;
    state.Apply();

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_tex, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    cur_state.Apply();
}

} // namespace OpenGL
