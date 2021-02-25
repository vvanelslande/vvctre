// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>

namespace Log {

struct Entry;

std::string FormatLogMessage(const Entry& entry);
void PrintColoredMessage(const Entry& entry);

} // namespace Log
