// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <dynarmic/A32/a32.h>
#include <dynarmic/exclusive_monitor.h>
#include "common/assert.h"
#include "core/arm/arm_dynarmic.h"
#include "core/arm/arm_dynarmic_cp15.h"
#include "core/core.h"
#include "core/hle/kernel/svc.h"
#include "core/memory.h"

class DynarmicUserCallbacks final : public Dynarmic::A32::UserCallbacks {
public:
    explicit DynarmicUserCallbacks(ARM_Dynarmic& parent)
        : parent(parent), svc_context(parent.system), memory(parent.memory) {}

    ~DynarmicUserCallbacks() = default;

    std::uint8_t MemoryRead8(VAddr vaddr) override {
        return memory.Read8(vaddr);
    }

    std::uint16_t MemoryRead16(VAddr vaddr) override {
        return memory.Read16(vaddr);
    }

    std::uint32_t MemoryRead32(VAddr vaddr) override {
        return memory.Read32(vaddr);
    }

    std::uint64_t MemoryRead64(VAddr vaddr) override {
        return memory.Read64(vaddr);
    }

    void MemoryWrite8(VAddr vaddr, std::uint8_t value) override {
        memory.Write8(vaddr, value);
    }

    void MemoryWrite16(VAddr vaddr, std::uint16_t value) override {
        memory.Write16(vaddr, value);
    }

    void MemoryWrite32(VAddr vaddr, std::uint32_t value) override {
        memory.Write32(vaddr, value);
    }

    void MemoryWrite64(VAddr vaddr, std::uint64_t value) override {
        memory.Write64(vaddr, value);
    }

    bool MemoryWriteExclusive8(VAddr vaddr, std::uint8_t value,
                               [[maybe_unused]] std::uint8_t expected) override {
        MemoryWrite8(vaddr, value);
        return true;
    }

    bool MemoryWriteExclusive16(VAddr vaddr, std::uint16_t value,
                                [[maybe_unused]] std::uint16_t expected) override {
        MemoryWrite16(vaddr, value);
        return true;
    }

    bool MemoryWriteExclusive32(VAddr vaddr, std::uint32_t value,
                                [[maybe_unused]] std::uint32_t expected) override {
        MemoryWrite32(vaddr, value);
        return true;
    }

    bool MemoryWriteExclusive64(VAddr vaddr, std::uint64_t value,
                                [[maybe_unused]] std::uint64_t expected) override {
        MemoryWrite64(vaddr, value);
        return true;
    }

    void InterpreterFallback(VAddr pc, std::size_t num_instructions) override {}

    void CallSVC(std::uint32_t swi) override {
        svc_context.CallSVC(swi);
    }

    void ExceptionRaised(VAddr pc, Dynarmic::A32::Exception exception) override {
        switch (exception) {
        case Dynarmic::A32::Exception::UndefinedInstruction:
        case Dynarmic::A32::Exception::UnpredictableInstruction:
        case Dynarmic::A32::Exception::Breakpoint:
            break;
        case Dynarmic::A32::Exception::SendEvent:
        case Dynarmic::A32::Exception::SendEventLocal:
        case Dynarmic::A32::Exception::WaitForInterrupt:
        case Dynarmic::A32::Exception::WaitForEvent:
        case Dynarmic::A32::Exception::Yield:
        case Dynarmic::A32::Exception::PreloadData:
        case Dynarmic::A32::Exception::PreloadDataWithIntentToWrite:
            return;
        }

        ASSERT_MSG(false, "ExceptionRaised(exception = {}, pc = {:08X}, code = {:08X})",
                   static_cast<std::size_t>(exception), pc, MemoryReadCode(pc));
    }

    void AddTicks(std::uint64_t ticks) override {
        parent.GetTimer().AddTicks(ticks);
    }

    std::uint64_t GetTicksRemaining() override {
        const s64 ticks = parent.GetTimer().GetDowncount();
        return static_cast<u64>(ticks <= 0 ? 0 : ticks);
    }

    ARM_Dynarmic& parent;
    Kernel::SVCContext svc_context;
    Memory::MemorySystem& memory;
};

void ThreadContext::Reset() {
    context.Regs() = {};
    context.SetCpsr(0);
    context.ExtRegs() = {};
    context.SetFpscr(0);
    fpexc = 0;
}

void ThreadContext::SetCpuRegister(std::size_t index, u32 value) {
    context.Regs()[index] = value;
}

void ThreadContext::SetCpsr(u32 value) {
    context.SetCpsr(value);
}

void ThreadContext::SetFpscr(u32 value) {
    context.SetFpscr(value);
}

ARM_Dynarmic::ARM_Dynarmic(Core::System* system, Memory::MemorySystem& memory, u32 id,
                           std::shared_ptr<Core::Timing::Timer> timer,
                           Dynarmic::ExclusiveMonitor* exclusive_monitor)
    : id(id), timer(timer), system(*system), memory(memory),
      cb(std::make_unique<DynarmicUserCallbacks>(*this)), exclusive_monitor(exclusive_monitor) {
    PageTableChanged();
}

