// Copyright 2013 Dolphin Emulator Project / 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Common {

/// CPU capabilities that may be detected by this module
struct CPUCaps {
    bool sse4_1;
};

/**
 * Gets the supported capabilities of the host CPU
 * Assumes the CPU supports the CPUID instruction
 * Those that don't would likely not support vvctre at all anyway
 * @returns Reference to a CPUCaps struct with the detected host CPU capabilities
 */
const CPUCaps& GetCPUCaps();

} // namespace Common
