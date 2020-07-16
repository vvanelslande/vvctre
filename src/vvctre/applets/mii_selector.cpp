// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <SDL.h>
#include "common/file_util.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/settings.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "vvctre/applets/mii_selector.h"

namespace Frontend {

SDL2_MiiSelector::SDL2_MiiSelector(EmuWindow_SDL2& emu_window) : emu_window(emu_window) {}

void SDL2_MiiSelector::Setup(const MiiSelectorConfig& config) {
    MiiSelector::Setup(config);

    const std::string nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory(nand_directory, true);

    std::vector<HLE::Applets::MiiData> miis{
        HLE::Applets::MiiSelector::GetStandardMiiResult().selected_mii_data};

    auto archive_result = extdata_archive_factory.Open(Service::PTM::ptm_shared_extdata_id, 0);
    if (archive_result.Succeeded()) {
        auto archive = std::move(archive_result).Unwrap();

        FileSys::Path file_path = "/CFL_DB.dat";
        FileSys::Mode mode{};
        mode.read_flag.Assign(1);

        auto file_result = archive->OpenFile(file_path, mode);
        if (file_result.Succeeded()) {
            auto file = std::move(file_result).Unwrap();
            u32 saved_miis_offset = 0x8;

            // The Mii Maker has a 100 Mii limit on the 3DS
            for (int i = 0; i < 100; ++i) {
                HLE::Applets::MiiData mii;
                file->Read(saved_miis_offset, sizeof(mii), reinterpret_cast<u8*>(&mii));
                if (mii.mii_id != 0) {
                    miis.push_back(mii);
                }
                saved_miis_offset += sizeof(mii);
            }
        }
    }

    EmuWindow_SDL2::MiiSelectorData data{config, miis, 1, {}};
    emu_window.mii_selector_data = &data;

    SDL_GL_SetSwapInterval(1);

    while (emu_window.IsOpen() && emu_window.mii_selector_data != nullptr) {
        VideoCore::g_renderer->SwapBuffers();
    }

    SDL_GL_SetSwapInterval(Settings::values.enable_vsync ? 1 : 0);
    Finalize(data.code, data.selected_mii);
}

} // namespace Frontend
