// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <boost/container_hash/hash.hpp>
#include <thread>
#include <unordered_map>
#include <variant>
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/settings.h"
#include "video_core/renderer/shader_disk_cache.h"
#include "video_core/renderer/shader_manager.h"
#include "video_core/video_core.h"

namespace OpenGL {

static u64 GetUniqueIdentifier(const Pica::Regs& registers, const std::vector<u32>& code) {
    std::size_t hash = 0;
    u64 registers_hash =
        Common::ComputeHash64(registers.reg_array.data(), Pica::Regs::NUM_REGS * sizeof(u32));
    boost::hash_combine(hash, registers_hash);
    if (code.size() > 0) {
        u64 code_hash = Common::ComputeHash64(code.data(), code.size() * sizeof(u32));
        boost::hash_combine(hash, code_hash);
    }
    return static_cast<u64>(hash);
}

static std::tuple<PicaVSConfig, Pica::Shader::ShaderSetup> BuildVsConfigFromEntry(
    const ShaderDiskCacheEntry& entry) {
    Pica::Shader::ProgramCode program_code{};
    Pica::Shader::SwizzleData swizzle_data{};
    std::copy_n(entry.GetCode().begin(), Pica::Shader::MAX_PROGRAM_CODE_LENGTH,
                program_code.begin());
    std::copy_n(entry.GetCode().begin() + Pica::Shader::MAX_PROGRAM_CODE_LENGTH,
                Pica::Shader::MAX_SWIZZLE_DATA_LENGTH, swizzle_data.begin());
    Pica::Shader::ShaderSetup setup;
    setup.program_code = program_code;
    setup.swizzle_data = swizzle_data;
    return {PicaVSConfig{entry.GetRegisters().vs, setup}, setup};
}

static void SetShaderUniformBlockBinding(GLuint shader, const char* name, UniformBindings binding,
                                         std::size_t expected_size) {
    const GLuint ub_index = glGetUniformBlockIndex(shader, name);
    if (ub_index == GL_INVALID_INDEX) {
        return;
    }
    GLint ub_size = 0;
    glGetActiveUniformBlockiv(shader, ub_index, GL_UNIFORM_BLOCK_DATA_SIZE, &ub_size);
    ASSERT_MSG(ub_size == expected_size, "Uniform block size did not match! Got {}, expected {}",
               static_cast<int>(ub_size), expected_size);
    glUniformBlockBinding(shader, ub_index, static_cast<GLuint>(binding));
}

static void SetShaderUniformBlockBindings(GLuint shader) {
    SetShaderUniformBlockBinding(shader, "shader_data", UniformBindings::Common,
                                 sizeof(UniformData));
    SetShaderUniformBlockBinding(shader, "vs_config", UniformBindings::VS, sizeof(VSUniformData));
}

static void SetShaderSamplerBinding(GLuint shader, const char* name,
                                    TextureUnits::TextureUnit binding) {
    GLint uniform_tex = glGetUniformLocation(shader, name);
    if (uniform_tex != -1) {
        glUniform1i(uniform_tex, binding.id);
    }
}

static void SetShaderImageBinding(GLuint shader, const char* name, GLuint binding) {
    GLint uniform_tex = glGetUniformLocation(shader, name);
    if (uniform_tex != -1) {
        glUniform1i(uniform_tex, static_cast<GLint>(binding));
    }
}

static void SetShaderSamplerBindings(GLuint shader) {
    OpenGLState cur_state = OpenGLState::GetCurState();
    GLuint old_program = std::exchange(cur_state.draw.shader_program, shader);
    cur_state.Apply();

    // Set the texture samplers to correspond to different texture units
    SetShaderSamplerBinding(shader, "tex0", TextureUnits::PicaTexture(0));
    SetShaderSamplerBinding(shader, "tex1", TextureUnits::PicaTexture(1));
    SetShaderSamplerBinding(shader, "tex2", TextureUnits::PicaTexture(2));
    SetShaderSamplerBinding(shader, "tex_cube", TextureUnits::TextureCube);

    // Set the texture samplers to correspond to different lookup table texture units
    SetShaderSamplerBinding(shader, "texture_buffer_lut_rg", TextureUnits::TextureBufferLUT_RG);
    SetShaderSamplerBinding(shader, "texture_buffer_lut_rgba", TextureUnits::TextureBufferLUT_RGBA);

    SetShaderImageBinding(shader, "shadow_buffer", ImageUnits::ShadowBuffer);
    SetShaderImageBinding(shader, "shadow_texture_px", ImageUnits::ShadowTexturePX);
    SetShaderImageBinding(shader, "shadow_texture_nx", ImageUnits::ShadowTextureNX);
    SetShaderImageBinding(shader, "shadow_texture_py", ImageUnits::ShadowTexturePY);
    SetShaderImageBinding(shader, "shadow_texture_ny", ImageUnits::ShadowTextureNY);
    SetShaderImageBinding(shader, "shadow_texture_pz", ImageUnits::ShadowTexturePZ);
    SetShaderImageBinding(shader, "shadow_texture_nz", ImageUnits::ShadowTextureNZ);

    cur_state.draw.shader_program = old_program;
    cur_state.Apply();
}

void PicaUniformsData::SetFromRegs(const Pica::ShaderRegs& regs,
                                   const Pica::Shader::ShaderSetup& setup) {
    std::transform(std::begin(setup.uniforms.b), std::end(setup.uniforms.b), std::begin(bools),
                   [](bool value) -> BoolAligned { return {value ? GL_TRUE : GL_FALSE}; });
    std::transform(std::begin(regs.int_uniforms), std::end(regs.int_uniforms), std::begin(i),
                   [](const auto& value) -> GLuvec4 {
                       return {value.x.Value(), value.y.Value(), value.z.Value(), value.w.Value()};
                   });
    std::transform(std::begin(setup.uniforms.f), std::end(setup.uniforms.f), std::begin(f),
                   [](const auto& value) -> GLvec4 {
                       return {value.x.ToFloat32(), value.y.ToFloat32(), value.z.ToFloat32(),
                               value.w.ToFloat32()};
                   });
}

/**
 * An object representing a shader program staging. It can be either a shader object or a program
 * object, depending on whether separable program is used.
 */
class OGLShaderStage {
public:
    explicit OGLShaderStage(bool separable) {
        if (separable) {
            shader_or_program = OGLProgram();
        } else {
            shader_or_program = OGLShader();
        }
    }

