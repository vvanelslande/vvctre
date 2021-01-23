// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include "common/common_types.h"
#include "core/hle/service/service.h"

namespace FileSys {
class ArchiveBackend;
} // namespace FileSys

namespace Core {
class System;
} // namespace Core

namespace Service::CFG {

enum SystemModel {
    NINTENDO_3DS = 0,
    NINTENDO_3DS_XL = 1,
    NEW_NINTENDO_3DS = 2,
    NINTENDO_2DS = 3,
    NEW_NINTENDO_3DS_XL = 4,
    NEW_NINTENDO_2DS_XL = 5
};

enum SystemLanguage {
    LANGUAGE_JP = 0,
    LANGUAGE_EN = 1,
    LANGUAGE_FR = 2,
    LANGUAGE_DE = 3,
    LANGUAGE_IT = 4,
    LANGUAGE_ES = 5,
    LANGUAGE_ZH = 6,
    LANGUAGE_KO = 7,
    LANGUAGE_NL = 8,
    LANGUAGE_PT = 9,
    LANGUAGE_RU = 10,
    LANGUAGE_TW = 11
};

enum SoundOutputMode { SOUND_MONO = 0, SOUND_STEREO = 1, SOUND_SURROUND = 2 };

struct EULAVersion {
    u8 minor;
    u8 major;
};

/// Block header in the config savedata file
struct SaveConfigBlockEntry {
    u32 block_id;       ///< The id of the current block
    u32 offset_or_data; ///< This is the absolute offset to the block data if the size is greater
                        /// than 4 bytes, otherwise it contains the data itself
    u16 size;           ///< The size of the block
    u16 flags;          ///< The flags of the block, possibly used for access control
};

static constexpr u16 C(const char code[2]) {
    return code[0] | (code[1] << 8);
}

static const std::array<u16, 187> country_codes = {{
    0,       C("JP"), 0,       0,       0,       0,       0,       0,       // 0-7
    C("AI"), C("AG"), C("AR"), C("AW"), C("BS"), C("BB"), C("BZ"), C("BO"), // 8-15
    C("BR"), C("VG"), C("CA"), C("KY"), C("CL"), C("CO"), C("CR"), C("DM"), // 16-23
    C("DO"), C("EC"), C("SV"), C("GF"), C("GD"), C("GP"), C("GT"), C("GY"), // 24-31
    C("HT"), C("HN"), C("JM"), C("MQ"), C("MX"), C("MS"), C("AN"), C("NI"), // 32-39
    C("PA"), C("PY"), C("PE"), C("KN"), C("LC"), C("VC"), C("SR"), C("TT"), // 40-47
    C("TC"), C("US"), C("UY"), C("VI"), C("VE"), 0,       0,       0,       // 48-55
    0,       0,       0,       0,       0,       0,       0,       0,       // 56-63
    C("AL"), C("AU"), C("AT"), C("BE"), C("BA"), C("BW"), C("BG"), C("HR"), // 64-71
    C("CY"), C("CZ"), C("DK"), C("EE"), C("FI"), C("FR"), C("DE"), C("GR"), // 72-79
    C("HU"), C("IS"), C("IE"), C("IT"), C("LV"), C("LS"), C("LI"), C("LT"), // 80-87
    C("LU"), C("MK"), C("MT"), C("ME"), C("MZ"), C("NA"), C("NL"), C("NZ"), // 88-95
    C("NO"), C("PL"), C("PT"), C("RO"), C("RU"), C("RS"), C("SK"), C("SI"), // 96-103
    C("ZA"), C("ES"), C("SZ"), C("SE"), C("CH"), C("TR"), C("GB"), C("ZM"), // 104-111
    C("ZW"), C("AZ"), C("MR"), C("ML"), C("NE"), C("TD"), C("SD"), C("ER"), // 112-119
    C("DJ"), C("SO"), C("AD"), C("GI"), C("GG"), C("IM"), C("JE"), C("MC"), // 120-127
    C("TW"), 0,       0,       0,       0,       0,       0,       0,       // 128-135
    C("KR"), 0,       0,       0,       0,       0,       0,       0,       // 136-143
    C("HK"), C("MO"), 0,       0,       0,       0,       0,       0,       // 144-151
    C("ID"), C("SG"), C("TH"), C("PH"), C("MY"), 0,       0,       0,       // 152-159
    C("CN"), 0,       0,       0,       0,       0,       0,       0,       // 160-167
    C("AE"), C("IN"), C("EG"), C("OM"), C("QA"), C("KW"), C("SA"), C("SY"), // 168-175
    C("BH"), C("JO"), 0,       0,       0,       0,       0,       0,       // 176-183
    C("SM"), C("VA"), C("BM"),                                              // 184-186
}};

class Module final {
public:
    Module();
    ~Module();

