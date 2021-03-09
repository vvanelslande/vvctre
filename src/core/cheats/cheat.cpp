// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/cheats/cheat.h"
#include "core/core.h"
#include "core/hle/service/hid/hid.h"
#include "core/memory.h"

namespace Cheats {

struct State {
    u32 reg = 0;
    u32 offset = 0;
    u32 if_flag = 0;
    u32 loop_count = 0;
    std::size_t loop_back_line = 0;
    std::size_t current_line_nr = 0;
    bool loop_flag = false;
};

template <typename T, typename ReadFunction, typename WriteFunction>
static inline std::enable_if_t<std::is_integral_v<T>> WriteOp(const Cheat::Line& line,
                                                              const State& state,
                                                              ReadFunction read_func,
                                                              WriteFunction write_func,
                                                              Core::System& system) {
    u32 addr = line.address + state.offset;
    write_func(addr, static_cast<T>(line.value));
    system.InvalidateCacheRange(addr, sizeof(T));
}

template <typename T, typename ReadFunction, typename CompareFunc>
static inline std::enable_if_t<std::is_integral_v<T>> CompOp(const Cheat::Line& line, State& state,
                                                             ReadFunction read_func,
                                                             CompareFunc comp) {
    u32 addr = line.address + state.offset;
    T val = read_func(addr);
    if (!comp(val)) {
        state.if_flag++;
    }
}

static inline void LoadOffsetOp(Memory::MemorySystem& memory, const Cheat::Line& line,
                                State& state) {
    u32 addr = line.address + state.offset;
    state.offset = memory.Read32(addr);
}

static inline void LoopOp(const Cheat::Line& line, State& state) {
    state.loop_flag = state.loop_count < line.value;
    state.loop_count++;
    state.loop_back_line = state.current_line_nr;
}

static inline void TerminateOp(State& state) {
    if (state.if_flag > 0) {
        state.if_flag--;
    }
}

static inline void LoopExecuteVariantOp(State& state) {
    if (state.loop_flag) {
        state.current_line_nr = state.loop_back_line - 1;
    } else {
        state.loop_count = 0;
    }
}

static inline void FullTerminateOp(State& state) {
    if (state.loop_flag) {
        state.current_line_nr = state.loop_back_line - 1;
    } else {
        state.offset = 0;
        state.reg = 0;
        state.loop_count = 0;
        state.if_flag = 0;
        state.loop_flag = false;
    }
}

static inline void SetOffsetOp(const Cheat::Line& line, State& state) {
    state.offset = line.value;
}

static inline void AddValueOp(const Cheat::Line& line, State& state) {
    state.reg += line.value;
}

static inline void SetValueOp(const Cheat::Line& line, State& state) {
    state.reg = line.value;
}

template <typename T, typename ReadFunction, typename WriteFunction>
static inline std::enable_if_t<std::is_integral_v<T>> IncrementiveWriteOp(const Cheat::Line& line,
                                                                          State& state,
                                                                          ReadFunction read_func,
                                                                          WriteFunction write_func,
                                                                          Core::System& system) {
    u32 addr = line.value + state.offset;
    write_func(addr, static_cast<T>(state.reg));
    system.InvalidateCacheRange(addr, sizeof(T));
    state.offset += sizeof(T);
}

template <typename T, typename ReadFunction>
static inline std::enable_if_t<std::is_integral_v<T>> LoadOp(const Cheat::Line& line, State& state,
                                                             ReadFunction read_func) {

    u32 addr = line.value + state.offset;
    state.reg = read_func(addr);
}

static inline void AddOffsetOp(const Cheat::Line& line, State& state) {
    state.offset += line.value;
}

static inline void JokerOp(const Cheat::Line& line, State& state, const Core::System& system) {
    const u32 pad_state = system.ServiceManager()
                              .GetService<Service::HID::Module::Interface>("hid:USER")
                              ->GetModule()
                              ->GetPadState()
                              .hex;
    bool pressed = (pad_state & line.value) == line.value;
    if (!pressed) {
        state.if_flag++;
    }
}

static inline void PatchOp(const Cheat::Line& line, State& state, Core::System& system,
                           const std::vector<Cheat::Line>& lines) {
    if (state.if_flag > 0) {
        // Skip over the additional patch lines
        state.current_line_nr += static_cast<int>(std::ceil(line.value / 8.0));
        return;
    }
    u32 num_bytes = line.value;
    u32 addr = line.address + state.offset;
    system.InvalidateCacheRange(addr, num_bytes);

    bool first = true;
    u32 bit_offset = 0;
    if (num_bytes > 0)
        state.current_line_nr++; // skip over the current code
    while (num_bytes >= 4) {
        u32 tmp = first ? lines[state.current_line_nr].first : lines[state.current_line_nr].value;
        if (!first && num_bytes > 4) {
            state.current_line_nr++;
        }
        first = !first;
        system.Memory().Write32(addr, tmp);
        addr += 4;
        num_bytes -= 4;
    }
    while (num_bytes > 0) {
        u32 tmp =
            (first ? lines[state.current_line_nr].first : lines[state.current_line_nr].value) >>
            bit_offset;
        system.Memory().Write8(addr, tmp);
        addr += 1;
        num_bytes -= 1;
        bit_offset += 8;
    }
}

Cheat::Line::Line(const std::string& line) {
    constexpr std::size_t length = 17;

    if (line.length() != length) {
        type = Line::Type::Null;
        string = line;
        LOG_ERROR(Core_Cheats, "Cheat contains invalid line: {}", line);
        valid = false;
        return;
    }

    try {
        std::string type_temp = line.substr(0, 1);
        // 0xD types have extra subtype value, i.e. 0xDA
        std::string sub_type_temp;
        if (type_temp == "D" || type_temp == "d") {
            sub_type_temp = line.substr(1, 1);
        }
        type = static_cast<Line::Type>(std::stoi(type_temp + sub_type_temp, 0, 16));
        first = std::stoul(line.substr(0, 8), 0, 16);
        address = first & 0x0FFFFFFF;
        value = std::stoul(line.substr(9, 8), 0, 16);
        string = line;
    } catch (const std::logic_error&) {
        type = Line::Type::Null;
        string = line;
        LOG_ERROR(Core_Cheats, "Cheat contains invalid line: {}", line);
        valid = false;
    }
}

Cheat::Cheat(std::string name, std::vector<Line> lines, std::string comments, const bool enabled)
    : name(std::move(name)), lines(std::move(lines)), comments(std::move(comments)),
      enabled(enabled) {}

Cheat::Cheat(std::string name, std::string code, std::string comments, const bool enabled)
    : name(std::move(name)), comments(std::move(comments)), enabled(enabled) {
    std::vector<std::string> code_lines;
    Common::SplitString(code, '\n', code_lines);

    std::vector<Line> temp_lines;

    for (std::size_t i = 0; i < code_lines.size(); ++i) {
        if (!code_lines[i].empty()) {
            temp_lines.emplace_back(code_lines[i]);
        }
    }

    lines = std::move(temp_lines);
}

Cheat::~Cheat() = default;

void Cheat::Execute(Core::System& system) const {
    State state;

    Memory::MemorySystem& memory = system.Memory();
    auto Read8 = [&memory](VAddr addr) { return memory.Read8(addr); };
    auto Read16 = [&memory](VAddr addr) { return memory.Read16(addr); };
    auto Read32 = [&memory](VAddr addr) { return memory.Read32(addr); };
    auto Write8 = [&memory](VAddr addr, u8 value) { memory.Write8(addr, value); };
    auto Write16 = [&memory](VAddr addr, u16 value) { memory.Write16(addr, value); };
    auto Write32 = [&memory](VAddr addr, u32 value) { memory.Write32(addr, value); };

    for (state.current_line_nr = 0; state.current_line_nr < lines.size(); state.current_line_nr++) {
        Line line = lines[state.current_line_nr];

        if (state.if_flag > 0) {
            switch (line.type) {
            case Line::Type::GreaterThan32:
            case Line::Type::LessThan32:
            case Line::Type::EqualTo32:
            case Line::Type::NotEqualTo32:
            case Line::Type::GreaterThan16WithMask:
            case Line::Type::LessThan16WithMask:
            case Line::Type::EqualTo16WithMask:
            case Line::Type::NotEqualTo16WithMask:
            case Line::Type::Joker:
                // Increment the if_flag to handle the end if correctly
                state.if_flag++;
                break;
            case Line::Type::Patch:
                // EXXXXXXX YYYYYYYY
                // Copies YYYYYYYY bytes from (current code location + 8) to [XXXXXXXX + offset].
                // We need to call this here to skip the additional patch lines
                PatchOp(line, state, system, lines);
                break;
            case Line::Type::Terminator:
                // D0000000 00000000 - ENDIF
                TerminateOp(state);
                break;
            case Line::Type::FullTerminator:
                // D2000000 00000000 - END; offset = 0; reg = 0;
                FullTerminateOp(state);
                break;
            default:
                break;
            }
            // Do not execute any other op code
            continue;
        }
        switch (line.type) {
        case Line::Type::Null:
            break;
        case Line::Type::Write32:
            // 0XXXXXXX YYYYYYYY - word[XXXXXXX+offset] = YYYYYYYY
            WriteOp<u32>(line, state, Read32, Write32, system);
            break;
        case Line::Type::Write16:
            // 1XXXXXXX 0000YYYY - half[XXXXXXX+offset] = YYYY
            WriteOp<u16>(line, state, Read16, Write16, system);
            break;
        case Line::Type::Write8:
            // 2XXXXXXX 000000YY - byte[XXXXXXX+offset] = YY
            WriteOp<u8>(line, state, Read8, Write8, system);
            break;
        case Line::Type::GreaterThan32:
            // 3XXXXXXX YYYYYYYY - Execute next block IF YYYYYYYY > word[XXXXXXX]   ;unsigned
            CompOp<u32>(line, state, Read32, [&line](u32 val) -> bool { return line.value > val; });
            break;
        case Line::Type::LessThan32:
            // 4XXXXXXX YYYYYYYY - Execute next block IF YYYYYYYY < word[XXXXXXX]   ;unsigned
            CompOp<u32>(line, state, Read32, [&line](u32 val) -> bool { return line.value < val; });
            break;
        case Line::Type::EqualTo32:
            // 5XXXXXXX YYYYYYYY - Execute next block IF YYYYYYYY == word[XXXXXXX]   ;unsigned
            CompOp<u32>(line, state, Read32,
                        [&line](u32 val) -> bool { return line.value == val; });
            break;
        case Line::Type::NotEqualTo32:
            // 6XXXXXXX YYYYYYYY - Execute next block IF YYYYYYYY != word[XXXXXXX]   ;unsigned
            CompOp<u32>(line, state, Read32,
                        [&line](u32 val) -> bool { return line.value != val; });
            break;
        case Line::Type::GreaterThan16WithMask:
            // 7XXXXXXX ZZZZYYYY - Execute next block IF YYYY > ((not ZZZZ) AND half[XXXXXXX])
            CompOp<u16>(line, state, Read16, [&line](u16 val) -> bool {
                return static_cast<u16>(line.value) > (static_cast<u16>(~line.value >> 16) & val);
            });
            break;
        case Line::Type::LessThan16WithMask:
            // 8XXXXXXX ZZZZYYYY - Execute next block IF YYYY < ((not ZZZZ) AND half[XXXXXXX])
            CompOp<u16>(line, state, Read16, [&line](u16 val) -> bool {
                return static_cast<u16>(line.value) < (static_cast<u16>(~line.value >> 16) & val);
            });
            break;
        case Line::Type::EqualTo16WithMask:
            // 9XXXXXXX ZZZZYYYY - Execute next block IF YYYY = ((not ZZZZ) AND half[XXXXXXX])
            CompOp<u16>(line, state, Read16, [&line](u16 val) -> bool {
                return static_cast<u16>(line.value) == (static_cast<u16>(~line.value >> 16) & val);
            });
            break;
        case Line::Type::NotEqualTo16WithMask:
            // AXXXXXXX ZZZZYYYY - Execute next block IF YYYY <> ((not ZZZZ) AND half[XXXXXXX])
            CompOp<u16>(line, state, Read16, [&line](u16 val) -> bool {
                return static_cast<u16>(line.value) != (static_cast<u16>(~line.value >> 16) & val);
            });
            break;
        case Line::Type::LoadOffset:
            // BXXXXXXX 00000000 - offset = word[XXXXXXX+offset]
            LoadOffsetOp(system.Memory(), line, state);
            break;
        case Line::Type::Loop: {
            // C0000000 YYYYYYYY - LOOP next block YYYYYYYY times
            // TODO(B3N30): Support nested loops if necessary
            LoopOp(line, state);
            break;
        }
        case Line::Type::Terminator: {
            // D0000000 00000000 - END IF
            TerminateOp(state);
            break;
        }
        case Line::Type::LoopExecuteVariant: {
            // D1000000 00000000 - END LOOP
            LoopExecuteVariantOp(state);
            break;
        }
        case Line::Type::FullTerminator: {
            // D2000000 00000000 - NEXT & Flush
            FullTerminateOp(state);
            break;
        }
        case Line::Type::SetOffset: {
            // D3000000 XXXXXXXX – Sets the offset to XXXXXXXX
            SetOffsetOp(line, state);
            break;
        }
        case Line::Type::AddValue: {
            // D4000000 XXXXXXXX – reg += XXXXXXXX
            AddValueOp(line, state);
            break;
        }
        case Line::Type::SetValue: {
            // D5000000 XXXXXXXX – reg = XXXXXXXX
            SetValueOp(line, state);
            break;
        }
        case Line::Type::IncrementiveWrite32: {
            // D6000000 XXXXXXXX – (32bit) [XXXXXXXX+offset] = reg ; offset += 4
            IncrementiveWriteOp<u32>(line, state, Read32, Write32, system);
            break;
        }
        case Line::Type::IncrementiveWrite16: {
            // D7000000 XXXXXXXX – (16bit) [XXXXXXXX+offset] = reg & 0xffff ; offset += 2
            IncrementiveWriteOp<u16>(line, state, Read16, Write16, system);
            break;
        }
        case Line::Type::IncrementiveWrite8: {
            // D8000000 XXXXXXXX – (16bit) [XXXXXXXX+offset] = reg & 0xff ; offset++
            IncrementiveWriteOp<u8>(line, state, Read8, Write8, system);
            break;
        }
        case Line::Type::Load32: {
            // D9000000 XXXXXXXX – reg = [XXXXXXXX+offset]
            LoadOp<u32>(line, state, Read32);
            break;
        }
        case Line::Type::Load16: {
            // DA000000 XXXXXXXX – reg = [XXXXXXXX+offset] & 0xFFFF
            LoadOp<u16>(line, state, Read16);
            break;
        }
        case Line::Type::Load8: {
            // DB000000 XXXXXXXX – reg = [XXXXXXXX+offset] & 0xFF
            LoadOp<u8>(line, state, Read8);
            break;
        }
        case Line::Type::AddOffset: {
            // DC000000 XXXXXXXX – offset + XXXXXXXX
            AddOffsetOp(line, state);
            break;
        }
        case Line::Type::Joker: {
            // DD000000 XXXXXXXX – if KEYPAD has value XXXXXXXX execute next block
            JokerOp(line, state, system);
            break;
        }
        case Line::Type::Patch: {
            // EXXXXXXX YYYYYYYY
            // Copies YYYYYYYY bytes from (current code location + 8) to [XXXXXXXX + offset].
            PatchOp(line, state, system, lines);
            break;
        }
        }
    }
}

bool Cheat::IsEnabled() const {
    return enabled;
}

void Cheat::SetEnabled(bool enabled_) {
    enabled = enabled_;
}

std::string Cheat::GetComments() const {
    return comments;
}

std::string Cheat::GetName() const {
    return name;
}

std::string Cheat::GetCode() const {
    std::string result;

    for (const Line& line : lines) {
        result += line.string + '\n';
    }

    return result;
}

std::string Cheat::ToString() const {
    std::string result = '[' + name + "]\n";

    if (enabled) {
        result += "*vvctre_enabled\n";
    }

    std::vector<std::string> comment_lines;
    Common::SplitString(comments, '\n', comment_lines);
    for (const auto& comment_line : comment_lines) {
        result += "*" + comment_line + '\n';
    }

    result += GetCode() + '\n';

    return result;
}

} // namespace Cheats
