// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <boost/optional.hpp>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/service.h"
#include "network/room_member.h"

namespace Core {
class System;
} // namespace Core

namespace Kernel {
class Event;
class SharedMemory;
} // namespace Kernel

// Local-WLAN service

namespace Service::NWM {

using MacAddress = std::array<u8, 6>;

const std::size_t ApplicationDataSize = 0xC8;
const u8 DefaultNetworkChannel = 11;

// Number of milliseconds in a TU.
const double MillisecondsPerTU = 1.024;
// Interval measured in TU, the default value is 100TU = 102.4ms
const u16 DefaultBeaconInterval = 100;

/// The maximum number of nodes that can exist in an UDS session.
constexpr u32 UDSMaxNodes = 16;

struct NodeInfo {
    u64_le friend_code_seed;
    std::array<u16_le, 10> username;
    INSERT_PADDING_BYTES(4);
    u16_le network_node_id;
    INSERT_PADDING_BYTES(6);

    void Reset() {
        friend_code_seed = 0;
        username.fill(0);
        network_node_id = 0;
    }
};

static_assert(sizeof(NodeInfo) == 40, "NodeInfo has incorrect size.");

using NodeList = std::vector<NodeInfo>;

enum class NetworkStatus : u32 {
    NotConnected = 3,
    ConnectedAsHost = 6,
    Connecting = 7,
    ConnectedAsClient = 9,
    ConnectedAsSpectator = 10,
};

enum class NetworkStatusChangeReason : u32 {
    None = 0,
    ConnectionEstablished = 1,
    ConnectionLost = 4,
};

struct ConnectionStatus {
    enum_le<NetworkStatus> status;
    enum_le<NetworkStatusChangeReason> status_change_reason;
    u16_le network_node_id;
    u16_le changed_nodes;
    u16_le nodes[UDSMaxNodes];
    u8 total_nodes;
    u8 max_nodes;
    u16_le node_bitmask;
};

static_assert(sizeof(ConnectionStatus) == 0x30, "ConnectionStatus has incorrect size.");

struct NetworkInfo {
    std::array<u8, 6> host_mac_address;
    u8 channel;
    INSERT_PADDING_BYTES(1);
    u8 initialized;
    INSERT_PADDING_BYTES(3);
    std::array<u8, 3> oui_value;
    u8 oui_type;
    // This field is received as BigEndian from the game.
    u32_be wlan_comm_id;
    u8 id;
    INSERT_PADDING_BYTES(1);
    u16_be attributes;
    u32_be network_id;
    u8 total_nodes;
    u8 max_nodes;
    INSERT_PADDING_BYTES(2);
    INSERT_PADDING_BYTES(0x1F);
    u8 application_data_size;
    std::array<u8, ApplicationDataSize> application_data;
};

static_assert(offsetof(NetworkInfo, oui_value) == 0xC, "oui_value is at the wrong offset.");
static_assert(offsetof(NetworkInfo, wlan_comm_id) == 0x10, "wlancommid is at the wrong offset.");
static_assert(sizeof(NetworkInfo) == 0x108, "NetworkInfo has incorrect size.");

/// Additional block tag ids in the Beacon and Association Response frames
enum class TagId : u8 {
    SSID = 0,
    SupportedRates = 1,
    DSParameterSet = 2,
    TrafficIndicationMap = 5,
    CountryInformation = 7,
    ERPInformation = 42,
    VendorSpecific = 221
};

class NWM_UDS final : public ServiceFramework<NWM_UDS> {
public:
    explicit NWM_UDS(Core::System& system);
    ~NWM_UDS();

private:
    Core::System& system;

    void UpdateNetworkAttribute(Kernel::HLERequestContext& ctx);
    void Shutdown(Kernel::HLERequestContext& ctx);
    void DestroyNetwork(Kernel::HLERequestContext& ctx);
    void DisconnectNetwork(Kernel::HLERequestContext& ctx);
    void GetConnectionStatus(Kernel::HLERequestContext& ctx);
    void GetNodeInformation(Kernel::HLERequestContext& ctx);
    void RecvBeaconBroadcastData(Kernel::HLERequestContext& ctx);
    void SetApplicationData(Kernel::HLERequestContext& ctx);
    void Bind(Kernel::HLERequestContext& ctx);
    void Unbind(Kernel::HLERequestContext& ctx);
    void PullPacket(Kernel::HLERequestContext& ctx);
    void SendTo(Kernel::HLERequestContext& ctx);
    void GetChannel(Kernel::HLERequestContext& ctx);
    void InitializeWithVersion(Kernel::HLERequestContext& ctx);
    void InitializeDeprecated(Kernel::HLERequestContext& ctx);
    void BeginHostingNetwork(Kernel::HLERequestContext& ctx);
    void BeginHostingNetworkDeprecated(Kernel::HLERequestContext& ctx);
    void ConnectToNetwork(Kernel::HLERequestContext& ctx);
    void ConnectToNetworkDeprecated(Kernel::HLERequestContext& ctx);
    void EjectClient(Kernel::HLERequestContext& ctx);
    void DecryptBeaconData(Kernel::HLERequestContext& ctx, u16 command_id);

