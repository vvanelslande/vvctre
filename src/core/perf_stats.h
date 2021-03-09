// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include "common/common_types.h"
#include "common/thread.h"

namespace Core {

class PerfStats {
public:
    void EndSystemFrame();
    double GetLastFrameTimeScale() const;

private:
    mutable std::mutex object_mutex;

    /// Point when the previous system frame ended
    std::chrono::high_resolution_clock::time_point previous_frame_end =
        std::chrono::high_resolution_clock::now();

    /// Total visible duration (including frame-limiting, etc.) of the previous system frame
    std::chrono::high_resolution_clock::duration previous_frame_length =
        std::chrono::high_resolution_clock::duration::zero();
};

class FrameLimiter {
public:
    void DoFrameLimiting(std::chrono::microseconds current_system_time_us);

    void SetFrameAdvancing(bool value);
    void AdvanceFrame();
    bool FrameAdvancingEnabled() const;

private:
    /// Emulated system time (in microseconds) at the last limiter invocation
    std::chrono::microseconds previous_system_time_us{0};

    /// Walltime at the last limiter invocation
    std::chrono::high_resolution_clock::time_point previous_walltime =
        std::chrono::high_resolution_clock::now();

    /// Accumulated difference between walltime and emulated time
    std::chrono::microseconds frame_limiting_delta_err{0};

    /// Whether to use frame advancing (i.e. frame by frame)
    std::atomic<bool> frame_advancing_enabled{false};

    /// Event to advance the frame when frame advancing is enabled
    Common::Event frame_advance_event;
};

} // namespace Core
