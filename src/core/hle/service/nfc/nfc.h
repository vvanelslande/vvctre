// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include "common/common_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class Event;
} // namespace Kernel

namespace Service::NFC {

namespace ErrCodes {
enum {
    CommandInvalidForState = 512,
};
} // namespace ErrCodes

// TODO(FearlessTobi): Add more members to this struct
struct AmiiboData {
    std::array<u8, 7> uuid;
    INSERT_PADDING_BYTES(0x4D);
    u16_le char_id;
    u8 char_variant;
    u8 figure_type;
    u16_be model_number;
    u8 series;
    INSERT_PADDING_BYTES(0x1C1);
};
static_assert(sizeof(AmiiboData) == 0x21C, "AmiiboData is an invalid size");

enum class TagState : u8 {
    NotInitialized = 0,
    NotScanning = 1,
    Scanning = 2,
    TagInRange = 3,
    TagOutOfRange = 4,
    TagDataLoaded = 5,
    Unknown6 = 6,
};

enum class CommunicationStatus : u8 {
    AttemptInitialize = 1,
    NfcInitialized = 2,
};

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> nfc, const char* name, u32 max_session);
        ~Interface();

        std::shared_ptr<Module> GetModule() const;

        void LoadAmiibo(const AmiiboData& amiibo_data);
        void RemoveAmiibo();
        AmiiboData GetAmiiboData();

    protected:
        void Initialize(Kernel::HLERequestContext& ctx);
        void Shutdown(Kernel::HLERequestContext& ctx);
        void StartCommunication(Kernel::HLERequestContext& ctx);
        void StopCommunication(Kernel::HLERequestContext& ctx);
        void StartTagScanning(Kernel::HLERequestContext& ctx);
        void StopTagScanning(Kernel::HLERequestContext& ctx);
        void LoadAmiiboData(Kernel::HLERequestContext& ctx);
        void ResetTagScanState(Kernel::HLERequestContext& ctx);
        void GetTagInRangeEvent(Kernel::HLERequestContext& ctx);
        void GetTagOutOfRangeEvent(Kernel::HLERequestContext& ctx);
        void GetTagState(Kernel::HLERequestContext& ctx);
        void CommunicationGetStatus(Kernel::HLERequestContext& ctx);
        void GetTagInfo(Kernel::HLERequestContext& ctx);
        void GetAmiiboConfig(Kernel::HLERequestContext& ctx);
        void Unknown0x1A(Kernel::HLERequestContext& ctx);
        void GetIdentificationBlock(Kernel::HLERequestContext& ctx);
        void GetAmiiboSettings(Kernel::HLERequestContext& ctx);
        void OpenAppData(Kernel::HLERequestContext& ctx);

    private:
        std::shared_ptr<Module> nfc;
    };

private:
    // Sync nfc_tag_state with amiibo_in_range and signal events on state change.
    void SyncTagState();

    std::shared_ptr<Kernel::Event> tag_in_range_event;
    std::shared_ptr<Kernel::Event> tag_out_of_range_event;
    TagState nfc_tag_state = TagState::NotInitialized;
    CommunicationStatus nfc_status = CommunicationStatus::NfcInitialized;

    AmiiboData amiibo_data{};
    bool amiibo_in_range = false;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::NFC
