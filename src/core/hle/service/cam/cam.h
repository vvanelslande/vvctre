// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <deque>
#include <future>
#include <memory>
#include <vector>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
} // namespace Core

namespace Camera {
class CameraInterface;
} // namespace Camera

namespace Core {
struct TimingEventType;
} // namespace Core

namespace Kernel {
class Process;
} // namespace Kernel

namespace Service::CAM {

enum CameraIndex {
    OuterRightCamera = 0,
    InnerCamera = 1,
    OuterLeftCamera = 2,

    NumCameras = 3,
};

enum class Effect : u8 {
    None = 0,
    Mono = 1,
    Sepia = 2,
    Negative = 3,
    Negafilm = 4,
    Sepia01 = 5,
};

enum class Flip : u8 {
    None = 0,
    Horizontal = 1,
    Vertical = 2,
    Reverse = 3,
};

enum class Size : u8 {
    VGA = 0,
    QVGA = 1,
    QQVGA = 2,
    CIF = 3,
    QCIF = 4,
    DS_LCD = 5,
    DS_LCDx4 = 6,
    CTR_TOP_LCD = 7,
    CTR_BOTTOM_LCD = QVGA,
};

enum class FrameRate : u8 {
    Rate_15 = 0,
    Rate_15_To_5 = 1,
    Rate_15_To_2 = 2,
    Rate_10 = 3,
    Rate_8_5 = 4,
    Rate_5 = 5,
    Rate_20 = 6,
    Rate_20_To_5 = 7,
    Rate_30 = 8,
    Rate_30_To_5 = 9,
    Rate_15_To_10 = 10,
    Rate_20_To_10 = 11,
    Rate_30_To_10 = 12,
};

enum class ShutterSoundType : u8 {
    Normal = 0,
    Movie = 1,
    MovieEnd = 2,
};

enum class WhiteBalance : u8 {
    BalanceAuto = 0,
    Balance3200K = 1,
    Balance4150K = 2,
    Balance5200K = 3,
    Balance6000K = 4,
    Balance7000K = 5,
    BalanceMax = 6,
    BalanceNormal = BalanceAuto,
    BalanceTungsten = Balance3200K,
    BalanceWhiteFluorescentLight = Balance4150K,
    BalanceDaylight = Balance5200K,
    BalanceCloudy = Balance6000K,
    BalanceHorizon = Balance6000K,
    BalanceShade = Balance7000K,
};

enum class PhotoMode : u8 {
    Normal = 0,
    Portrait = 1,
    Landscape = 2,
    Nightview = 3,
    Letter0 = 4,
};

enum class LensCorrection : u8 {
    Off = 0,
    On70 = 1,
    On90 = 2,
    Dark = Off,
    Normal = On70,
    Bright = On90,
};

enum class Contrast : u8 {
    Pattern01 = 1,
    Pattern02 = 2,
    Pattern03 = 3,
    Pattern04 = 4,
    Pattern05 = 5,
    Pattern06 = 6,
    Pattern07 = 7,
    Pattern08 = 8,
    Pattern09 = 9,
    Pattern10 = 10,
    Pattern11 = 11,
    Low = Pattern05,
    Normal = Pattern06,
    High = Pattern07,
};

enum class OutputFormat : u8 {
    YUV422 = 0,
    RGB565 = 1,
};

/// Stereo camera calibration data.
struct StereoCameraCalibrationData {
    u8 isValidRotationXY; ///< Bool indicating whether the X and Y rotation data is valid.
    INSERT_PADDING_BYTES(3);
    float_le scale;        ///< Scale to match the left camera image with the right.
    float_le rotationZ;    ///< Z axis rotation to match the left camera image with the right.
    float_le translationX; ///< X axis translation to match the left camera image with the right.
    float_le translationY; ///< Y axis translation to match the left camera image with the right.
    float_le rotationX;    ///< X axis rotation to match the left camera image with the right.
    float_le rotationY;    ///< Y axis rotation to match the left camera image with the right.
    float_le angleOfViewRight; ///< Right camera angle of view.
    float_le angleOfViewLeft;  ///< Left camera angle of view.
    float_le distanceToChart;  ///< Distance between cameras and measurement chart.
    float_le distanceCameras;  ///< Distance between left and right cameras.
    s16_le imageWidth;         ///< Image width.
    s16_le imageHeight;        ///< Image height.
    INSERT_PADDING_BYTES(16);
};
static_assert(sizeof(StereoCameraCalibrationData) == 64,
              "StereoCameraCalibrationData structure size is wrong");

/**
 * Resolution parameters for the camera.
 * The native resolution of 3DS camera is 640 * 480. The captured image will be cropped in the
 * region [crop_x0, crop_x1] * [crop_y0, crop_y1], and then scaled to size width * height as the
 * output image. Note that all cropping coordinates are inclusive.
 */
struct Resolution {
    u16 width;
    u16 height;
    u16 crop_x0;
    u16 crop_y0;
    u16 crop_x1;
    u16 crop_y1;
};

struct PackageParameterWithoutContext {
    u8 camera_select;
    s8 exposure;
    WhiteBalance white_balance;
    s8 sharpness;
    bool auto_exposure;
    bool auto_white_balance;
    PhotoMode photo_mode;
    Contrast contrast;
    LensCorrection lens_correction;
    bool noise_filter;
    u8 padding;
    s16 auto_exposure_window_x;
    s16 auto_exposure_window_y;
    s16 auto_exposure_window_width;
    s16 auto_exposure_window_height;
    s16 auto_white_balance_window_x;
    s16 auto_white_balance_window_y;
    s16 auto_white_balance_window_width;
    s16 auto_white_balance_window_height;
    INSERT_PADDING_WORDS(4);
};

static_assert(sizeof(PackageParameterWithoutContext) == 44,
              "PackageParameterCameraWithoutContext structure size is wrong");

struct PackageParameterWithContext {
    u8 camera_select;
    u8 context_select;
    Flip flip;
    Effect effect;
    Size size;
    INSERT_PADDING_BYTES(3);
    INSERT_PADDING_WORDS(3);

