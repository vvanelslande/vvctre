// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>
#include "common/common_types.h"
#include "video_core/shader/shader.h"

namespace Pica::Shader {

class Compiler;

class Engine {
public:
    Engine();
    ~Engine();

    void SetupBatch(ShaderSetup& setup, unsigned int entry_point);
    void Run(const ShaderSetup& setup, UnitState& state) const;

private:
    std::unordered_map<u64, std::unique_ptr<Compiler>> cache;
};

} // namespace Pica::Shader
