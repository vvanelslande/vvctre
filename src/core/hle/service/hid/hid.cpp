// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <numeric>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/3ds.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/apt/applet_manager.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/hid/hid_spvr.h"
#include "core/hle/service/hid/hid_user.h"
#include "core/hle/service/service.h"
#include "core/movie.h"
#include "video_core/video_core.h"

namespace Service::HID {

// Updating period for each HID device. These empirical values are measured from a 11.2 3DS.
constexpr u64 pad_update_ticks = BASE_CLOCK_RATE_ARM11 / 234;
constexpr u64 accelerometer_update_ticks = BASE_CLOCK_RATE_ARM11 / 104;
constexpr u64 gyroscope_update_ticks = BASE_CLOCK_RATE_ARM11 / 101;

constexpr float accelerometer_coef = 512.0f; // measured from hw test result
constexpr float gyroscope_coef = 14.375f; // got from hwtest GetGyroscopeLowRawToDpsCoefficient call

static DirectionState GetStickDirectionState(s16 circle_pad_x, s16 circle_pad_y) {
    // 30 degree and 60 degree are angular thresholds for directions
    constexpr float TAN30 = 0.577350269f;
    constexpr float TAN60 = 1 / TAN30;
    // A circle pad radius greater than 40 will trigger circle pad direction
    constexpr int CIRCLE_PAD_THRESHOLD_SQUARE = 40 * 40;
    DirectionState state{false, false, false, false};

    if (circle_pad_x * circle_pad_x + circle_pad_y * circle_pad_y > CIRCLE_PAD_THRESHOLD_SQUARE) {
        const float t = std::abs(static_cast<float>(circle_pad_y) / circle_pad_x);

        if (circle_pad_x != 0 && t < TAN60) {
            if (circle_pad_x > 0) {
                state.right = true;
            } else {
                state.left = true;
            }
        }

        if (circle_pad_x == 0 || t > TAN30) {
            if (circle_pad_y > 0) {
                state.up = true;
            } else {
                state.down = true;
            }
        }
    }

    return state;
}

void Module::LoadInputDevices() {
    std::transform(Settings::values.buttons.begin() + Settings::NativeButton::BUTTON_HID_BEGIN,
                   Settings::values.buttons.begin() + Settings::NativeButton::BUTTON_HID_END,
                   buttons.begin(), Input::CreateDevice<Input::ButtonDevice>);
    circle_pad = Input::CreateDevice<Input::AnalogDevice>(
        Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
    motion_device = Input::CreateDevice<Input::MotionDevice>(Settings::values.motion_device);
    touch_device = Input::CreateDevice<Input::TouchDevice>(Settings::values.touch_device);
    home_button = Input::CreateDevice<Input::ButtonDevice>(
        Settings::values.buttons[Settings::NativeButton::Home]);
}

void Module::UpdatePadCallback(std::uintptr_t user_data, s64 cycles_late) {
    if (is_device_reload_pending.exchange(false)) {
        LoadInputDevices();
    }

    if (home_button->GetStatus()) {
        if (Settings::values.region_value == Settings::REGION_VALUE_AUTO_SELECT) {
            LOG_ERROR(Service_HID, "Can't open HOME Menu because region is Auto-select");
        } else {
            const u64 title_id = Service::APT::GetTitleIdForApplet(
                Service::APT::AppletId::HomeMenu, static_cast<u32>(Settings::values.region_value));
            const std::string path =
                Service::AM::GetTitleContentPath(Service::FS::MediaType::NAND, title_id);
            if (FileUtil::Exists(path)) {
                system.SetResetFilePath(path);
                system.RequestReset();
                return;
            } else {
                LOG_ERROR(Service_HID, "HOME Menu not installed");
            }
        }
    }

    SharedMem* mem = reinterpret_cast<SharedMem*>(shared_mem->GetPointer());
    PadState state = GetPadState();

    // Get current circle pad position and update circle pad direction
    const auto [circle_pad_x_f, circle_pad_y_f] = GetCirclePadState();

    // xperia64: 0x9A seems to be the calibrated limit of the circle pad
    // Verified by using Input Redirector with very large-value digital inputs
    // on the circle pad and calibrating using the system settings application
    constexpr int MAX_CIRCLEPAD_POS = 0x9A; // Max value for a circle pad position
    // These are rounded rather than truncated on actual hardware
    s16 circle_pad_new_x = static_cast<s16>(std::roundf(circle_pad_x_f * MAX_CIRCLEPAD_POS));
    s16 circle_pad_new_y = static_cast<s16>(std::roundf(circle_pad_y_f * MAX_CIRCLEPAD_POS));
    s16 circle_pad_x =
        (circle_pad_new_x + std::accumulate(circle_pad_old_x.begin(), circle_pad_old_x.end(), 0)) /
        CIRCLE_PAD_AVERAGING;
    s16 circle_pad_y =
        (circle_pad_new_y + std::accumulate(circle_pad_old_y.begin(), circle_pad_old_y.end(), 0)) /
        CIRCLE_PAD_AVERAGING;
    circle_pad_old_x.erase(circle_pad_old_x.begin());
    circle_pad_old_x.push_back(circle_pad_new_x);
    circle_pad_old_y.erase(circle_pad_old_y.begin());
    circle_pad_old_y.push_back(circle_pad_new_y);

    Core::Movie::GetInstance().HandlePadAndCircleStatus(state, circle_pad_x, circle_pad_y);

    if (!custom_pad_state) {
        const DirectionState direction = GetStickDirectionState(circle_pad_x, circle_pad_y);
        state.circle_up.Assign(direction.up);
        state.circle_down.Assign(direction.down);
        state.circle_left.Assign(direction.left);
        state.circle_right.Assign(direction.right);
    }

    mem->pad.current_state.hex = state.hex;
    mem->pad.index = next_pad_index;
    next_pad_index = (next_pad_index + 1) % mem->pad.entries.size();

    // Get the previous pad state
    u32 last_entry_index = (mem->pad.index - 1) % mem->pad.entries.size();
    PadDataEntry& old_entry = mem->pad.entries[last_entry_index];

    // Compute bitmask with 1s for bits different from the old state
    PadState changed = {{(state.hex ^ old_entry.current_state.hex)}};

    // Get the current pad entry
    PadDataEntry& pad_entry = mem->pad.entries[mem->pad.index];

    // Update entry properties
    pad_entry.current_state.hex = state.hex;
    pad_entry.delta_additions.hex = changed.hex & state.hex;
    pad_entry.delta_removals.hex = changed.hex & old_entry.current_state.hex;
    pad_entry.circle_pad_x = (old_entry.circle_pad_x + circle_pad_x) / 2;
    pad_entry.circle_pad_y = (old_entry.circle_pad_y + circle_pad_y) / 2;

    // If we just updated index 0, provide a new timestamp
    if (mem->pad.index == 0) {
        mem->pad.index_reset_ticks_previous = mem->pad.index_reset_ticks;
        mem->pad.index_reset_ticks = (s64)system.CoreTiming().GetTicks();
    }

    mem->touch.index = next_touch_index;
    next_touch_index = (next_touch_index + 1) % mem->touch.entries.size();

    // Get the current touch entry
    TouchDataEntry& touch_entry = mem->touch.entries[mem->touch.index];
    const auto [x, y, pressed] = GetTouchState();
    touch_entry.x = static_cast<u16>(x * Core::kScreenBottomWidth);
    touch_entry.y = static_cast<u16>(y * Core::kScreenBottomHeight);
    touch_entry.valid.Assign(pressed ? 1 : 0);

    Core::Movie::GetInstance().HandleTouchStatus(touch_entry);

    // TODO(bunnei): We're not doing anything with offset 0xA8 + 0x18 of HID SharedMemory, which
    // supposedly is "Touch-screen entry, which contains the raw coordinate data prior to being
    // converted to pixel coordinates." (http://3dbrew.org/wiki/HID_Shared_Memory#Offset_0xA8).

    // If we just updated index 0, provide a new timestamp
    if (mem->touch.index == 0) {
        mem->touch.index_reset_ticks_previous = mem->touch.index_reset_ticks;
        mem->touch.index_reset_ticks = (s64)system.CoreTiming().GetTicks();
    }

    // Signal both handles when there's an update to Pad or touch
    event_pad_or_touch_1->Signal();
    event_pad_or_touch_2->Signal();

    // TODO(xperia64): How the 3D Slider is updated by the HID module needs to be RE'd
    // and possibly moved to its own Core::Timing event.
    system.Kernel().GetSharedPageHandler().GetSharedPage().sliderstate_3d =
        static_cast<float_le>(Settings::values.factor_3d / 100.0f);

    // Reschedule recurrent event
    system.CoreTiming().ScheduleEvent(pad_update_ticks - cycles_late, pad_update_event);
}

void Module::UpdateAccelerometerCallback(std::uintptr_t user_data, s64 cycles_late) {
    SharedMem* mem = reinterpret_cast<SharedMem*>(shared_mem->GetPointer());

    mem->accelerometer.index = next_accelerometer_index;
    next_accelerometer_index = (next_accelerometer_index + 1) % mem->accelerometer.entries.size();

    [[maybe_unused]] auto [accel, gyro] = GetMotionState();
    accel *= accelerometer_coef;

    // TODO(wwylele): do a time stretch like the one in UpdateGyroscopeCallback
    // The time stretch formula should be like
    // stretched_vector = (raw_vector - gravity) * stretch_ratio + gravity

    AccelerometerDataEntry& accelerometer_entry =
        mem->accelerometer.entries[mem->accelerometer.index];

    accelerometer_entry.x = static_cast<s16>(accel.x);
    accelerometer_entry.y = static_cast<s16>(accel.y);
    accelerometer_entry.z = static_cast<s16>(accel.z);

    Core::Movie::GetInstance().HandleAccelerometerStatus(accelerometer_entry);

    // Make up "raw" entry
    // TODO(wwylele):
    // From hardware testing, the raw_entry values are approximately, but not exactly, as twice as
    // corresponding entries (or with a minus sign). It may caused by system calibration to the
    // accelerometer. Figure out how it works, or, if no game reads raw_entry, the following three
    // lines can be removed and leave raw_entry unimplemented.
    mem->accelerometer.raw_entry.x = -2 * accelerometer_entry.x;
    mem->accelerometer.raw_entry.z = 2 * accelerometer_entry.y;
    mem->accelerometer.raw_entry.y = -2 * accelerometer_entry.z;

    // If we just updated index 0, provide a new timestamp
    if (mem->accelerometer.index == 0) {
        mem->accelerometer.index_reset_ticks_previous = mem->accelerometer.index_reset_ticks;
        mem->accelerometer.index_reset_ticks = (s64)system.CoreTiming().GetTicks();
    }

    event_accelerometer->Signal();

    // Reschedule recurrent event
    system.CoreTiming().ScheduleEvent(accelerometer_update_ticks - cycles_late,
                                      accelerometer_update_event);
}

void Module::UpdateGyroscopeCallback(std::uintptr_t user_data, s64 cycles_late) {
    SharedMem* mem = reinterpret_cast<SharedMem*>(shared_mem->GetPointer());

    mem->gyroscope.index = next_gyroscope_index;
    next_gyroscope_index = (next_gyroscope_index + 1) % mem->gyroscope.entries.size();

    GyroscopeDataEntry& gyroscope_entry = mem->gyroscope.entries[mem->gyroscope.index];

    [[maybe_unused]] auto [accel, gyro] = GetMotionState();
    double stretch = system.perf_stats->GetLastFrameTimeScale();
    gyro *= gyroscope_coef * static_cast<float>(stretch);
    gyroscope_entry.x = static_cast<s16>(gyro.x);
    gyroscope_entry.y = static_cast<s16>(gyro.y);
    gyroscope_entry.z = static_cast<s16>(gyro.z);

    Core::Movie::GetInstance().HandleGyroscopeStatus(gyroscope_entry);

    // Make up "raw" entry
    mem->gyroscope.raw_entry.x = gyroscope_entry.x;
    mem->gyroscope.raw_entry.z = -gyroscope_entry.y;
    mem->gyroscope.raw_entry.y = gyroscope_entry.z;

    // If we just updated index 0, provide a new timestamp
    if (mem->gyroscope.index == 0) {
        mem->gyroscope.index_reset_ticks_previous = mem->gyroscope.index_reset_ticks;
        mem->gyroscope.index_reset_ticks = (s64)system.CoreTiming().GetTicks();
    }

    event_gyroscope->Signal();

    // Reschedule recurrent event
    system.CoreTiming().ScheduleEvent(gyroscope_update_ticks - cycles_late, gyroscope_update_event);
}

void Module::Interface::GetIPCHandles(Kernel::HLERequestContext& ctx) {
    IPC::RequestBuilder rb(ctx, 0xA, 1, 7);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(hid->shared_mem, hid->event_pad_or_touch_1, hid->event_pad_or_touch_2,
                       hid->event_accelerometer, hid->event_gyroscope, hid->event_debug_pad);
}

void Module::Interface::EnableAccelerometer(Kernel::HLERequestContext& ctx) {
    ++hid->enable_accelerometer_count;

    // Schedules the accelerometer update event if the accelerometer was just enabled
    if (hid->enable_accelerometer_count == 1) {
        hid->system.CoreTiming().ScheduleEvent(accelerometer_update_ticks,
                                               hid->accelerometer_update_event);
    }

    IPC::RequestBuilder rb(ctx, 0x11, 1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_HID, "called");
}

void Module::Interface::DisableAccelerometer(Kernel::HLERequestContext& ctx) {
    --hid->enable_accelerometer_count;

    // Unschedules the accelerometer update event if the accelerometer was just disabled
    if (hid->enable_accelerometer_count == 0) {
        hid->system.CoreTiming().UnscheduleEvent(hid->accelerometer_update_event, 0);
    }

    IPC::RequestBuilder rb(ctx, 0x12, 1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_HID, "called");
}

void Module::Interface::EnableGyroscopeLow(Kernel::HLERequestContext& ctx) {
    ++hid->enable_gyroscope_count;

    // Schedules the gyroscope update event if the gyroscope was just enabled
    if (hid->enable_gyroscope_count == 1) {
        hid->system.CoreTiming().ScheduleEvent(gyroscope_update_ticks, hid->gyroscope_update_event);
    }

    IPC::RequestBuilder rb(ctx, 0x13, 1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_HID, "called");
}

void Module::Interface::DisableGyroscopeLow(Kernel::HLERequestContext& ctx) {
    --hid->enable_gyroscope_count;

    // Unschedules the gyroscope update event if the gyroscope was just disabled
    if (hid->enable_gyroscope_count == 0) {
        hid->system.CoreTiming().UnscheduleEvent(hid->gyroscope_update_event, 0);
    }

    IPC::RequestBuilder rb(ctx, 0x14, 1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_HID, "called");
}

void Module::Interface::GetGyroscopeLowRawToDpsCoefficient(Kernel::HLERequestContext& ctx) {
    IPC::RequestBuilder rb(ctx, 0x15, 2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(gyroscope_coef);
}

void Module::Interface::GetGyroscopeLowCalibrateParam(Kernel::HLERequestContext& ctx) {
    IPC::RequestBuilder rb(ctx, 0x16, 6, 0);
    rb.Push(RESULT_SUCCESS);

    const s16 param_unit = 6700; // An approximate value taken from hw
    GyroscopeCalibrateParam param = {
        {0, param_unit, -param_unit},
        {0, param_unit, -param_unit},
        {0, param_unit, -param_unit},
    };
    rb.PushRaw(param);

    LOG_WARNING(Service_HID, "(STUBBED) called");
}

void Module::Interface::GetSoundVolume(Kernel::HLERequestContext& ctx) {
    const u8 volume = static_cast<u8>(0x3F * Settings::values.audio_volume);

    IPC::RequestBuilder rb(ctx, 0x17, 2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(volume);
}

Module::Interface::Interface(std::shared_ptr<Module> hid, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), hid(std::move(hid)) {}

std::shared_ptr<Module> Module::Interface::GetModule() const {
    return hid;
}

Module::Module(Core::System& system) : system(system) {
    using namespace Kernel;

    shared_mem =
        system.Kernel()
            .CreateSharedMemory(nullptr, 0x1000, MemoryPermission::ReadWrite,
                                MemoryPermission::Read, 0, MemoryRegion::BASE, "HID:SharedMemory")
            .Unwrap();

    // Create event handles
    event_pad_or_touch_1 = system.Kernel().CreateEvent(ResetType::OneShot, "HID:EventPadOrTouch1");
    event_pad_or_touch_2 = system.Kernel().CreateEvent(ResetType::OneShot, "HID:EventPadOrTouch2");
    event_accelerometer = system.Kernel().CreateEvent(ResetType::OneShot, "HID:EventAccelerometer");
    event_gyroscope = system.Kernel().CreateEvent(ResetType::OneShot, "HID:EventGyroscope");
    event_debug_pad = system.Kernel().CreateEvent(ResetType::OneShot, "HID:EventDebugPad");

    // Register update callbacks
    Core::Timing& timing = system.CoreTiming();
    pad_update_event = timing.RegisterEvent("HID::UpdatePadCallback",
                                            [this](std::uintptr_t user_data, s64 cycles_late) {
                                                UpdatePadCallback(user_data, cycles_late);
                                            });
    accelerometer_update_event = timing.RegisterEvent(
        "HID::UpdateAccelerometerCallback", [this](std::uintptr_t user_data, s64 cycles_late) {
            UpdateAccelerometerCallback(user_data, cycles_late);
        });
    gyroscope_update_event = timing.RegisterEvent(
        "HID::UpdateGyroscopeCallback", [this](std::uintptr_t user_data, s64 cycles_late) {
            UpdateGyroscopeCallback(user_data, cycles_late);
        });

    timing.ScheduleEvent(pad_update_ticks, pad_update_event);

    circle_pad_old_x.assign(CIRCLE_PAD_AVERAGING - 1, 0);
    circle_pad_old_y.assign(CIRCLE_PAD_AVERAGING - 1, 0);
}

void Module::ReloadInputDevices() {
    is_device_reload_pending.store(true);
}

void Module::SetCustomPadState(std::optional<PadState> state) {
    custom_pad_state = std::move(state);
}

const PadState Module::GetPadState() const {
    if (custom_pad_state) {
        return *custom_pad_state;
    }

    using namespace Settings::NativeButton;
    PadState state;
    state.a.Assign(buttons[A - BUTTON_HID_BEGIN]->GetStatus());
    state.b.Assign(buttons[B - BUTTON_HID_BEGIN]->GetStatus());
    state.x.Assign(buttons[X - BUTTON_HID_BEGIN]->GetStatus());
    state.y.Assign(buttons[Y - BUTTON_HID_BEGIN]->GetStatus());
    state.right.Assign(buttons[Right - BUTTON_HID_BEGIN]->GetStatus());
    state.left.Assign(buttons[Left - BUTTON_HID_BEGIN]->GetStatus());
    state.up.Assign(buttons[Up - BUTTON_HID_BEGIN]->GetStatus());
    state.down.Assign(buttons[Down - BUTTON_HID_BEGIN]->GetStatus());
    state.l.Assign(buttons[L - BUTTON_HID_BEGIN]->GetStatus());
    state.r.Assign(buttons[R - BUTTON_HID_BEGIN]->GetStatus());
    state.start.Assign(buttons[Start - BUTTON_HID_BEGIN]->GetStatus());
    state.select.Assign(buttons[Select - BUTTON_HID_BEGIN]->GetStatus());
    state.debug.Assign(buttons[Debug - BUTTON_HID_BEGIN]->GetStatus());
    state.gpio14.Assign(buttons[Gpio14 - BUTTON_HID_BEGIN]->GetStatus());
    return state;
}

void Module::SetCustomCirclePadState(std::optional<std::tuple<float, float>> state) {
    custom_circle_pad_state = std::move(state);
}

const std::tuple<float, float> Module::GetCirclePadState() const {
    return custom_circle_pad_state.has_value() ? *custom_circle_pad_state : circle_pad->GetStatus();
}

void Module::SetCustomTouchState(std::optional<std::tuple<float, float, bool>> state) {
    custom_touch_state = std::move(state);
}

const std::tuple<float, float, bool> Module::GetTouchState() const {
    return custom_touch_state.has_value() ? *custom_touch_state : touch_device->GetStatus();
}

void Module::SetCustomMotionState(
    std::optional<std::tuple<Common::Vec3<float>, Common::Vec3<float>>> state) {
    custom_motion_state = std::move(state);
}

const std::tuple<Common::Vec3<float>, Common::Vec3<float>> Module::GetMotionState() const {
    return custom_motion_state.has_value() ? *custom_motion_state : motion_device->GetStatus();
}

std::shared_ptr<Module> GetModule(Core::System& system) {
    std::shared_ptr<Service::HID::Module::Interface> hid =
        system.ServiceManager().GetService<Service::HID::Module::Interface>("hid:USER");
    if (hid == nullptr) {
        return nullptr;
    }
    return hid->GetModule();
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto hid = std::make_shared<Module>(system);
    std::make_shared<User>(hid)->InstallAsService(service_manager);
    std::make_shared<Spvr>(hid)->InstallAsService(service_manager);
}

} // namespace Service::HID
