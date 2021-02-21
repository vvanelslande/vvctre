// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_types.h"

namespace Camera {

std::vector<u16> ConvertRgb24ToYuv422(const std::vector<unsigned char>& source, int width,
                                      int height);

std::vector<unsigned char> FlipRgb24ImageHorizontally(const int width, const int height,
                                                      const std::vector<unsigned char>& image);

std::vector<unsigned char> FlipRgb24ImageVertically(const int width, const int height,
                                                    const std::vector<unsigned char>& image);

} // namespace Camera
