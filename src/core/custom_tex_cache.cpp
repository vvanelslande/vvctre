// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/format.h>
#include <stb_image.h>
#include "common/file_util.h"
#include "common/texture.h"
#include "core/core.h"
#include "core/custom_tex_cache.h"

namespace Core {

CustomTexCache::CustomTexCache() = default;

CustomTexCache::~CustomTexCache() = default;

bool CustomTexCache::IsTextureDumped(const u64 hash) const {
    return dumped_textures.count(hash);
}

void CustomTexCache::SetTextureDumped(const u64 hash) {
    dumped_textures.insert(hash);
}

bool CustomTexCache::IsTextureCached(const u64 hash) const {
    return custom_textures.count(hash);
}

const CustomTexInfo& CustomTexCache::LookupTexture(const u64 hash) const {
    return custom_textures.at(hash);
}

void CustomTexCache::CacheTexture(const u64 hash, const std::vector<u8>& tex, const u32 width,
                                  const u32 height) {
    custom_textures[hash] = {width, height, tex};
}

void CustomTexCache::AddTexturePath(const u64 hash, const std::string& path,
                                    const Settings::PreloadCustomTexturesFolder folder) {
    if (custom_texture_paths.count(hash)) {
        LOG_ERROR(Core, "Textures {} and {} conflict!", custom_texture_paths[hash].path, path);
    } else {
        custom_texture_paths[hash] = {path, hash, folder};
    }
}

void CustomTexCache::FindCustomTextures() {
    // Custom textures are currently stored as
    // [TitleID]/tex1_[width]x[height]_[64-bit hash]_[format].[extension]

    const auto f = [this](Settings::PreloadCustomTexturesFolder folder) {
        const std::string path = fmt::format(
            "{}textures/{:016X}/",
            FileUtil::GetUserPath(folder == Settings::PreloadCustomTexturesFolder::Load
                                      ? FileUtil::UserPath::LoadDir
                                      : FileUtil::UserPath::PreloadDir),
            Core::System::GetInstance().Kernel().GetCurrentProcess()->codeset->program_id);

        if (FileUtil::Exists(path)) {
            FileUtil::FSTEntry texture_dir;
            std::vector<FileUtil::FSTEntry> textures;
            FileUtil::ScanDirectoryTree(path, texture_dir, 64);
            FileUtil::GetAllFilesFromNestedEntries(texture_dir, textures);

            for (const auto& file : textures) {
                if (file.is_directory || (file.virtual_name.substr(0, 5) != "tex1_")) {
                    continue;
                }

                u32 width;
                u32 height;
                u64 hash;

                if (std::sscanf(file.virtual_name.c_str(), "tex1_%ux%u_%llX%*s", &width, &height,
                                &hash) == 3) {
                    AddTexturePath(hash, file.physical_name, folder);
                }
            }
        }
    };

    f(Settings::PreloadCustomTexturesFolder::Load);

    if (Settings::values.preload_custom_textures &&
        Settings::values.preload_custom_textures_folder ==
            Settings::PreloadCustomTexturesFolder::Preload) {
        f(Settings::PreloadCustomTexturesFolder::Preload);
    }
}

void CustomTexCache::PreloadTextures(
    std::function<void(std::size_t current, std::size_t total)> callback) {
    std::size_t current = 1;
    for (const auto& path : custom_texture_paths) {
        if (path.second.folder == Settings::values.preload_custom_textures_folder) {
            Core::CustomTexInfo tex_info;
            unsigned char* image =
                stbi_load(path.second.path.c_str(), reinterpret_cast<int*>(&tex_info.width),
                          reinterpret_cast<int*>(&tex_info.height), nullptr, 4);
            if (image != nullptr) {
                tex_info.tex.resize(tex_info.width * tex_info.height * 4);
                std::memcpy(tex_info.tex.data(), image, tex_info.tex.size());
                free(image);

                // Make sure the texture size is a power of 2
                std::bitset<32> width_bits(tex_info.width);
                std::bitset<32> height_bits(tex_info.height);
                if (width_bits.count() == 1 && height_bits.count() == 1) {
                    LOG_DEBUG(Render_OpenGL, "Loaded custom texture from {}", path.second.path);
                    Common::FlipRGBA8Texture(tex_info.tex, tex_info.width, tex_info.height);
                    CacheTexture(path.second.hash, tex_info.tex, tex_info.width, tex_info.height);
                    callback(current++, custom_texture_paths.size());
                } else {
                    LOG_ERROR(Render_OpenGL, "Texture {} size is not a power of 2",
                              path.second.path);
                }
            } else {
                LOG_ERROR(Render_OpenGL, "Failed to load custom texture {}", path.second.path);
            }
        }
    }
}

bool CustomTexCache::CustomTextureExists(u64 hash) const {
    return custom_texture_paths.count(hash);
}

const CustomTexPathInfo& CustomTexCache::LookupTexturePathInfo(u64 hash) const {
    return custom_texture_paths.at(hash);
}

bool CustomTexCache::IsTexturePathMapEmpty() const {
    return custom_texture_paths.size() == 0;
}

} // namespace Core
