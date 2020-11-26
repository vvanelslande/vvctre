// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "common/common_types.h"
#include "core/loader/loader.h"

namespace Loader {

class AppLoader_ELF final : public AppLoader {
public:
    AppLoader_ELF(FileUtil::IOFile&& file, std::string filename)
        : AppLoader(std::move(file)), filename(std::move(filename)) {}

    static FileType IdentifyType(FileUtil::IOFile& file);

    FileType GetFileType() override {
        return IdentifyType(file);
    }

    ResultStatus Load(std::shared_ptr<Kernel::Process>& process) override;

private:
    std::string filename;
};

} // namespace Loader
