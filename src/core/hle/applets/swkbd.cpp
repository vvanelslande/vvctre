// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <string>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/applets/swkbd.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"
#include "core/hle/service/gsp/gsp.h"
#include "core/hle/service/hid/hid.h"
#include "core/memory.h"

namespace HLE::Applets {

ResultCode SoftwareKeyboard::ReceiveParameter(Service::APT::MessageParameter const& parameter) {
    switch (parameter.signal) {
    case Service::APT::SignalType::Request: {
        // The LibAppJustStarted message contains a buffer with the size of the framebuffer shared
        // memory.
        // Create the SharedMemory that will hold the framebuffer data
        Service::APT::CaptureBufferInfo capture_info;
        ASSERT(sizeof(capture_info) == parameter.buffer.size());

        std::memcpy(&capture_info, parameter.buffer.data(), sizeof(capture_info));

        // Create a SharedMemory that directly points to this heap block.
        framebuffer_memory = Core::System::GetInstance().Kernel().CreateSharedMemoryForApplet(
            0, capture_info.size, Kernel::MemoryPermission::ReadWrite,
            Kernel::MemoryPermission::ReadWrite, "SoftwareKeyboard Memory");

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

    case Service::APT::SignalType::Message: {
        // Callback result
        ASSERT_MSG(parameter.buffer.size() == sizeof(config),
                   "The size of the parameter (SoftwareKeyboardConfig) is wrong");

        std::memcpy(&config, parameter.buffer.data(), parameter.buffer.size());

        switch (config.callback_result) {
        case SoftwareKeyboardCallbackResult::OK:
            // Finish execution
            Finalize();
            return RESULT_SUCCESS;

        case SoftwareKeyboardCallbackResult::Close:
            // Let the frontend display error and quit
            frontend_applet->ShowError(Common::UTF16BufferToUTF8(config.callback_msg));
            config.return_code = SoftwareKeyboardResult::BannedInput;
            config.text_offset = config.text_length = 0;
            Finalize();
            return RESULT_SUCCESS;

        case SoftwareKeyboardCallbackResult::Continue:
            // Let the frontend display error and get input again
            // The input will be sent for validation again on next Update().
            frontend_applet->ShowError(Common::UTF16BufferToUTF8(config.callback_msg));
            frontend_applet->Execute(ToFrontendConfig(config));
            return RESULT_SUCCESS;

        default:
            UNREACHABLE();
        }
    }

    default: {
        LOG_ERROR(Service_APT, "unsupported signal {}", static_cast<u32>(parameter.signal));
        UNIMPLEMENTED();
        // TODO(Subv): Find the right error code
        return ResultCode(-1);
    }
    }
}

ResultCode SoftwareKeyboard::StartImpl(Service::APT::AppletStartupParameter const& parameter) {
    ASSERT_MSG(parameter.buffer.size() == sizeof(config),
               "The size of the parameter (SoftwareKeyboardConfig) is wrong");

    std::memcpy(&config, parameter.buffer.data(), parameter.buffer.size());
    text_memory = std::static_pointer_cast<Kernel::SharedMemory, Kernel::Object>(parameter.object);

    frontend_applet = Core::System::GetInstance().GetSoftwareKeyboard();
    ASSERT(frontend_applet);

    frontend_applet->Execute(ToFrontendConfig(config));

    is_running = true;
    return RESULT_SUCCESS;
}

void SoftwareKeyboard::Update() {
    if (!frontend_applet->DataReady()) {
        return;
    }

    const Frontend::KeyboardData& data = frontend_applet->ReceiveData();

    switch (config.num_buttons_m1) {
    case SoftwareKeyboardButtonConfig::SingleButton: {
        config.return_code = SoftwareKeyboardResult::D0Click;

        std::u16string text = Common::UTF8ToUTF16(data.text);
        config.text_length = static_cast<u16>(text.size());

        // Include a null terminator
        std::memcpy(text_memory->GetPointer(), text.c_str(),
                    (text.length() + 1) * sizeof(char16_t));

        break;
    }
    case SoftwareKeyboardButtonConfig::DualButton: {
        if (data.button == 0) {
            config.return_code = SoftwareKeyboardResult::D1Click0;
        } else {
            config.return_code = SoftwareKeyboardResult::D1Click1;

            std::u16string text = Common::UTF8ToUTF16(data.text);
            config.text_length = static_cast<u16>(text.size());

            // Include a null terminator
            std::memcpy(text_memory->GetPointer(), text.c_str(),
                        (text.length() + 1) * sizeof(char16_t));
        }

        break;
    }
    case SoftwareKeyboardButtonConfig::TripleButton: {
        if (data.button == 0) {
            config.return_code = SoftwareKeyboardResult::D2Click0;
        } else if (data.button == 1) {
            config.return_code = SoftwareKeyboardResult::D2Click1;

            std::u16string text = Common::UTF8ToUTF16(data.text);
            config.text_length = static_cast<u16>(text.size());

            // Include a null terminator
            std::memcpy(text_memory->GetPointer(), text.c_str(),
                        (text.length() + 1) * sizeof(char16_t));
        } else {
            config.return_code = SoftwareKeyboardResult::D2Click2;

            std::u16string text = Common::UTF8ToUTF16(data.text);
            config.text_length = static_cast<u16>(text.size());

            // Include a null terminator
            std::memcpy(text_memory->GetPointer(), text.c_str(),
                        (text.length() + 1) * sizeof(char16_t));
        }
        break;
    }
    case SoftwareKeyboardButtonConfig::NoButton: {
        // TODO: find out what is actually returned
        config.return_code = SoftwareKeyboardResult::None;

        config.text_length = 0;

        break;
    }
    default:
        LOG_CRITICAL(Applet_SWKBD, "Unknown button config {}",
                     static_cast<u32>(config.num_buttons_m1));
        UNREACHABLE();
    }

    config.text_offset = 0;

    if (config.filter_flags & HLE::Applets::SoftwareKeyboardFilter::Callback) {
        // Send the message to invoke callback
        Service::APT::MessageParameter message;
        message.buffer.resize(sizeof(SoftwareKeyboardConfig));
        std::memcpy(message.buffer.data(), &config, message.buffer.size());
        message.signal = Service::APT::SignalType::Message;
        message.destination_id = Service::APT::AppletId::Application;
        message.sender_id = id;
        SendParameter(message);
    } else {
        Finalize();
    }
}

void SoftwareKeyboard::Finalize() {
    // Let the application know that we're closing
    Service::APT::MessageParameter message;
    message.buffer.resize(sizeof(SoftwareKeyboardConfig));
    std::memcpy(message.buffer.data(), &config, message.buffer.size());
    message.signal = Service::APT::SignalType::WakeupByExit;
    message.destination_id = Service::APT::AppletId::Application;
    message.sender_id = id;
    SendParameter(message);

    framebuffer_memory.reset();
    text_memory.reset();
    is_running = false;
}

Frontend::KeyboardConfig SoftwareKeyboard::ToFrontendConfig(
    const SoftwareKeyboardConfig& config) const {
    Frontend::KeyboardConfig frontend_config;
    frontend_config.button_config =
        static_cast<Frontend::ButtonConfig>(static_cast<u32>(config.num_buttons_m1));
    frontend_config.accept_mode =
        static_cast<Frontend::AcceptedInput>(static_cast<u32>(config.valid_input));
    frontend_config.multiline_mode = config.multiline;
    frontend_config.max_text_length = config.max_text_length;
    frontend_config.max_digits = config.max_digits;
    frontend_config.hint_text = Common::UTF16BufferToUTF8(config.hint_text);
    for (const auto& text : config.button_text) {
        frontend_config.button_text.push_back(Common::UTF16BufferToUTF8(text));
    }
    frontend_config.filters.prevent_digit =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Digits);
    frontend_config.filters.prevent_at =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::At);
    frontend_config.filters.prevent_percent =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Percent);
    frontend_config.filters.prevent_backslash =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Backslash);
    frontend_config.filters.prevent_profanity =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Profanity);
    frontend_config.filters.enable_callback =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Callback);
    return frontend_config;
}

} // namespace HLE::Applets
