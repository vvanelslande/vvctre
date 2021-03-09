// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <iostream>
#include <memory>
#include <shared_mutex>
#include <vector>
#include "common/common_types.h"

namespace Core {
class System;
struct TimingEventType;
} // namespace Core

namespace CoreTiming {
struct EventType;
} // namespace CoreTiming

namespace Cheats {

class Cheat;

class Engine {
public:
    explicit Engine(Core::System& system);
    ~Engine();

    std::vector<std::shared_ptr<Cheat>> GetCheats() const;
    void Add(const std::shared_ptr<Cheat>& cheat);
    void Remove(const int index);
    void Update(const int index, const std::shared_ptr<Cheat>& new_cheat);
    void Load();
    void Save() const;

private:
    void RunCallback(std::uintptr_t user_data, int cycles_late);

    std::vector<std::shared_ptr<Cheat>> cheats;
    mutable std::shared_mutex cheats_mutex;
    Core::TimingEventType* event;
    Core::System& system;
};

} // namespace Cheats
