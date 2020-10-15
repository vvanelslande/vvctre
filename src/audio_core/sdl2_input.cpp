// This is based on cubeb_input.cpp
// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include <vector>
#include <SDL.h>
#include <cubeb/cubeb.h>
#include "audio_core/sdl2_input.h"
#include "common/logging/log.h"

namespace AudioCore {

using SampleQueue = Common::SPSCQueue<Frontend::Mic::Samples>;

struct SDL2Input::Impl {
    SDL_AudioDeviceID device_id = 0;

    std::unique_ptr<SampleQueue> sample_queue{};
    u8 sample_size_in_bytes = 0;

    static void DataCallback(void* impl_, u8* buffer, int buffer_size);
};

SDL2Input::SDL2Input(std::string device_name) : device_name(std::move(device_name)) {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        LOG_CRITICAL(Service_MIC, "SDL_Init(SDL_INIT_AUDIO) failed");
        return;
    }
    impl = std::make_unique<Impl>();
    impl->sample_queue = std::make_unique<SampleQueue>();
}

SDL2Input::~SDL2Input() {
    if (impl->device_id <= 0 || !is_sampling) {
        return;
    }

    SDL_CloseAudioDevice(impl->device_id);
}

void SDL2Input::StartSampling(const Frontend::Mic::Parameters& params) {
    if (params.sign == Frontend::Mic::Signedness::Unsigned) {
        LOG_ERROR(Audio,
                  "Application requested unsupported unsigned PCM format. Falling back to signed");
    }

    impl->sample_size_in_bytes = params.sample_size / 8;

    parameters = params;
    is_sampling = true;

    SDL_AudioSpec desired_audiospec;
    SDL_zero(desired_audiospec);
    desired_audiospec.format = AUDIO_S16;
    desired_audiospec.channels = 1;
    desired_audiospec.freq = params.sample_rate;
    desired_audiospec.samples = (params.sample_rate / 60) * 2;
    desired_audiospec.userdata = impl.get();
    desired_audiospec.callback = &Impl::DataCallback;

    SDL_AudioSpec obtained_audiospec;
    SDL_zero(obtained_audiospec);

    impl->device_id = SDL_OpenAudioDevice(
        (device_name != "auto" && !device_name.empty()) ? device_name.c_str() : nullptr, 1,
        &desired_audiospec, &obtained_audiospec, 0);
    if (impl->device_id <= 0) {
        LOG_ERROR(Service_MIC, "SDL_OpenAudioDevice failed: {}", SDL_GetError());
        return;
    }

    SDL_PauseAudioDevice(impl->device_id, 0);
}

void SDL2Input::StopSampling() {
    // TODO(xperia64): Destroy the stream for now to avoid a leak because StartSampling
    // reinitializes the stream every time
    if (impl == nullptr || impl->device_id <= 0 || !is_sampling) {
        return;
    }
    SDL_CloseAudioDevice(impl->device_id);
    impl->device_id = 0;
    is_sampling = false;
}

void SDL2Input::AdjustSampleRate(u32 sample_rate) {
    parameters.sample_rate = sample_rate;
    if (impl == nullptr || impl->device_id <= 0 || !is_sampling) {
        return;
    }
    StopSampling();
    StartSampling(parameters);
}

Frontend::Mic::Samples SDL2Input::Read() {
    if (impl == nullptr) {
        return {};
    }
    Frontend::Mic::Samples samples{};
    Frontend::Mic::Samples queue;
    while (impl->sample_queue->Pop(queue)) {
        samples.insert(samples.end(), queue.begin(), queue.end());
    }
    return samples;
}

void SDL2Input::Impl::DataCallback(void* impl_, u8* buffer, int buffer_size) {
    Impl* impl = static_cast<Impl*>(impl_);
    if (impl == nullptr) {
        return;
    }

    constexpr auto resample_s16_s8 = [](s16 sample) {
        return static_cast<u8>(static_cast<u16>(sample) >> 8);
    };

    const std::size_t num_frames = buffer_size / 2;

    std::vector<u8> samples{};
    samples.reserve(num_frames * impl->sample_size_in_bytes);
    if (impl->sample_size_in_bytes == 1) {
        // If the sample format is 8bit, then resample back to 8bit before passing back to core
        for (std::size_t i = 0; i < num_frames; i++) {
            s16 data;
            std::memcpy(&data, buffer + i * 2, sizeof(data));
            samples.push_back(resample_s16_s8(data));
        }
    } else {
        // Otherwise copy all of the samples to the buffer (which will be treated as s16 by core)
        samples.insert(samples.begin(), buffer, buffer + num_frames * impl->sample_size_in_bytes);
    }
    impl->sample_queue->Push(samples);
}

std::vector<std::string> ListSDL2InputDevices() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        LOG_CRITICAL(Audio_Sink, "SDL_InitSubSystem failed with: {}", SDL_GetError());
        return {};
    }

    std::vector<std::string> device_list;
    const int device_count = SDL_GetNumAudioDevices(1);
    for (int i = 0; i < device_count; ++i) {
        device_list.push_back(SDL_GetAudioDeviceName(i, 1));
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    return device_list;
}

} // namespace AudioCore
