// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <curl/curl.h>
#include <mbedtls/ssl.h>
#include <stb_image.h>
#include <stb_image_resize.h>
#include "common/assert.h"
#include "common/file_util.h"
#include "vvctre/camera/image.h"
#include "vvctre/camera/util.h"

namespace Camera {

ImageCamera::ImageCamera(const std::string& file, const Service::CAM::Flip& flip) {
    flip_horizontal = basic_flip_horizontal =
        (flip == Service::CAM::Flip::Horizontal) || (flip == Service::CAM::Flip::Reverse);

    flip_vertical = basic_flip_vertical =
        (flip == Service::CAM::Flip::Vertical) || (flip == Service::CAM::Flip::Reverse);

    while (unmodified_image.empty()) {
        if (file.find("http://") != std::string::npos ||
            file.find("https://") != std::string::npos) {
            CURL* curl = curl_easy_init();
            if (curl == nullptr) {
                LOG_DEBUG(Service_CAM, "curl_easy_init failed");
                continue;
            }

            CURLcode error = curl_easy_setopt(curl, CURLOPT_URL, file.c_str());
            if (error != CURLE_OK) {
                LOG_DEBUG(Service_CAM, "{}", curl_easy_strerror(error));
                curl_easy_cleanup(curl);
                continue;
            }

            error = curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            if (error != CURLE_OK) {
                LOG_DEBUG(Service_CAM, "{}", curl_easy_strerror(error));
                curl_easy_cleanup(curl);
                continue;
            }

            std::string body;

            error = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
            if (error != CURLE_OK) {
                LOG_DEBUG(Service_CAM, "{}", curl_easy_strerror(error));
                curl_easy_cleanup(curl);
                continue;
            }

            error = curl_easy_setopt(
                curl, CURLOPT_WRITEFUNCTION,
                static_cast<curl_write_callback>(
                    [](char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
                        const std::size_t realsize = size * nmemb;
                        static_cast<std::string*>(userdata)->append(ptr, realsize);
                        return realsize;
                    }));
            if (error != CURLE_OK) {
                LOG_DEBUG(Service_CAM, "{}", curl_easy_strerror(error));
                curl_easy_cleanup(curl);
                continue;
            }

            error = curl_easy_setopt(
                curl, CURLOPT_SSL_CTX_FUNCTION,
                static_cast<CURLcode (*)(CURL * curl, void* ssl_ctx, void* userptr)>(
                    [](CURL* curl, void* ssl_ctx, void* userptr) {
                        void* chain = Common::CreateCertificateChainWithSystemCertificates();
                        if (chain != nullptr) {
                            mbedtls_ssl_conf_ca_chain(static_cast<mbedtls_ssl_config*>(ssl_ctx),
                                                      static_cast<mbedtls_x509_crt*>(chain), NULL);
                        } else {
                            return curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
                        }
                        return CURLE_OK;
                    }));
            if (error != CURLE_OK) {
                LOG_DEBUG(Service_CAM, "{}", curl_easy_strerror(error));
                curl_easy_cleanup(curl);
                continue;
            }

            error = curl_easy_perform(curl);
            if (error != CURLE_OK) {
                LOG_DEBUG(Service_CAM, "{}", curl_easy_strerror(error));
                curl_easy_cleanup(curl);
                continue;
            }

            long status_code;
            error = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
            if (error != CURLE_OK) {
                LOG_DEBUG(Service_CAM, "{}", curl_easy_strerror(error));
                curl_easy_cleanup(curl);
                continue;
            }

            curl_easy_cleanup(curl);

            if (status_code != 200) {
                continue;
            }

            unsigned char* uc =
                stbi_load_from_memory((stbi_uc const*)body.data(), static_cast<int>(body.size()),
                                      &width, &height, nullptr, 3);

            if (uc != nullptr) {
                unmodified_image.resize(width * height * 3);
                std::memcpy(unmodified_image.data(), uc, unmodified_image.size());
                resized_image = unmodified_image;
                std::free(uc);
            }
        } else {
            unsigned char* uc = stbi_load(file.c_str(), &width, &height, nullptr, 3);

            if (uc != nullptr) {
                unmodified_image.resize(width * height * 3);
                std::memcpy(unmodified_image.data(), uc, unmodified_image.size());
                resized_image = unmodified_image;
                std::free(uc);
            }
        }
    }
}

ImageCamera::~ImageCamera() = default;

void ImageCamera::SetResolution(const Service::CAM::Resolution& resolution) {
    requested_width = resolution.width;
    requested_height = resolution.height;

    resized_image.resize(requested_width * requested_height * 3);

    ASSERT(stbir_resize_uint8(unmodified_image.data(), width, height, 0, resized_image.data(),
                              requested_width, requested_height, 0, 3) == 1);
}

void ImageCamera::SetFormat(Service::CAM::OutputFormat format) {
    this->format = format;
}

std::vector<u16> ImageCamera::ReceiveFrame() {
    std::vector<unsigned char> image = resized_image;

    if (flip_horizontal) {
        image = FlipRgb24ImageHorizontally(requested_width, requested_height, image);
    }

    if (flip_vertical) {
        image = FlipRgb24ImageVertically(requested_width, requested_height, image);
    }

    if (format == Service::CAM::OutputFormat::RGB565) {
        std::vector<u16> frame(requested_width * requested_height, 0);
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
        return ConvertRgb24ToYuv422(image, requested_width, requested_height);
    }

    UNIMPLEMENTED();

    return {};
}

void ImageCamera::StartCapture() {}
void ImageCamera::StopCapture() {}

void ImageCamera::SetFlip(Service::CAM::Flip flip) {
    flip_horizontal = basic_flip_horizontal ^ (flip == Service::CAM::Flip::Horizontal ||
                                               flip == Service::CAM::Flip::Reverse);

    flip_vertical = basic_flip_vertical ^
                    (flip == Service::CAM::Flip::Vertical || flip == Service::CAM::Flip::Reverse);
}

void ImageCamera::SetEffect(Service::CAM::Effect effect) {
    UNIMPLEMENTED();
}

std::unique_ptr<CameraInterface> ImageCameraFactory::Create(const std::string& file,
                                                            const Service::CAM::Flip& flip) {
    return std::make_unique<ImageCamera>(file, flip);
}

} // namespace Camera
