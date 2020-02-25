// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <glad/glad.h>
#include "common/file_util.h"
#include "common/string_util.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/service/ptm/ptm.h"
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

    const std::string title = config.title.empty() ? "Mii Selector" : config.title;
    std::atomic<bool> done{false};
    std::size_t selected_mii = 0;
    u32 code = 1;

    ImGuiIO& io = ImGui::GetIO();

    EmuWindow_SDL2::WindowCallback cb;
    cb.function = [&] {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin(title.c_str(), nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::ListBoxHeader("")) {
                for (std::size_t index = 0; index < miis.size(); ++index) {
                    if (ImGui::Selectable(
                            Common::UTF16BufferToUTF8(miis[index].mii_name).c_str())) {
                        selected_mii = index;
                        code = 0;
                        done = true;
                    }
                }
                ImGui::ListBoxFooter();
            }
            if (ImGui::Button("Cancel")) {
                done = true;
            }
            ImGui::End();
        }
    };

    emu_window.windows.emplace("SDL2_MiiSelector", &cb);

    while (emu_window.IsOpen() && !done) {
        VideoCore::g_renderer->SwapBuffers();
    }

    Finalize(code, miis[selected_mii]);
}

} // namespace Frontend
