// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/arm_dynarmic.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/config_mem.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"
#include "core/settings.h"

namespace Kernel {

KernelSystem::KernelSystem(Memory::MemorySystem& memory, Core::Timing& timing,
                           std::function<void()> prepare_reschedule_callback, u32 system_mode)
    : memory(memory), timing(timing),
      prepare_reschedule_callback(std::move(prepare_reschedule_callback)) {
    MemoryInit(system_mode);

    resource_limits = std::make_unique<ResourceLimitList>(*this);

    thread_managers.push_back(std::make_unique<ThreadManager>(*this, 0));
    if (Settings::values.enable_core_2) {
        thread_managers.push_back(std::make_unique<ThreadManager>(*this, 1));
    }

    timer_manager = std::make_unique<TimerManager>(timing);
    stored_processes.assign(Settings::values.enable_core_2 ? 2 : 1, nullptr);

    next_thread_id = 1;
}

KernelSystem::~KernelSystem() {
    ResetThreadIDs();
}

ResourceLimitList& KernelSystem::ResourceLimit() {
    return *resource_limits;
}

const ResourceLimitList& KernelSystem::ResourceLimit() const {
    return *resource_limits;
}

u32 KernelSystem::GenerateObjectID() {
    return next_object_id++;
}

std::shared_ptr<Process> KernelSystem::GetCurrentProcess() const {
    return current_process;
}

void KernelSystem::SetCurrentProcess(std::shared_ptr<Process> process) {
    current_process = process;
    SetCurrentMemoryPageTable(&process->vm_manager.page_table);
}

void KernelSystem::SetCurrentProcessForCPU(std::shared_ptr<Process> process, u32 core_id) {
    if (current_cpu->GetID() == core_id) {
        current_process = process;
        SetCurrentMemoryPageTable(&process->vm_manager.page_table);
    } else {
        stored_processes[core_id] = process;
    }
}

void KernelSystem::SetCurrentMemoryPageTable(Memory::PageTable* page_table) {
    memory.SetCurrentPageTable(page_table);
    if (current_cpu != nullptr) {
        current_cpu->PageTableChanged(); // Notify the CPU the page table in memory has changed
    }
}

void KernelSystem::SetCPUs(std::vector<std::shared_ptr<ARM_Dynarmic>> cpus) {
    ASSERT(cpus.size() == thread_managers.size());
    u32 i = 0;

    for (const auto& cpu : cpus) {
        thread_managers[i++]->SetCPU(*cpu);
    }
}

void KernelSystem::SetRunningCPU(ARM_Dynarmic* cpu) {
    if (current_process != nullptr) {
        stored_processes[current_cpu->GetID()] = current_process;
    }

    current_cpu = cpu;
    timing.SetCurrentTimer(cpu->GetID());

    if (stored_processes[current_cpu->GetID()] != nullptr) {
        SetCurrentProcess(stored_processes[current_cpu->GetID()]);
    }
}

ThreadManager& KernelSystem::GetThreadManager(u32 core_id) {
    return *thread_managers[core_id];
}

const ThreadManager& KernelSystem::GetThreadManager(u32 core_id) const {
    return *thread_managers[core_id];
}

ThreadManager& KernelSystem::GetCurrentThreadManager() {
    return *thread_managers[current_cpu->GetID()];
}

const ThreadManager& KernelSystem::GetCurrentThreadManager() const {
    return *thread_managers[current_cpu->GetID()];
}

TimerManager& KernelSystem::GetTimerManager() {
    return *timer_manager;
}

const TimerManager& KernelSystem::GetTimerManager() const {
    return *timer_manager;
}

SharedPage::Handler& KernelSystem::GetSharedPageHandler() {
    return *shared_page_handler;
}

const SharedPage::Handler& KernelSystem::GetSharedPageHandler() const {
    return *shared_page_handler;
}

void KernelSystem::AddNamedPort(std::string name, std::shared_ptr<ClientPort> port) {
    named_ports.emplace(std::move(name), std::move(port));
}

u32 KernelSystem::NewThreadId() {
    return next_thread_id++;
}

void KernelSystem::ResetThreadIDs() {
    next_thread_id = 0;
}

} // namespace Kernel