    void Create(const char* source, GLenum type) {
        if (shader_or_program.index() == 0) {
            std::get<OGLShader>(shader_or_program).Create(source, type);
        } else {
            OGLShader shader;
            shader.Create(source, type);
            OGLProgram& program = std::get<OGLProgram>(shader_or_program);
            program.Create(true, {shader.handle});
            SetShaderUniformBlockBindings(program.handle);
            SetShaderSamplerBindings(program.handle);
        }
    }

    GLuint GetHandle() const {
        if (shader_or_program.index() == 0) {
            return std::get<OGLShader>(shader_or_program).handle;
        } else {
            return std::get<OGLProgram>(shader_or_program).handle;
        }
    }

private:
    std::variant<OGLShader, OGLProgram> shader_or_program;
};

class TrivialVertexShader {
public:
    explicit TrivialVertexShader(bool separable) : program(separable) {
        program.Create(GenerateTrivialVertexShader(separable).c_str(), GL_VERTEX_SHADER);
    }

    GLuint Get() const {
        return program.GetHandle();
    }

private:
    OGLShaderStage program;
};

template <typename KeyConfigType, std::string (*CodeGenerator)(const KeyConfigType&, bool),
          GLenum ShaderType>
class ShaderCache {
public:
    explicit ShaderCache(bool separable) : separable(separable) {}
    std::tuple<GLuint, bool> Get(const KeyConfigType& config) {
        auto [iter, new_shader] = shaders.emplace(config, OGLShaderStage{separable});
        OGLShaderStage& cached_shader = iter->second;
        if (new_shader) {
            const std::string result = CodeGenerator(config, separable);
            cached_shader.Create(result.c_str(), ShaderType);
        }
        return {cached_shader.GetHandle(), new_shader};
    }

private:
    bool separable;
    std::unordered_map<KeyConfigType, OGLShaderStage> shaders;
};

// This is a cache designed for shaders translated from PICA shaders. The first cache matches the
// config structure like a normal cache does. On cache miss, the second cache matches the generated
// GLSL code. The configuration is like this because there might be leftover code in the PICA shader
// program buffer from the previous shader, which is hashed into the config, resulting several
// different config values from the same shader program.
template <typename KeyConfigType,
          std::optional<std::string> (*CodeGenerator)(const Pica::Shader::ShaderSetup&,
                                                      const KeyConfigType&, bool),
          GLenum ShaderType>
class ShaderDoubleCache {
public:
    explicit ShaderDoubleCache(bool separable) : separable(separable) {}
    std::tuple<GLuint, bool> Get(const KeyConfigType& key, const Pica::Shader::ShaderSetup& setup) {
        auto map_it = shader_map.find(key);
        if (map_it == shader_map.end()) {
            const std::optional<std::string> program = CodeGenerator(setup, key, separable);
            if (!program) {
                shader_map[key] = nullptr;
                return {0, false};
            }

            auto [iter, new_shader] = shader_cache.emplace(*program, OGLShaderStage{separable});
            OGLShaderStage& cached_shader = iter->second;
            if (new_shader) {
                cached_shader.Create(program->c_str(), ShaderType);
            }
            shader_map[key] = &cached_shader;
            return {cached_shader.GetHandle(), new_shader};
        }

        if (map_it->second == nullptr) {
            return {0, false};
        }

        return {map_it->second->GetHandle(), false};
    }

private:
    bool separable;
    std::unordered_map<KeyConfigType, OGLShaderStage*> shader_map;
    std::unordered_map<std::string, OGLShaderStage> shader_cache;
};

using ProgrammableVertexShaders =
    ShaderDoubleCache<PicaVSConfig, &GenerateVertexShader, GL_VERTEX_SHADER>;

using FixedGeometryShaders =
    ShaderCache<PicaFixedGSConfig, &GenerateFixedGeometryShader, GL_GEOMETRY_SHADER>;

using FragmentShaders = ShaderCache<PicaFSConfig, &GenerateFragmentShader, GL_FRAGMENT_SHADER>;

class ShaderProgramManager::Impl {
public:
    explicit Impl(bool separable, bool enable_hacks)
        : enable_hacks(enable_hacks), separable(separable), programmable_vertex_shaders(separable),
          trivial_vertex_shader(separable), fixed_geometry_shaders(separable),
          fragment_shaders(separable) {
        if (separable) {
            pipeline.Create();
        }
        disk_cache = std::make_unique<ShaderDiskCache>(separable);
    }

