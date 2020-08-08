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

/**
 * Class to manage and query performance/timing statistics. All public functions of this class are
 * thread-safe unless stated otherwise.
 */
class PerfStats {
public:
    ~PerfStats();

    using Clock = std::chrono::high_resolution_clock;

    void BeginSystemFrame();
    void EndSystemFrame();

    /**
     * Gets the ratio between walltime and the emulated time of the previous system frame. This is
     * useful for scaling inputs or outputs moving between the two time domains.
     */
    double GetLastFrameTimeScale() const;

private:
    mutable std::mutex object_mutex;

    /// Current index for writing to the perf_history array
    std::size_t current_index{0};

    /// Stores an hour of historical frametime data useful for processing and tracking performance
    /// regressions with code changes.
    std::array<double, 216000> perf_history{};

    /// Point when the previous system frame ended
    Clock::time_point previous_frame_end = Clock::now();

    /// Point when the current system frame began
    Clock::time_point frame_begin = Clock::now();

    /// Total visible duration (including frame-limiting, etc.) of the previous system frame
    Clock::duration previous_frame_length = Clock::duration::zero();
};

class FrameLimiter {
public:
    using Clock = std::chrono::high_resolution_clock;

    void DoFrameLimiting(std::chrono::microseconds current_system_time_us);

    void SetFrameAdvancing(bool value);
    void AdvanceFrame();
    bool FrameAdvancingEnabled() const;

private:
    /// Emulated system time (in microseconds) at the last limiter invocation
    std::chrono::microseconds previous_system_time_us{0};

    /// Walltime at the last limiter invocation
    Clock::time_point previous_walltime = Clock::now();

    /// Accumulated difference between walltime and emulated time
    std::chrono::microseconds frame_limiting_delta_err{0};

    /// Whether to use frame advancing (i.e. frame by frame)
    std::atomic_bool frame_advancing_enabled;

    /// Event to advance the frame when frame advancing is enabled
    Common::Event frame_advance_event;
};

} // namespace Core
