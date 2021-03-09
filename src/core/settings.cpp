// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include "audio_core/dsp_interface.h"
#include "core/core.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/hle/service/mic_u.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/settings.h"
#include "video_core/renderer/renderer.h"
#include "video_core/video_core.h"

namespace Settings {

Values values;

void Apply() {
    InputCommon::ReloadInputDevices();

    VideoCore::g_hardware_shader_enabled = values.use_hardware_shader;
    VideoCore::g_hardware_shader_accurate_multiplication =
        values.hardware_shader_accurate_multiplication;

    if (VideoCore::g_renderer) {
        VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
    }

    VideoCore::g_renderer_background_color_update_requested = true;
    VideoCore::g_renderer_sampler_update_requested = true;
    VideoCore::g_renderer_shader_update_requested = true;
    VideoCore::g_texture_filter_update_requested = true;

    Core::System& system = Core::System::GetInstance();
    if (system.IsInitialized()) {
        AudioCore::DspInterface& dsp = system.DSP();
        dsp.SetSink(values.audio_sink_id, values.audio_device_id);
        dsp.EnableStretching(values.enable_audio_stretching);

        if (std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(system)) {
            hid->ReloadInputDevices();
        }

        if (std::shared_ptr<Service::IR::IR_USER> ir_user =
                system.ServiceManager().GetService<Service::IR::IR_USER>("ir:USER")) {
            ir_user->ReloadInputDevices();
        }

        std::shared_ptr<Service::CAM::Module> cam = Service::CAM::GetModule(system);
        if (cam != nullptr) {
            cam->ReloadCameraDevices();
        }

        Service::MIC::ReloadMic(system);
    }
}

} // namespace Settings