    struct ShaderTuple {
        GLuint vs = 0;
        GLuint gs = 0;
        GLuint fs = 0;

        bool operator==(const ShaderTuple& rhs) const {
            return std::tie(vs, gs, fs) == std::tie(rhs.vs, rhs.gs, rhs.fs);
        }

        bool operator!=(const ShaderTuple& rhs) const {
            return std::tie(vs, gs, fs) != std::tie(rhs.vs, rhs.gs, rhs.fs);
        }

        struct Hash {
            std::size_t operator()(const ShaderTuple& tuple) const {
                std::size_t hash = 0;
                boost::hash_combine(hash, tuple.vs);
                boost::hash_combine(hash, tuple.gs);
                boost::hash_combine(hash, tuple.fs);
                return hash;
            }
        };
    };

    bool enable_hacks;
    bool separable;

    ShaderTuple current;

    ProgrammableVertexShaders programmable_vertex_shaders;
    TrivialVertexShader trivial_vertex_shader;

    FixedGeometryShaders fixed_geometry_shaders;

    FragmentShaders fragment_shaders;
    std::unordered_map<ShaderTuple, OGLProgram, ShaderTuple::Hash> program_cache;
    OGLPipeline pipeline;
    std::unique_ptr<ShaderDiskCache> disk_cache;
};

ShaderProgramManager::ShaderProgramManager(bool separable, bool enable_hacks)
    : impl(std::make_unique<Impl>(separable, enable_hacks)) {}

ShaderProgramManager::~ShaderProgramManager() = default;

bool ShaderProgramManager::UseProgrammableVertexShader(const Pica::Regs& regs,
                                                       Pica::Shader::ShaderSetup& setup) {
    PicaVSConfig config{regs.vs, setup};
    auto [handle, new_shader] = impl->programmable_vertex_shaders.Get(config, setup);
    if (handle == 0) {
        return false;
    }
    impl->current.vs = handle;
    if (Settings::values.use_hardware_shader && Settings::values.enable_disk_shader_cache &&
        new_shader) {
        std::vector<u32> code{setup.program_code.begin(), setup.program_code.end()};
        code.insert(code.end(), setup.swizzle_data.begin(), setup.swizzle_data.end());
        const u64 unique_identifier = GetUniqueIdentifier(regs, code);
        const ShaderDiskCacheEntry entry{unique_identifier, ProgramType::VS, regs, std::move(code)};
        impl->disk_cache->Add(entry);
    }
    return true;
}

void ShaderProgramManager::UseTrivialVertexShader() {
    impl->current.vs = impl->trivial_vertex_shader.Get();
}

void ShaderProgramManager::UseFixedGeometryShader(const Pica::Regs& regs) {
    PicaFixedGSConfig gs_config(regs);
    auto [handle, _] = impl->fixed_geometry_shaders.Get(gs_config);
    impl->current.gs = handle;
}

void ShaderProgramManager::UseTrivialGeometryShader() {
    impl->current.gs = 0;
}

void ShaderProgramManager::UseFragmentShader(const Pica::Regs& regs) {
    PicaFSConfig config = PicaFSConfig::BuildFromRegs(regs);
    auto [handle, new_shader] = impl->fragment_shaders.Get(config);
    impl->current.fs = handle;
    if (Settings::values.use_hardware_shader && Settings::values.enable_disk_shader_cache &&
        new_shader) {
        u64 unique_identifier = GetUniqueIdentifier(regs, {});
        ShaderDiskCacheEntry entry{unique_identifier, ProgramType::FS, regs, {}};
        impl->disk_cache->Add(entry);
    }
}

void ShaderProgramManager::ApplyTo(OpenGLState& state) {
    if (impl->separable) {
        if (impl->enable_hacks) {
            // Without this reseting, AMD sometimes freezes when one stage is changed but not
            // for the others. On the other hand, including this reset seems to introduce memory
            // leak in Intel Graphics.
            glUseProgramStages(
                impl->pipeline.handle,
                GL_VERTEX_SHADER_BIT | GL_GEOMETRY_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 0);
        }

        glUseProgramStages(impl->pipeline.handle, GL_VERTEX_SHADER_BIT, impl->current.vs);
        glUseProgramStages(impl->pipeline.handle, GL_GEOMETRY_SHADER_BIT, impl->current.gs);
        glUseProgramStages(impl->pipeline.handle, GL_FRAGMENT_SHADER_BIT, impl->current.fs);
        state.draw.shader_program = 0;
        state.draw.program_pipeline = impl->pipeline.handle;
    } else {
        OGLProgram& cached_program = impl->program_cache[impl->current];
        if (cached_program.handle == 0) {
            cached_program.Create(false, {impl->current.vs, impl->current.gs, impl->current.fs});
            SetShaderUniformBlockBindings(cached_program.handle);
            SetShaderSamplerBindings(cached_program.handle);
        }
        state.draw.shader_program = cached_program.handle;
    }
}

void ShaderProgramManager::LoadDiskCache() {
    Core::System& system = Core::System::GetInstance();

    if (!impl->separable) {
        LOG_ERROR(Render_OpenGL,
                  "Cannot load disk cache as separate shader programs are unsupported!");
        return;
    }

    const auto entries = impl->disk_cache->Load();
    if (!entries) {
        return;
    }

    SCOPE_EXIT({ system.DiskShaderCacheCallback(false, 0, 0); });

    for (std::size_t i = 0; i < entries->size(); ++i) {
        const auto& entry = entries->at(i);
        const u64 unique_identifier = entry.GetUniqueIdentifier();

        GLuint handle = 0;

        if (entry.GetType() == ProgramType::VS) {
            auto [conf, setup] = BuildVsConfigFromEntry(entry);
            auto [h, _] = impl->programmable_vertex_shaders.Get(conf, setup);
            handle = h;
        } else if (entry.GetType() == ProgramType::FS) {
            PicaFSConfig conf = PicaFSConfig::BuildFromRegs(entry.GetRegisters());
            auto [h, _] = impl->fragment_shaders.Get(conf);
            handle = h;
        } else {
            LOG_ERROR(Render_OpenGL,
                      "Unsupported shader type ({}) found in disk shader cache, deleting it",
                      static_cast<u32>(entry.GetType()));
            impl->disk_cache->Delete();
            return;
        }

        if (handle == 0) {
            LOG_ERROR(Render_OpenGL, "Compilation failed, deleting cache");
            impl->disk_cache->Delete();
            return;
        }

        system.DiskShaderCacheCallback(true, i + 1, entries->size());
    }
}

} // namespace OpenGL
