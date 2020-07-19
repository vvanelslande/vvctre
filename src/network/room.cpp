// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <iomanip>
#include <mutex>
#include <random>
#include <regex>
#include <sstream>
#include <thread>
#include "enet/enet.h"
#include "network/packet.h"
#include "network/room.h"

namespace Network {

class Room::RoomImpl {
public:
    // This MAC address is used to generate a 'Nintendo' like Mac address.
    const MacAddress NintendoOUI;
    std::mt19937 random_gen; ///< Random number generator. Used for GenerateMacAddress

    ENetHost* server = nullptr; ///< Network interface.

    RoomInformation room_information; ///< Information about this room.

    struct Member {
        std::string nickname;   ///< The nickname of the member.
        GameInfo game_info;     ///< The current game of the member
        MacAddress mac_address; ///< The assigned mac address of the member.
        ENetPeer* peer;         ///< The remote peer.
    };
    using MemberList = std::vector<Member>;
    MemberList members;              ///< Information about the members of this room
    mutable std::mutex member_mutex; ///< Mutex for locking the members list

    RoomImpl()
        : NintendoOUI{0x00, 0x1F, 0x32, 0x00, 0x00, 0x00}, random_gen(std::random_device()()) {}

    std::atomic<bool> run{true};

    /// Thread that receives and dispatches network packets
    std::unique_ptr<std::thread> room_thread;

    /// Thread function that will receive and dispatch messages until the room is destroyed.
    void ServerLoop();
    void StartLoop();

    /**
     * Parses and answers a room join request from a client.
     * Validates the uniqueness of the username and assigns the MAC address
     * that the client will use for the remainder of the connection.
     */
    void HandleJoinRequest(const ENetEvent* event);

    /**
     * Sends a ID_ROOM_IS_FULL message telling the client that the room is full.
     */
    void SendRoomIsFull(ENetPeer* client);

    /**
     * Notifies the member that its connection attempt was successful,
     * and it is now part of the room.
     */
    void SendJoinSuccess(ENetPeer* client, MacAddress mac_address);

    /**
     * Notifies the members that the room is closed,
     */
    void SendCloseMessage();

    /**
     * Sends a status message to all the connected clients.
     */
    void SendStatusMessage(StatusMessageTypes type, const std::string& nickname);

    /**
     * Sends the information about the room, along with the list of members to every connected
     * client in the room.
     */
    void BroadcastRoomInformation();

    /**
     * Generates a free MAC address to assign to a new client.
     * The first 3 bytes are the NintendoOUI 0x00, 0x1F, 0x32
     */
    MacAddress GenerateMacAddress();

    /**
     * Broadcasts this packet to all members except the sender.
     * @param event The ENet event containing the data
     */
    void HandleWifiPacket(const ENetEvent* event);

    /**
     * Extracts a chat entry from a received ENet packet and adds it to the chat queue.
     * @param event The ENet event that was received.
     */
    void HandleChatPacket(const ENetEvent* event);

    /**
     * Extracts the game name from a received ENet packet and broadcasts it.
     * @param event The ENet event that was received.
     */
    void HandleGameNamePacket(const ENetEvent* event);

    /**
     * Removes the client from the members list if it was in it and announces the change
     * to all other clients.
     */
    void HandleClientDisconnection(ENetPeer* client);
};

// RoomImpl
void Room::RoomImpl::ServerLoop() {
    while (run) {
        ENetEvent event;
        if (enet_host_service(server, &event, 50) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                switch (event.packet->data[0]) {
                case IdJoinRequest:
                    HandleJoinRequest(&event);
                    break;
                case IdSetGameInfo:
                    HandleGameNamePacket(&event);
                    break;
                case IdWifiPacket:
                    HandleWifiPacket(&event);
                    break;
                case IdChatMessage:
                    HandleChatPacket(&event);
                    break;
                default:
                    break;
                }
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                HandleClientDisconnection(event.peer);
                break;
            case ENET_EVENT_TYPE_NONE:
            case ENET_EVENT_TYPE_CONNECT:
                break;
            }
        }
    }
    // Close the connection to all members:
    SendCloseMessage();
}

void Room::RoomImpl::StartLoop() {
    room_thread = std::make_unique<std::thread>(&Room::RoomImpl::ServerLoop, this);
}

