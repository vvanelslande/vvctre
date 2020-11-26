// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "common/common_types.h"
#include "core/file_sys/cia_container.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
} // namespace Core

namespace Service::FS {
enum class MediaType : u32;
} // namespace Service::FS

namespace Service::AM {

namespace ErrCodes {
enum {
    CIACurrentlyInstalling = 4,
    InvalidTID = 31,
    EmptyCIA = 32,
    TryingToUninstallSystemApp = 44,
    InvalidTIDInList = 60,
    InvalidCIAHeader = 104,
};
} // namespace ErrCodes

enum class CIAInstallState : u32 {
    InstallStarted,
    HeaderLoaded,
    CertLoaded,
    TicketLoaded,
    TMDLoaded,
    ContentWritten,
};

enum class InstallStatus : u32 {
    Success,
    ErrorFailedToOpenFile,
    ErrorFileNotFound,
    ErrorAborted,
    ErrorInvalid,
    ErrorEncrypted,
};

constexpr std::size_t TITLE_ID_VALID_LENGTH = 16;
constexpr u16 PLATFORM_CTR = 0x0004;
constexpr u16 CATEGORY_SYSTEM = 0x0010;
constexpr u16 CATEGORY_DLP = 0x0001;
constexpr u8 VARIATION_SYSTEM = 0x02;
constexpr u32 TID_HIGH_UPDATE = 0x0004000E;
constexpr u32 TID_HIGH_DLC = 0x0004008C;

// Progress callback for InstallCIA, receives bytes written and total bytes
using ProgressCallback = void(std::size_t, std::size_t);

// A file handled returned for CIAs to be written into and subsequently installed.
class CIAFile final : public FileSys::FileBackend {
public:
    explicit CIAFile(Service::FS::MediaType media_type);
    ~CIAFile();

    ResultVal<std::size_t> Read(u64 offset, std::size_t length, u8* buffer) const override;
    ResultCode WriteTicket();
    ResultCode WriteTitleMetadata();
    ResultVal<std::size_t> WriteContentData(u64 offset, std::size_t length, const u8* buffer);
    ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush,
                                 const u8* buffer) override;
    u64 GetSize() const override;
    bool SetSize(u64 size) const override;
    bool Close() const override;
    void Flush() const override;

private:
    // Whether it's installing an update, and what step of installation it is at
    bool is_update = false;
    CIAInstallState install_state = CIAInstallState::InstallStarted;

    // How much has been written total, CIAContainer for the installing CIA, buffer of all data
    // prior to content data, how much of each content index has been written, and where the CIA
    // is being installed to
    u64 written = 0;
    FileSys::CIAContainer container;
    std::vector<u8> data;
    std::vector<u64> content_written;
    Service::FS::MediaType media_type;

    class DecryptionState;
    std::unique_ptr<DecryptionState> decryption_state;
};

/**
 * Installs a CIA file from a specified file path.
 * @param path file path of the CIA file to install
 * @param update_callback callback function called during filesystem write
 * @returns bool whether the install was successful
 */
InstallStatus InstallCIA(const std::string& path,
                         std::function<ProgressCallback>&& update_callback = nullptr);

/**
 * Get the mediatype for an installed title
 * @param title_id the installed title ID
 * @returns MediaType which the installed title will reside on
 */
Service::FS::MediaType GetTitleMediaType(u64 title_id);

/**
 * Get the .tmd path for a title
 * @param media_type the media the title exists on
 * @param program_id the title ID to get
 * @param update set true if the incoming TMD should be used instead of the current TMD
 * @returns string path to the .tmd file if it exists, otherwise a path to create one is given.
 */
std::string GetTitleMetadataPath(Service::FS::MediaType media_type, u64 program_id,
                                 bool update = false);

/**
 * Get the .app path for a title's installed content index.
 * @param media_type the media the title exists on
 * @param program_id the title ID to get
 * @param index the content index to get
 * @param update set true if the incoming TMD should be used instead of the current TMD
 * @returns string path to the .app file
 */
