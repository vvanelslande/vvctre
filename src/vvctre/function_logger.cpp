// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include "common/logging/text_formatter.h"
#include "vvctre/function_logger.h"

namespace Log {

FunctionLogger::FunctionLogger(decltype(FunctionLogger::function) function, std::string name)
    : function(function), name(std::move(name)) {}

const char* FunctionLogger::GetName() const {
    return name.c_str();
}

void FunctionLogger::Write(const Entry& entry) {
    function(FormatLogMessage(entry).c_str());
}

} // namespace Log