void Room::RoomImpl::HandleJoinRequest(const ENetEvent* event) {
    {
        std::lock_guard lock(member_mutex);
        if (members.size() >= room_information.member_slots) {
            SendRoomIsFull(event->peer);
            return;
        }
    }
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);
    packet.IgnoreBytes(sizeof(u8)); // Ignore the message type
    std::string nickname;
    packet >> nickname;

    // At this point the client is ready to be added to the room.
    Member member{};
    member.nickname = nickname;
    member.mac_address = GenerateMacAddress();
    member.peer = event->peer;

    // Notify everyone that the user has joined.
    SendStatusMessage(IdMemberJoin, member.nickname);

    {
        std::lock_guard lock(member_mutex);
        members.push_back(std::move(member));
    }

    // Notify everyone that the room information has changed.
    BroadcastRoomInformation();
    SendJoinSuccess(event->peer, member.mac_address);
}

void Room::RoomImpl::SendRoomIsFull(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IdRoomIsFull);

    ENetPacket* enet_packet =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendJoinSuccess(ENetPeer* client, MacAddress mac_address) {
    Packet packet;
    packet << static_cast<u8>(IdJoinSuccess);
    packet << mac_address;
    ENetPacket* enet_packet =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendCloseMessage() {
    Packet packet;
    packet << static_cast<u8>(IdCloseRoom);
    std::lock_guard lock(member_mutex);
    if (!members.empty()) {
        ENetPacket* enet_packet =
            enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
        for (auto& member : members) {
            enet_peer_send(member.peer, 0, enet_packet);
        }
    }
    enet_host_flush(server);
    for (auto& member : members) {
        enet_peer_disconnect(member.peer, 0);
    }
}

void Room::RoomImpl::SendStatusMessage(StatusMessageTypes type, const std::string& nickname) {
    Packet packet;
    packet << static_cast<u8>(IdStatusMessage);
    packet << static_cast<u8>(type);
    packet << nickname;
    std::lock_guard lock(member_mutex);
    if (!members.empty()) {
        ENetPacket* enet_packet =
            enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
        for (auto& member : members) {
            enet_peer_send(member.peer, 0, enet_packet);
        }
    }
    enet_host_flush(server);
}

void Room::RoomImpl::BroadcastRoomInformation() {
    Packet packet;
    packet << static_cast<u8>(IdRoomInformation);
    packet << std::string("vvctre"); // name
    packet << std::string("");       // description
    packet << room_information.member_slots;
    packet << room_information.port;
    packet << std::string("any");    // preferred game
    packet << std::string("vvctre"); // owner

    packet << static_cast<u32>(members.size());
    {
        std::lock_guard lock(member_mutex);
        for (const auto& member : members) {
            packet << member.nickname;
            packet << member.mac_address;
            packet << member.game_info.name;
            packet << member.game_info.id;
            packet << std::string(); // username
            packet << std::string(); // display name
            packet << std::string(); // avatar
        }
    }

    ENetPacket* enet_packet =
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(server, 0, enet_packet);
    enet_host_flush(server);
}

MacAddress Room::RoomImpl::GenerateMacAddress() {
    MacAddress result_mac =
        NintendoOUI; // The first three bytes of each MAC address will be the NintendoOUI
    std::uniform_int_distribution<> dis(0x00, 0xFF); // Random byte between 0 and 0xFF
    for (std::size_t i = 3; i < result_mac.size(); ++i) {
        result_mac[i] = dis(random_gen);
    }
    return result_mac;
}

void Room::RoomImpl::HandleWifiPacket(const ENetEvent* event) {
    Packet in_packet;
    in_packet.Append(event->packet->data, event->packet->dataLength);
    in_packet.IgnoreBytes(sizeof(u8));         // Message type
    in_packet.IgnoreBytes(sizeof(u8));         // WifiPacket Type
    in_packet.IgnoreBytes(sizeof(u8));         // WifiPacket Channel
    in_packet.IgnoreBytes(sizeof(MacAddress)); // WifiPacket Transmitter Address
    MacAddress destination_address;
    in_packet >> destination_address;

    Packet out_packet;
    out_packet.Append(event->packet->data, event->packet->dataLength);
    ENetPacket* enet_packet = enet_packet_create(out_packet.GetData(), out_packet.GetDataSize(),
                                                 ENET_PACKET_FLAG_RELIABLE);

    if (destination_address ==
        BROADCAST_MAC_ADDRESS) { // Send the data to everyone except the sender
        std::lock_guard lock(member_mutex);
        bool sent_packet = false;
        for (const auto& member : members) {
            if (member.peer != event->peer) {
                sent_packet = true;
                enet_peer_send(member.peer, 0, enet_packet);
            }
        }

        if (!sent_packet) {
            enet_packet_destroy(enet_packet);
        }
    } else { // Send the data only to the destination client
        std::lock_guard lock(member_mutex);
        auto member = std::find_if(members.begin(), members.end(),
                                   [destination_address](const Member& member) -> bool {
                                       return member.mac_address == destination_address;
                                   });
        if (member != members.end()) {
            enet_peer_send(member->peer, 0, enet_packet);
        } else {
            enet_packet_destroy(enet_packet);
        }
    }
    enet_host_flush(server);
}

void Room::RoomImpl::HandleChatPacket(const ENetEvent* event) {
    Packet in_packet;
    in_packet.Append(event->packet->data, event->packet->dataLength);

    in_packet.IgnoreBytes(sizeof(u8)); // Ignore the message type
    std::string message;
    in_packet >> message;
    auto CompareNetworkAddress = [event](const Member member) -> bool {
        return member.peer == event->peer;
    };

    std::lock_guard lock(member_mutex);
    const auto sending_member = std::find_if(members.begin(), members.end(), CompareNetworkAddress);
    if (sending_member == members.end()) {
        return; // Received a chat message from a unknown sender
    }

    Packet out_packet;
    out_packet << static_cast<u8>(IdChatMessage);
    out_packet << sending_member->nickname;
    out_packet << std::string(); // username
    out_packet << message;

    ENetPacket* enet_packet = enet_packet_create(out_packet.GetData(), out_packet.GetDataSize(),
                                                 ENET_PACKET_FLAG_RELIABLE);
    bool sent_packet = false;
    for (const auto& member : members) {
        if (member.peer != event->peer) {
            sent_packet = true;
            enet_peer_send(member.peer, 0, enet_packet);
        }
    }

    if (!sent_packet) {
        enet_packet_destroy(enet_packet);
    }

    enet_host_flush(server);
}

void Room::RoomImpl::HandleGameNamePacket(const ENetEvent* event) {
    Packet in_packet;
    in_packet.Append(event->packet->data, event->packet->dataLength);

    in_packet.IgnoreBytes(sizeof(u8)); // Ignore the message type
    GameInfo game_info;
    in_packet >> game_info.name;
    in_packet >> game_info.id;

    {
        std::lock_guard lock(member_mutex);
        auto member =
            std::find_if(members.begin(), members.end(), [event](const Member& member) -> bool {
                return member.peer == event->peer;
            });
        if (member != members.end()) {
            member->game_info = game_info;
        }
    }
    BroadcastRoomInformation();
}

void Room::RoomImpl::HandleClientDisconnection(ENetPeer* client) {
    // Remove the client from the members list.
    std::string nickname, username;
    {
        std::lock_guard lock(member_mutex);
        auto member = std::find_if(members.begin(), members.end(), [client](const Member& member) {
            return member.peer == client;
        });
        if (member != members.end()) {
            nickname = member->nickname;
            members.erase(member);
        }
    }

    // Announce the change to all clients.
    enet_peer_disconnect(client, 0);
    if (!nickname.empty()) {
        SendStatusMessage(IdMemberLeave, nickname);
    }
    BroadcastRoomInformation();
}

// Room
Room::Room(const std::string& ip, u16 port, const u32 member_slots)
    : room_impl(std::make_unique<RoomImpl>()) {
    ENetAddress address;
    enet_address_set_host(&address, ip.c_str());
    address.port = port;

    // In order to send the room is full message to the connecting client, we need to leave one
    // slot open so enet won't reject the incoming connection without telling us
    room_impl->server = enet_host_create(&address, member_slots + 1, 1, 0, 0);
    if (!room_impl->server) {
        return;
    }

    room_impl->room_information.member_slots = member_slots;
    room_impl->room_information.port = port;

    room_impl->StartLoop();
}

Room::~Room() {
    room_impl->run = false;
    room_impl->room_thread->join();

    if (room_impl->server) {
        enet_host_destroy(room_impl->server);
    }
}

} // namespace Network
