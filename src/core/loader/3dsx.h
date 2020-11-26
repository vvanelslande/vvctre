// Copyright 2014 Dolphin Emulator Project / Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "common/common_types.h"
#include "core/loader/loader.h"

namespace Loader {

class AppLoader_THREEDSX final : public AppLoader {
public:
    AppLoader_THREEDSX(FileUtil::IOFile&& file, const std::string& filename,
                       const std::string& filepath)
        : AppLoader(std::move(file)), filename(std::move(filename)), filepath(filepath) {}

    static FileType IdentifyType(FileUtil::IOFile& file);

    FileType GetFileType() override {
        return IdentifyType(file);
    }

    ResultStatus Load(std::shared_ptr<Kernel::Process>& process) override;

    ResultStatus ReadIcon(std::vector<u8>& buffer) override;

    ResultStatus ReadRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) override;
    ResultStatus DumpRomFS(const std::string& target_path) override;

private:
    std::string filename;
    std::string filepath;
};

} // namespace Loader
