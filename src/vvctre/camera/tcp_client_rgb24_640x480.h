// Copyright 2021 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <mutex>
#include <optional>

#include <boost/asio.hpp>

#include "core/frontend/camera/factory.h"
#include "core/frontend/camera/interface.h"

namespace Camera {

class TCP_Client_RGB24_640x480_Camera : public CameraInterface {
public:
    TCP_Client_RGB24_640x480_Camera(const std::string& params_string,
                                    const Service::CAM::Flip& flip);

    ~TCP_Client_RGB24_640x480_Camera();

    void Connect(const boost::asio::ip::tcp::endpoint endpoint);
    void Read();

    void StartCapture() override;
    void StopCapture() override;
    void SetResolution(const Service::CAM::Resolution& resolution) override;
    void SetFlip(Service::CAM::Flip flip) override;
    void SetEffect(Service::CAM::Effect effect) override;
    void SetFormat(Service::CAM::OutputFormat format) override;
    std::vector<u16> ReceiveFrame() override;

private:
    int width{};
    int height{};
    Service::CAM::OutputFormat format{};
    bool flip_horizontal{};
    bool flip_vertical{};
    bool basic_flip_horizontal{};
    bool basic_flip_vertical{};

    boost::system::error_code ec;
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::socket socket;
    std::mutex mutex;
    std::thread thread;
    std::atomic<bool> return_nothing = true;
    std::array<unsigned char, 640 * 480 * 3> read_buffer;
    std::optional<std::array<unsigned char, 640 * 480 * 3>> image_640x480;
};

class TCP_Client_RGB24_640x480_Camera_Factory : public CameraFactory {
public:
    std::unique_ptr<CameraInterface> Create(const std::string& params_string,
                                            const Service::CAM::Flip& flip) override;
};

} // namespace Camera
