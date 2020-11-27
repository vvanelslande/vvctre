// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <fmt/format.h>
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "core/settings.h"
#include "video_core/renderer_opengl/gl_shader_disk_cache.h"

namespace OpenGL {

constexpr u8 DISK_SHADER_CACHE_VERSION = 1;

ShaderDiskCacheEntry::ShaderDiskCacheEntry(u64 unique_identifier, ProgramType type,
                                           Pica::Regs registers, std::vector<u32> code)
    : unique_identifier(unique_identifier), type(type), registers(registers),
      code(std::move(code)) {}

bool ShaderDiskCacheEntry::Load(FileUtil::IOFile& file) {
    if (file.ReadBytes(&unique_identifier, sizeof(unique_identifier)) !=
            sizeof(unique_identifier) ||
        file.ReadBytes(&type, sizeof(type)) != sizeof(type)) {
        return false;
    }

    u64 reg_array_len;
    if (file.ReadBytes(&reg_array_len, sizeof(u64)) != sizeof(u64)) {
        return false;
    }

    if (file.ReadArray(registers.reg_array.data(), reg_array_len) != reg_array_len) {
        return false;
    }

    // Read in type specific configuration
    if (type == ProgramType::VS) {
        u64 code_len;
        if (file.ReadBytes(&code_len, sizeof(u64)) != sizeof(u64)) {
            return false;
        }
        code.resize(code_len);
        if (file.ReadArray(code.data(), code_len) != code_len) {
            return false;
        }
    }

    return true;
}

bool ShaderDiskCacheEntry::Save(FileUtil::IOFile& file) const {
    if (file.WriteObject(unique_identifier) != 1 || file.WriteObject(static_cast<u8>(type)) != 1) {
        return false;
    }

    // Just for future proofing, save the sizes of the array to the file
    const std::size_t reg_array_len = Pica::Regs::NUM_REGS;
    if (file.WriteObject(static_cast<u64>(reg_array_len)) != 1) {
        return false;
    }
    if (file.WriteArray(registers.reg_array.data(), reg_array_len) != reg_array_len) {
        return false;
    }

    if (type == ProgramType::VS) {
        const std::size_t code_len = code.size();
        if (file.WriteObject(static_cast<u64>(code_len)) != 1) {
            return false;
        }
        if (file.WriteArray(code.data(), code_len) != code_len) {
            return false;
        }
    }
    return true;
}

ShaderDiskCache::ShaderDiskCache(bool separable) : separable(separable) {}

std::optional<std::vector<ShaderDiskCacheEntry>> ShaderDiskCache::Load() {
    if (GetProgramID() == 0) {
        return {};
    }

    tried_to_load = true;

    FileUtil::IOFile file(GetCacheFilePath(), "rb");
    if (!file.IsOpen()) {
        LOG_INFO(Render_OpenGL, "No disk shader cache found");
        return {};
    }

    u8 version;
    if (file.ReadBytes(&version, sizeof(version)) != sizeof(version)) {
        LOG_ERROR(Render_OpenGL, "Failed to read disk shader cache version");
        return std::nullopt;
    }
    if (version != DISK_SHADER_CACHE_VERSION) {
        LOG_INFO(Render_OpenGL, "Different disk shader cache version, deleting");
        file.Close();
        Delete();
        return std::nullopt;
    }

    // Version is valid, load the shaders
    std::vector<ShaderDiskCacheEntry> entries;
    while (file.Tell() < file.GetSize()) {
        ShaderDiskCacheEntry entry;
        if (!entry.Load(file)) {
            LOG_ERROR(Render_OpenGL, "Failed to load disk shader cache entry");
            return std::nullopt;
        }
        hashes.insert(entry.GetUniqueIdentifier());
        entries.push_back(std::move(entry));
    }

    LOG_INFO(Render_OpenGL, "Found a disk shader cache with {} entries", entries.size());
    return {entries};
}

void ShaderDiskCache::Add(const ShaderDiskCacheEntry& entry) {
    if (!IsUsable()) {
        return;
    }

    const u64 hash = entry.GetUniqueIdentifier();
    if (hashes.count(hash)) {
        return;
    }

    if (!FileUtil::CreateDir(FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir))) {
        LOG_ERROR(Render_OpenGL, "Failed to create user/shaders");
        return;
    }

    const std::string file_path = GetCacheFilePath();
    const bool existed = FileUtil::Exists(file_path);

    FileUtil::IOFile file(file_path, "ab");
    if (!file.IsOpen()) {
        LOG_ERROR(Render_OpenGL, "Failed to open disk shader cache");
        return;
    }
    if (!existed || file.GetSize() == 0) {
        // If the file didn't exist, write its version
        if (file.WriteObject(DISK_SHADER_CACHE_VERSION) != 1) {
            LOG_ERROR(Render_OpenGL, "Failed to write disk shader cache version");
            return;
        }
    }

    if (!entry.Save(file)) {
        LOG_ERROR(Render_OpenGL,
                  "Failed to save disk shader cache entry, deleting disk shader cache");
        file.Close();
        Delete();
        return;
    }

    hashes.insert(hash);
}

bool ShaderDiskCache::IsUsable() const {
    return tried_to_load;
}

std::string ShaderDiskCache::GetCacheFilePath() {
    return FileUtil::SanitizePath(fmt::format(
        "{}/{:016X}.vsc", FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir), GetProgramID()));
}

u64 ShaderDiskCache::GetProgramID() {
    if (program_id != 0) {
        return program_id;
    }
    if (Core::System::GetInstance().GetAppLoader().ReadProgramId(program_id) !=
        Loader::ResultStatus::Success) {
        return 0;
    }
    return program_id;
}

void ShaderDiskCache::Delete() {
    if (!FileUtil::Delete(GetCacheFilePath())) {
        LOG_ERROR(Render_OpenGL, "Failed to delete disk shader cache");
    }
}

} // namespace OpenGL
