// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <dynarmic/A32/a32.h>
#include <dynarmic/A32/context.h>
#include <map>
#include <memory>
#include "common/common_types.h"
#include "core/arm/arm_dynarmic_cp15.h"
#include "core/core_timing.h"

namespace Memory {
class PageTable;
class MemorySystem;
} // namespace Memory

namespace Core {
class System;
} // namespace Core

class DynarmicUserCallbacks;

class ARM_Dynarmic;

class ThreadContext {
public:
    void Reset();
    void SetCpuRegister(std::size_t index, u32 value);
    void SetCpsr(u32 value);
    void SetFpscr(u32 value);

private:
    friend class ARM_Dynarmic;

    Dynarmic::A32::Context context;
    u32 fpexc = 0;
};

class ARM_Dynarmic {
public:
    explicit ARM_Dynarmic(Core::System* system, Memory::MemorySystem& memory, u32 id,
                          std::shared_ptr<Core::Timing::Timer> timer,
                          Dynarmic::ExclusiveMonitor* exclusive_monitor);

    ~ARM_Dynarmic();

    void Run();

    void SetPC(u32 pc);
    u32 GetPC() const;
    u32 GetReg(int index) const;
    void SetReg(int index, u32 value);
    u32 GetVFPReg(int index) const;
    void SetVFPReg(int index, u32 value);
    u32 GetVFPSystemReg(int reg) const;
    void SetVFPSystemReg(int reg, u32 value);
    u32 GetCPSR() const;
    void SetCPSR(u32 cpsr);
    u32 GetCP15Register(int reg);
    void SetCP15Register(int reg, u32 value);

    std::unique_ptr<ThreadContext> NewContext() const;
    void SaveContext(std::unique_ptr<ThreadContext>& context);
    void LoadContext(const std::unique_ptr<ThreadContext>& context);

    void PrepareReschedule();

    void ClearInstructionCache();
    void InvalidateCacheRange(u32 start_address, std::size_t length);
    void PageTableChanged();

    Core::Timing::Timer& GetTimer() {
        return *timer;
    }

    const Core::Timing::Timer& GetTimer() const {
        return *timer;
    }

    u32 GetID() const {
        return id;
    }

private:
    friend class DynarmicUserCallbacks;

    std::unique_ptr<Dynarmic::A32::Jit> MakeJit();

    Core::System& system;
    Memory::MemorySystem& memory;
    std::unique_ptr<DynarmicUserCallbacks> cb;

    u32 fpexc = 0;
    CP15State cp15_state;

    Dynarmic::A32::Jit* jit = nullptr;
    Memory::PageTable* current_page_table = nullptr;
    std::map<Memory::PageTable*, std::unique_ptr<Dynarmic::A32::Jit>> jits;
    Dynarmic::ExclusiveMonitor* exclusive_monitor;

    u32 id;
    std::shared_ptr<Core::Timing::Timer> timer;
};
