// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sstream>
#include <string>
#include <vector>
#include "common/file_util.h"
#include "common/string_util.h"
#include "video_core/renderer/post_processing.h"

namespace OpenGL {

// The Dolphin shader header is added here for drop-in compatibility with most
// of Dolphin's "glsl" shaders, which use hlsl types, hence the #define's below
// It's fairly complete, but the features it's missing are:
// The font texture for the ascii shader (vvctre doesn't have an overlay font)
// GetTime (not used in any shader provided by Dolphin)
// GetOption* (used in only one shader provided by Dolphin; would require more
// configuration/frontend work)
constexpr char dolphin_shader_header[] = R"(#version 330 core

// hlsl to glsl types
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4
#define int2 ivec2
#define int3 ivec3
#define int4 ivec4

// hlsl to glsl function translation
#define frac fract
#define lerp mix

// Output variable
out float4 color;
// Input coordinates
in float2 frag_tex_coord;
// Resolution
uniform float4 i_resolution;
uniform float4 o_resolution;
// Layer
uniform int layer;

uniform sampler2D color_texture;
uniform sampler2D color_texture_r;

// Interfacing functions
float4 Sample()
{
    return texture(color_texture, frag_tex_coord);
}

float4 SampleLocation(float2 location)
{
    return texture(color_texture, location);
}

float4 SampleLayer(int layer)
{
    if(layer == 0)
        return texture(color_texture, frag_tex_coord);
    else
        return texture(color_texture_r, frag_tex_coord);
}

#define SampleOffset(offset) textureOffset(color_texture, frag_tex_coord, offset)

float2 GetResolution()
{
    return i_resolution.xy;
}

float2 GetInvResolution()
{
    return i_resolution.zw;
}

float2 GetIResolution()
{
    return i_resolution.xy;
}

float2 GetIInvResolution()
{
    return i_resolution.zw;
}

float2 GetOResolution()
{
    return o_resolution.xy;
}

float2 GetOInvResolution()
{
    return o_resolution.zw;
}

float2 GetCoordinates()
{
    return frag_tex_coord;
}

void SetOutput(float4 color_in)
{
    color = color_in;
}

)";

std::string GetPostProcessingShaderCode(const bool anaglyph, std::string_view shader) {
    std::string shader_dir = FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir);
    std::string shader_path;

    if (anaglyph) {
        shader_dir = shader_dir + "anaglyph";
    }

    // Examining the directory is done because the shader extension might have an odd case
    // This can be eliminated if it is specified that the shader extension must be lowercase
    const auto callback = [&shader, &shader_path](u64* num_entries_out,
                                                  const std::string& directory,
                                                  const std::string& virtual_name) -> bool {
        const std::string physical_name = directory + "/" + virtual_name;
        if (!FileUtil::IsDirectory(physical_name)) {
            std::size_t dot_pos = virtual_name.rfind(".");
            if (dot_pos != std::string::npos) {
                if (Common::ToLower(virtual_name.substr(dot_pos + 1)) == "glsl" &&
                    virtual_name.substr(0, dot_pos) == shader) {
                    shader_path = physical_name;
                    return false;
                }
            }
        }
        return true;
    };

    FileUtil::ForeachDirectoryEntry(nullptr, shader_dir, callback);
    if (shader_path.empty()) {
        return "";
    }

    std::ifstream file;
    OpenFStream(file, shader_path, std::ios_base::in);
    if (!file) {
        return "";
    }

    std::stringstream shader_text;
    shader_text << file.rdbuf();

    return dolphin_shader_header + shader_text.str();
}

} // namespace OpenGL
