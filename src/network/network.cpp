// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "enet/enet.h"
#include "network/network.h"

namespace Network {

static std::shared_ptr<RoomMember> g_room_member;

void Init() {
    if (enet_initialize() != 0) {
        LOG_ERROR(Network, "Error initializing ENet");
        return;
    }
    g_room_member = std::make_shared<RoomMember>();
    LOG_DEBUG(Network, "initialized OK");
}

std::weak_ptr<RoomMember> GetRoomMember() {
    return g_room_member;
}

void Shutdown() {
    if (g_room_member) {
        if (g_room_member->IsConnected()) {
            g_room_member->Leave();
        }
        g_room_member.reset();
    }
    enet_deinitialize();
    LOG_DEBUG(Network, "shutdown OK");
}

} // namespace Network
