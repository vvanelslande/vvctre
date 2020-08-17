// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <string>
#include <boost/crc.hpp>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend/applets/mii_selector.h"
#include "core/hle/applets/mii_selector.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE::Applets {

ResultCode MiiSelector::ReceiveParameter(const Service::APT::MessageParameter& parameter) {
    if (parameter.signal != Service::APT::SignalType::Request) {
        LOG_ERROR(Service_APT, "unsupported signal {}", static_cast<u32>(parameter.signal));
        UNIMPLEMENTED();
        // TODO(Subv): Find the right error code
        return ResultCode(-1);
    }

    // The LibAppJustStarted message contains a buffer with the size of the framebuffer shared
    // memory.
    // Create the SharedMemory that will hold the framebuffer data
    Service::APT::CaptureBufferInfo capture_info;
    ASSERT(sizeof(capture_info) == parameter.buffer.size());

    memcpy(&capture_info, parameter.buffer.data(), sizeof(capture_info));

    using Kernel::MemoryPermission;
    // Create a SharedMemory that directly points to this heap block.
    framebuffer_memory = Core::System::GetInstance().Kernel().CreateSharedMemoryForApplet(
        0, capture_info.size, MemoryPermission::ReadWrite, MemoryPermission::ReadWrite,
        "MiiSelector Memory");

    // Send the response message with the newly created SharedMemory
    Service::APT::MessageParameter result;
    result.signal = Service::APT::SignalType::Response;
    result.buffer.clear();
    result.destination_id = Service::APT::AppletId::Application;
    result.sender_id = id;
    result.object = framebuffer_memory;

    SendParameter(result);
    return RESULT_SUCCESS;
}

ResultCode MiiSelector::StartImpl(const Service::APT::AppletStartupParameter& parameter) {
    ASSERT_MSG(parameter.buffer.size() == sizeof(config),
               "The size of the parameter (MiiConfig) is wrong");

    memcpy(&config, parameter.buffer.data(), parameter.buffer.size());

    using namespace Frontend;
    frontend_applet = Core::System::GetInstance().GetMiiSelector();
    ASSERT(frontend_applet);

    MiiSelectorConfig frontend_config = ToFrontendConfig(config);
    frontend_applet->Setup(frontend_config);

    is_running = true;
    return RESULT_SUCCESS;
}

void MiiSelector::Update() {
    using namespace Frontend;
    const MiiSelectorData& data = frontend_applet->ReceiveData();
    result.return_code = data.return_code;
    if (result.return_code == 0) {
        result.selected_mii_data = data.mii;
        // Calculate the checksum of the selected Mii, see https://www.3dbrew.org/wiki/Mii#Checksum
        result.mii_data_checksum = boost::crc<16, 0x1021, 0, 0, false, false>(
            &result.selected_mii_data, sizeof(HLE::Applets::MiiData) + sizeof(result.unknown1));
        result.selected_guest_mii_index = 0xFFFFFFFF;
    }
    Finalize();
}

void MiiSelector::Finalize() {
    // Let the application know that we're closing
    Service::APT::MessageParameter message;
    message.buffer.resize(sizeof(MiiResult));
    std::memcpy(message.buffer.data(), &result, message.buffer.size());
    message.signal = Service::APT::SignalType::WakeupByExit;
    message.destination_id = Service::APT::AppletId::Application;
    message.sender_id = id;
    SendParameter(message);

    is_running = false;
}

MiiResult MiiSelector::GetStandardMiiResult() {
    std::array<u8, sizeof(MiiResult)> bytes{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x03, 0x00, 0x00,
        0x30, 0x15, 0xB1, 0x5C, 0x44, 0x04, 0x5B, 0x24, 0xB5, 0x99, 0xF2, 0x46, 0x84, 0x40, 0xF4,
        0x07, 0xBC, 0xAC, 0xD9, 0x00, 0x00, 0x82, 0x40, 0x76, 0x00, 0x76, 0x00, 0x63, 0x00, 0x74,
        0x00, 0x72, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40,
        0x00, 0x00, 0x21, 0x01, 0x02, 0x68, 0x44, 0x18, 0x26, 0x34, 0x46, 0x14, 0x81, 0x12, 0x17,
        0x68, 0x0D, 0x00, 0x00, 0x29, 0x00, 0x52, 0x48, 0x50, 0x56, 0x00, 0x61, 0x00, 0x6C, 0x00,
        0x65, 0x00, 0x6E, 0x00, 0x74, 0x00, 0x69, 0x00, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x7F, 0xCC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    MiiResult result;
    std::memcpy(&result, bytes.data(), bytes.size());

    return result;
}

Frontend::MiiSelectorConfig MiiSelector::ToFrontendConfig(const MiiConfig& config) const {
    Frontend::MiiSelectorConfig frontend_config;
    frontend_config.enable_cancel_button = config.enable_cancel_button == 1;
    frontend_config.title = Common::UTF16BufferToUTF8(config.title);
    frontend_config.initially_selected_mii_index = config.initially_selected_mii_index;
    return frontend_config;
}
} // namespace HLE::Applets
