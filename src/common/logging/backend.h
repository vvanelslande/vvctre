// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include "common/logging/filter.h"
#include "common/logging/log.h"

namespace Log {

class Filter;

/**
 * A log entry. Log entries are stored in a structured format to permit more varied output
 * formatting on different frontends, as well as facilitating filtering and aggregation.
 */
struct Entry {
    std::chrono::microseconds timestamp;
    Class log_class;
    Level level;
    const char* file;
    unsigned int line;
    std::string function;
    std::string message;
    bool final_entry = false;

    Entry() = default;
    Entry(Entry&& o) = default;

    Entry& operator=(Entry&& o) = default;
    Entry& operator=(const Entry& o) = default;
};

/// Interface for logging backends
class Backend {
public:
    virtual ~Backend() = default;

    virtual void SetFilter(const Filter& new_filter) {
        filter = new_filter;
    }

    virtual const char* GetName() const = 0;
    virtual void Write(const Entry& entry) = 0;

private:
    Filter filter;
};

/// Backend that writes to stderr with color
class ColorConsoleBackend : public Backend {
public:
    const char* GetName() const override {
        return "color_console";
    }

    void Write(const Entry& entry) override;
};

void AddBackend(std::unique_ptr<Backend> backend);
void RemoveBackend(std::string_view name);

/**
 * Returns the name of the passed log class as a C-string. Subclasses are separated by periods
 * instead of underscores as in the enumeration.
 */
const char* GetLogClassName(Class log_class);

/**
 * Returns the name of the passed log level as a C-string.
 */
const char* GetLevelName(Level level);

/**
 * The global filter will prevent any messages from even being processed if they are filtered. Each
 * backend can have a filter, but if the level is lower than the global filter, the backend will
 * never get the message
 */
void SetGlobalFilter(const Filter& filter);

} // namespace Log