    Resolution GetResolution() const;
};

static_assert(sizeof(PackageParameterWithContext) == 20,
              "PackageParameterWithContext structure size is wrong");

struct PackageParameterWithContextDetail {
    u8 camera_select;
    u8 context_select;
    Flip flip;
    Effect effect;
    Resolution resolution;
    INSERT_PADDING_WORDS(3);

    Resolution GetResolution() const {
        return resolution;
    }
};

static_assert(sizeof(PackageParameterWithContextDetail) == 28,
              "PackageParameterWithContextDetail structure size is wrong");

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module();
    void ReloadCameraDevices();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> cam, const char* name, u32 max_session);
        ~Interface();

        std::shared_ptr<Module> GetModule() const;

    protected:
        void StartCapture(Kernel::HLERequestContext& ctx);
        void StopCapture(Kernel::HLERequestContext& ctx);
        void IsBusy(Kernel::HLERequestContext& ctx);
        void ClearBuffer(Kernel::HLERequestContext& ctx);
        void GetVsyncInterruptEvent(Kernel::HLERequestContext& ctx);
        void GetBufferErrorInterruptEvent(Kernel::HLERequestContext& ctx);
        void SetReceiving(Kernel::HLERequestContext& ctx);
        void IsFinishedReceiving(Kernel::HLERequestContext& ctx);
        void SetTransferLines(Kernel::HLERequestContext& ctx);
        void GetMaxLines(Kernel::HLERequestContext& ctx);
        void SetTransferBytes(Kernel::HLERequestContext& ctx);
        void GetTransferBytes(Kernel::HLERequestContext& ctx);
        void GetMaxBytes(Kernel::HLERequestContext& ctx);
        void SetTrimming(Kernel::HLERequestContext& ctx);
        void IsTrimming(Kernel::HLERequestContext& ctx);
        void SetTrimmingParams(Kernel::HLERequestContext& ctx);
        void GetTrimmingParams(Kernel::HLERequestContext& ctx);
        void SetTrimmingParamsCenter(Kernel::HLERequestContext& ctx);
        void Activate(Kernel::HLERequestContext& ctx);
        void SwitchContext(Kernel::HLERequestContext& ctx);
        void FlipImage(Kernel::HLERequestContext& ctx);
        void SetDetailSize(Kernel::HLERequestContext& ctx);
        void SetSize(Kernel::HLERequestContext& ctx);
        void SetFrameRate(Kernel::HLERequestContext& ctx);
        void SetEffect(Kernel::HLERequestContext& ctx);
        void SetOutputFormat(Kernel::HLERequestContext& ctx);
        void SynchronizeVsyncTiming(Kernel::HLERequestContext& ctx);
        void GetLatestVsyncTiming(Kernel::HLERequestContext& ctx);
        void GetStereoCameraCalibrationData(Kernel::HLERequestContext& ctx);
        void SetPackageParameterWithoutContext(Kernel::HLERequestContext& ctx);
        void SetPackageParameterWithContext(Kernel::HLERequestContext& ctx);
        void SetPackageParameterWithContextDetail(Kernel::HLERequestContext& ctx);
        void GetSuitableY2rStandardCoefficient(Kernel::HLERequestContext& ctx);
        void PlayShutterSound(Kernel::HLERequestContext& ctx);
        void DriverInitialize(Kernel::HLERequestContext& ctx);
        void DriverFinalize(Kernel::HLERequestContext& ctx);

    private:
        std::shared_ptr<Module> cam;
    };

