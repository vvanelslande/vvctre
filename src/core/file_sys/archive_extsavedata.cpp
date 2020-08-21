// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <vector>
#include <fmt/format.h>
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/disk_archive.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/path_parser.h"
#include "core/file_sys/savedata_archive.h"
#include "core/hle/service/fs/archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/**
 * A modified version of DiskFile for fixed-size file used by ExtSaveData
 * The file size can't be changed by SetSize or Write.
 */
class FixSizeDiskFile : public DiskFile {
public:
    FixSizeDiskFile(FileUtil::IOFile&& file, const Mode& mode,
                    std::unique_ptr<DelayGenerator> delay_generator_)
        : DiskFile(std::move(file), mode, std::move(delay_generator_)) {
        size = GetSize();
    }

    bool SetSize(u64 size) const override {
        return false;
    }

    ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush,
                                 const u8* buffer) override {
        if (offset > size) {
            return FS_ERROR_WRITE_BEYOND_END;
        } else if (offset == size) {
            return MakeResult<std::size_t>(0);
        }

        if (offset + length > size) {
            length = size - offset;
        }

        return DiskFile::Write(offset, length, flush, buffer);
    }

private:
    u64 size{};
};

class ExtSaveDataDelayGenerator : public DelayGenerator {
public:
    u64 GetReadDelayNs(std::size_t length) override {
        // This is the delay measured for a savedata read,
        // not for extsaveData
        // For now we will take that
        static constexpr u64 slope(183);
        static constexpr u64 offset(524879);
        static constexpr u64 minimum(631826);
        u64 ipc_delay_nanoseconds =
            std::max<u64>(static_cast<u64>(length) * slope + offset, minimum);
        return ipc_delay_nanoseconds;
    }

    u64 GetOpenDelayNs() override {
        // This is the delay measured on N3DS with
        // https://gist.github.com/FearlessTobi/929b68489f4abb2c6cf81d56970a20b4
        // from the results the average of each length was taken.
        static constexpr u64 IPCDelayNanoseconds(3085068);
        return IPCDelayNanoseconds;
    }
};

/**
 * Archive backend for general extsave data archive type.
 * The behaviour of ExtSaveDataArchive is almost the same as SaveDataArchive, except for
 *  - file size can't be changed once created (thus creating zero-size file and openning with create
 *    flag are prohibited);
 *  - always open a file with read+write permission.
 */
class ExtSaveDataArchive : public SaveDataArchive {
public:
    explicit ExtSaveDataArchive(const std::string& mount_point,
                                std::unique_ptr<DelayGenerator> delay_generator_)
        : SaveDataArchive(mount_point) {
        delay_generator = std::move(delay_generator_);
    }

    std::string GetName() const override {
        return "ExtSaveDataArchive: " + mount_point;
    }

    ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path,
                                                     const Mode& mode) const override {
        LOG_DEBUG(Service_FS, "called path={} mode={:01X}", path.DebugStr(), mode.hex);

        const PathParser path_parser(path);

        if (!path_parser.IsValid()) {
            LOG_ERROR(Service_FS, "Invalid path {}", path.DebugStr());
            return FS_ERROR_INVALID_PATH;
        }

        if (mode.hex == 0) {
            LOG_ERROR(Service_FS, "Empty open mode");
            return FS_ERROR_UNSUPPORTED_OPEN_FLAGS;
        }

        if (mode.create_flag) {
            LOG_ERROR(Service_FS, "Create flag is not supported");
            return FS_ERROR_UNSUPPORTED_OPEN_FLAGS;
        }

        const auto full_path = path_parser.BuildHostPath(mount_point);

        switch (path_parser.GetHostStatus(mount_point)) {
        case PathParser::InvalidMountPoint:
            LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point {}", mount_point);
            return FS_ERROR_FILE_NOT_FOUND;
        case PathParser::PathNotFound:
            LOG_ERROR(Service_FS, "Path not found {}", full_path);
            return FS_ERROR_PATH_NOT_FOUND;
        case PathParser::FileInPath:
        case PathParser::DirectoryFound:
            LOG_ERROR(Service_FS, "Unexpected file or directory in {}", full_path);
            return FS_ERROR_UNEXPECTED_FILE_OR_DIRECTORY;
        case PathParser::NotFound:
            LOG_ERROR(Service_FS, "{} not found", full_path);
            return FS_ERROR_FILE_NOT_FOUND;
        case PathParser::FileFound:
            break; // Expected 'success' case
        }

        FileUtil::IOFile file(full_path, "r+b");
        if (!file.IsOpen()) {
            LOG_CRITICAL(Service_FS, "(unreachable) Unknown error opening {}", full_path);
            return FS_ERROR_FILE_NOT_FOUND;
        }

        Mode rwmode;
        rwmode.write_flag.Assign(1);
        rwmode.read_flag.Assign(1);
        std::unique_ptr<DelayGenerator> delay_generator =
            std::make_unique<ExtSaveDataDelayGenerator>();
        auto disk_file =
            std::make_unique<FixSizeDiskFile>(std::move(file), rwmode, std::move(delay_generator));
        return MakeResult<std::unique_ptr<FileBackend>>(std::move(disk_file));
    }

    ResultCode CreateFile(const Path& path, u64 size) const override {
        if (size == 0) {
            LOG_ERROR(Service_FS, "Zero-size file is not supported");
            return FS_ERROR_UNSUPPORTED_OPEN_FLAGS;
        }
        return SaveDataArchive::CreateFile(path, size);
    }
};

