// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/shader/compiler.h"
#include "video_core/shader/engine.h"
#include "video_core/shader/shader.h"

namespace Pica::Shader {

Engine::Engine() = default;
Engine::~Engine() = default;

void Engine::SetupBatch(ShaderSetup& setup, unsigned int entry_point) {
    ASSERT(entry_point < MAX_PROGRAM_CODE_LENGTH);
    setup.engine_data.entry_point = entry_point;

    u64 code_hash = setup.GetProgramCodeHash();
    u64 swizzle_hash = setup.GetSwizzleDataHash();

    u64 cache_key = code_hash ^ swizzle_hash;
    auto iter = cache.find(cache_key);
    if (iter != cache.end()) {
        setup.engine_data.cached_shader = iter->second.get();
    } else {
        std::unique_ptr<Compiler> shader = std::make_unique<Compiler>();
        shader->Compile(&setup.program_code, &setup.swizzle_data);
        setup.engine_data.cached_shader = shader.get();
        cache.emplace_hint(iter, cache_key, std::move(shader));
    }
}

void Engine::Run(const ShaderSetup& setup, UnitState& state) const {
    ASSERT(setup.engine_data.cached_shader != nullptr);

    const Compiler* compiler = static_cast<const Compiler*>(setup.engine_data.cached_shader);
    compiler->Run(setup, state, setup.engine_data.entry_point);
}

} // namespace Pica::Shader
