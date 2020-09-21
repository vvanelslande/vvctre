// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include "audio_core/dsp_interface.h"
#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/hle/service/mic_u.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/settings.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Settings {

Values values;

void Apply() {
    GDBStub::SetServerPort(values.gdbstub_port);
    GDBStub::ToggleServer(values.use_gdbstub);
    InputCommon::ReloadInputDevices();

    VideoCore::g_hardware_renderer_enabled = values.use_hardware_renderer;
    VideoCore::g_shader_jit_enabled = values.use_shader_jit;
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
    if (system.IsPoweredOn()) {
        AudioCore::DspInterface& dsp = system.DSP();
        dsp.SetSink(values.audio_sink_id, values.audio_device_id);
        dsp.EnableStretching(values.enable_audio_stretching);

        std::shared_ptr<Service::HID::Module> hid = Service::HID::GetModule(system);
        if (hid != nullptr) {
            hid->ReloadInputDevices();
        }

        Service::SM::ServiceManager& sm = system.ServiceManager();
        std::shared_ptr<Service::IR::IR_USER> ir_user =
            sm.GetService<Service::IR::IR_USER>("ir:USER");
        if (ir_user != nullptr) {
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