struct ExtSaveDataArchivePath {
    u32_le media_type;
    u32_le save_low;
    u32_le save_high;
};

static_assert(sizeof(ExtSaveDataArchivePath) == 12, "Incorrect path size");

std::string GetExtSaveDataPath(std::string_view mount_point, const Path& path) {
    std::vector<u8> vec_data = path.AsBinary();

    ExtSaveDataArchivePath path_data;
    std::memcpy(&path_data, vec_data.data(), sizeof(path_data));

    return fmt::format("{}{:08X}/{:08X}/", mount_point, path_data.save_high, path_data.save_low);
}

std::string GetExtDataContainerPath(std::string_view mount_point, bool shared) {
    if (shared) {
        return fmt::format("{}data/{}/extdata/", mount_point, SYSTEM_ID);
    }
    return fmt::format("{}Nintendo 3DS/{}/{}/extdata/", mount_point, SYSTEM_ID, SDCARD_ID);
}

std::string GetExtDataPathFromId(std::string_view mount_point, u64 extdata_id) {
    const u32 high = static_cast<u32>(extdata_id >> 32);
    const u32 low = static_cast<u32>(extdata_id & 0xFFFFFFFF);

    return fmt::format("{}{:08x}/{:08x}/", GetExtDataContainerPath(mount_point, false), high, low);
}

Path ConstructExtDataBinaryPath(u32 media_type, u32 high, u32 low) {
    ExtSaveDataArchivePath path;
    path.media_type = media_type;
    path.save_high = high;
    path.save_low = low;

    std::vector<u8> binary_path(sizeof(path));
    std::memcpy(binary_path.data(), &path, binary_path.size());

    return {binary_path};
}

ArchiveFactory_ExtSaveData::ArchiveFactory_ExtSaveData(const std::string& mount_location,
                                                       bool shared)
    : shared(shared), mount_point(GetExtDataContainerPath(mount_location, shared)) {
    LOG_DEBUG(Service_FS, "Directory {} set as base for ExtSaveData.", mount_point);
}

Path ArchiveFactory_ExtSaveData::GetCorrectedPath(const Path& path) {
    if (!shared)
        return path;

    static constexpr u32 SharedExtDataHigh = 0x48000;

    ExtSaveDataArchivePath new_path;
    std::memcpy(&new_path, path.AsBinary().data(), sizeof(new_path));

    // The FS module overwrites the high value of the saveid when dealing with the SharedExtSaveData
    // archive.
    new_path.save_high = SharedExtDataHigh;

    std::vector<u8> binary_data(sizeof(new_path));
    std::memcpy(binary_data.data(), &new_path, binary_data.size());

    return {binary_data};
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_ExtSaveData::Open(const Path& path,
                                                                            u64 program_id) {
    std::string fullpath = GetExtSaveDataPath(mount_point, GetCorrectedPath(path)) + "user/";
    if (!FileUtil::Exists(fullpath)) {
        // TODO(Subv): Verify the archive behavior of SharedExtSaveData compared to ExtSaveData.
        // ExtSaveData seems to return FS_NotFound (120) when the archive doesn't exist.
        if (!shared) {
            return FS_ERROR_NOT_FOUND_INVALID_STATE;
        } else {
            return FS_ERROR_NOT_FORMATTED;
        }
    }
    std::unique_ptr<DelayGenerator> delay_generator = std::make_unique<ExtSaveDataDelayGenerator>();
    auto archive = std::make_unique<ExtSaveDataArchive>(fullpath, std::move(delay_generator));
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_ExtSaveData::Format(const Path& path,
                                              const FileSys::ArchiveFormatInfo& format_info,
                                              u64 program_id) {
    auto corrected_path = GetCorrectedPath(path);

    // These folders are always created with the ExtSaveData
    std::string user_path = GetExtSaveDataPath(mount_point, corrected_path) + "user/";
    std::string boss_path = GetExtSaveDataPath(mount_point, corrected_path) + "boss/";
    FileUtil::CreateFullPath(user_path);
    FileUtil::CreateFullPath(boss_path);

    // Write the format metadata
    std::string metadata_path = GetExtSaveDataPath(mount_point, corrected_path) + "metadata";
    FileUtil::IOFile file(metadata_path, "wb");

    if (!file.IsOpen()) {
        // TODO(Subv): Find the correct error code
        return ResultCode(-1);
    }

    file.WriteBytes(&format_info, sizeof(format_info));
    return RESULT_SUCCESS;
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_ExtSaveData::GetFormatInfo(const Path& path,
                                                                       u64 program_id) const {
    std::string metadata_path = GetExtSaveDataPath(mount_point, path) + "metadata";
    FileUtil::IOFile file(metadata_path, "rb");

    if (!file.IsOpen()) {
        LOG_ERROR(Service_FS, "Could not open metadata information for archive");
        // TODO(Subv): Verify error code
        return FS_ERROR_NOT_FORMATTED;
    }

    ArchiveFormatInfo info = {};
    file.ReadBytes(&info, sizeof(info));
    return MakeResult<ArchiveFormatInfo>(info);
}

void ArchiveFactory_ExtSaveData::WriteIcon(const Path& path, const u8* icon_data,
                                           std::size_t icon_size) {
    std::string game_path = FileSys::GetExtSaveDataPath(GetMountPoint(), path);
    FileUtil::IOFile icon_file(game_path + "icon", "wb");
    icon_file.WriteBytes(icon_data, icon_size);
}

} // namespace FileSys
