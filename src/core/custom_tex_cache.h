// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "common/common_types.h"
#include "core/settings.h"

namespace Core {

struct CustomTexInfo {
    u32 width;
    u32 height;
    std::vector<u8> tex;
};

// This is to avoid parsing the filename multiple times
struct CustomTexPathInfo {
    std::string path;
    u64 hash;
    Settings::PreloadCustomTexturesFolder folder;
};

class CustomTexCache {
public:
    CustomTexCache();
    ~CustomTexCache();

    bool IsTextureDumped(const u64 hash) const;
    void SetTextureDumped(const u64 hash);
    bool IsTextureCached(const u64 hash) const;
    const CustomTexInfo& LookupTexture(const u64 hash) const;
    void CacheTexture(const u64 hash, const std::vector<u8>& tex, const u32 width,
                      const u32 height);
    void AddTexturePath(const u64 hash, const std::string& path,
                        const Settings::PreloadCustomTexturesFolder folder);
    void FindCustomTextures();
    void PreloadTextures(std::function<void(std::size_t current, std::size_t total)> callback);
    bool CustomTextureExists(u64 hash) const;
    const CustomTexPathInfo& LookupTexturePathInfo(u64 hash) const;
    bool IsTexturePathMapEmpty() const;

private:
    std::unordered_set<u64> dumped_textures;
    std::unordered_map<u64, CustomTexInfo> custom_textures;
    std::unordered_map<u64, CustomTexPathInfo> custom_texture_paths;
};

} // namespace Core