std::string GetTitleContentPath(FS::MediaType media_type, u64 program_id, std::size_t index = 0,
                                bool update = false);

/**
 * Get the folder for a title's installed content.
 * @param media_type the media the title exists on
 * @param program_id the title ID to get
 * @returns string path to the title folder
 */
std::string GetTitlePath(Service::FS::MediaType media_type, u64 program_id);

/**
 * Get the title/ folder for a storage medium.
 * @param media_type the storage medium to get the path for
 * @returns string path to the folder
 */
std::string GetMediaTitlePath(Service::FS::MediaType media_type);

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module();

    /// Scans all storage mediums for titles for listing.
    void ScanForAllTitles();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> am, const char* name, u32 max_session);
        ~Interface();

        std::shared_ptr<Module> GetModule();

    protected:
        void GetNumPrograms(Kernel::HLERequestContext& ctx);
        void FindDLCContentInfos(Kernel::HLERequestContext& ctx);
        void ListDLCContentInfos(Kernel::HLERequestContext& ctx);
        void DeleteContents(Kernel::HLERequestContext& ctx);
        void GetProgramList(Kernel::HLERequestContext& ctx);
        void GetProgramInfos(Kernel::HLERequestContext& ctx);
        void DeleteUserProgram(Kernel::HLERequestContext& ctx);
        void GetProductCode(Kernel::HLERequestContext& ctx);
        void GetDLCTitleInfos(Kernel::HLERequestContext& ctx);
        void GetPatchTitleInfos(Kernel::HLERequestContext& ctx);
        void ListDataTitleTicketInfos(Kernel::HLERequestContext& ctx);
        void GetDLCContentInfoCount(Kernel::HLERequestContext& ctx);
        void DeleteTicket(Kernel::HLERequestContext& ctx);
        void GetNumTickets(Kernel::HLERequestContext& ctx);
        void GetTicketList(Kernel::HLERequestContext& ctx);
        void QueryAvailableTitleDatabase(Kernel::HLERequestContext& ctx);
        void CheckContentRights(Kernel::HLERequestContext& ctx);
        void CheckContentRightsIgnorePlatform(Kernel::HLERequestContext& ctx);
        void BeginImportProgram(Kernel::HLERequestContext& ctx);
        void BeginImportProgramTemporarily(Kernel::HLERequestContext& ctx);
        void EndImportProgram(Kernel::HLERequestContext& ctx);
        void EndImportProgramWithoutCommit(Kernel::HLERequestContext& ctx);
        void CommitImportPrograms(Kernel::HLERequestContext& ctx);
        void GetProgramInfoFromCia(Kernel::HLERequestContext& ctx);
        void GetSystemMenuDataFromCia(Kernel::HLERequestContext& ctx);
        void GetDependencyListFromCia(Kernel::HLERequestContext& ctx);
        void GetTransferSizeFromCia(Kernel::HLERequestContext& ctx);
        void GetCoreVersionFromCia(Kernel::HLERequestContext& ctx);
        void GetRequiredSizeFromCia(Kernel::HLERequestContext& ctx);
        void DeleteProgram(Kernel::HLERequestContext& ctx);
        void GetSystemUpdaterMutex(Kernel::HLERequestContext& ctx);
        void GetMetaSizeFromCia(Kernel::HLERequestContext& ctx);
        void GetMetaDataFromCia(Kernel::HLERequestContext& ctx);

    private:
        std::shared_ptr<Module> am;
    };

private:
    /**
     * Scans the for titles in a storage medium for listing.
     * @param media_type the storage medium to scan
     */
    void ScanForTitles(Service::FS::MediaType media_type);

    Core::System& system;
    bool cia_installing = false;
    std::array<std::vector<u64_le>, 3> am_title_list;
    std::shared_ptr<Kernel::Mutex> system_updater_mutex;
};

std::shared_ptr<Module> GetModule(Core::System& system);
void InstallInterfaces(Core::System& system);

} // namespace Service::AM
