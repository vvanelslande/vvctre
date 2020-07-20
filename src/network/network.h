// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "network/room_member.h"

namespace Network {

/// Initializes and registers the network device, and the room member.
void Init();

/// Returns a pointer to the room member handle
std::weak_ptr<RoomMember> GetRoomMember();

/// Unregisters the network device and the room member, and shut them down.
void Shutdown();

} // namespace Network
