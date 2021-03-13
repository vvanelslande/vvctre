// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <nihstro/shader_bytecode.h>
#include <type_traits>
#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/hash.h"
#include "common/vector_math.h"
#include "video_core/pica_types.h"
#include "video_core/regs_rasterizer.h"
#include "video_core/regs_shader.h"

using nihstro::DestRegister;
using nihstro::RegisterType;
using nihstro::SourceRegister;

namespace Pica::Shader {

constexpr unsigned MAX_PROGRAM_CODE_LENGTH = 4096;
constexpr unsigned MAX_SWIZZLE_DATA_LENGTH = 4096;
using ProgramCode = std::array<u32, MAX_PROGRAM_CODE_LENGTH>;
using SwizzleData = std::array<u32, MAX_SWIZZLE_DATA_LENGTH>;

struct AttributeBuffer {
    alignas(16) Common::Vec4<float24> attr[16];
};

/// Handler type for receiving vertex outputs from vertex shader or geometry shader
using VertexHandler = std::function<void(const AttributeBuffer&)>;

/// Handler type for signaling to invert the vertex order of the next triangle
using WindingSetter = std::function<void()>;

struct OutputVertex {
    Common::Vec4<float24> pos;
    Common::Vec4<float24> quat;
    Common::Vec4<float24> color;
    Common::Vec2<float24> tc0;
    Common::Vec2<float24> tc1;
    float24 tc0_w;
    INSERT_PADDING_WORDS(1);
    Common::Vec3<float24> view;
    INSERT_PADDING_WORDS(1);
    Common::Vec2<float24> tc2;

    static void ValidateSemantics(const RasterizerRegs& regs);
    static OutputVertex FromAttributeBuffer(const RasterizerRegs& regs,
                                            const AttributeBuffer& output);
};
#define ASSERT_POS(var, pos)                                                                       \
    static_assert(offsetof(OutputVertex, var) == pos * sizeof(float24), "Semantic at wrong "       \
                                                                        "offset.")
ASSERT_POS(pos, RasterizerRegs::VSOutputAttributes::POSITION_X);
ASSERT_POS(quat, RasterizerRegs::VSOutputAttributes::QUATERNION_X);
ASSERT_POS(color, RasterizerRegs::VSOutputAttributes::COLOR_R);
ASSERT_POS(tc0, RasterizerRegs::VSOutputAttributes::TEXCOORD0_U);
ASSERT_POS(tc1, RasterizerRegs::VSOutputAttributes::TEXCOORD1_U);
ASSERT_POS(tc0_w, RasterizerRegs::VSOutputAttributes::TEXCOORD0_W);
ASSERT_POS(view, RasterizerRegs::VSOutputAttributes::VIEW_X);
ASSERT_POS(tc2, RasterizerRegs::VSOutputAttributes::TEXCOORD2_U);
#undef ASSERT_POS
static_assert(std::is_pod<OutputVertex>::value, "Structure is not POD");
static_assert(sizeof(OutputVertex) == 24 * sizeof(float), "OutputVertex has invalid size");

/**
 * This structure contains state information for primitive emitting in geometry shader.
 */
struct GSEmitter {
    std::array<AttributeBuffer, 3> buffer;
    u8 vertex_id;
    bool prim_emit;
    bool winding;
    u32 output_mask;

    // Function objects are hidden behind a raw pointer to make the structure standard layout type,
    // for JIT to use offsetof to access other members.
    struct Handlers {
        VertexHandler vertex_handler;
        WindingSetter winding_setter;
    } * handlers;

    GSEmitter();
    ~GSEmitter();
    void Emit(Common::Vec4<float24> (&output_regs)[16]);
};
static_assert(std::is_standard_layout<GSEmitter>::value, "GSEmitter is not standard layout type");

/**
 * This structure contains the state information that needs to be unique for a shader unit. The 3DS
 * has four shader units that process shaders in parallel. At the present, vvctre only implements a
 * single shader unit that processes all shaders serially. Putting the state information in a struct
 * here will make it easier for us to parallelize the shader processing later.
 */
struct UnitState {
    explicit UnitState(GSEmitter* emitter = nullptr);
    struct Registers {
        // The registers are accessed by the shader JIT using SSE instructions, and are therefore
        // required to be 16-byte aligned.
        alignas(16) Common::Vec4<float24> input[16];
        alignas(16) Common::Vec4<float24> temporary[16];
        alignas(16) Common::Vec4<float24> output[16];
    } registers;
    static_assert(std::is_pod<Registers>::value, "Structure is not POD");

