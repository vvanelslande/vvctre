// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include "common/common_types.h"

namespace Network {

constexpr u32 NETWORK_VERSION = 4;
constexpr u16 DEFAULT_PORT = 24872;
constexpr u32 DEFAULT_MEMBER_SLOTS = 2;

using MacAddress = std::array<u8, 6>;

/// A special MAC address that tells the room we're joining to assign us a MAC address
/// automatically.
constexpr MacAddress NO_PREFERRED_MAC_ADDRESS = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 802.11 broadcast MAC address
constexpr MacAddress BROADCAST_MAC_ADDRESS = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// The different types of messages that can be sent. The first byte of each packet defines the type
enum RoomMessageTypes : u8 {
    IdJoinRequest = 1,
    IdJoinSuccess,
    IdRoomInformation,
    IdSetGameInfo,
    IdWifiPacket,
    IdChatMessage,
    IdNicknameCollisionOrNicknameInvalid,
    IdMacCollision,
    IdVersionMismatch,
    IdWrongPassword,
    IdCloseRoom,
    IdRoomIsFull,
    IdConsoleIdCollision,
    IdStatusMessage,
    IdHostKicked,
    IdHostBanned,
};

/// Types of system status messages
enum StatusMessageTypes : u8 {
    IdMemberJoin = 1,  ///< Member joining
    IdMemberLeave,     ///< Member leaving
    IdMemberKicked,    ///< A member is kicked from the room
    IdMemberBanned,    ///< A member is banned from the room
    IdAddressUnbanned, ///< Someone is unbanned from the room
};

struct RoomInformation {
    std::string name;           ///< Name of the server
    std::string description;    ///< Server description
    u32 member_slots;           ///< Maximum number of members in this room
    u16 port;                   ///< The port of this room
    std::string preferred_game; ///< Game to advertise that you want to play
    u64 preferred_game_id;      ///< Title ID for the advertised game
};

struct GameInfo {
    std::string name;
    u64 id = 0;
};

} // namespace Network
