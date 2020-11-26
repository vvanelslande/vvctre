// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include "common/common_types.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
} // namespace Core

namespace Service::FS {

class ArchiveManager;

struct ClientSlot : public Kernel::SessionRequestHandler::SessionDataBase {
    // We retrieves program ID for client process on FS::Initialize(WithSDKVersion)
    // Real 3DS matches program ID and process ID based on data registered by loader via fs:REG, so
    // theoretically the program ID for FS client and for process codeset can mismatch if the loader
    // behaviour is modified. Since we don't emulate fs:REG mechanism, we assume the program ID is
    // the same as codeset ID and fetch from there directly.
    u64 program_id = 0;
};

struct ProgramInfo {
    u64 program_id;
    MediaType media_type;
};

class FS_USER final : public ServiceFramework<FS_USER, ClientSlot> {
public:
    explicit FS_USER(Core::System& system);

    // On real HW this is part of fs:REG. But since that module is only used by loader and pm, which
    // we HLEed, we can just directly use it here
    void Register(u32 process_id, u64 program_id, const std::string& filepath);

    void SetCurrentGamecardPath(std::string value);
    const std::string& GetCurrentGamecardPath() const;

    void SetProgramInfoMap(std::unordered_map<u32, ProgramInfo> value);
    const std::unordered_map<u32, ProgramInfo>& GetProgramInfoMap() const;

private:
    void Initialize(Kernel::HLERequestContext& ctx);
    void OpenFile(Kernel::HLERequestContext& ctx);
    void OpenFileDirectly(Kernel::HLERequestContext& ctx);
    void DeleteFile(Kernel::HLERequestContext& ctx);
    void RenameFile(Kernel::HLERequestContext& ctx);
    void DeleteDirectory(Kernel::HLERequestContext& ctx);
    void DeleteDirectoryRecursively(Kernel::HLERequestContext& ctx);
    void CreateFile(Kernel::HLERequestContext& ctx);
    void CreateDirectory(Kernel::HLERequestContext& ctx);
    void RenameDirectory(Kernel::HLERequestContext& ctx);
    void OpenDirectory(Kernel::HLERequestContext& ctx);
    void OpenArchive(Kernel::HLERequestContext& ctx);
    void CloseArchive(Kernel::HLERequestContext& ctx);
    void IsSdmcDetected(Kernel::HLERequestContext& ctx);
    void IsSdmcWriteable(Kernel::HLERequestContext& ctx);
    void FormatSaveData(Kernel::HLERequestContext& ctx);
    void FormatThisUserSaveData(Kernel::HLERequestContext& ctx);
    void GetFreeBytes(Kernel::HLERequestContext& ctx);
    void GetSdmcArchiveResource(Kernel::HLERequestContext& ctx);
    void GetNandArchiveResource(Kernel::HLERequestContext& ctx);
    void CreateExtSaveData(Kernel::HLERequestContext& ctx);
    void DeleteExtSaveData(Kernel::HLERequestContext& ctx);
    void CardSlotIsInserted(Kernel::HLERequestContext& ctx);
    void DeleteSystemSaveData(Kernel::HLERequestContext& ctx);
    void CreateSystemSaveData(Kernel::HLERequestContext& ctx);
    void CreateLegacySystemSaveData(Kernel::HLERequestContext& ctx);
    void InitializeWithSdkVersion(Kernel::HLERequestContext& ctx);
    void SetPriority(Kernel::HLERequestContext& ctx);
    void GetPriority(Kernel::HLERequestContext& ctx);
    void GetArchiveResource(Kernel::HLERequestContext& ctx);
    void GetFormatInfo(Kernel::HLERequestContext& ctx);
    void GetProgramLaunchInfo(Kernel::HLERequestContext& ctx);
    void ObsoletedCreateExtSaveData(Kernel::HLERequestContext& ctx);
    void ObsoletedDeleteExtSaveData(Kernel::HLERequestContext& ctx);
    void GetSpecialContentIndex(Kernel::HLERequestContext& ctx);
    void GetNumSeeds(Kernel::HLERequestContext& ctx);
    void AddSeed(Kernel::HLERequestContext& ctx);
    void SetSaveDataSecureValue(Kernel::HLERequestContext& ctx);
    void GetSaveDataSecureValue(Kernel::HLERequestContext& ctx);
    void GetThisSaveDataSecureValue(Kernel::HLERequestContext& ctx);

    static ResultVal<u16> GetSpecialContentIndexFromGameCard(u64 title_id, SpecialContentType type);
    static ResultVal<u16> GetSpecialContentIndexFromTMD(MediaType media_type, u64 title_id,
                                                        SpecialContentType type);

    u32 priority = -1;

    std::unordered_map<u32, ProgramInfo> program_info_map;
    std::string current_gamecard_path;

    Core::System& system;
    ArchiveManager& archives;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::FS
