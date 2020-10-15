// This is based on cubeb_input.h
// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "core/frontend/mic.h"

namespace AudioCore {

class SDL2Input final : public Frontend::Mic::Interface {
public:
    explicit SDL2Input(std::string device_name);
    ~SDL2Input() override;

    void StartSampling(const Frontend::Mic::Parameters& params) override;
    void StopSampling() override;
    void AdjustSampleRate(u32 sample_rate) override;
    Frontend::Mic::Samples Read() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::string device_name;
};

std::vector<std::string> ListSDL2InputDevices();

} // namespace AudioCore