    void SetSystemModel(SystemModel model);
    SystemModel GetSystemModel();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> cfg, const char* name, u32 max_session);
        ~Interface();

        std::shared_ptr<Module> GetModule() const;

        void GetCountryCodeString(Kernel::HLERequestContext& ctx);
        void GetCountryCodeID(Kernel::HLERequestContext& ctx);
        void SecureInfoGetRegion(Kernel::HLERequestContext& ctx, u16 id);
        void GenHashConsoleUnique(Kernel::HLERequestContext& ctx);
        void GetRegionCanadaUSA(Kernel::HLERequestContext& ctx);
        void GetSystemModel(Kernel::HLERequestContext& ctx);
        void GetModelNintendo2DS(Kernel::HLERequestContext& ctx);
        void GetConfigInfoBlk2(Kernel::HLERequestContext& ctx);
        void GetConfigInfoBlk8(Kernel::HLERequestContext& ctx, u16 id);
        void SetConfigInfoBlk4(Kernel::HLERequestContext& ctx, u16 id);
        void UpdateConfigNANDSavegame(Kernel::HLERequestContext& ctx, u16 id);
        void FormatConfig(Kernel::HLERequestContext& ctx);
        void IsFangateSupported(Kernel::HLERequestContext& ctx);

        /// A helper function for dispatching service functions that have multiple IDs
        template <void (Interface::*function)(Kernel::HLERequestContext& ctx, u16 id), u16 id>
        void D(Kernel::HLERequestContext& ctx) {
            (this->*function)(ctx, id);
        }

    private:
        std::shared_ptr<Module> cfg;
    };

private:
    ResultVal<void*> GetConfigInfoBlockPointer(u32 block_id, u32 size, u32 flag);
    ResultCode GetConfigInfoBlock(u32 block_id, u32 size, u32 flag, void* output);
    ResultCode SetConfigInfoBlock(u32 block_id, u32 size, u32 flag, const void* input);
    ResultCode CreateConfigInfoBlk(u32 block_id, u16 size, u16 flags, const void* data);
    ResultCode DeleteConfigNANDSaveFile();
    ResultCode FormatConfig();
    ResultCode LoadConfigNANDSaveFile();

public:
    u32 GetRegionValue();
    void SetPreferredRegionCodes(const std::vector<u32>& region_codes);
    void SetUsername(const std::u16string& name);
    std::u16string GetUsername();
    void SetBirthday(u8 month, u8 day);
    std::tuple<u8, u8> GetBirthday();
    void SetSystemLanguage(SystemLanguage language);
    SystemLanguage GetSystemLanguage();
    void SetSoundOutputMode(SoundOutputMode mode);
    SoundOutputMode GetSoundOutputMode();
    void SetCountry(u8 country_code);
    u8 GetCountryCode();
    void GenerateConsoleUniqueId(u32& random_number, u64& console_id);
    ResultCode SetConsoleUniqueId(u32 random_number, u64 console_id);
    u64 GetConsoleUniqueId();
    void SetEULAVersion(const EULAVersion& version);
    EULAVersion GetEULAVersion();
    ResultCode UpdateConfigNANDSavegame();

private:
    static constexpr u32 CONFIG_SAVEFILE_SIZE = 0x8000;
    std::array<u8, CONFIG_SAVEFILE_SIZE> cfg_config_file_buffer;
    std::unique_ptr<FileSys::ArchiveBackend> cfg_system_save_data_archive;
    u32 preferred_region_code = 0;
};

std::shared_ptr<Module> GetModule(Core::System& system);

void InstallInterfaces(Core::System& system);

std::string GetConsoleIdHash(Core::System& system);

} // namespace Service::CFG
