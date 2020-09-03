// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class HLERequestContext;
}

namespace Service::ERR {

struct ErrInfo {
    struct {
        u8 specifier;          // 0x0
        u8 rev_high;           // 0x1
        u16 rev_low;           // 0x2
        u32 result_code;       // 0x4
        u32 pc_address;        // 0x8
        u32 pid;               // 0xC
        u32 title_id_low;      // 0x10
        u32 title_id_high;     // 0x14
        u32 app_title_id_low;  // 0x18
        u32 app_title_id_high; // 0x1C
    } common;

    union {
        struct {
            char data[0x60]; // 0x20
        } generic;

        struct {
            struct {
                u8 type;
                INSERT_PADDING_BYTES(3);
                u32 sr;
                u32 ar;
                u32 fpexc;
                u32 fpinst;
                u32 fpinst2;
            } info;

            struct {
                std::array<u32, 16> arm_regs;
                u32 cpsr;
            } context;

            INSERT_PADDING_WORDS(1);
        } exception;

        struct {
            char message[0x60]; // 0x20
        } result_failure;
    };
};

/// Interface to "err:f" service
class ERR_F final : public ServiceFramework<ERR_F> {
public:
    explicit ERR_F(Core::System& system);
    ~ERR_F();

private:
    /* ThrowFatalError function
     * Inputs:
     *       0 : Header code [0x00010800]
     *    1-32 : FatalErrInfo
     * Outputs:
     *       0 : Header code
     *       1 : Result code
     */
    void ThrowFatalError(Kernel::HLERequestContext& ctx);

    Core::System& system;
};

void InstallInterfaces(Core::System& system);

extern ErrInfo errinfo;

} // namespace Service::ERR
