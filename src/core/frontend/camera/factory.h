// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "core/frontend/camera/interface.h"

namespace Camera {

class CameraFactory {
public:
    virtual ~CameraFactory();

    /**
     * Creates a camera object.
     * @param parameter Parameter string to create the camera. The implementation can decide the
     *                  meaning of this string.
     * @param flip The image flip to apply
     * @returns a unique_ptr to the created camera object.
     */
    virtual std::unique_ptr<CameraInterface> Create(const std::string& parameter,
                                                    const Service::CAM::Flip& flip) = 0;
};

/**
 * Registers an external camera factory.
 * @param name Identifier of the camera factory.
 * @param factory Camera factory to register.
 */
void RegisterFactory(const std::string& name, std::unique_ptr<CameraFactory> factory);

/**
 * Creates a camera from the factory.
 * @param name Identifier of the camera factory.
 * @param parameter Parameter to create the camera.
 *                  The meaning of this string is defined by the factory.
 */
std::unique_ptr<CameraInterface> CreateCamera(const std::string& name, const std::string& parameter,
                                              const Service::CAM::Flip& flip);

} // namespace Camera
