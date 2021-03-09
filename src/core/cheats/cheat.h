// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include "common/common_types.h"

namespace Core {
class System;
}

namespace Cheats {

class Cheat {
public:
    struct Line {
        explicit Line(const std::string& line);

        enum class Type {
            Null = -0x1,
            Write32 = 0x00,
            Write16 = 0x01,
            Write8 = 0x02,
            GreaterThan32 = 0x03,
            LessThan32 = 0x04,
            EqualTo32 = 0x05,
            NotEqualTo32 = 0x06,
            GreaterThan16WithMask = 0x07,
            LessThan16WithMask = 0x08,
            EqualTo16WithMask = 0x09,
            NotEqualTo16WithMask = 0x0A,
            LoadOffset = 0x0B,
            Loop = 0x0C,
            Terminator = 0xD0,
            LoopExecuteVariant = 0xD1,
            FullTerminator = 0xD2,
            SetOffset = 0xD3,
            AddValue = 0xD4,
            SetValue = 0xD5,
            IncrementiveWrite32 = 0xD6,
            IncrementiveWrite16 = 0xD7,
            IncrementiveWrite8 = 0xD8,
            Load32 = 0xD9,
            Load16 = 0xDA,
            Load8 = 0xDB,
            AddOffset = 0xDC,
            Joker = 0xDD,
            Patch = 0xE,
        };

        Type type;
        u32 address;
        u32 value;
        u32 first;
        std::string string;
        bool valid = true;
    };

    explicit Cheat(std::string name, std::vector<Line> lines, std::string comments,
                   const bool enabled);

    explicit Cheat(std::string name, std::string code, std::string comments,
                   const bool enabled = false);

    ~Cheat();

    void Execute(Core::System& system) const;

    bool IsEnabled() const;
    void SetEnabled(bool enabled);

    std::string GetComments() const;
    std::string GetName() const;
    std::string GetCode() const;
    std::string ToString() const;

private:
    std::atomic<bool> enabled = false;
    const std::string name;
    std::vector<Line> lines;
    const std::string comments;
};

} // namespace Cheats
