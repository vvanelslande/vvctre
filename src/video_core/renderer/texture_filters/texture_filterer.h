// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <glad/glad.h>
#include <memory>
#include <string_view>
#include <vector>
#include "common/common_types.h"
#include "common/math_util.h"
#include "video_core/renderer/surface_params.h"
#include "video_core/renderer/texture_filters/texture_filter_base.h"

namespace OpenGL {

class TextureFilterer {
public:
    explicit TextureFilterer(std::string_view filter_name, u16 scale_factor);

    // Returns true if the filter actually changed
    bool Reset(std::string_view new_filter_name, u16 new_scale_factor);

    // Returns true if there is no active filter
    bool IsNull() const;

    // Returns true if the texture was able to be filtered
    bool Filter(GLuint src_tex, const Common::Rectangle<u32>& src_rect, GLuint dst_tex,
                const Common::Rectangle<u32>& dst_rect, SurfaceParams::SurfaceType type,
                GLuint read_fb_handle, GLuint draw_fb_handle);

    static std::vector<std::string_view> GetFilterNames();

private:
    std::string_view filter_name = "none";
    std::unique_ptr<TextureFilterBase> filter;
};

} // namespace OpenGL
