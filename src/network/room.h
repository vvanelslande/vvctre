// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "common/common_types.h"

namespace Network {

class Room {
public:
    explicit Room(const std::string& ip, const u16 port, const u32 member_slots);
    ~Room();

private:
    class RoomImpl;
    std::unique_ptr<RoomImpl> room_impl;
};

} // namespace Network
