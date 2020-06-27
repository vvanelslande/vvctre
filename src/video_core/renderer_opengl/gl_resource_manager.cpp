// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include <glad/glad.h>
#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_vars.h"

namespace OpenGL {

void OGLTexture::Create() {
    if (handle != 0) {
        return;
    }

    glGenTextures(1, &handle);
}

void OGLTexture::Release() {
    if (handle == 0) {
        return;
    }

    glDeleteTextures(1, &handle);
    OpenGLState::GetCurState().ResetTexture(handle).Apply();
    handle = 0;
}

void OGLTexture::Allocate(GLenum target, GLsizei levels, GLenum internalformat,
                                 GLenum format, GLenum type, GLsizei width, GLsizei height,
                                 GLsizei depth) {
    const bool tex_storage = GLAD_GL_ARB_texture_storage || GLES;

    switch (target) {
    case GL_TEXTURE_1D:
    case GL_TEXTURE:
        if (tex_storage) {
            glTexStorage1D(target, levels, internalformat, width);
        } else {
            for (GLsizei level{0}; level < levels; ++level) {
                glTexImage1D(target, level, internalformat, width, 0, format, type, nullptr);
                width >>= 1;
            }
        }
        break;
    case GL_TEXTURE_2D:
    case GL_TEXTURE_1D_ARRAY:
    case GL_TEXTURE_RECTANGLE:
    case GL_TEXTURE_CUBE_MAP:
        if (tex_storage) {
            glTexStorage2D(target, levels, internalformat, width, height);
        } else {
            for (GLsizei level{0}; level < levels; ++level) {
                glTexImage2D(target, level, internalformat, width, height, 0, format, type,
                             nullptr);
                width >>= 1;
                if (target != GL_TEXTURE_1D_ARRAY)
                    height >>= 1;
            }
        }
        break;
    case GL_TEXTURE_3D:
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_CUBE_MAP_ARRAY:
        if (tex_storage) {
            glTexStorage3D(target, levels, internalformat, width, height, depth);
        } else {
            for (GLsizei level{0}; level < levels; ++level) {
                glTexImage3D(target, level, internalformat, width, height, depth, 0, format, type,
                             nullptr);
            }
            width >>= 1;
            height >>= 1;
            if (target == GL_TEXTURE_3D)
                depth >>= 1;
        }
        break;
    }

    if (!tex_storage) {
        glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, levels - 1);
    }
}

void OGLSampler::Create() {
    if (handle != 0) {
        return;
    }

    glGenSamplers(1, &handle);
}

void OGLSampler::Release() {
    if (handle == 0) {
        return;
    }

    glDeleteSamplers(1, &handle);
    OpenGLState::GetCurState().ResetSampler(handle).Apply();
    handle = 0;
}

void OGLShader::Create(const char* source, GLenum type) {
    if (handle != 0) {
        return;
    }

    if (source == nullptr) {
        return;
    }

    handle = LoadShader(source, type);
}

void OGLShader::Release() {
    if (handle == 0) {
        return;
    }

    glDeleteShader(handle);
    handle = 0;
}

void OGLProgram::Create(bool separable_program, const std::vector<GLuint>& shaders) {
    if (handle != 0) {
        return;
    }

    handle = LoadProgram(separable_program, shaders);
}

void OGLProgram::Create(const char* vert_shader, const char* frag_shader) {
    OGLShader vert, frag;
    vert.Create(vert_shader, GL_VERTEX_SHADER);
    frag.Create(frag_shader, GL_FRAGMENT_SHADER);

    Create(false, {vert.handle, frag.handle});
}

void OGLProgram::Release() {
    if (handle == 0) {
        return;
    }

    glDeleteProgram(handle);
    OpenGLState::GetCurState().ResetProgram(handle).Apply();
    handle = 0;
}

void OGLPipeline::Create() {
    if (handle != 0) {
        return;
    }

    glGenProgramPipelines(1, &handle);
}

void OGLPipeline::Release() {
    if (handle == 0) {
        return;
    }

    glDeleteProgramPipelines(1, &handle);
    OpenGLState::GetCurState().ResetPipeline(handle).Apply();
    handle = 0;
}

void OGLBuffer::Create() {
    if (handle != 0) {
        return;
    }

    glGenBuffers(1, &handle);
}

void OGLBuffer::Release() {
    if (handle == 0) {
        return;
    }

    glDeleteBuffers(1, &handle);
    OpenGLState::GetCurState().ResetBuffer(handle).Apply();
    handle = 0;
}

void OGLVertexArray::Create() {
    if (handle != 0) {
        return;
    }

    glGenVertexArrays(1, &handle);
}

void OGLVertexArray::Release() {
    if (handle == 0) {
        return;
    }

    glDeleteVertexArrays(1, &handle);
    OpenGLState::GetCurState().ResetVertexArray(handle).Apply();
    handle = 0;
}

void OGLFramebuffer::Create() {
    if (handle != 0) {
        return;
    }

    glGenFramebuffers(1, &handle);
}

void OGLFramebuffer::Release() {
    if (handle == 0) {
        return;
    }

    glDeleteFramebuffers(1, &handle);
    OpenGLState::GetCurState().ResetFramebuffer(handle).Apply();
    handle = 0;
}

} // namespace OpenGL
