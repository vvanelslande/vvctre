// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <chrono>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <iterator>
#include <mutex>
#include <numeric>
#include <thread>
#include "core/hw/gpu.h"
#include "core/perf_stats.h"
#include "core/settings.h"

using namespace std::chrono_literals;

namespace Core {

void PerfStats::EndSystemFrame() {
    std::lock_guard<std::mutex> lock(object_mutex);

    std::chrono::high_resolution_clock::time_point frame_end =
        std::chrono::high_resolution_clock::now();

    previous_frame_length = frame_end - previous_frame_end;
    previous_frame_end = frame_end;
}

double PerfStats::GetLastFrameTimeScale() const {
    std::lock_guard<std::mutex> lock(object_mutex);

    constexpr double FRAME_LENGTH = 1.0 / GPU::SCREEN_REFRESH_RATE;

    return std::chrono::duration_cast<std::chrono::duration<double, std::chrono::seconds::period>>(
               previous_frame_length)
               .count() /
           FRAME_LENGTH;
}

void FrameLimiter::DoFrameLimiting(std::chrono::microseconds current_system_time_us) {
    if (frame_advancing_enabled) {
        // Frame advancing is enabled: wait on event instead of doing framelimiting
        frame_advance_event.Wait();
        return;
    }

    if (!Settings::values.limit_speed) {
        return;
    }

    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    const double sleep_scale = Settings::values.speed_limit / 100.0;

    // Max lag caused by slow frames. Shouldn't be more than the length of a frame at the current
    // speed percent or it will clamp too much and prevent this from properly limiting to that
    // percent. High values means it'll take longer after a slow frame to recover and start limiting
    const std::chrono::microseconds max_lag_time_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::duration<double, std::chrono::microseconds::period>(25ms / sleep_scale));
    frame_limiting_delta_err += std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::duration<double, std::chrono::microseconds::period>(
            (current_system_time_us - previous_system_time_us) / sleep_scale));
    frame_limiting_delta_err -=
        std::chrono::duration_cast<std::chrono::microseconds>(now - previous_walltime);
    frame_limiting_delta_err =
        std::clamp(frame_limiting_delta_err, -max_lag_time_us, max_lag_time_us);

    if (frame_limiting_delta_err > std::chrono::microseconds::zero()) {
        std::this_thread::sleep_for(frame_limiting_delta_err);

        const std::chrono::high_resolution_clock::time_point now_after_sleep =
            std::chrono::high_resolution_clock::now();

        frame_limiting_delta_err -=
            std::chrono::duration_cast<std::chrono::microseconds>(now_after_sleep - now);

        now = now_after_sleep;
    }

    previous_system_time_us = current_system_time_us;
    previous_walltime = now;
}

void FrameLimiter::SetFrameAdvancing(bool value) {
    const bool was_enabled = frame_advancing_enabled.exchange(value);
    if (was_enabled && !value) {
        // Set the event to let emulation continue
        frame_advance_event.Set();
    }
}

void FrameLimiter::AdvanceFrame() {
    if (!frame_advancing_enabled) {
        // Start frame advancing
        frame_advancing_enabled = true;
    }

    frame_advance_event.Set();
}

bool FrameLimiter::FrameAdvancingEnabled() const {
    return frame_advancing_enabled;
}

} // namespace Core
