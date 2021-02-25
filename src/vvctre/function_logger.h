// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/logging/backend.h"

namespace Log {

struct FunctionLogger : Log::Backend {
    std::string name;
    void (*function)(const char* log);

    explicit FunctionLogger(std::string name, decltype(FunctionLogger::function) function);

    const char* GetName() const override;
    void Write(const Entry& entry) override;
};

} // namespace Log