    template <u16 command_id>
    void DecryptBeaconData(Kernel::HLERequestContext& ctx);

    ResultVal<std::shared_ptr<Kernel::Event>> Initialize(
        u32 sharedmem_size, const NodeInfo& node, u16 version,
        std::shared_ptr<Kernel::SharedMemory> sharedmem);

    ResultCode BeginHostingNetwork(const u8* network_info_buffer, std::size_t network_info_size,
                                   std::vector<u8> passphrase);

    void ConnectToNetwork(Kernel::HLERequestContext& ctx, u16 command_id,
                          const u8* network_info_buffer, std::size_t network_info_size,
                          u8 connection_type, std::vector<u8> passphrase);

    void BeaconBroadcastCallback(std::uintptr_t user_data, s64 cycles_late);

    /**
     * Returns a list of received 802.11 beacon frames from the specified sender and with the
     * specified wlan_comm_id since the last call.
     */
    std::list<Network::WifiPacket> GetReceivedBeacons(const MacAddress& sender, u32 wlan_comm_id);

    void SendPacket(Network::WifiPacket& packet);

    /*
     * Returns an available index in the nodes array for the
     * currently-hosted UDS network.
     */
    u16 GetNextAvailableNodeId();

    void BroadcastNodeMap();
    void HandleNodeMapPacket(const Network::WifiPacket& packet);
    void HandleBeaconFrame(const Network::WifiPacket& packet);
    void HandleAssociationResponseFrame(const Network::WifiPacket& packet);
    void HandleEAPoLPacket(const Network::WifiPacket& packet);
    void HandleSecureDataPacket(const Network::WifiPacket& packet);

    /*
     * Start a connection sequence with an UDS server. The sequence starts by sending an 802.11
     * authentication frame with SEQ1.
     */
    void StartConnectionSequence(const MacAddress& server);

    /// Sends an Association Response frame to the specified mac address
    void SendAssociationResponseFrame(const MacAddress& address);

    /*
     * Handles the authentication request frame and sends the authentication response and
     * association response frames. Once an Authentication frame with SEQ1 is received by the
     * server, it responds with an Authentication frame containing SEQ2, and immediately sends an
     * Association response frame containing the details of the access point and the assigned
     * association id for the new client.
     */
    void HandleAuthenticationFrame(const Network::WifiPacket& packet);

    /// Handles the deauthentication frames sent from clients to hosts, when they leave a session
    void HandleDeauthenticationFrame(const Network::WifiPacket& packet);

    void HandleDataFrame(const Network::WifiPacket& packet);

    /// Callback to parse and handle a received wifi packet.
    void OnWifiPacketReceived(const Network::WifiPacket& packet);

    boost::optional<Network::MacAddress> GetNodeMacAddress(u16 dest_node_id, u8 flags);

    // Event that is signaled every time the connection status changes.
    std::shared_ptr<Kernel::Event> connection_status_event;

    // Shared memory provided by the application to store the receive buffer.
    // This is not currently used.
    std::shared_ptr<Kernel::SharedMemory> recv_buffer_memory;

    // Connection status of this 3DS.
    ConnectionStatus connection_status{};

    std::atomic<bool> initialized{false};

    /* Node information about the current network.
     * The amount of elements in this vector is always the maximum number
     * of nodes specified in the network configuration.
     * The first node is always the host.
     */
    NodeList node_info;

    // Node information about our own system.
    NodeInfo current_node;

    struct BindNodeData {
        u32 bind_node_id;    ///< Id of the bind node associated with this data.
        u8 channel;          ///< Channel that this bind node was bound to.
        u16 network_node_id; ///< Node id this bind node is associated with, only packets from this
                             /// network node will be received.
        std::shared_ptr<Kernel::Event> event;         ///< Receive event for this bind node.
        std::deque<std::vector<u8>> received_packets; ///< List of packets received on this channel.
    };

    // Mapping of data channels to their internal data.
    std::unordered_map<u32, BindNodeData> channel_data;

    // The WiFi network channel that the network is currently on.
    // Since we're not actually interacting with physical radio waves, this is just a dummy value.
    u8 network_channel = DefaultNetworkChannel;

    // Information about the network that we're currently connected to.
    NetworkInfo network_info;

    // Mapping of mac addresses to their respective node_ids.
    struct Node {
        bool connected;
        u16 node_id;
    };

    std::map<MacAddress, Node> node_map;

    // Event that will generate and send the 802.11 beacon frames.
    Core::TimingEventType* beacon_broadcast_event;

    // Callback identifier for the OnWifiPacketReceived event.
    Network::RoomMember::CallbackHandle<Network::WifiPacket> wifi_packet_received;

    // Mutex to synchronize access to the connection status between the emulation thread and the
    // network thread.
    std::mutex connection_status_mutex;

    std::shared_ptr<Kernel::Event> connection_event;

    // Mutex to synchronize access to the list of received beacons between the emulation thread and
    // the network thread.
    std::mutex beacon_mutex;

    // List of the last <MaxBeaconFrames> beacons received from the network.
    std::list<Network::WifiPacket> received_beacons;
};

} // namespace Service::NWM
