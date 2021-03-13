// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <vector>
#include "common/assert.h"
#include "core/core.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

void ReleaseThreadMutexes(Thread* thread) {
    for (std::shared_ptr<Mutex>& m : thread->held_mutexes) {
        m->lock_count = 0;
        m->holding_thread = nullptr;
        m->WakeupAllWaitingThreads();
    }

    thread->held_mutexes.clear();
}

Mutex::Mutex(KernelSystem& kernel) : WaitObject(kernel), kernel(kernel) {}
Mutex::~Mutex() {}

std::shared_ptr<Mutex> KernelSystem::CreateMutex(bool initial_locked, std::string name) {
    std::shared_ptr<Mutex> mutex = std::make_shared<Mutex>(*this);

    mutex->lock_count = 0;
    mutex->name = std::move(name);
    mutex->holding_thread = nullptr;

    // Acquire mutex with current thread if initialized as locked
    if (initial_locked) {
        mutex->Acquire(thread_managers[current_cpu->GetID()]->GetCurrentThread());
    }

    return mutex;
}

bool Mutex::ShouldWait(const Thread* thread) const {
    return lock_count > 0 && thread != holding_thread.get();
}

void Mutex::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");

    // Actually "acquire" the mutex only if we don't already have it
    if (lock_count == 0) {
        priority = thread->current_priority;
        thread->held_mutexes.insert(SharedFrom(this));
        holding_thread = SharedFrom(thread);
        thread->UpdatePriority();
        kernel.PrepareReschedule();
    }

    lock_count++;
}

ResultCode Mutex::Release(Thread* thread) {
    // We can only release the mutex if it's held by the calling thread.
    if (thread != holding_thread.get()) {
        if (holding_thread) {
            LOG_ERROR(
                Kernel,
                "Tried to release a mutex (owned by thread id {}) from a different thread id {}",
                holding_thread->thread_id, thread->thread_id);
        }
        return ResultCode(ErrCodes::WrongLockingThread, ErrorModule::Kernel,
                          ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
    }

    // Note: It should not be possible for the situation where the mutex has a holding thread with a
    // zero lock count to occur. The real kernel still checks for this, so we do too.
    if (lock_count <= 0)
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::Kernel,
                          ErrorSummary::InvalidState, ErrorLevel::Permanent);

    lock_count--;

    // Yield to the next thread only if we've fully released the mutex
    if (lock_count == 0) {
        holding_thread->held_mutexes.erase(SharedFrom(this));
        holding_thread->UpdatePriority();
        holding_thread = nullptr;
        WakeupAllWaitingThreads();
        kernel.PrepareReschedule();
    }

    return RESULT_SUCCESS;
}

void Mutex::AddWaitingThread(std::shared_ptr<Thread> thread) {
    WaitObject::AddWaitingThread(thread);
    thread->pending_mutexes.insert(SharedFrom(this));
    UpdatePriority();
}

void Mutex::RemoveWaitingThread(Thread* thread) {
    WaitObject::RemoveWaitingThread(thread);
    thread->pending_mutexes.erase(SharedFrom(this));
    UpdatePriority();
}

void Mutex::UpdatePriority() {
    if (holding_thread == nullptr) {
        return;
    }

    u32 best_priority = ThreadPrioLowest;
    for (const std::shared_ptr<Thread>& waiter : GetWaitingThreads()) {
        if (waiter->current_priority < best_priority) {
            best_priority = waiter->current_priority;
        }
    }

    if (best_priority != priority) {
        priority = best_priority;
        holding_thread->UpdatePriority();
    }
}

} // namespace Kernel
