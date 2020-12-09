// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <string>
#include <unordered_set>

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/regs.h"
#include "video_core/renderer_opengl/gl_shader_decompiler.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"

namespace Core {
class System;
} // namespace Core

namespace FileUtil {
class IOFile;
} // namespace FileUtil

namespace OpenGL {

class ShaderDiskCacheEntry {
public:
    explicit ShaderDiskCacheEntry(u64 unique_identifier, ProgramType type, Pica::Regs registers,
                                  std::vector<u32> code);
    ShaderDiskCacheEntry() = default;
    ~ShaderDiskCacheEntry() = default;

    bool Load(FileUtil::IOFile& file);
    bool Save(FileUtil::IOFile& file) const;

    u64 GetUniqueIdentifier() const {
        return unique_identifier;
    }

    ProgramType GetType() const {
        return type;
    }

    const std::vector<u32>& GetCode() const {
        return code;
    }

    const Pica::Regs& GetRegisters() const {
        return registers;
    }

private:
    u64 unique_identifier{};
    ProgramType type{};
    Pica::Regs registers{};
    std::vector<u32> code;
};

class ShaderDiskCache {
public:
    explicit ShaderDiskCache(bool separable);
    ~ShaderDiskCache() = default;

    std::optional<std::vector<ShaderDiskCacheEntry>> Load();
    void Add(const ShaderDiskCacheEntry& entry);
    void Delete();

private:
    bool IsUsable() const;
    FileUtil::IOFile Append();
    std::string GetCacheFilePath();
    u64 GetProgramID();

    std::unordered_set<u64> hashes;
    bool tried_to_load = false;
    bool separable = false;
    u64 program_id = 0;
};

} // namespace OpenGL