private:
    void CompletionEventCallBack(u64 port_id, s64);
    void VsyncInterruptEventCallBack(u64 port_id, s64 cycles_late);

    // Starts a receiving process on the specified port. This can only be called when is_busy = true
    // and is_receiving = false.
    void StartReceiving(int port_id);

    // Cancels any ongoing receiving processes at the specified port. This is used by functions that
    // stop capturing.
    // TODO: what is the exact behaviour on real 3DS when stopping capture during an ongoing
    //       process? Will the completion event still be signaled?
    void CancelReceiving(int port_id);

    // Activates the specified port with the specfied camera.
    void ActivatePort(int port_id, int camera_id);

    template <typename PackageParameterType>
    ResultCode SetPackageParameter(const PackageParameterType& package);

    struct ContextConfig {
        Flip flip{Flip::None};
        Effect effect{Effect::None};
        OutputFormat format{OutputFormat::YUV422};
        Resolution resolution = {0, 0, 0, 0, 0, 0};
    };

    struct CameraConfig {
        std::unique_ptr<Camera::CameraInterface> impl;
        std::array<ContextConfig, 2> contexts;
        int current_context{0};
    };

    struct PortConfig {
        int camera_id{0};

        bool is_active{false}; // Set when the port is activated by an Activate call.

        // Set if SetReceiving is called when is_busy = false. When StartCapture is called then,
        // this will trigger a receiving process and reset itself.
        bool is_pending_receiving{false};

        // Set when StartCapture is called and reset when StopCapture is called.
        bool is_busy{false};

        bool is_receiving{false}; // Set when there is an ongoing receiving process.

        bool is_trimming{false};
        u16 x0{0}; // X of starting position for trimming
        u16 y0{0}; // Y of starting position for trimming
        u16 x1{0}; // X of ending position for trimming
        u16 y1{0}; // Y of ending position for trimming

        u16 transfer_bytes{256};

        std::shared_ptr<Kernel::Event> completion_event;
        std::shared_ptr<Kernel::Event> buffer_error_interrupt_event;
        std::shared_ptr<Kernel::Event> vsync_interrupt_event;

        std::deque<s64> vsync_timings;

        std::future<std::vector<u16>> capture_result; // Will hold the received frame.
        Kernel::Process* dest_process = nullptr;
        VAddr dest = 0;    // The destination address of the receiving process
        u32 dest_size = 0; // The destination size of the receiving process

        void Clear();
    };

    void LoadCameraImplementation(CameraConfig& camera, int camera_id);

    Core::System& system;
    std::array<CameraConfig, NumCameras> cameras;
    std::array<PortConfig, 2> ports;
    Core::TimingEventType* completion_event_callback;
    Core::TimingEventType* vsync_interrupt_event_callback;
    std::atomic<bool> is_camera_reload_pending{false};
};

std::shared_ptr<Module> GetModule(Core::System& system);

void InstallInterfaces(Core::System& system);

} // namespace Service::CAM
