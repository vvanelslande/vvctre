// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "common/file_util.h"
#include "core/file_sys/romfs_reader.h"
#include "core/hle/kernel/object.h"

namespace Kernel {
struct AddressMapping;
class Process;
} // namespace Kernel

namespace Loader {

enum class FileType {
    Unknown,
    CCI,
    CXI,
    CIA,
    ELF,
    THREEDSX,
};

enum class ResultStatus {
    Success,
    Error,
    ErrorInvalidFormat,
    ErrorNotImplemented,
    ErrorNotLoaded,
    ErrorNotUsed,
    ErrorAlreadyLoaded,
    ErrorMemoryAllocationFailed,
    ErrorEncrypted,
};

constexpr u32 MakeMagic(char a, char b, char c, char d) {
    return a | b << 8 | c << 16 | d << 24;
}

class AppLoader : NonCopyable {
public:
    explicit AppLoader(FileUtil::IOFile&& file) : file(std::move(file)) {}
    virtual ~AppLoader() {}

    virtual FileType GetFileType() = 0;
    virtual ResultStatus Load(std::shared_ptr<Kernel::Process>& process) = 0;

    /**
     * Loads the system mode that this application needs.
     * This function defaults to 2 (96MB allocated to the application) if it can't read the
     * information.
     * @returns A pair with the optional system mode, and the status.
     */
    virtual std::pair<std::optional<u32>, ResultStatus> LoadKernelSystemMode() {
        // 96MB allocated to the application.
        return std::make_pair(2, ResultStatus::Success);
    }

    virtual ResultStatus IsExecutable(bool& out_executable) {
        out_executable = true;
        return ResultStatus::Success;
    }

    virtual ResultStatus ReadCode(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    virtual ResultStatus ReadIcon(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    virtual ResultStatus ReadBanner(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    virtual ResultStatus ReadLogo(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    virtual ResultStatus ReadProgramId(u64& out_program_id) {
        return ResultStatus::ErrorNotImplemented;
    }

    virtual ResultStatus ReadExtdataId(u64& out_extdata_id) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the RomFS of the application
     * Since the RomFS can be huge, we return a file reference instead of copying to a buffer
     * @param romfs_file The file containing the RomFS
     * @returns ResultStatus result of function
     */
    virtual ResultStatus ReadRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
        return ResultStatus::ErrorNotImplemented;
    }

    virtual ResultStatus DumpRomFS(const std::string& target_path) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the update RomFS of the application
     * Since the RomFS can be huge, we return a file reference instead of copying to a buffer
     * @param romfs_file The file containing the RomFS
     * @returns ResultStatus result of function
     */
    virtual ResultStatus ReadUpdateRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
        return ResultStatus::ErrorNotImplemented;
    }

    virtual ResultStatus DumpUpdateRomFS(const std::string& target_path) {
        return ResultStatus::ErrorNotImplemented;
    }

    virtual ResultStatus ReadTitle(std::string& title) {
        return ResultStatus::ErrorNotImplemented;
    }

protected:
    FileUtil::IOFile file;
    bool is_loaded = false;
};

/**
 * Identifies a bootable file and return a suitable loader
 * @param filename String filename of bootable file
 * @returns best loader for this file
 */
std::unique_ptr<AppLoader> GetLoader(const std::string& filename);

} // namespace Loader
