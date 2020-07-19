// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include "common/common_types.h"
#include "network/common.h"

namespace Network {

class Room {
public:
    explicit Room(const std::string& ip = "0.0.0.0", u16 port = DEFAULT_PORT,
                  const u32 member_slots = DEFAULT_MEMBER_SLOTS);
    ~Room();

private:
    class RoomImpl;
    std::unique_ptr<RoomImpl> room_impl;
};

} // namespace Network
