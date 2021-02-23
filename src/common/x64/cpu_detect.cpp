// Copyright 2013 Dolphin Emulator Project / 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/x64/cpu_detect.h"

#ifdef _WIN32
#include <intrin.h>
#else
static inline void __cpuidex(int info[4], int function_id, int subfunction_id) {
    info[0] = function_id;    // eax
    info[2] = subfunction_id; // ecx
    __asm__("cpuid"
            : "=a"(info[0]), "=b"(info[1]), "=c"(info[2]), "=d"(info[3])
            : "a"(function_id), "c"(subfunction_id));
}

static inline void __cpuid(int info[4], int function_id) {
    return __cpuidex(info, function_id, 0);
}
#endif

namespace Common {

static CPUCaps Detect() {
    CPUCaps caps = {};

    int cpu_id[4];

    __cpuid(cpu_id, 0x00000000);
    u32 max_std_fn = cpu_id[0]; // EAX

    if (max_std_fn >= 1) {
        __cpuid(cpu_id, 0x00000001);

        caps.sse4_1 = (cpu_id[2] >> 19) & 1;
    }

    return caps;
}

const CPUCaps& GetCPUCaps() {
    static CPUCaps caps = Detect();
    return caps;
}

} // namespace Common
