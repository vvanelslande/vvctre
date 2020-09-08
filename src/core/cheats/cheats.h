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

class CheatBase;

class CheatEngine {
public:
    explicit CheatEngine(Core::System& system);
    ~CheatEngine();

    std::vector<std::shared_ptr<CheatBase>> GetCheats() const;
    void AddCheat(const std::shared_ptr<CheatBase>& cheat);
    void RemoveCheat(int index);
    void UpdateCheat(int index, const std::shared_ptr<CheatBase>& new_cheat);
    void LoadCheatsFromFile();
    void LoadCheatsFromStream(std::istream& stream);
    void SaveCheatsToFile() const;
    void SaveCheatsToStream(std::ostream& stream) const;

private:
    void RunCallback(std::uintptr_t user_data, int cycles_late);

    std::vector<std::shared_ptr<CheatBase>> cheats_list;
    mutable std::shared_mutex cheats_list_mutex;
    Core::TimingEventType* event;
    Core::System& system;
};
} // namespace Cheats
