// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/frontend/camera/factory.h"
#include "core/frontend/camera/interface.h"

namespace Camera {

class ImageCamera : public CameraInterface {
public:
    ImageCamera(const std::string& file, const Service::CAM::Flip& flip);
    ~ImageCamera();
    void StartCapture() override;
    void StopCapture() override;
    void SetResolution(const Service::CAM::Resolution& resolution) override;
    void SetFlip(Service::CAM::Flip flip) override;
    void SetEffect(Service::CAM::Effect effect) override;
    void SetFormat(Service::CAM::OutputFormat format) override;
    std::vector<u16> ReceiveFrame() override;
    bool IsPreviewAvailable() override;

private:
    int file_width{};
    int file_height{};
    int requested_width{};
    int requested_height{};
    Service::CAM::OutputFormat format{};
    bool flip_horizontal{};
    bool flip_vertical{};
    bool basic_flip_horizontal{};
    bool basic_flip_vertical{};

    std::vector<unsigned char> unmodified_image;
    std::vector<unsigned char> resized_image;
};

class ImageCameraFactory : public CameraFactory {
public:
    std::unique_ptr<CameraInterface> Create(const std::string& file,
                                            const Service::CAM::Flip& flip) override;
};

} // namespace Camera
