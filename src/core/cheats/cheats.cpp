// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fstream>
#include <functional>
#include <fmt/format.h>
#include "common/file_util.h"
#include "core/cheats/cheats.h"
#include "core/cheats/gateway_cheat.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/process.h"
#include "core/hw/gpu.h"

namespace Cheats {

// Luma3DS uses this interval for applying cheats, so to keep consistent behavior
// we use the same value
constexpr u64 run_interval_ticks = 50'000'000;

CheatEngine::CheatEngine(Core::System& system_) : system(system_) {
    LoadCheatsFromFile();
    event = system.CoreTiming().RegisterEvent(
        "CheatCore::run_event",
        [this](u64 thread_id, s64 cycle_late) { RunCallback(thread_id, cycle_late); });
    system.CoreTiming().ScheduleEvent(run_interval_ticks, event);
}

CheatEngine::~CheatEngine() {
    system.CoreTiming().UnscheduleEvent(event, 0);
}

std::vector<std::shared_ptr<CheatBase>> CheatEngine::GetCheats() const {
    std::shared_lock<std::shared_mutex> lock(cheats_list_mutex);
    return cheats_list;
}

void CheatEngine::AddCheat(const std::shared_ptr<CheatBase>& cheat) {
    std::unique_lock<std::shared_mutex> lock(cheats_list_mutex);
    cheats_list.push_back(cheat);
}

void CheatEngine::RemoveCheat(int index) {
    std::unique_lock<std::shared_mutex> lock(cheats_list_mutex);
    if (index < 0 || index >= cheats_list.size()) {
        LOG_ERROR(Core_Cheats, "Invalid index {}", index);
        return;
    }
    cheats_list.erase(cheats_list.begin() + index);
}

void CheatEngine::UpdateCheat(int index, const std::shared_ptr<CheatBase>& new_cheat) {
    std::unique_lock<std::shared_mutex> lock(cheats_list_mutex);
    if (index < 0 || index >= cheats_list.size()) {
        LOG_ERROR(Core_Cheats, "Invalid index {}", index);
        return;
    }
    cheats_list[index] = new_cheat;
}

void CheatEngine::LoadCheatsFromFile() {
    const std::string cheat_dir = FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir);
    const std::string filepath = fmt::format(
        "{}{:016X}.txt", cheat_dir, system.Kernel().GetCurrentProcess()->codeset->program_id);

    if (!FileUtil::IsDirectory(cheat_dir)) {
        FileUtil::CreateDir(cheat_dir);
    }

    if (!FileUtil::Exists(filepath)) {
        return;
    }

    std::ifstream gateway_cheats_file_stream;
    OpenFStream(gateway_cheats_file_stream, filepath, std::ios_base::in);
    if (!gateway_cheats_file_stream) {
        return;
    }

    LoadCheatsFromStream(gateway_cheats_file_stream);
}

void CheatEngine::LoadCheatsFromStream(std::istream& stream) {
    std::vector<std::unique_ptr<CheatBase>> gateway_cheats = GatewayCheat::Load(stream);
    {
        std::unique_lock<std::shared_mutex> lock(cheats_list_mutex);
        cheats_list.clear();
        std::move(gateway_cheats.begin(), gateway_cheats.end(), std::back_inserter(cheats_list));
    }
}

void CheatEngine::SaveCheatsToFile() const {
    const std::string cheat_dir = FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir);
    const std::string filepath = fmt::format(
        "{}{:016X}.txt", cheat_dir, system.Kernel().GetCurrentProcess()->codeset->program_id);

    if (!FileUtil::IsDirectory(cheat_dir)) {
        FileUtil::CreateDir(cheat_dir);
    }

    std::ofstream file;
    OpenFStream(file, filepath, std::ios_base::out);

    const std::vector<std::shared_ptr<CheatBase>> cheats = GetCheats();
    for (const std::shared_ptr<CheatBase>& cheat : cheats) {
        file << cheat->ToString();
    }

    file.flush();
}

void CheatEngine::SaveCheatsToStream(std::ostream& stream) const {
    const std::vector<std::shared_ptr<CheatBase>> cheats = GetCheats();
    for (const std::shared_ptr<CheatBase>& cheat : cheats) {
        stream << cheat->ToString();
    }
}

void CheatEngine::RunCallback([[maybe_unused]] std::uintptr_t user_data, int cycles_late) {
    {
        std::shared_lock<std::shared_mutex> lock(cheats_list_mutex);
        for (auto& cheat : cheats_list) {
            if (cheat->IsEnabled()) {
                cheat->Execute(system);
            }
        }
    }
    system.CoreTiming().ScheduleEvent(run_interval_ticks - cycles_late, event);
}

} // namespace Cheats
