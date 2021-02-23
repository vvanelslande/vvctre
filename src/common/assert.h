// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdlib>
#include "common/common_funcs.h"
#include "common/logging/log.h"

template <typename Fn>
#if defined(_WIN32)
__declspec(noinline, noreturn)
#elif defined(__GNUC__)
    __attribute__((noinline, noreturn, cold))
#endif
    static void assert_noinline_call(const Fn& fn) {
    fn();
    Crash();
    std::exit(1);
}

#define ASSERT(_a_)                                                                                \
    do                                                                                             \
        if (!(_a_)) {                                                                              \
            assert_noinline_call([] { LOG_CRITICAL(Debug, "Assertion Failed!"); });                \
        }                                                                                          \
    while (0)

#define ASSERT_MSG(_a_, ...)                                                                       \
    do                                                                                             \
        if (!(_a_)) {                                                                              \
            assert_noinline_call([&] { LOG_CRITICAL(Debug, "Assertion Failed!\n" __VA_ARGS__); }); \
        }                                                                                          \
    while (0)

#define UNREACHABLE() assert_noinline_call([] { LOG_CRITICAL(Debug, "Unreachable code!"); })
#define UNREACHABLE_MSG(...)                                                                       \
    assert_noinline_call([&] { LOG_CRITICAL(Debug, "Unreachable code!\n" __VA_ARGS__); })

#ifdef _DEBUG
#define DEBUG_ASSERT(_a_) ASSERT(_a_)
#define DEBUG_ASSERT_MSG(_a_, ...) ASSERT_MSG(_a_, __VA_ARGS__)
#else
#define DEBUG_ASSERT(_a_)
#define DEBUG_ASSERT_MSG(_a_, _desc_, ...)
#endif

#define UNIMPLEMENTED() LOG_CRITICAL(Debug, "Unimplemented code!")
#define UNIMPLEMENTED_MSG(_a_, ...) LOG_CRITICAL(Debug, _a_, __VA_ARGS__)
