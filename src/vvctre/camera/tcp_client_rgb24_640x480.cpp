// Copyright 2021 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <curl/curl.h>
#include <stb_image_resize.h>
#include "common/assert.h"
#include "common/file_util.h"
#include "common/param_package.h"
#include "vvctre/camera/tcp_client_rgb24_640x480.h"
#include "vvctre/camera/util.h"

namespace Camera {

TCP_Client_RGB24_640x480_Camera::TCP_Client_RGB24_640x480_Camera(const std::string& params_string,
                                                                 const Service::CAM::Flip& flip)
    : socket(io_service), thread([this, params_string] {
          Common::ParamPackage params(params_string);

          Connect(boost::asio::ip::tcp::endpoint(
              boost::asio::ip::make_address(params.Get("ip", "127.0.0.1")),
              static_cast<u16>(params.Get("port", 8000))));

          io_service.run();
      }) {
    flip_horizontal = basic_flip_horizontal =
        (flip == Service::CAM::Flip::Horizontal) || (flip == Service::CAM::Flip::Reverse);

    flip_vertical = basic_flip_vertical =
        (flip == Service::CAM::Flip::Vertical) || (flip == Service::CAM::Flip::Reverse);
}

TCP_Client_RGB24_640x480_Camera::~TCP_Client_RGB24_640x480_Camera() {
    socket.cancel();
    io_service.stop();
    thread.join();
}

void TCP_Client_RGB24_640x480_Camera::Connect(const boost::asio::ip::tcp::endpoint endpoint) {
    socket.async_connect(endpoint, [this, endpoint](const boost::system::error_code& error) {
        if (error) {
            Connect(endpoint);
        } else {
            Read();
        }
    });
}

void TCP_Client_RGB24_640x480_Camera::Read() {
    boost::asio::async_read(
        socket, boost::asio::buffer(read_buffer),
        [this](const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (error) {
                return;
            }

            {
                std::unique_lock<std::mutex> lock(mutex);
                image_640x480 = read_buffer;
            }

            Read();
        });
}

void TCP_Client_RGB24_640x480_Camera::SetResolution(const Service::CAM::Resolution& resolution) {
    width = resolution.width;
    height = resolution.height;
}

void TCP_Client_RGB24_640x480_Camera::SetFormat(Service::CAM::OutputFormat format) {
    this->format = format;
}

std::vector<u16> TCP_Client_RGB24_640x480_Camera::ReceiveFrame() {
    std::unique_lock<std::mutex> lock(mutex);

    if (!image_640x480) {
        return {};
    }

    std::vector<unsigned char> image(image_640x480->begin(), image_640x480->end());

    if (width != 640 || height != 480) {
        std::vector<unsigned char> resized_image(width * height * 3);

        ASSERT(stbir_resize_uint8(image.data(), 640, 480, 0, resized_image.data(), width, height, 0,
                                  3) == 1);

        image = resized_image;
    }

    if (flip_horizontal) {
        image = FlipRgb24ImageHorizontally(width, height, image);
    }

    if (flip_vertical) {
        image = FlipRgb24ImageVertically(width, height, image);
    }

    if (format == Service::CAM::OutputFormat::RGB565) {
        std::vector<u16> frame(width * height, 0);
        std::size_t offset = 0;
        std::size_t pixel = 0;

        while (offset < image.size()) {
            const unsigned char r = image[offset++];
            const unsigned char g = image[offset++];
            const unsigned char b = image[offset++];
            frame[pixel++] = ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | (b >> 3);
        }

        return frame;
    } else if (format == Service::CAM::OutputFormat::YUV422) {
        return ConvertRgb24ToYuv422(image, width, height);
    }

    return {};
}

void TCP_Client_RGB24_640x480_Camera::StartCapture() {}
void TCP_Client_RGB24_640x480_Camera::StopCapture() {}

void TCP_Client_RGB24_640x480_Camera::SetFlip(Service::CAM::Flip flip) {
    flip_horizontal = basic_flip_horizontal ^ (flip == Service::CAM::Flip::Horizontal ||
                                               flip == Service::CAM::Flip::Reverse);

    flip_vertical = basic_flip_vertical ^
                    (flip == Service::CAM::Flip::Vertical || flip == Service::CAM::Flip::Reverse);
}

void TCP_Client_RGB24_640x480_Camera::SetEffect(Service::CAM::Effect effect) {
    UNIMPLEMENTED();
}

std::unique_ptr<CameraInterface> TCP_Client_RGB24_640x480_Camera_Factory::Create(
    const std::string& params_string, const Service::CAM::Flip& flip) {
    return std::make_unique<TCP_Client_RGB24_640x480_Camera>(params_string, flip);
}

} // namespace Camera