ARM_Dynarmic::~ARM_Dynarmic() = default;

void ARM_Dynarmic::Run() {
    ASSERT(memory.GetCurrentPageTable() == current_page_table);
    jit->Run();
}

void ARM_Dynarmic::SetPC(u32 pc) {
    jit->Regs()[15] = pc;
}

u32 ARM_Dynarmic::GetPC() const {
    return jit->Regs()[15];
}

u32 ARM_Dynarmic::GetReg(int index) const {
    return jit->Regs()[index];
}

void ARM_Dynarmic::SetReg(int index, u32 value) {
    jit->Regs()[index] = value;
}

u32 ARM_Dynarmic::GetVFPReg(int index) const {
    return jit->ExtRegs()[index];
}

void ARM_Dynarmic::SetVFPReg(int index, u32 value) {
    jit->ExtRegs()[index] = value;
}

u32 ARM_Dynarmic::GetVFPSystemReg(int reg) const {
    switch (reg) {
    case 1: // FPSCR
        return jit->Fpscr();
    case 2: // FPEXC
        return fpexc;
    default:
        UNREACHABLE_MSG("Unknown VFP system register: {}", static_cast<std::size_t>(reg));
    }
}

void ARM_Dynarmic::SetVFPSystemReg(int reg, u32 value) {
    switch (reg) {
    case 1: // FPSCR
        jit->SetFpscr(value);
        return;
    case 2: // FPEXC
        fpexc = value;
        return;
    default:
        UNREACHABLE_MSG("Unknown VFP system register: {}", static_cast<std::size_t>(reg));
    }
}

u32 ARM_Dynarmic::GetCPSR() const {
    return jit->Cpsr();
}

void ARM_Dynarmic::SetCPSR(u32 cpsr) {
    jit->SetCpsr(cpsr);
}

u32 ARM_Dynarmic::GetCP15Register(int reg) {
    switch (reg) {
    case 69: // UPRW
        return cp15_state.cp15_thread_uprw;
    case 70: // URO
        return cp15_state.cp15_thread_uro;
    default:
        UNREACHABLE_MSG("Unknown CP15 register: {}", static_cast<std::size_t>(reg));
    }
}

void ARM_Dynarmic::SetCP15Register(int reg, u32 value) {
    switch (reg) {
    case 69: // UPRW
        cp15_state.cp15_thread_uprw = value;
        return;
    case 70: // URO
        cp15_state.cp15_thread_uro = value;
        return;
    default:
        UNREACHABLE_MSG("Unknown CP15 register: {}", static_cast<std::size_t>(reg));
    }
}

std::unique_ptr<ThreadContext> ARM_Dynarmic::NewContext() const {
    return std::make_unique<ThreadContext>();
}

void ARM_Dynarmic::SaveContext(std::unique_ptr<ThreadContext>& context) {
    ASSERT(context != nullptr);

    jit->SaveContext(context->context);
    context->fpexc = fpexc;
}

void ARM_Dynarmic::LoadContext(const std::unique_ptr<ThreadContext>& context) {
    ASSERT(context != nullptr);

    jit->LoadContext(context->context);
    fpexc = context->fpexc;
}

void ARM_Dynarmic::PrepareReschedule() {
    if (jit->IsExecuting()) {
        jit->HaltExecution();
    }
}

void ARM_Dynarmic::ClearInstructionCache() {
    for (const auto& j : jits) {
        j.second->ClearCache();
    }
}

void ARM_Dynarmic::InvalidateCacheRange(u32 start_address, std::size_t length) {
    jit->InvalidateCacheRange(start_address, length);
}

void ARM_Dynarmic::PageTableChanged() {
    current_page_table = memory.GetCurrentPageTable();

    auto iter = jits.find(current_page_table);
    if (iter != jits.end()) {
        jit = iter->second.get();
        return;
    }

    auto new_jit = MakeJit();
    jit = new_jit.get();
    jits.emplace(current_page_table, std::move(new_jit));
}

std::unique_ptr<Dynarmic::A32::Jit> ARM_Dynarmic::MakeJit() {
    Dynarmic::A32::UserConfig config;
    config.global_monitor = exclusive_monitor;
    config.callbacks = cb.get();
    if (current_page_table) {
        config.page_table = &current_page_table->pointers;
        config.fastmem_pointer = current_page_table->fastmem_base.Get();
    }
    config.coprocessors[15] = std::make_shared<DynarmicCP15>(cp15_state);
    config.define_unpredictable_behaviour = true;
    config.absolute_offset_page_table = true;
    return std::make_unique<Dynarmic::A32::Jit>(config);
}