    bool conditional_code[2];

    // Two Address registers and one loop counter
    // TODO: How many bits do these actually have?
    s32 address_registers[3];

    GSEmitter* emitter_ptr;

    static std::size_t InputOffset(const SourceRegister& reg) {
        switch (reg.GetRegisterType()) {
        case RegisterType::Input:
            return offsetof(UnitState, registers.input) +
                   reg.GetIndex() * sizeof(Common::Vec4<float24>);

        case RegisterType::Temporary:
            return offsetof(UnitState, registers.temporary) +
                   reg.GetIndex() * sizeof(Common::Vec4<float24>);

        default:
            UNREACHABLE();
            return 0;
        }
    }

    static std::size_t OutputOffset(const DestRegister& reg) {
        switch (reg.GetRegisterType()) {
        case RegisterType::Output:
            return offsetof(UnitState, registers.output) +
                   reg.GetIndex() * sizeof(Common::Vec4<float24>);

        case RegisterType::Temporary:
            return offsetof(UnitState, registers.temporary) +
                   reg.GetIndex() * sizeof(Common::Vec4<float24>);

        default:
            UNREACHABLE();
            return 0;
        }
    }

    /**
     * Loads the unit state with an input vertex.
     *
     * @param config Shader configuration registers corresponding to the unit.
     * @param input Attribute buffer to load into the input registers.
     */
    void LoadInput(const ShaderRegs& config, const AttributeBuffer& input);

    void WriteOutput(const ShaderRegs& config, AttributeBuffer& output);
};

/**
 * This is an extended shader unit state that represents the special unit that can run both vertex
 * shader and geometry shader. It contains an additional primitive emitter and utilities for
 * geometry shader.
 */
struct GSUnitState : public UnitState {
    GSUnitState();
    void SetVertexHandler(VertexHandler vertex_handler, WindingSetter winding_setter);
    void ConfigOutput(const ShaderRegs& config);

    GSEmitter emitter;
};

struct Uniforms {
    // The float uniforms are accessed by the shader JIT using SSE instructions, and are
    // therefore required to be 16-byte aligned.
    alignas(16) Common::Vec4<float24> f[96];

    std::array<bool, 16> b;
    std::array<Common::Vec4<u8>, 4> i;

    static std::size_t GetFloatUniformOffset(unsigned index) {
        return offsetof(Uniforms, f) + index * sizeof(Common::Vec4<float24>);
    }

    static std::size_t GetBoolUniformOffset(unsigned index) {
        return offsetof(Uniforms, b) + index * sizeof(bool);
    }

    static std::size_t GetIntUniformOffset(unsigned index) {
        return offsetof(Uniforms, i) + index * sizeof(Common::Vec4<u8>);
    }
};

struct ShaderSetup {
    Uniforms uniforms;

    ProgramCode program_code;
    SwizzleData swizzle_data;

    struct EngineData {
        unsigned int entry_point;
        const void* cached_shader = nullptr;
    } engine_data;

    void MarkProgramCodeDirty() {
        program_code_hash_dirty = true;
    }

    void MarkSwizzleDataDirty() {
        swizzle_data_hash_dirty = true;
    }

    u64 GetProgramCodeHash() {
        if (program_code_hash_dirty) {
            program_code_hash = Common::ComputeHash64(&program_code, sizeof(program_code));
            program_code_hash_dirty = false;
        }
        return program_code_hash;
    }

    u64 GetSwizzleDataHash() {
        if (swizzle_data_hash_dirty) {
            swizzle_data_hash = Common::ComputeHash64(&swizzle_data, sizeof(swizzle_data));
            swizzle_data_hash_dirty = false;
        }
        return swizzle_data_hash;
    }

private:
    bool program_code_hash_dirty = true;
    bool swizzle_data_hash_dirty = true;
    u64 program_code_hash = 0xDEADC0DE;
    u64 swizzle_data_hash = 0xDEADC0DE;
};

class Engine;

Engine* GetEngine();
void Shutdown();

} // namespace Pica::Shader
