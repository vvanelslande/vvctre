// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "audio_core/dsp_interface.h"
#include "core/hle/kernel/event.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::DSP {

class DSP_DSP final : public ServiceFramework<DSP_DSP> {
public:
    explicit DSP_DSP(Core::System& system);
    ~DSP_DSP();

    /// There are three types of interrupts
    static constexpr std::size_t NUM_INTERRUPT_TYPE = 3;
    enum class InterruptType : u32 { Zero = 0, One = 1, Pipe = 2 };

    /// Actual service implementation only has 6 'slots' for interrupts.
    static constexpr std::size_t MAX_NUMBER_OF_INTERRUPT_EVENTS = 6;

    /// Signal interrupt on pipe
    void SignalInterrupt(InterruptType type, AudioCore::DspPipe pipe);

private:
    void RecvData(Kernel::HLERequestContext& ctx);
    void RecvDataIsReady(Kernel::HLERequestContext& ctx);
    void SetSemaphore(Kernel::HLERequestContext& ctx);
    void ConvertProcessAddressFromDspDram(Kernel::HLERequestContext& ctx);
    void WriteProcessPipe(Kernel::HLERequestContext& ctx);
    void ReadPipe(Kernel::HLERequestContext& ctx);
    void GetPipeReadableSize(Kernel::HLERequestContext& ctx);
    void ReadPipeIfPossible(Kernel::HLERequestContext& ctx);
    void LoadComponent(Kernel::HLERequestContext& ctx);
    void UnloadComponent(Kernel::HLERequestContext& ctx);
    void FlushDataCache(Kernel::HLERequestContext& ctx);
    void InvalidateDataCache(Kernel::HLERequestContext& ctx);
    void RegisterInterruptEvents(Kernel::HLERequestContext& ctx);
    void GetSemaphoreEventHandle(Kernel::HLERequestContext& ctx);
    void SetSemaphoreMask(Kernel::HLERequestContext& ctx);
    void GetHeadphoneStatus(Kernel::HLERequestContext& ctx);
    void ForceHeadphoneOut(Kernel::HLERequestContext& ctx);

    /// Returns the Interrupt Event for a given pipe
    std::shared_ptr<Kernel::Event>& GetInterruptEvent(InterruptType type, AudioCore::DspPipe pipe);

    /// Checks if we are trying to register more than 6 events
    bool HasTooManyEventsRegistered() const;

    Core::System& system;

    std::shared_ptr<Kernel::Event> semaphore_event;
    u16 preset_semaphore = 0;

    std::shared_ptr<Kernel::Event> interrupt_zero = nullptr; /// Currently unknown purpose
    std::shared_ptr<Kernel::Event> interrupt_one = nullptr;  /// Currently unknown purpose

    /// Each DSP pipe has an associated interrupt
    std::array<std::shared_ptr<Kernel::Event>, AudioCore::num_dsp_pipe> pipes = {{}};
};

void InstallInterfaces(Core::System& system);

} // namespace Service::DSP
