// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstdlib>
#include <utility>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <asl/JSON.h>
#include <fmt/format.h>
#include <imgui.h>
#include <whereami.h>
#include "common/common_funcs.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "common/texture.h"
#include "core/3ds.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/cheats/gateway_cheat.h"
#include "core/core.h"
#include "core/hle/kernel/ipc_debugger/recorder.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/err_f.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nwm/nwm_uds.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/sm/sm.h"
#include "core/memory.h"
#include "core/movie.h"
#include "core/settings.h"
#include "network/room.h"
#include "network/room_member.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
#include "vvctre/common.h"
#include "vvctre/function_logger.h"
#include "vvctre/plugins.h"

#ifdef _WIN32
#define VVCTRE_STRDUP _strdup
#else
#define VVCTRE_STRDUP strdup
#endif

PluginManager::PluginManager(Core::System& core, SDL_Window* window) : window(window) {
    int length = wai_getExecutablePath(nullptr, 0, nullptr);
    std::string vvctre_folder(length, '\0');
    wai_getExecutablePath(&vvctre_folder[0], length, nullptr);

    FileUtil::FSTEntry entries;
    FileUtil::ScanDirectoryTree(vvctre_folder, entries);
    for (const FileUtil::FSTEntry& entry : entries.children) {
        if (entry.isDirectory) {
            continue;
        }

        const std::string full_path =
            FileUtil::SanitizePath(fmt::format("{}/{}", entry.physicalName, entry.virtualName),
                                   FileUtil::DirectorySeparator::PlatformDefault);

#ifdef _WIN32
        if (entry.virtualName == "SDL2.dll") {
            continue;
        }

        if (FileUtil::GetExtensionFromFilename(full_path) != "dll") {
            continue;
        }
#else
        if (FileUtil::GetExtensionFromFilename(full_path) != "so") {
            continue;
        }
#endif

        void* handle = SDL_LoadObject(full_path.c_str());
        if (handle == NULL) {
            fmt::print("Plugin {} failed to load: {}\n", entry.virtualName, SDL_GetError());
        } else {
            void* get_required_function_count =
                SDL_LoadFunction(handle, "GetRequiredFunctionCount");
            void* get_required_function_names =
                SDL_LoadFunction(handle, "GetRequiredFunctionNames");
            void* plugin_loaded = SDL_LoadFunction(handle, "PluginLoaded");
            if (get_required_function_count == nullptr || get_required_function_names == nullptr ||
                plugin_loaded == nullptr) {
                fmt::print("Plugin {} failed to load: {}\n", entry.virtualName, SDL_GetError());
                SDL_UnloadObject(handle);
            } else {
                Plugin plugin;
                plugin.handle = handle;
                plugin.before_drawing_fps = (decltype(Plugin::before_drawing_fps))SDL_LoadFunction(
                    handle, "BeforeDrawingFPS");
                plugin.add_menu = (decltype(Plugin::add_menu))SDL_LoadFunction(handle, "AddMenu");
                plugin.add_tab = (decltype(Plugin::add_tab))SDL_LoadFunction(handle, "AddTab");
                plugin.after_swap_window = (decltype(Plugin::after_swap_window))SDL_LoadFunction(
                    handle, "AfterSwapWindow");
                plugin.screenshot_callback =
                    (decltype(Plugin::screenshot_callback))SDL_LoadFunction(handle,
                                                                            "ScreenshotCallback");
                void* log = SDL_LoadFunction(handle, "Log");
                if (log != nullptr) {
                    Log::AddBackend(std::make_unique<Log::FunctionLogger>(
                        (decltype(Log::FunctionLogger::function))log,
                        fmt::format("Plugin {}", entry.virtualName)));
                }
                void* override_wlan_comm_id_check =
                    SDL_LoadFunction(handle, "OverrideWlanCommIdCheck");
                if (override_wlan_comm_id_check != nullptr) {
                    Service::NWM::OverrideWlanCommIdCheck =
                        [f = (bool (*)(u32, u32))override_wlan_comm_id_check](
                            u32 in_beacon, u32 requested) { return f(in_beacon, requested); };
                }
                void* override_on_load_failed_function =
                    SDL_LoadFunction(handle, "OverrideOnLoadFailed");
                if (override_on_load_failed_function != nullptr) {
                    core.SetOnLoadFailed([f = (void (*)(Core::System::ResultStatus))
                                              override_on_load_failed_function](
                                             Core::System::ResultStatus result) { f(result); });
                }

                int count = ((int (*)())get_required_function_count)();
                const char** required_function_names =
                    ((const char** (*)())get_required_function_names)();
                std::vector<void*> required_functions(count);
                for (int i = 0; i < count; ++i) {
                    required_functions[i] = function_map[std::string(required_function_names[i])];
                }
                ((void (*)(void* core, void* plugin_manager, void* functions[]))plugin_loaded)(
                    static_cast<void*>(&core), static_cast<void*>(this), required_functions.data());

                plugins.push_back(std::move(plugin));
                fmt::print("Plugin {} loaded\n", entry.virtualName);
            }
        }
    }
}

PluginManager::~PluginManager() {
    for (const auto& plugin : plugins) {
        SDL_UnloadObject(plugin.handle);
    }
}

void PluginManager::InitialSettingsOpening() {
    for (const auto& plugin : plugins) {
        void* initial_settings_opening = SDL_LoadFunction(plugin.handle, "InitialSettingsOpening");
        if (initial_settings_opening != nullptr) {
            ((void (*)())initial_settings_opening)();
        }
    }
}

void PluginManager::InitialSettingsOkPressed() {
    for (const auto& plugin : plugins) {
        void* initial_settings_ok_pressed =
            SDL_LoadFunction(plugin.handle, "InitialSettingsOkPressed");
        if (initial_settings_ok_pressed != nullptr) {
            ((void (*)())initial_settings_ok_pressed)();
        }
    }
}

void PluginManager::BeforeLoading() {
    for (const auto& plugin : plugins) {
        void* before_loading = SDL_LoadFunction(plugin.handle, "BeforeLoading");
        if (before_loading != nullptr) {
            ((void (*)())before_loading)();
        }
    }
}

void PluginManager::BeforeLoadingAfterFirstTime() {
    for (const auto& plugin : plugins) {
        void* before_loading_after_first_time =
            SDL_LoadFunction(plugin.handle, "BeforeLoadingAfterFirstTime");
        if (before_loading_after_first_time != nullptr) {
            ((void (*)())before_loading_after_first_time)();
        }
    }
}

void PluginManager::EmulationStarting() {
    for (const auto& plugin : plugins) {
        void* emulation_starting = SDL_LoadFunction(plugin.handle, "EmulationStarting");
        if (emulation_starting != nullptr) {
            ((void (*)())emulation_starting)();
        }
    }
}

void PluginManager::EmulationStartingAfterFirstTime() {
    for (const auto& plugin : plugins) {
        void* emulation_starting_after_first_time =
            SDL_LoadFunction(plugin.handle, "EmulationStartingAfterFirstTime");
        if (emulation_starting_after_first_time != nullptr) {
            ((void (*)())emulation_starting_after_first_time)();
        }
    }
}

void PluginManager::EmulatorClosing() {
    for (const auto& plugin : plugins) {
        void* emulator_closing = SDL_LoadFunction(plugin.handle, "EmulatorClosing");
        if (emulator_closing != nullptr) {
            ((void (*)())emulator_closing)();
        }
    }
}

void PluginManager::FatalError() {
    for (const auto& plugin : plugins) {
        void* fatal_error = SDL_LoadFunction(plugin.handle, "FatalError");
        if (fatal_error != nullptr) {
            ((void (*)())fatal_error)();
        }
    }
}

void PluginManager::BeforeDrawingFPS() {
    for (const auto& plugin : plugins) {
        if (plugin.before_drawing_fps != nullptr) {
            plugin.before_drawing_fps();
        }
    }
}

void PluginManager::AddMenus() {
    for (const auto& plugin : plugins) {
        if (plugin.add_menu != nullptr) {
            plugin.add_menu();
        }
    }
}

void PluginManager::AddTabs() {
    for (const auto& plugin : plugins) {
        if (plugin.add_tab != nullptr) {
            plugin.add_tab();
        }
    }
}

void PluginManager::AfterSwapWindow() {
    for (const auto& plugin : plugins) {
        if (plugin.after_swap_window != nullptr) {
            plugin.after_swap_window();
        }
    }
}

void* PluginManager::NewButtonDevice(const char* params) {
    auto& b = buttons.emplace_back(Input::CreateDevice<Input::ButtonDevice>(std::string(params)));
    return static_cast<void*>(b.get());
}

void PluginManager::DeleteButtonDevice(void* device) {
    auto itr = std::find_if(std::begin(buttons), std::end(buttons),
                            [device](auto& b) { return b.get() == device; });
    if (itr != buttons.end()) {
        buttons.erase(itr);
    }
}

void PluginManager::CallScreenshotCallbacks(void* data) {
    for (const auto& plugin : plugins) {
        if (plugin.screenshot_callback != nullptr) {
            plugin.screenshot_callback(data);
        }
    }
}

// Functions plugins can use

// File
void vvctre_load_file(void* core, const char* path) {
    static_cast<Core::System*>(core)->SetResetFilePath(std::string(path));
    static_cast<Core::System*>(core)->RequestReset();
}

bool vvctre_install_cia(const char* path) {
    return Service::AM::InstallCIA(std::string(path)) == Service::AM::InstallStatus::Success;
}

bool vvctre_load_amiibo(void* core, const char* path) {
    FileUtil::IOFile file(std::string(path), "rb");
    Service::NFC::AmiiboData data;

    if (file.ReadArray(&data, 1) == 1) {
        std::shared_ptr<Service::NFC::Module::Interface> nfc =
            static_cast<Core::System*>(core)
                ->ServiceManager()
                .GetService<Service::NFC::Module::Interface>("nfc:u");
        if (nfc != nullptr) {
            nfc->LoadAmiibo(data);
            return true;
        }
    }

    return false;
}

void vvctre_load_amiibo_from_memory(void* core, const u8 data[540]) {
    Service::NFC::AmiiboData data_{};
    std::memcpy(&data_, data, sizeof(data_));

    std::shared_ptr<Service::NFC::Module::Interface> nfc =
        static_cast<Core::System*>(core)
            ->ServiceManager()
            .GetService<Service::NFC::Module::Interface>("nfc:u");
    if (nfc != nullptr) {
        nfc->LoadAmiibo(data_);
    }
}

bool vvctre_load_amiibo_decrypted(void* core, const char* path) {
    FileUtil::IOFile file(std::string(path), "rb");
    std::array<u8, 540> array;

    if (file.ReadBytes(array.data(), 540) == 540) {
        Service::NFC::AmiiboData data{};
        std::memcpy(&data.uuid, &array[0x1D4], data.uuid.size());
        std::memcpy(&data.char_id, &array[0x1DC], 8);

        std::shared_ptr<Service::NFC::Module::Interface> nfc =
            static_cast<Core::System*>(core)
                ->ServiceManager()
                .GetService<Service::NFC::Module::Interface>("nfc:u");
        if (nfc != nullptr) {
            nfc->LoadAmiibo(data);
            return true;
        }
    }

    return false;
}

void vvctre_load_amiibo_from_memory_decrypted(void* core, const u8 data[540]) {
    Service::NFC::AmiiboData data_{};
    std::memcpy(&data_.uuid, &data[0x1D4], data_.uuid.size());
    std::memcpy(&data_.char_id, &data[0x1DC], 8);

    std::shared_ptr<Service::NFC::Module::Interface> nfc =
        static_cast<Core::System*>(core)
            ->ServiceManager()
            .GetService<Service::NFC::Module::Interface>("nfc:u");
    if (nfc != nullptr) {
        nfc->LoadAmiibo(data_);
    }
}

void vvctre_get_amiibo_data(void* core, u8 data[540]) {
    std::shared_ptr<Service::NFC::Module::Interface> nfc =
        static_cast<Core::System*>(core)
            ->ServiceManager()
            .GetService<Service::NFC::Module::Interface>("nfc:u");
    if (nfc != nullptr) {
        const Service::NFC::AmiiboData data_ = nfc->GetAmiiboData();
        std::memcpy(data, &data_, sizeof(data_));
    }
}

void vvctre_remove_amiibo(void* core) {
    std::shared_ptr<Service::NFC::Module::Interface> nfc =
        static_cast<Core::System*>(core)
            ->ServiceManager()
            .GetService<Service::NFC::Module::Interface>("nfc:u");
    if (nfc != nullptr) {
        nfc->RemoveAmiibo();
    }
}

u64 vvctre_get_program_id(void* core) {
    return static_cast<Core::System*>(core)->Kernel().GetCurrentProcess()->codeset->program_id;
}

const char* vvctre_get_process_name(void* core) {
    return static_cast<Core::System*>(core)->Kernel().GetCurrentProcess()->codeset->name.c_str();
}

// Emulation
void vvctre_restart(void* core) {
    static_cast<Core::System*>(core)->RequestReset();
}

void vvctre_set_paused(void* plugin_manager, bool paused) {
    static_cast<PluginManager*>(plugin_manager)->paused = paused;
}

bool vvctre_get_paused(void* plugin_manager) {
    return static_cast<PluginManager*>(plugin_manager)->paused;
}

bool vvctre_emulation_running(void* core) {
    return static_cast<Core::System*>(core)->IsPoweredOn();
}

// Memory
u8 vvctre_read_u8(void* core, VAddr address) {
    return static_cast<Core::System*>(core)->Memory().Read8(address);
}

void vvctre_write_u8(void* core, VAddr address, u8 value) {
    static_cast<Core::System*>(core)->Memory().Write8(address, value);
}

u16 vvctre_read_u16(void* core, VAddr address) {
    return static_cast<Core::System*>(core)->Memory().Read16(address);
}

void vvctre_write_u16(void* core, VAddr address, u16 value) {
    static_cast<Core::System*>(core)->Memory().Write16(address, value);
}

u32 vvctre_read_u32(void* core, VAddr address) {
    return static_cast<Core::System*>(core)->Memory().Read32(address);
}

void vvctre_write_u32(void* core, VAddr address, u32 value) {
    static_cast<Core::System*>(core)->Memory().Write32(address, value);
}

u64 vvctre_read_u64(void* core, VAddr address) {
    return static_cast<Core::System*>(core)->Memory().Read64(address);
}

void vvctre_write_u64(void* core, VAddr address, u64 value) {
    static_cast<Core::System*>(core)->Memory().Write64(address, value);
}

void vvctre_invalidate_cache_range(void* core, u32 address, std::size_t length) {
    static_cast<Core::System*>(core)->CPU().InvalidateCacheRange(address, length);
}

// Debugging
void vvctre_set_pc(void* core, u32 addr) {
    static_cast<Core::System*>(core)->CPU().SetPC(addr);
}

u32 vvctre_get_pc(void* core) {
    return static_cast<Core::System*>(core)->CPU().GetPC();
}

void vvctre_set_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetReg(index, value);
}

u32 vvctre_get_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetReg(index);
}

void vvctre_set_vfp_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetVFPReg(index, value);
}

u32 vvctre_get_vfp_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetVFPReg(index);
}

void vvctre_set_vfp_system_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetVFPSystemReg(static_cast<VFPSystemRegister>(index),
                                                            value);
}

u32 vvctre_get_vfp_system_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetVFPSystemReg(
        static_cast<VFPSystemRegister>(index));
}

void vvctre_set_cp15_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetCP15Register(static_cast<CP15Register>(index),
                                                            value);
}

u32 vvctre_get_cp15_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetCP15Register(
        static_cast<CP15Register>(index));
}

void vvctre_ipc_recorder_set_enabled(void* core, bool enabled) {
    static_cast<Core::System*>(core)->Kernel().GetIPCRecorder().SetEnabled(enabled);
}

bool vvctre_ipc_recorder_get_enabled(void* core) {
    return static_cast<Core::System*>(core)->Kernel().GetIPCRecorder().IsEnabled();
}

void vvctre_ipc_recorder_bind_callback(void* core, void (*callback)(const char* json)) {
    static_cast<Core::System*>(core)->Kernel().GetIPCRecorder().BindCallback(
        [callback](const IPCDebugger::RequestRecord& record) {
            asl::Var json;
            json["id"] = record.id;
            json["status"] = static_cast<int>(record.status);
            json["function_name"] = record.function_name.c_str();
            json["is_hle"] = record.is_hle;
            json["client_process"]["type"] = record.client_process.type.c_str();
            json["client_process"]["name"] = record.client_process.name.c_str();
            json["client_process"]["id"] = record.client_process.id;
            json["client_thread"]["type"] = record.client_thread.type.c_str();
            json["client_thread"]["name"] = record.client_thread.name.c_str();
            json["client_thread"]["id"] = record.client_thread.id;
            json["client_session"]["type"] = record.client_session.type.c_str();
            json["client_session"]["name"] = record.client_session.name.c_str();
            json["client_session"]["id"] = record.client_session.id;
            json["client_port"]["type"] = record.client_port.type.c_str();
            json["client_port"]["name"] = record.client_port.name.c_str();
            json["client_port"]["id"] = record.client_port.id;
            json["server_process"]["type"] = record.server_process.type.c_str();
            json["server_process"]["name"] = record.server_process.name.c_str();
            json["server_process"]["id"] = record.server_process.id;
            json["server_thread"]["type"] = record.server_thread.type.c_str();
            json["server_thread"]["name"] = record.server_thread.name.c_str();
            json["server_thread"]["id"] = record.server_thread.id;
            json["server_session"]["type"] = record.server_session.type.c_str();
            json["server_session"]["name"] = record.server_session.name.c_str();
            json["server_session"]["id"] = record.server_session.id;
            for (std::size_t i = 0; i < record.untranslated_request_cmdbuf.size(); ++i) {
                json["untranslated_request_buf"][i] = record.untranslated_request_cmdbuf[i];
            }
            for (std::size_t i = 0; i < record.translated_request_cmdbuf.size(); ++i) {
                json["translated_request_cmdbuf"][i] = record.translated_request_cmdbuf[i];
            }
            for (std::size_t i = 0; i < record.untranslated_reply_cmdbuf.size(); ++i) {
                json["untranslated_reply_cmdbuf"][i] = record.untranslated_reply_cmdbuf[i];
            }
            for (std::size_t i = 0; i < record.translated_reply_cmdbuf.size(); ++i) {
                json["translated_reply_cmdbuf"][i] = record.translated_reply_cmdbuf[i];
            }

            callback(*asl::Json::encode(json));
        });
}

const char* vvctre_get_service_name_by_port_id(void* core, u32 port) {
    return VVCTRE_STRDUP(
        static_cast<Core::System*>(core)->ServiceManager().GetServiceNameByPortId(port).c_str());
}

// Cheats
int vvctre_cheat_count(void* core) {
    return static_cast<int>(static_cast<Core::System*>(core)->CheatEngine().GetCheats().size());
}

const char* vvctre_get_cheat(void* core, int index) {
    return VVCTRE_STRDUP(
        static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->ToString().c_str());
}

const char* vvctre_get_cheat_name(void* core, int index) {
    return VVCTRE_STRDUP(
        static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetName().c_str());
}

const char* vvctre_get_cheat_comments(void* core, int index) {
    return VVCTRE_STRDUP(
        static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetComments().c_str());
}

const char* vvctre_get_cheat_type(void* core, int index) {
    return VVCTRE_STRDUP(
        static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetType().c_str());
}

const char* vvctre_get_cheat_code(void* core, int index) {
    return VVCTRE_STRDUP(
        static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetCode().c_str());
}

void vvctre_set_cheat_enabled(void* core, int index, bool enabled) {
    static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->SetEnabled(enabled);
}

bool vvctre_get_cheat_enabled(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->IsEnabled();
}

void vvctre_add_gateway_cheat(void* core, const char* name, const char* code,
                              const char* comments) {
    static_cast<Core::System*>(core)->CheatEngine().AddCheat(std::make_shared<Cheats::GatewayCheat>(
        std::string(name), std::string(code), std::string(comments)));
}

void vvctre_remove_cheat(void* core, int index) {
    static_cast<Core::System*>(core)->CheatEngine().RemoveCheat(index);
}

void vvctre_update_gateway_cheat(void* core, int index, const char* name, const char* code,
                                 const char* comments) {
    static_cast<Core::System*>(core)->CheatEngine().UpdateCheat(
        index, std::make_shared<Cheats::GatewayCheat>(std::string(name), std::string(code),
                                                      std::string(comments)));
}

void vvctre_load_cheats_from_file(void* core) {
    static_cast<Core::System*>(core)->CheatEngine().LoadCheatsFromFile();
}

void vvctre_save_cheats_to_file(void* core) {
    static_cast<Core::System*>(core)->CheatEngine().SaveCheatsToFile();
}

// Camera
void vvctre_reload_camera_images(void* core) {
    auto cam = Service::CAM::GetModule(*static_cast<Core::System*>(core));
    if (cam != nullptr) {
        cam->ReloadCameraDevices();
    }
}

// GUI
void vvctre_gui_push_item_width(float item_width) {
    ImGui::PushItemWidth(item_width);
}

void vvctre_gui_pop_item_width() {
    ImGui::PopItemWidth();
}

void vvctre_gui_get_content_region_max(float out[2]) {
    const ImVec2 vec = ImGui::GetContentRegionMax();
    out[0] = vec.x;
    out[1] = vec.y;
}

void vvctre_gui_get_content_region_avail(float out[2]) {
    const ImVec2 vec = ImGui::GetContentRegionAvail();
    out[0] = vec.x;
    out[1] = vec.y;
}

void vvctre_gui_get_window_content_region_min(float out[2]) {
    const ImVec2 vec = ImGui::GetWindowContentRegionMin();
    out[0] = vec.x;
    out[1] = vec.y;
}

void vvctre_gui_get_window_content_region_max(float out[2]) {
    const ImVec2 vec = ImGui::GetWindowContentRegionMax();
    out[0] = vec.x;
    out[1] = vec.y;
}

float vvctre_gui_get_window_content_region_width() {
    return ImGui::GetWindowContentRegionWidth();
}

float vvctre_gui_get_scroll_x() {
    return ImGui::GetScrollX();
}

float vvctre_gui_get_scroll_y() {
    return ImGui::GetScrollY();
}

float vvctre_gui_get_scroll_max_x() {
    return ImGui::GetScrollMaxX();
}

float vvctre_gui_get_scroll_max_y() {
    return ImGui::GetScrollMaxY();
}

void vvctre_gui_set_scroll_x(float scroll_x) {
    ImGui::SetScrollX(scroll_x);
}

void vvctre_gui_set_scroll_y(float scroll_y) {
    ImGui::SetScrollY(scroll_y);
}

void vvctre_gui_set_scroll_here_x(float center_x_ratio) {
    ImGui::SetScrollHereX(center_x_ratio);
}

void vvctre_gui_set_scroll_here_y(float center_y_ratio) {
    ImGui::SetScrollHereY(center_y_ratio);
}

void vvctre_gui_set_scroll_from_pos_x(float local_x, float center_x_ratio) {
    ImGui::SetScrollFromPosX(local_x, center_x_ratio);
}

void vvctre_gui_set_scroll_from_pos_y(float local_y, float center_y_ratio) {
    ImGui::SetScrollFromPosY(local_y, center_y_ratio);
}

void vvctre_gui_set_next_item_width(float item_width) {
    ImGui::SetNextItemWidth(item_width);
}

float vvctre_gui_calc_item_width() {
    return ImGui::CalcItemWidth();
}

void vvctre_gui_push_text_wrap_pos(float wrap_local_pos_x) {
    ImGui::PushTextWrapPos(wrap_local_pos_x);
}

void vvctre_gui_pop_text_wrap_pos() {
    ImGui::PopTextWrapPos();
}

void vvctre_gui_push_allow_keyboard_focus(bool allow_keyboard_focus) {
    ImGui::PushAllowKeyboardFocus(allow_keyboard_focus);
}

void vvctre_gui_pop_allow_keyboard_focus() {
    ImGui::PopAllowKeyboardFocus();
}

void vvctre_gui_push_button_repeat(bool repeat) {
    ImGui::PushButtonRepeat(repeat);
}

void vvctre_gui_pop_button_repeat() {
    ImGui::PopButtonRepeat();
}

void vvctre_gui_push_font(void* font) {
    ImGui::PushFont(static_cast<ImFont*>(font));
}

void vvctre_gui_pop_font() {
    ImGui::PopFont();
}

void vvctre_gui_push_style_color(ImGuiCol idx, const float r, const float g, const float b,
                                 const float a) {
    ImGui::PushStyleColor(idx, ImVec4(r, g, b, a));
}

void vvctre_gui_pop_style_color(int count) {
    ImGui::PopStyleColor(count);
}

void vvctre_gui_push_style_var_float(ImGuiStyleVar idx, float val) {
    ImGui::PushStyleVar(idx, val);
}

void vvctre_gui_push_style_var_2floats(ImGuiStyleVar idx, float val[2]) {
    ImGui::PushStyleVar(idx, ImVec2(val[0], val[1]));
}

void vvctre_gui_pop_style_var(int count) {
    ImGui::PopStyleVar(count);
}

void vvctre_gui_same_line() {
    ImGui::SameLine();
}

void vvctre_gui_new_line() {
    ImGui::NewLine();
}

void vvctre_gui_bullet() {
    ImGui::Bullet();
}

void vvctre_gui_indent() {
    ImGui::Indent();
}

void vvctre_gui_unindent() {
    ImGui::Unindent();
}

void vvctre_gui_begin_group() {
    ImGui::BeginGroup();
}

void vvctre_gui_end_group() {
    ImGui::EndGroup();
}

void vvctre_gui_get_cursor_pos(float out[2]) {
    const ImVec2 vec = ImGui::GetCursorPos();
    out[0] = vec.x;
    out[1] = vec.y;
}

float vvctre_gui_get_cursor_pos_x() {
    return ImGui::GetCursorPosX();
}

float vvctre_gui_get_cursor_pos_y() {
    return ImGui::GetCursorPosY();
}

void vvctre_gui_set_cursor_pos(float local_x, float local_y) {
    ImGui::SetCursorPos(ImVec2(local_x, local_y));
}

void vvctre_gui_set_cursor_pos_x(float local_x) {
    ImGui::SetCursorPosX(local_x);
}

void vvctre_gui_set_cursor_pos_y(float local_y) {
    ImGui::SetCursorPosY(local_y);
}

void vvctre_gui_get_cursor_start_pos(float out[2]) {
    const ImVec2 vec = ImGui::GetCursorStartPos();
    out[0] = vec.x;
    out[1] = vec.y;
}

void vvctre_gui_get_cursor_screen_pos(float out[2]) {
    const ImVec2 vec = ImGui::GetCursorScreenPos();
    out[0] = vec.x;
    out[1] = vec.y;
}

void vvctre_gui_set_cursor_screen_pos(float x, float y) {
    ImGui::SetCursorScreenPos(ImVec2(x, y));
}

void vvctre_gui_align_text_to_frame_padding() {
    ImGui::AlignTextToFramePadding();
}

float vvctre_gui_get_text_line_height() {
    return ImGui::GetTextLineHeight();
}

float vvctre_gui_get_text_line_height_with_spacing() {
    return ImGui::GetTextLineHeightWithSpacing();
}

float vvctre_gui_get_frame_height() {
    return ImGui::GetFrameHeight();
}

float vvctre_gui_get_frame_height_with_spacing() {
    return ImGui::GetFrameHeightWithSpacing();
}

void vvctre_gui_push_id_string(const char* id) {
    ImGui::PushID(id);
}

void vvctre_gui_push_id_string_with_begin_and_end(const char* begin, const char* end) {
    ImGui::PushID(begin, end);
}

void vvctre_gui_push_id_void(void* id) {
    ImGui::PushID(id);
}

void vvctre_gui_push_id_int(int id) {
    ImGui::PushID(id);
}

void vvctre_gui_pop_id() {
    ImGui::PopID();
}

void vvctre_gui_spacing() {
    ImGui::Spacing();
}

void vvctre_gui_separator() {
    ImGui::Separator();
}

void vvctre_gui_dummy(float width, float height) {
    ImGui::Dummy(ImVec2(width, height));
}

void vvctre_gui_tooltip(const char* text) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(text);
        ImGui::EndTooltip();
    }
}

void vvctre_gui_begin_tooltip() {
    ImGui::BeginTooltip();
}

bool vvctre_gui_is_item_hovered(ImGuiHoveredFlags flags) {
    return ImGui::IsItemHovered(flags);
}

bool vvctre_gui_is_item_focused() {
    return ImGui::IsItemFocused();
}

bool vvctre_gui_is_item_clicked(int button) {
    return ImGui::IsItemClicked(button);
}

bool vvctre_gui_is_item_visible() {
    return ImGui::IsItemVisible();
}

bool vvctre_gui_is_item_edited() {
    return ImGui::IsItemEdited();
}

bool vvctre_gui_is_item_activated() {
    return ImGui::IsItemActivated();
}

bool vvctre_gui_is_item_deactivated() {
    return ImGui::IsItemDeactivated();
}

bool vvctre_gui_is_item_deactivated_after_edit() {
    return ImGui::IsItemDeactivatedAfterEdit();
}

bool vvctre_gui_is_item_toggled_open() {
    return ImGui::IsItemToggledOpen();
}

bool vvctre_gui_is_any_item_hovered() {
    return ImGui::IsAnyItemHovered();
}

bool vvctre_gui_is_any_item_active() {
    return ImGui::IsAnyItemActive();
}

bool vvctre_gui_is_any_item_focused() {
    return ImGui::IsAnyItemFocused();
}

void vvctre_gui_get_item_rect_min(float out[2]) {
    const ImVec2 min = ImGui::GetItemRectMin();
    out[0] = min.x;
    out[1] = min.y;
}

void vvctre_gui_get_item_rect_max(float out[2]) {
    const ImVec2 max = ImGui::GetItemRectMax();
    out[0] = max.x;
    out[1] = max.y;
}

void vvctre_gui_get_item_rect_size(float out[2]) {
    const ImVec2 size = ImGui::GetItemRectSize();
    out[0] = size.x;
    out[1] = size.y;
}

void vvctre_gui_set_item_allow_overlap() {
    ImGui::SetItemAllowOverlap();
}

bool vvctre_gui_is_rect_visible_size(float width, float height) {
    return ImGui::IsRectVisible(ImVec2(width, height));
}

bool vvctre_gui_is_rect_visible_min_max(float min[2], float max[2]) {
    return ImGui::IsRectVisible(ImVec2(min[0], min[1]), ImVec2(max[0], max[1]));
}

void vvctre_gui_end_tooltip() {
    ImGui::EndTooltip();
}

void vvctre_gui_text(const char* text) {
    ImGui::TextUnformatted(text);
}

void vvctre_gui_text_ex(const char* text, const char* end) {
    ImGui::TextUnformatted(text, end);
}

void vvctre_gui_text_colored(float red, float green, float blue, float alpha, const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(red, green, blue, alpha));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

bool vvctre_gui_button(const char* label) {
    return ImGui::Button(label);
}

bool vvctre_gui_small_button(const char* label) {
    return ImGui::SmallButton(label);
}

bool vvctre_gui_color_button(const char* tooltip, float red, float green, float blue, float alpha,
                             ImGuiColorEditFlags flags) {
    return ImGui::ColorButton(tooltip, ImVec4(red, green, blue, alpha), flags);
}

bool vvctre_gui_color_button_ex(const char* tooltip, float red, float green, float blue,
                                float alpha, ImGuiColorEditFlags flags, float width, float height) {
    return ImGui::ColorButton(tooltip, ImVec4(red, green, blue, alpha), flags,
                              ImVec2(width, height));
}

bool vvctre_gui_invisible_button(const char* id, float width, float height) {
    return ImGui::InvisibleButton(id, ImVec2(width, height));
}

bool vvctre_gui_radio_button(const char* label, bool active) {
    return ImGui::RadioButton(label, active);
}

bool vvctre_gui_image_button(void* texture_id, float width, float height, float uv0[2],
                             float uv1[2], int frame_padding, float background_color[4],
                             float tint_color[4]) {
    return ImGui::ImageButton(
        texture_id, ImVec2(width, height), ImVec2(uv0[0], uv0[1]), ImVec2(uv1[0], uv1[1]),
        frame_padding,
        ImVec4(background_color[0], background_color[1], background_color[2], background_color[3]),
        ImVec4(tint_color[0], tint_color[1], tint_color[2], tint_color[3]));
}

bool vvctre_gui_checkbox(const char* label, bool* checked) {
    return ImGui::Checkbox(label, checked);
}

bool vvctre_gui_begin(const char* name) {
    return ImGui::Begin(name);
}

bool vvctre_gui_begin_ex(const char* name, bool* open, ImGuiWindowFlags flags) {
    return ImGui::Begin(name, open, flags);
}

bool vvctre_gui_begin_overlay(const char* name, float initial_x, float initial_y) {
    ImGui::SetNextWindowPos(ImVec2(initial_x, initial_y), ImGuiCond_Appearing);
    return ImGui::Begin(name, nullptr,
                        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize);
}

bool vvctre_gui_begin_auto_resize(const char* name) {
    return ImGui::Begin(name, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
}

bool vvctre_gui_begin_child(const char* id, float width, float height, bool border,
                            ImGuiWindowFlags flags) {
    return ImGui::BeginChild(id, ImVec2(width, height), border, flags);
}

bool vvctre_gui_begin_child_frame(const char* id, float width, float height,
                                  ImGuiWindowFlags flags) {
    return ImGui::BeginChildFrame(ImGui::GetID(id), ImVec2(width, height), flags);
}

bool vvctre_gui_begin_popup(const char* id, ImGuiWindowFlags flags) {
    return ImGui::BeginPopup(id, flags);
}

bool vvctre_gui_begin_popup_modal(const char* name, bool* open, ImGuiWindowFlags flags) {
    return ImGui::BeginPopupModal(name, open, flags);
}

bool vvctre_gui_begin_popup_context_item(const char* id, ImGuiPopupFlags flags) {
    return ImGui::BeginPopupContextItem(id, flags);
}

bool vvctre_gui_begin_popup_context_window(const char* id, ImGuiPopupFlags flags) {
    return ImGui::BeginPopupContextWindow(id, flags);
}

bool vvctre_gui_begin_popup_context_void(const char* id, ImGuiPopupFlags flags) {
    return ImGui::BeginPopupContextVoid(id, flags);
}

void vvctre_gui_end() {
    ImGui::End();
}

void vvctre_gui_end_child() {
    ImGui::EndChild();
}

void vvctre_gui_end_child_frame() {
    ImGui::EndChildFrame();
}

void vvctre_gui_end_popup() {
    ImGui::EndPopup();
}

void vvctre_gui_open_popup(const char* id, ImGuiPopupFlags flags) {
    ImGui::OpenPopup(id, flags);
}

void vvctre_gui_open_popup_on_item_click(const char* id, ImGuiPopupFlags flags) {
    ImGui::OpenPopupOnItemClick(id, flags);
}

void vvctre_gui_close_current_popup() {
    ImGui::CloseCurrentPopup();
}

bool vvctre_gui_is_popup_open(const char* id, ImGuiPopupFlags flags) {
    return ImGui::IsPopupOpen(id, flags);
}

bool vvctre_gui_begin_menu(const char* label) {
    return ImGui::BeginMenu(label);
}

void vvctre_gui_end_menu() {
    ImGui::EndMenu();
}

bool vvctre_gui_begin_tab(const char* label) {
    return ImGui::BeginTabItem(label);
}

bool vvctre_gui_begin_tab_ex(const char* label, bool* open, ImGuiTabItemFlags flags) {
    return ImGui::BeginTabItem(label, open, flags);
}

bool vvctre_gui_begin_tab_bar(const char* id, ImGuiTabBarFlags flags) {
    return ImGui::BeginTabBar(id, flags);
}

void vvctre_gui_end_tab_bar() {
    ImGui::EndTabBar();
}

void vvctre_gui_set_tab_closed(const char* name) {
    ImGui::SetTabItemClosed(name);
}

void vvctre_gui_end_tab() {
    ImGui::EndTabItem();
}

bool vvctre_gui_menu_item(const char* label) {
    return ImGui::MenuItem(label);
}

bool vvctre_gui_menu_item_with_check_mark(const char* label, bool* checked) {
    return ImGui::MenuItem(label, nullptr, checked);
}

void vvctre_gui_plot_lines(const char* label, const float* values, int values_count,
                           int values_offset, const char* overlay_text, float scale_min,
                           float scale_max, float graph_width, float graph_height, int stride) {
    ImGui::PlotLines(label, values, values_count, values_offset, overlay_text, scale_min, scale_max,
                     ImVec2(graph_width, graph_height), stride);
}

void vvctre_gui_plot_lines_getter(const char* label, float (*values_getter)(void* data, int idx),
                                  void* data, int values_count, int values_offset,
                                  const char* overlay_text, float scale_min, float scale_max,
                                  float graph_width, float graph_height) {
    ImGui::PlotLines(label, values_getter, data, values_count, values_offset, overlay_text,
                     scale_min, scale_max, ImVec2(graph_width, graph_height));
}

void vvctre_gui_plot_histogram(const char* label, const float* values, int values_count,
                               int values_offset, const char* overlay_text, float scale_min,
                               float scale_max, float graph_width, float graph_height, int stride) {
    ImGui::PlotHistogram(label, values, values_count, values_offset, overlay_text, scale_min,
                         scale_max, ImVec2(graph_width, graph_height), stride);
}

void vvctre_gui_plot_histogram_getter(const char* label,
                                      float (*values_getter)(void* data, int idx), void* data,
                                      int values_count, int values_offset, const char* overlay_text,
                                      float scale_min, float scale_max, float graph_width,
                                      float graph_height) {
    ImGui::PlotHistogram(label, values_getter, data, values_count, values_offset, overlay_text,
                         scale_min, scale_max, ImVec2(graph_width, graph_height));
}

bool vvctre_gui_begin_listbox(const char* label) {
    return ImGui::ListBoxHeader(label);
}

void vvctre_gui_end_listbox() {
    ImGui::ListBoxFooter();
}

bool vvctre_gui_begin_combo_box(const char* label, const char* preview) {
    return ImGui::BeginCombo(label, preview);
}

void vvctre_gui_end_combo_box() {
    ImGui::EndCombo();
}

bool vvctre_gui_selectable(const char* label) {
    return ImGui::Selectable(label);
}

bool vvctre_gui_selectable_with_selected(const char* label, bool* selected) {
    return ImGui::Selectable(label, selected);
}

bool vvctre_gui_text_input(const char* label, char* buffer, std::size_t buffer_size) {
    return ImGui::InputText(label, buffer, buffer_size);
}

bool vvctre_gui_text_input_multiline(const char* label, char* buffer, std::size_t buffer_size) {
    return ImGui::InputTextMultiline(label, buffer, buffer_size);
}

bool vvctre_gui_text_input_with_hint(const char* label, const char* hint, char* buffer,
                                     std::size_t buffer_size) {
    return ImGui::InputTextWithHint(label, hint, buffer, buffer_size);
}

bool vvctre_gui_text_input_ex(const char* label, char* buffer, std::size_t buffer_size,
                              ImGuiInputTextFlags flags) {
    return ImGui::InputText(label, buffer, buffer_size, flags);
}

bool vvctre_gui_text_input_multiline_ex(const char* label, char* buffer, std::size_t buffer_size,
                                        float width, float height, ImGuiInputTextFlags flags) {
    return ImGui::InputTextMultiline(label, buffer, buffer_size, ImVec2(width, height), flags);
}

bool vvctre_gui_text_input_with_hint_ex(const char* label, const char* hint, char* buffer,
                                        std::size_t buffer_size, ImGuiInputTextFlags flags) {
    return ImGui::InputTextWithHint(label, hint, buffer, buffer_size, flags);
}

bool vvctre_gui_u8_input(const char* label, u8* value) {
    return ImGui::InputScalar(label, ImGuiDataType_U8, value);
}

bool vvctre_gui_u8_input_ex(const char* label, u8* value, const u8* step, const u8* step_fast,
                            const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalar(label, ImGuiDataType_U8, value, step, step_fast, format, flags);
}

bool vvctre_gui_u8_inputs(const char* label, u8* values, int components, const u8* step,
                          const u8* step_fast, const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalarN(label, ImGuiDataType_U8, values, components, step, step_fast, format,
                               flags);
}

bool vvctre_gui_u16_input(const char* label, u16* value) {
    return ImGui::InputScalar(label, ImGuiDataType_U16, value);
}

bool vvctre_gui_u16_input_ex(const char* label, u16* value, const u16* step, const u16* step_fast,
                             const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalar(label, ImGuiDataType_U16, value, step, step_fast, format, flags);
}

bool vvctre_gui_u16_inputs(const char* label, u16* values, int components, const u16* step,
                           const u16* step_fast, const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalarN(label, ImGuiDataType_U16, values, components, step, step_fast,
                               format, flags);
}

bool vvctre_gui_u32_input(const char* label, u32* value) {
    return ImGui::InputScalar(label, ImGuiDataType_U32, value);
}

bool vvctre_gui_u32_input_ex(const char* label, u32* value, const u32* step, const u32* step_fast,
                             const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalar(label, ImGuiDataType_U32, value, step, step_fast, format, flags);
}

bool vvctre_gui_u32_inputs(const char* label, u32* values, int components, const u32* step,
                           const u32* step_fast, const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalarN(label, ImGuiDataType_U32, values, components, step, step_fast,
                               format, flags);
}

bool vvctre_gui_u64_input(const char* label, u64* value) {
    return ImGui::InputScalar(label, ImGuiDataType_U64, value);
}

bool vvctre_gui_u64_input_ex(const char* label, u64* value, const u64* step, const u64* step_fast,
                             const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalar(label, ImGuiDataType_U64, value, step, step_fast, format, flags);
}

bool vvctre_gui_u64_inputs(const char* label, u64* values, int components, const u64* step,
                           const u64* step_fast, const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalarN(label, ImGuiDataType_U64, values, components, step, step_fast,
                               format, flags);
}

bool vvctre_gui_s8_input(const char* label, s8* value) {
    return ImGui::InputScalar(label, ImGuiDataType_S8, value);
}

bool vvctre_gui_s8_input_ex(const char* label, s8* value, const s8* step, const s8* step_fast,
                            const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalar(label, ImGuiDataType_S8, value, step, step_fast, format, flags);
}

bool vvctre_gui_s8_inputs(const char* label, s8* values, int components, const s8* step,
                          const s8* step_fast, const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalarN(label, ImGuiDataType_S8, values, components, step, step_fast, format,
                               flags);
}

bool vvctre_gui_s16_input(const char* label, s16* value) {
    return ImGui::InputScalar(label, ImGuiDataType_S16, value);
}

bool vvctre_gui_s16_input_ex(const char* label, s16* value, const s16* step, const s16* step_fast,
                             const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalar(label, ImGuiDataType_S16, value, step, step_fast, format, flags);
}

bool vvctre_gui_s16_inputs(const char* label, s16* values, int components, const s16* step,
                           const s16* step_fast, const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalarN(label, ImGuiDataType_S16, values, components, step, step_fast,
                               format, flags);
}

bool vvctre_gui_int_input(const char* label, int* value, int step, int step_fast) {
    return ImGui::InputInt(label, value, step, step_fast);
}

bool vvctre_gui_int_input_ex(const char* label, int* value, const int* step, const int* step_fast,
                             const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalar(label, ImGuiDataType_S32, value, step, step_fast, format, flags);
}

bool vvctre_gui_int_inputs(const char* label, int* values, int components, const int* step,
                           const int* step_fast, const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalarN(label, ImGuiDataType_S32, values, components, step, step_fast,
                               format, flags);
}

bool vvctre_gui_s64_input(const char* label, s64* value) {
    return ImGui::InputScalar(label, ImGuiDataType_S64, value);
}

bool vvctre_gui_s64_input_ex(const char* label, s64* value, const s64* step, const s64* step_fast,
                             const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalar(label, ImGuiDataType_S32, value, step, step_fast, format, flags);
}

bool vvctre_gui_s64_inputs(const char* label, s64* values, int components, const s64* step,
                           const s64* step_fast, const char* format, ImGuiInputTextFlags flags) {
    return ImGui::InputScalarN(label, ImGuiDataType_S64, values, components, step, step_fast,
                               format, flags);
}

bool vvctre_gui_float_input(const char* label, float* value, float step, float step_fast) {
    return ImGui::InputFloat(label, value, step, step_fast);
}

bool vvctre_gui_float_input_ex(const char* label, float* value, const float* step,
                               const float* step_fast, const char* format,
                               ImGuiInputTextFlags flags) {
    return ImGui::InputScalar(label, ImGuiDataType_Float, value, step, step_fast, format, flags);
}

bool vvctre_gui_float_inputs(const char* label, float* values, int components, const float* step,
                             const float* step_fast, const char* format,
                             ImGuiInputTextFlags flags) {
    return ImGui::InputScalarN(label, ImGuiDataType_Float, values, components, step, step_fast,
                               format, flags);
}

bool vvctre_gui_double_input(const char* label, double* value, double step, double step_fast) {
    return ImGui::InputDouble(label, value, step, step_fast);
}

bool vvctre_gui_double_input_ex(const char* label, double* value, const double* step,
                                const double* step_fast, const char* format,
                                ImGuiInputTextFlags flags) {
    return ImGui::InputScalar(label, ImGuiDataType_Double, value, step, step_fast, format, flags);
}

bool vvctre_gui_double_inputs(const char* label, double* values, int components, const double* step,
                              const double* step_fast, const char* format,
                              ImGuiInputTextFlags flags) {
    return ImGui::InputScalarN(label, ImGuiDataType_Double, values, components, step, step_fast,
                               format, flags);
}

bool vvctre_gui_color_edit(const char* label, float* color, ImGuiColorEditFlags flags) {
    return ImGui::ColorEdit4(label, color, flags);
}

bool vvctre_gui_color_picker(const char* label, float* color, ImGuiColorEditFlags flags) {
    return ImGui::ColorPicker4(label, color, flags);
}

bool vvctre_gui_color_picker_ex(const char* label, float* color, ImGuiColorEditFlags flags,
                                const float* ref_col) {
    return ImGui::ColorPicker4(label, color, flags, ref_col);
}

void vvctre_gui_progress_bar(float value, const char* overlay) {
    ImGui::ProgressBar(value, ImVec2(-1, 0), overlay);
}

void vvctre_gui_progress_bar_ex(float value, float width, float height, const char* overlay) {
    ImGui::ProgressBar(value, ImVec2(width, height), overlay);
}

bool vvctre_gui_slider_u8(const char* label, u8* value, const u8 minimum, const u8 maximum,
                          const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_U8, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_u8_ex(const char* label, u8* value, const u8 minimum, const u8 maximum,
                             const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalar(label, ImGuiDataType_U8, value, &minimum, &maximum, format, flags);
}

bool vvctre_gui_sliders_u8(const char* label, u8* values, int components, const u8 minimum,
                           const u8 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalarN(label, ImGuiDataType_U8, values, components, &minimum, &maximum,
                                format, flags);
}

bool vvctre_gui_slider_u16(const char* label, u16* value, const u16 minimum, const u16 maximum,
                           const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_U16, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_u16_ex(const char* label, u16* value, const u16 minimum, const u16 maximum,
                              const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalar(label, ImGuiDataType_U16, value, &minimum, &maximum, format, flags);
}

bool vvctre_gui_sliders_u16(const char* label, u16* values, int components, const u16 minimum,
                            const u16 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalarN(label, ImGuiDataType_U16, values, components, &minimum, &maximum,
                                format, flags);
}

bool vvctre_gui_slider_u32(const char* label, u32* value, const u32 minimum, const u32 maximum,
                           const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_U32, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_u32_ex(const char* label, u32* value, const u32 minimum, const u32 maximum,
                              const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalar(label, ImGuiDataType_U32, value, &minimum, &maximum, format, flags);
}

bool vvctre_gui_sliders_u32(const char* label, u32* values, int components, const u32 minimum,
                            const u32 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalarN(label, ImGuiDataType_U32, values, components, &minimum, &maximum,
                                format, flags);
}

bool vvctre_gui_slider_u64(const char* label, u64* value, const u64 minimum, const u64 maximum,
                           const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_U64, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_u64_ex(const char* label, u64* value, const u64 minimum, const u64 maximum,
                              const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalar(label, ImGuiDataType_U64, value, &minimum, &maximum, format, flags);
}

bool vvctre_gui_sliders_u64(const char* label, u64* values, int components, const u64 minimum,
                            const u64 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalarN(label, ImGuiDataType_U64, values, components, &minimum, &maximum,
                                format, flags);
}

bool vvctre_gui_slider_s8(const char* label, s8* value, const s8 minimum, const s8 maximum,
                          const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_S8, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_s8_ex(const char* label, s8* value, const s8 minimum, const s8 maximum,
                             const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalar(label, ImGuiDataType_S8, value, &minimum, &maximum, format, flags);
}

bool vvctre_gui_sliders_s8(const char* label, s8* values, int components, const s8 minimum,
                           const s8 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalarN(label, ImGuiDataType_S8, values, components, &minimum, &maximum,
                                format, flags);
}

bool vvctre_gui_slider_s16(const char* label, s16* value, const s16 minimum, const s16 maximum,
                           const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_S16, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_s16_ex(const char* label, s16* value, const s16 minimum, const s16 maximum,
                              const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalar(label, ImGuiDataType_S16, value, &minimum, &maximum, format, flags);
}

bool vvctre_gui_sliders_s16(const char* label, s16* values, int components, const s16 minimum,
                            const s16 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalarN(label, ImGuiDataType_S16, values, components, &minimum, &maximum,
                                format, flags);
}

bool vvctre_gui_slider_s32(const char* label, s32* value, const s32 minimum, const s32 maximum,
                           const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_S32, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_s32_ex(const char* label, s32* value, const s32 minimum, const s32 maximum,
                              const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalar(label, ImGuiDataType_S32, value, &minimum, &maximum, format, flags);
}

bool vvctre_gui_sliders_s32(const char* label, s32* values, int components, const s32 minimum,
                            const s32 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalarN(label, ImGuiDataType_S32, values, components, &minimum, &maximum,
                                format, flags);
}

bool vvctre_gui_slider_s64(const char* label, s64* value, const s64 minimum, const s64 maximum,
                           const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_S64, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_s64_ex(const char* label, s64* value, const s64 minimum, const s64 maximum,
                              const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalar(label, ImGuiDataType_S64, value, &minimum, &maximum, format, flags);
}

bool vvctre_gui_sliders_s64(const char* label, s64* values, int components, const s64 minimum,
                            const s64 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalarN(label, ImGuiDataType_S64, values, components, &minimum, &maximum,
                                format, flags);
}

bool vvctre_gui_slider_float(const char* label, float* value, const float minimum,
                             const float maximum, const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_Float, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_float_ex(const char* label, float* value, const float minimum,
                                const float maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalar(label, ImGuiDataType_Float, value, &minimum, &maximum, format,
                               flags);
}

bool vvctre_gui_sliders_float(const char* label, float* values, int components, const float minimum,
                              const float maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalarN(label, ImGuiDataType_Float, values, components, &minimum, &maximum,
                                format, flags);
}

bool vvctre_gui_slider_double(const char* label, double* value, const double minimum,
                              const double maximum, const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_Double, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_double_ex(const char* label, double* value, const double minimum,
                                 const double maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderScalar(label, ImGuiDataType_Double, value, &minimum, &maximum, format,
                               flags);
}

bool vvctre_gui_sliders_double(const char* label, double* values, int components,
                               const double minimum, const double maximum, const char* format,
                               ImGuiSliderFlags flags) {
    return ImGui::SliderScalarN(label, ImGuiDataType_Double, values, components, &minimum, &maximum,
                                format, flags);
}

bool vvctre_gui_slider_angle(const char* label, float* rad, float degrees_min, float degrees_max,
                             const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderAngle(label, rad, degrees_min, degrees_max, format, flags);
}

bool vvctre_gui_vertical_slider_u8(const char* label, float width, float height, u8* value,
                                   const u8 minimum, const u8 maximum, const char* format,
                                   ImGuiSliderFlags flags) {
    return ImGui::VSliderScalar(label, ImVec2(width, height), ImGuiDataType_U8, value, &minimum,
                                &maximum, format, flags);
}

bool vvctre_gui_vertical_slider_u16(const char* label, float width, float height, u16* value,
                                    const u16 minimum, const u16 maximum, const char* format,
                                    ImGuiSliderFlags flags) {
    return ImGui::VSliderScalar(label, ImVec2(width, height), ImGuiDataType_U16, value, &minimum,
                                &maximum, format, flags);
}

bool vvctre_gui_vertical_slider_u32(const char* label, float width, float height, u32* value,
                                    const u32 minimum, const u32 maximum, const char* format,
                                    ImGuiSliderFlags flags) {
    return ImGui::VSliderScalar(label, ImVec2(width, height), ImGuiDataType_U32, value, &minimum,
                                &maximum, format, flags);
}

bool vvctre_gui_vertical_slider_u64(const char* label, float width, float height, u64* value,
                                    const u64 minimum, const u64 maximum, const char* format,
                                    ImGuiSliderFlags flags) {
    return ImGui::VSliderScalar(label, ImVec2(width, height), ImGuiDataType_U64, value, &minimum,
                                &maximum, format, flags);
}

bool vvctre_gui_vertical_slider_s8(const char* label, float width, float height, s8* value,
                                   const s8 minimum, const s8 maximum, const char* format,
                                   ImGuiSliderFlags flags) {
    return ImGui::VSliderScalar(label, ImVec2(width, height), ImGuiDataType_S8, value, &minimum,
                                &maximum, format, flags);
}

bool vvctre_gui_vertical_slider_s16(const char* label, float width, float height, s16* value,
                                    const s16 minimum, const s16 maximum, const char* format,
                                    ImGuiSliderFlags flags) {
    return ImGui::VSliderScalar(label, ImVec2(width, height), ImGuiDataType_S16, value, &minimum,
                                &maximum, format, flags);
}

bool vvctre_gui_vertical_slider_s32(const char* label, float width, float height, s32* value,
                                    const s32 minimum, const s32 maximum, const char* format,
                                    ImGuiSliderFlags flags) {
    return ImGui::VSliderScalar(label, ImVec2(width, height), ImGuiDataType_S32, value, &minimum,
                                &maximum, format, flags);
}

bool vvctre_gui_vertical_slider_s64(const char* label, float width, float height, s64* value,
                                    const s64 minimum, const s64 maximum, const char* format,
                                    ImGuiSliderFlags flags) {
    return ImGui::VSliderScalar(label, ImVec2(width, height), ImGuiDataType_S64, value, &minimum,
                                &maximum, format, flags);
}

bool vvctre_gui_vertical_slider_float(const char* label, float width, float height, float* value,
                                      const float minimum, const float maximum, const char* format,
                                      ImGuiSliderFlags flags) {
    return ImGui::VSliderScalar(label, ImVec2(width, height), ImGuiDataType_Float, value, &minimum,
                                &maximum, format, flags);
}

bool vvctre_gui_vertical_slider_double(const char* label, float width, float height, double* value,
                                       const double minimum, const double maximum,
                                       const char* format, ImGuiSliderFlags flags) {
    return ImGui::VSliderScalar(label, ImVec2(width, height), ImGuiDataType_Double, value, &minimum,
                                &maximum, format, flags);
}

bool vvctre_gui_drag_u8(const char* label, u8* value, float speed, const u8 minimum,
                        const u8 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::DragScalar(label, ImGuiDataType_U8, value, speed, &minimum, &maximum, format,
                             flags);
}

bool vvctre_gui_drags_u8(const char* label, u8* values, int components, float speed,
                         const u8 minimum, const u8 maximum, const char* format,
                         ImGuiSliderFlags flags) {
    return ImGui::DragScalarN(label, ImGuiDataType_U8, values, components, speed, &minimum,
                              &maximum, format, flags);
}

bool vvctre_gui_drag_u16(const char* label, u16* value, float speed, const u16 minimum,
                         const u16 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::DragScalar(label, ImGuiDataType_U16, value, speed, &minimum, &maximum, format,
                             flags);
}

bool vvctre_gui_drags_u16(const char* label, u16* values, int components, float speed,
                          const u16 minimum, const u16 maximum, const char* format,
                          ImGuiSliderFlags flags) {
    return ImGui::DragScalarN(label, ImGuiDataType_U16, values, components, speed, &minimum,
                              &maximum, format, flags);
}

bool vvctre_gui_drag_u32(const char* label, u32* value, float speed, const u32 minimum,
                         const u32 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::DragScalar(label, ImGuiDataType_U32, value, speed, &minimum, &maximum, format,
                             flags);
}

bool vvctre_gui_drags_u32(const char* label, u32* values, int components, float speed,
                          const u32 minimum, const u32 maximum, const char* format,
                          ImGuiSliderFlags flags) {
    return ImGui::DragScalarN(label, ImGuiDataType_U32, values, components, speed, &minimum,
                              &maximum, format, flags);
}

bool vvctre_gui_drag_u64(const char* label, u64* value, float speed, const u64 minimum,
                         const u64 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::DragScalar(label, ImGuiDataType_U64, value, speed, &minimum, &maximum, format,
                             flags);
}

bool vvctre_gui_drags_u64(const char* label, u64* values, int components, float speed,
                          const u64 minimum, const u64 maximum, const char* format,
                          ImGuiSliderFlags flags) {
    return ImGui::DragScalarN(label, ImGuiDataType_U64, values, components, speed, &minimum,
                              &maximum, format, flags);
}

bool vvctre_gui_drag_s8(const char* label, s8* value, float speed, const s8 minimum,
                        const s8 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::DragScalar(label, ImGuiDataType_S8, value, speed, &minimum, &maximum, format,
                             flags);
}

bool vvctre_gui_drags_s8(const char* label, s8* values, int components, float speed,
                         const s8 minimum, const s8 maximum, const char* format,
                         ImGuiSliderFlags flags) {
    return ImGui::DragScalarN(label, ImGuiDataType_S8, values, components, speed, &minimum,
                              &maximum, format, flags);
}

bool vvctre_gui_drag_s16(const char* label, s16* value, float speed, const s16 minimum,
                         const s16 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::DragScalar(label, ImGuiDataType_S16, value, speed, &minimum, &maximum, format,
                             flags);
}

bool vvctre_gui_drags_s16(const char* label, s16* values, int components, float speed,
                          const s16 minimum, const s16 maximum, const char* format,
                          ImGuiSliderFlags flags) {
    return ImGui::DragScalarN(label, ImGuiDataType_S16, values, components, speed, &minimum,
                              &maximum, format, flags);
}

bool vvctre_gui_drag_s32(const char* label, s32* value, float speed, const s32 minimum,
                         const s32 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::DragScalar(label, ImGuiDataType_S32, value, speed, &minimum, &maximum, format,
                             flags);
}

bool vvctre_gui_drags_s32(const char* label, s32* values, int components, float speed,
                          const s32 minimum, const s32 maximum, const char* format,
                          ImGuiSliderFlags flags) {
    return ImGui::DragScalarN(label, ImGuiDataType_S32, values, components, speed, &minimum,
                              &maximum, format, flags);
}

bool vvctre_gui_drag_s64(const char* label, s64* value, float speed, const s64 minimum,
                         const s64 maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::DragScalar(label, ImGuiDataType_S64, value, speed, &minimum, &maximum, format,
                             flags);
}

bool vvctre_gui_drags_s64(const char* label, s64* values, int components, float speed,
                          const s64 minimum, const s64 maximum, const char* format,
                          ImGuiSliderFlags flags) {
    return ImGui::DragScalarN(label, ImGuiDataType_S64, values, components, speed, &minimum,
                              &maximum, format, flags);
}

bool vvctre_gui_drag_float(const char* label, float* value, float speed, const float minimum,
                           const float maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::DragScalar(label, ImGuiDataType_Float, value, speed, &minimum, &maximum, format,
                             flags);
}

bool vvctre_gui_drags_float(const char* label, float* values, int components, float speed,
                            const float minimum, const float maximum, const char* format,
                            ImGuiSliderFlags flags) {
    return ImGui::DragScalarN(label, ImGuiDataType_Float, values, components, speed, &minimum,
                              &maximum, format, flags);
}

bool vvctre_gui_drag_double(const char* label, double* value, float speed, const double minimum,
                            const double maximum, const char* format, ImGuiSliderFlags flags) {
    return ImGui::DragScalar(label, ImGuiDataType_Double, value, speed, &minimum, &maximum, format,
                             flags);
}

bool vvctre_gui_drags_double(const char* label, double* values, int components, float speed,
                             const double minimum, const double maximum, const char* format,
                             ImGuiSliderFlags flags) {
    return ImGui::DragScalarN(label, ImGuiDataType_Double, values, components, speed, &minimum,
                              &maximum, format, flags);
}

void vvctre_gui_image(void* texture_id, float width, float height, float uv0[2], float uv1[2],
                      float tint_color[4], float border_color[4]) {
    ImGui::Image(texture_id, ImVec2(width, height), ImVec2(uv0[0], uv0[1]), ImVec2(uv1[0], uv1[1]),
                 ImVec4(tint_color[0], tint_color[1], tint_color[2], tint_color[3]),
                 ImVec4(border_color[0], border_color[1], border_color[2], border_color[3]));
}

void vvctre_gui_columns(int count, const char* id, bool border) {
    ImGui::Columns(count, id, border);
}

void vvctre_gui_next_column() {
    ImGui::NextColumn();
}

int vvctre_gui_get_column_index() {
    return ImGui::GetColumnIndex();
}

float vvctre_gui_get_column_width(int column_index) {
    return ImGui::GetColumnWidth(column_index);
}

void vvctre_gui_set_column_width(int column_index, float width) {
    ImGui::SetColumnWidth(column_index, width);
}

float vvctre_gui_get_column_offset(int column_index) {
    return ImGui::GetColumnOffset(column_index);
}

void vvctre_gui_set_column_offset(int column_index, float offset_x) {
    ImGui::SetColumnOffset(column_index, offset_x);
}

int vvctre_gui_get_columns_count() {
    return ImGui::GetColumnsCount();
}

bool vvctre_gui_tree_node_string(const char* label, ImGuiTreeNodeFlags flags) {
    return ImGui::TreeNodeEx(label, flags);
}

void vvctre_gui_tree_push_string(const char* id) {
    ImGui::TreePush(id);
}

void vvctre_gui_tree_push_void(const void* id) {
    ImGui::TreePush(id);
}

void vvctre_gui_tree_pop() {
    ImGui::TreePop();
}

float vvctre_gui_get_tree_node_to_label_spacing() {
    return ImGui::GetTreeNodeToLabelSpacing();
}

bool vvctre_gui_collapsing_header(const char* label, ImGuiTreeNodeFlags flags) {
    return ImGui::CollapsingHeader(label, flags);
}

void vvctre_gui_set_next_item_open(bool is_open) {
    ImGui::SetNextItemOpen(is_open);
}

void vvctre_gui_set_color(int index, float r, float g, float b, float a) {
    ImGui::GetStyle().Colors[index] = ImVec4(r, g, b, a);
}

void vvctre_gui_get_color(int index, float color_out[4]) {
    const ImVec4 color = ImGui::GetStyle().Colors[index];
    color_out[0] = color.x;
    color_out[1] = color.y;
    color_out[2] = color.z;
    color_out[3] = color.w;
}

void vvctre_gui_set_font(void* data, int data_size, float font_size) {
    ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, data_size, font_size);
}

void* vvctre_gui_set_font_and_get_pointer(void* data, int data_size, float font_size) {
    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, data_size, font_size);
}

bool vvctre_gui_is_window_appearing() {
    return ImGui::IsWindowAppearing();
}

bool vvctre_gui_is_window_collapsed() {
    return ImGui::IsWindowCollapsed();
}

bool vvctre_gui_is_window_focused(ImGuiFocusedFlags flags) {
    return ImGui::IsWindowFocused(flags);
}

bool vvctre_gui_is_window_hovered(ImGuiHoveredFlags flags) {
    return ImGui::IsWindowHovered(flags);
}

void vvctre_gui_get_window_pos(float out[2]) {
    const ImVec2 pos = ImGui::GetWindowPos();
    out[0] = pos.x;
    out[1] = pos.y;
}

void vvctre_gui_get_window_size(float out[2]) {
    const ImVec2 size = ImGui::GetWindowSize();
    out[0] = size.x;
    out[1] = size.y;
}

void vvctre_gui_set_next_window_pos(float x, float y, int condition, float pivot[2]) {
    ImGui::SetNextWindowPos(ImVec2(x, y), condition, ImVec2(pivot[0], pivot[1]));
}

void vvctre_gui_set_next_window_size(float width, float height, int condition) {
    ImGui::SetNextWindowSize(ImVec2(width, height), condition);
}

void vvctre_gui_set_next_window_size_constraints(float min[2], float max[2]) {
    ImGui::SetNextWindowSizeConstraints(ImVec2(min[0], min[1]), ImVec2(max[0], max[1]));
}

void vvctre_gui_set_next_window_content_size(float width, float height) {
    ImGui::SetNextWindowContentSize(ImVec2(width, height));
}

void vvctre_gui_set_next_window_collapsed(bool collapsed, int condition) {
    ImGui::SetNextWindowCollapsed(collapsed, condition);
}

void vvctre_gui_set_next_window_focus() {
    ImGui::SetNextWindowFocus();
}

void vvctre_gui_set_next_window_bg_alpha(float alpha) {
    ImGui::SetNextWindowBgAlpha(alpha);
}

void vvctre_gui_set_window_pos(float x, float y, ImGuiCond condition) {
    ImGui::SetWindowPos(ImVec2(x, y), condition);
}

void vvctre_gui_set_window_size(float width, float height, ImGuiCond condition) {
    ImGui::SetWindowSize(ImVec2(width, height), condition);
}

void vvctre_gui_set_window_collapsed(bool collapsed, ImGuiCond condition) {
    ImGui::SetWindowCollapsed(collapsed, condition);
}

void vvctre_gui_set_window_focus() {
    ImGui::SetWindowFocus();
}

void vvctre_gui_set_window_font_scale(float scale) {
    ImGui::SetWindowFontScale(scale);
}

void vvctre_gui_set_window_pos_named(const char* name, float x, float y, ImGuiCond condition) {
    ImGui::SetWindowPos(name, ImVec2(x, y), condition);
}

void vvctre_gui_set_window_size_named(const char* name, float width, float height,
                                      ImGuiCond condition) {
    ImGui::SetWindowSize(name, ImVec2(width, height), condition);
}

void vvctre_gui_set_window_collapsed_named(const char* name, bool collapsed, ImGuiCond condition) {
    ImGui::SetWindowCollapsed(name, collapsed, condition);
}

void vvctre_gui_set_window_focus_named(const char* name) {
    ImGui::SetWindowFocus(name);
}

bool vvctre_gui_is_key_down(int key) {
    return ImGui::IsKeyDown(key);
}

bool vvctre_gui_is_key_pressed(int key, bool repeat) {
    return ImGui::IsKeyPressed(key, repeat);
}

bool vvctre_gui_is_key_released(int key) {
    return ImGui::IsKeyReleased(key);
}

int vvctre_gui_get_key_pressed_amount(int key, float repeat_delay, float rate) {
    return ImGui::GetKeyPressedAmount(key, repeat_delay, rate);
}

void vvctre_gui_capture_keyboard_from_app(bool want_capture_keyboard_value) {
    ImGui::CaptureKeyboardFromApp(want_capture_keyboard_value);
}

bool vvctre_gui_is_mouse_down(ImGuiMouseButton button) {
    return ImGui::IsMouseDown(button);
}

bool vvctre_gui_is_mouse_clicked(ImGuiMouseButton button, bool repeat) {
    return ImGui::IsMouseClicked(button, repeat);
}

bool vvctre_gui_is_mouse_released(ImGuiMouseButton button) {
    return ImGui::IsMouseReleased(button);
}

bool vvctre_gui_is_mouse_double_clicked(ImGuiMouseButton button) {
    return ImGui::IsMouseDoubleClicked(button);
}

bool vvctre_gui_is_mouse_hovering_rect(const float min[2], const float max[2], bool clip) {
    return ImGui::IsMouseHoveringRect(ImVec2(min[0], min[1]), ImVec2(max[0], max[1]), clip);
}

bool vvctre_gui_is_mouse_pos_valid(const float pos[2]) {
    if (pos == nullptr) {
        return ImGui::IsMousePosValid();
    } else {
        const ImVec2 vec(pos[0], pos[1]);
        return ImGui::IsMousePosValid(&vec);
    }
}

bool vvctre_gui_is_any_mouse_down() {
    return ImGui::IsAnyMouseDown();
}

void vvctre_gui_get_mouse_pos(float out[2]) {
    const ImVec2 vec = ImGui::GetMousePos();
    out[0] = vec.x;
    out[1] = vec.y;
}

void vvctre_gui_get_mouse_pos_on_opening_current_popup(float out[2]) {
    const ImVec2 vec = ImGui::GetMousePosOnOpeningCurrentPopup();
    out[0] = vec.x;
    out[1] = vec.y;
}

bool vvctre_gui_is_mouse_dragging(ImGuiMouseButton button, float lock_threshold) {
    return ImGui::IsMouseDragging(button, lock_threshold);
}

void vvctre_gui_get_mouse_drag_delta(ImGuiMouseButton button, float lock_threshold, float out[2]) {
    const ImVec2 vec = ImGui::GetMouseDragDelta(button, lock_threshold);
    out[0] = vec.x;
    out[1] = vec.y;
}

void vvctre_gui_reset_mouse_drag_delta(ImGuiMouseButton button) {
    ImGui::ResetMouseDragDelta(button);
}

ImGuiMouseCursor vvctre_gui_get_mouse_cursor() {
    return ImGui::GetMouseCursor();
}

void vvctre_gui_set_mouse_cursor(ImGuiMouseCursor cursor_type) {
    ImGui::SetMouseCursor(cursor_type);
}

void vvctre_gui_capture_mouse_from_app(bool want_capture_mouse_value) {
    ImGui::CaptureMouseFromApp(want_capture_mouse_value);
}

void vvctre_gui_style_set_alpha(float value) {
    ImGui::GetStyle().Alpha = value;
}

void vvctre_gui_style_set_window_padding(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding.x = value[0];
    style.WindowPadding.y = value[1];
}

void vvctre_gui_style_set_window_rounding(float value) {
    ImGui::GetStyle().WindowRounding = value;
}

void vvctre_gui_style_set_window_border_size(float value) {
    ImGui::GetStyle().WindowBorderSize = value;
}

void vvctre_gui_style_set_window_min_size(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowMinSize.x = value[0];
    style.WindowMinSize.y = value[1];
}

void vvctre_gui_style_set_window_title_align(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowTitleAlign.x = value[0];
    style.WindowTitleAlign.y = value[1];
}

void vvctre_gui_style_set_child_rounding(float value) {
    ImGui::GetStyle().ChildRounding = value;
}

void vvctre_gui_style_set_child_border_size(float value) {
    ImGui::GetStyle().ChildBorderSize = value;
}

void vvctre_gui_style_set_popup_rounding(float value) {
    ImGui::GetStyle().PopupRounding = value;
}

void vvctre_gui_style_set_popup_border_size(float value) {
    ImGui::GetStyle().PopupBorderSize = value;
}

void vvctre_gui_style_set_frame_padding(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.FramePadding.x = value[0];
    style.FramePadding.y = value[1];
}

void vvctre_gui_style_set_frame_rounding(float value) {
    ImGui::GetStyle().FrameRounding = value;
}

void vvctre_gui_style_set_frame_border_size(float value) {
    ImGui::GetStyle().FrameBorderSize = value;
}

void vvctre_gui_style_set_item_spacing(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.ItemSpacing.x = value[0];
    style.ItemSpacing.y = value[1];
}

void vvctre_gui_style_set_item_inner_spacing(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.ItemInnerSpacing.x = value[0];
    style.ItemInnerSpacing.y = value[1];
}

void vvctre_gui_style_set_indent_spacing(float value) {
    ImGui::GetStyle().IndentSpacing = value;
}

void vvctre_gui_style_set_scrollbar_size(float value) {
    ImGui::GetStyle().ScrollbarSize = value;
}

void vvctre_gui_style_set_scrollbar_rounding(float value) {
    ImGui::GetStyle().ScrollbarRounding = value;
}

void vvctre_gui_style_set_grab_min_size(float value) {
    ImGui::GetStyle().GrabMinSize = value;
}

void vvctre_gui_style_set_grab_rounding(float value) {
    ImGui::GetStyle().GrabRounding = value;
}

void vvctre_gui_style_set_tab_rounding(float value) {
    ImGui::GetStyle().TabRounding = value;
}

void vvctre_gui_style_set_button_text_align(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.ButtonTextAlign.x = value[0];
    style.ButtonTextAlign.y = value[1];
}

void vvctre_gui_style_set_selectable_text_align(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.SelectableTextAlign.x = value[0];
    style.SelectableTextAlign.y = value[1];
}

float vvctre_gui_style_get_alpha() {
    return ImGui::GetStyle().Alpha;
}

void vvctre_gui_style_get_window_padding(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    value[0] = style.WindowPadding.x;
    value[1] = style.WindowPadding.y;
}

float vvctre_gui_style_get_window_rounding() {
    return ImGui::GetStyle().WindowRounding;
}

float vvctre_gui_style_get_window_border_size() {
    return ImGui::GetStyle().WindowBorderSize;
}

void vvctre_gui_style_get_window_min_size(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    value[0] = style.WindowMinSize.x;
    value[1] = style.WindowMinSize.y;
}

void vvctre_gui_style_get_window_title_align(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    value[0] = style.WindowTitleAlign.x;
    value[1] = style.WindowTitleAlign.y;
}

float vvctre_gui_style_get_child_rounding() {
    return ImGui::GetStyle().ChildRounding;
}

float vvctre_gui_style_get_child_border_size() {
    return ImGui::GetStyle().ChildBorderSize;
}

float vvctre_gui_style_get_popup_rounding() {
    return ImGui::GetStyle().PopupRounding;
}

float vvctre_gui_style_get_popup_border_size() {
    return ImGui::GetStyle().PopupBorderSize;
}

void vvctre_gui_style_get_frame_padding(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    value[0] = style.FramePadding.x;
    value[1] = style.FramePadding.y;
}

float vvctre_gui_style_get_frame_rounding() {
    return ImGui::GetStyle().FrameRounding;
}

float vvctre_gui_style_get_frame_border_size() {
    return ImGui::GetStyle().FrameBorderSize;
}

void vvctre_gui_style_get_item_spacing(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    value[0] = style.ItemSpacing.x;
    value[1] = style.ItemSpacing.y;
}

void vvctre_gui_style_get_item_inner_spacing(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    value[0] = style.ItemInnerSpacing.x;
    value[1] = style.ItemInnerSpacing.y;
}

float vvctre_gui_style_get_indent_spacing() {
    return ImGui::GetStyle().IndentSpacing;
}

float vvctre_gui_style_get_scrollbar_size() {
    return ImGui::GetStyle().ScrollbarSize;
}

float vvctre_gui_style_get_scrollbar_rounding() {
    return ImGui::GetStyle().ScrollbarRounding;
}

float vvctre_gui_style_get_grab_min_size() {
    return ImGui::GetStyle().GrabMinSize;
}

float vvctre_gui_style_get_grab_rounding() {
    return ImGui::GetStyle().GrabRounding;
}

float vvctre_gui_style_get_tab_rounding() {
    return ImGui::GetStyle().TabRounding;
}

void vvctre_gui_style_get_button_text_align(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    value[0] = style.ButtonTextAlign.x;
    value[0] = style.ButtonTextAlign.y;
}

void vvctre_gui_style_get_selectable_text_align(float value[2]) {
    ImGuiStyle& style = ImGui::GetStyle();
    value[0] = style.SelectableTextAlign.x;
    value[1] = style.SelectableTextAlign.y;
}

u64 vvctre_get_dear_imgui_version() {
    return IMGUI_VERSION_NUM;
}

// OS window
void vvctre_set_os_window_size(void* plugin_manager, int width, int height) {
    SDL_SetWindowSize(static_cast<PluginManager*>(plugin_manager)->window, width, height);
}

void vvctre_get_os_window_size(void* plugin_manager, int* width, int* height) {
    SDL_GetWindowSize(static_cast<PluginManager*>(plugin_manager)->window, width, height);
}

void vvctre_set_os_window_minimum_size(void* plugin_manager, int width, int height) {
    SDL_SetWindowMinimumSize(static_cast<PluginManager*>(plugin_manager)->window, width, height);
}

void vvctre_get_os_window_minimum_size(void* plugin_manager, int* width, int* height) {
    SDL_GetWindowMinimumSize(static_cast<PluginManager*>(plugin_manager)->window, width, height);
}

void vvctre_set_os_window_maximum_size(void* plugin_manager, int width, int height) {
    SDL_SetWindowMaximumSize(static_cast<PluginManager*>(plugin_manager)->window, width, height);
}

void vvctre_get_os_window_maximum_size(void* plugin_manager, int* width, int* height) {
    SDL_GetWindowMaximumSize(static_cast<PluginManager*>(plugin_manager)->window, width, height);
}

void vvctre_set_os_window_position(void* plugin_manager, int x, int y) {
    SDL_SetWindowPosition(static_cast<PluginManager*>(plugin_manager)->window, x, y);
}

void vvctre_get_os_window_position(void* plugin_manager, int* x, int* y) {
    SDL_GetWindowPosition(static_cast<PluginManager*>(plugin_manager)->window, x, y);
}

void vvctre_set_os_window_title(void* plugin_manager, const char* title) {
    SDL_SetWindowTitle(static_cast<PluginManager*>(plugin_manager)->window, title);
}

const char* vvctre_get_os_window_title(void* plugin_manager) {
    return SDL_GetWindowTitle(static_cast<PluginManager*>(plugin_manager)->window);
}

// Button devices
void* vvctre_button_device_new(void* plugin_manager, const char* params) {
    PluginManager* pm = static_cast<PluginManager*>(plugin_manager);
    return pm->NewButtonDevice(params);
}

void vvctre_button_device_delete(void* plugin_manager, void* device) {
    PluginManager* pm = static_cast<PluginManager*>(plugin_manager);
    pm->DeleteButtonDevice(device);
}

bool vvctre_button_device_get_state(void* device) {
    return static_cast<Input::ButtonDevice*>(device)->GetStatus();
}

// TAS
void vvctre_movie_prepare_for_playback(const char* path) {
    Core::Movie::GetInstance().PrepareForPlayback(path);
}

void vvctre_movie_prepare_for_recording() {
    Core::Movie::GetInstance().PrepareForRecording();
}

void vvctre_movie_play(const char* path) {
    Core::Movie::GetInstance().StartPlayback(std::string(path));
}

void vvctre_movie_record(const char* path) {
    Core::Movie::GetInstance().StartRecording(std::string(path));
}

bool vvctre_movie_is_playing() {
    return Core::Movie::GetInstance().IsPlayingInput();
}

bool vvctre_movie_is_recording() {
    return Core::Movie::GetInstance().IsRecordingInput();
}

void vvctre_movie_stop() {
    Core::Movie::GetInstance().Shutdown();
}

void vvctre_set_frame_advancing_enabled(void* core, bool enabled) {
    static_cast<Core::System*>(core)->frame_limiter.SetFrameAdvancing(enabled);
}

bool vvctre_get_frame_advancing_enabled(void* core) {
    return static_cast<Core::System*>(core)->frame_limiter.FrameAdvancingEnabled();
}

void vvctre_advance_frame(void* core) {
    static_cast<Core::System*>(core)->frame_limiter.AdvanceFrame();
}

// Remote control
void vvctre_set_custom_pad_state(void* core, u32 state) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomPadState(Service::HID::PadState{state});
}

void vvctre_use_real_pad_state(void* core) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomPadState(std::nullopt);
}

u32 vvctre_get_pad_state(void* core) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    return hid->GetPadState().hex;
}

void vvctre_set_custom_circle_pad_state(void* core, float x, float y) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomCirclePadState(std::make_tuple(x, y));
}

void vvctre_use_real_circle_pad_state(void* core) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomCirclePadState(std::nullopt);
}

void vvctre_get_circle_pad_state(void* core, float* x_out, float* y_out) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    const auto [x, y] = hid->GetCirclePadState();
    *x_out = x;
    *y_out = y;
}

void vvctre_set_custom_circle_pad_pro_state(void* core, float x, float y, bool zl, bool zr) {
    std::shared_ptr<Service::IR::IR_USER> ir =
        static_cast<Core::System*>(core)->ServiceManager().GetService<Service::IR::IR_USER>(
            "ir:USER");

    ir->SetCustomCirclePadProState(std::make_tuple(x, y, zl, zr));
}

void vvctre_use_real_circle_pad_pro_state(void* core) {
    std::shared_ptr<Service::IR::IR_USER> ir =
        static_cast<Core::System*>(core)->ServiceManager().GetService<Service::IR::IR_USER>(
            "ir:USER");

    ir->SetCustomCirclePadProState(std::nullopt);
}

void vvctre_get_circle_pad_pro_state(void* core, float* x_out, float* y_out, bool* zl_out,
                                     bool* zr_out) {
    std::shared_ptr<Service::IR::IR_USER> ir =
        static_cast<Core::System*>(core)->ServiceManager().GetService<Service::IR::IR_USER>(
            "ir:USER");

    const auto [x, y, zl, zr] = ir->GetCirclePadProState();
    *x_out = x;
    *y_out = y;
    *zl_out = zl;
    *zr_out = zr;
}

void vvctre_set_custom_touch_state(void* core, float x, float y, bool pressed) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomTouchState(std::make_tuple(x, y, pressed));
}

void vvctre_use_real_touch_state(void* core) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomTouchState(std::nullopt);
}

bool vvctre_get_touch_state(void* core, float* x_out, float* y_out) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    const auto [x, y, pressed] = hid->GetTouchState();
    *x_out = x;
    *y_out = y;

    return pressed;
}

void vvctre_set_custom_motion_state(void* core, float accelerometer[3], float gyroscope[3]) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomMotionState(
        std::make_tuple(Common::Vec3f(accelerometer[0], accelerometer[1], accelerometer[2]),
                        Common::Vec3f(gyroscope[0], gyroscope[1], gyroscope[2])));
}

void vvctre_use_real_motion_state(void* core) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomPadState(std::nullopt);
}

void vvctre_get_motion_state(void* core, float accelerometer_out[3], float gyroscope_out[3]) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    const auto [accelerometer, gyroscope] = hid->GetMotionState();
    accelerometer_out[0] = accelerometer.x;
    accelerometer_out[1] = accelerometer.y;
    accelerometer_out[2] = accelerometer.z;
    gyroscope_out[0] = gyroscope.x;
    gyroscope_out[1] = gyroscope.y;
    gyroscope_out[2] = gyroscope.z;
}

bool vvctre_screenshot(void* plugin_manager, void* data) {
    const Layout::FramebufferLayout& layout =
        VideoCore::g_renderer->GetRenderWindow().GetFramebufferLayout();

    return VideoCore::RequestScreenshot(
        data,
        [=] {
            const auto convert_bgra_to_rgba = [](const std::vector<u8>& input,
                                                 const Layout::FramebufferLayout& layout) {
                int offset = 0;
                std::vector<u8> output(input.size());

                for (u32 y = 0; y < layout.height; ++y) {
                    for (u32 x = 0; x < layout.width; ++x) {
                        output[offset] = input[offset + 2];
                        output[offset + 1] = input[offset + 1];
                        output[offset + 2] = input[offset];
                        output[offset + 3] = input[offset + 3];

                        offset += 4;
                    }
                }

                return output;
            };

            std::vector<u8> v(layout.width * layout.height * 4);
            std::memcpy(v.data(), data, v.size());
            v = convert_bgra_to_rgba(v, layout);
            Common::FlipRGBA8Texture(v, static_cast<u64>(layout.width),
                                     static_cast<u64>(layout.height));
            std::memcpy(data, v.data(), v.size());

            PluginManager* pm = static_cast<PluginManager*>(plugin_manager);
            pm->CallScreenshotCallbacks(data);
        },
        layout);
}

bool vvctre_screenshot_bottom_screen(void* plugin_manager, void* data, u32 width, u32 height) {
    Layout::FramebufferLayout layout;
    layout.width = width;
    layout.height = height;
    layout.top_screen.left = 0;
    layout.top_screen.top = 0;
    layout.top_screen.right = 0;
    layout.top_screen.bottom = 0;
    layout.top_screen_enabled = false;
    layout.bottom_screen.left = 0;
    layout.bottom_screen.top = 0;
    layout.bottom_screen.right = width;
    layout.bottom_screen.bottom = height;
    layout.bottom_screen_enabled = true;
    layout.is_rotated = true;

    return VideoCore::RequestScreenshot(
        data,
        [=] {
            const auto convert_bgra_to_rgba = [](const std::vector<u8>& input,
                                                 const Layout::FramebufferLayout& layout) {
                int offset = 0;
                std::vector<u8> output(input.size());

                for (u32 y = 0; y < layout.height; ++y) {
                    for (u32 x = 0; x < layout.width; ++x) {
                        output[offset] = input[offset + 2];
                        output[offset + 1] = input[offset + 1];
                        output[offset + 2] = input[offset];
                        output[offset + 3] = input[offset + 3];

                        offset += 4;
                    }
                }

                return output;
            };

            std::vector<u8> v(layout.width * layout.height * 4);
            std::memcpy(v.data(), data, v.size());
            v = convert_bgra_to_rgba(v, layout);
            Common::FlipRGBA8Texture(v, static_cast<u64>(layout.width),
                                     static_cast<u64>(layout.height));
            std::memcpy(data, v.data(), v.size());

            PluginManager* pm = static_cast<PluginManager*>(plugin_manager);
            pm->CallScreenshotCallbacks(data);
        },
        layout);
}

bool vvctre_screenshot_default_layout(void* plugin_manager, void* data) {
    const Layout::FramebufferLayout layout = Layout::DefaultFrameLayout(
        Core::kScreenTopWidth, Core::kScreenTopHeight + Core::kScreenBottomHeight, false, false);

    return VideoCore::RequestScreenshot(
        data,
        [=] {
            const auto convert_bgra_to_rgba = [](const std::vector<u8>& input,
                                                 const Layout::FramebufferLayout& layout) {
                int offset = 0;
                std::vector<u8> output(input.size());

                for (u32 y = 0; y < layout.height; ++y) {
                    for (u32 x = 0; x < layout.width; ++x) {
                        output[offset] = input[offset + 2];
                        output[offset + 1] = input[offset + 1];
                        output[offset + 2] = input[offset];
                        output[offset + 3] = input[offset + 3];

                        offset += 4;
                    }
                }

                return output;
            };

            std::vector<u8> v(layout.width * layout.height * 4);
            std::memcpy(v.data(), data, v.size());
            v = convert_bgra_to_rgba(v, layout);
            Common::FlipRGBA8Texture(v, static_cast<u64>(layout.width),
                                     static_cast<u64>(layout.height));
            std::memcpy(data, v.data(), v.size());

            PluginManager* pm = static_cast<PluginManager*>(plugin_manager);
            pm->CallScreenshotCallbacks(data);
        },
        layout);
}

// Settings
void vvctre_settings_apply() {
    Settings::Apply();
}

// Start Settings
void vvctre_settings_set_file_path(const char* value) {
    Settings::values.file_path = std::string(value);
}

const char* vvctre_settings_get_file_path() {
    return Settings::values.file_path.c_str();
}

void vvctre_settings_set_play_movie(const char* value) {
    Settings::values.play_movie = std::string(value);
}

const char* vvctre_settings_get_play_movie() {
    return Settings::values.play_movie.c_str();
}

void vvctre_settings_set_record_movie(const char* value) {
    Settings::values.record_movie = std::string(value);
}

const char* vvctre_settings_get_record_movie() {
    return Settings::values.record_movie.c_str();
}

void vvctre_settings_set_region_value(int value) {
    Settings::values.region_value = value;
}

int vvctre_settings_get_region_value() {
    return Settings::values.region_value;
}

void vvctre_settings_set_log_filter(const char* value) {
    Settings::values.log_filter = std::string(value);

    Log::Filter log_filter(Log::Level::Debug);
    log_filter.ParseFilterString(Settings::values.log_filter);
    Log::SetGlobalFilter(log_filter);
}

const char* vvctre_settings_get_log_filter() {
    return Settings::values.log_filter.c_str();
}

void vvctre_settings_set_initial_clock(int value) {
    Settings::values.initial_clock = static_cast<Settings::InitialClock>(value);
}

int vvctre_settings_get_initial_clock() {
    return static_cast<int>(Settings::values.initial_clock);
}

void vvctre_settings_set_unix_timestamp(u64 value) {
    Settings::values.unix_timestamp = value;
}

u64 vvctre_settings_get_unix_timestamp() {
    return Settings::values.unix_timestamp;
}

void vvctre_settings_set_use_virtual_sd(bool value) {
    Settings::values.use_virtual_sd = value;
}

bool vvctre_settings_get_use_virtual_sd() {
    return Settings::values.use_virtual_sd;
}

void vvctre_settings_set_record_frame_times(bool value) {
    Settings::values.record_frame_times = value;
}

bool vvctre_settings_get_record_frame_times() {
    return Settings::values.record_frame_times;
}

void vvctre_settings_enable_gdbstub(u16 port) {
    Settings::values.use_gdbstub = true;
    Settings::values.gdbstub_port = port;
}

void vvctre_settings_disable_gdbstub() {
    Settings::values.use_gdbstub = false;
}

bool vvctre_settings_is_gdb_stub_enabled() {
    return Settings::values.use_gdbstub;
}

u16 vvctre_settings_get_gdb_stub_port() {
    return Settings::values.use_gdbstub;
}

// General Settings
void vvctre_settings_set_use_cpu_jit(bool value) {
    Settings::values.use_cpu_jit = value;
}

bool vvctre_settings_get_use_cpu_jit() {
    return Settings::values.use_cpu_jit;
}

void vvctre_settings_set_limit_speed(bool value) {
    Settings::values.limit_speed = value;
}

bool vvctre_settings_get_limit_speed() {
    return Settings::values.limit_speed;
}

void vvctre_settings_set_speed_limit(u16 value) {
    Settings::values.speed_limit = value;
}

u16 vvctre_settings_get_speed_limit() {
    return Settings::values.speed_limit;
}

void vvctre_settings_set_use_custom_cpu_ticks(bool value) {
    Settings::values.use_custom_cpu_ticks = value;
}

bool vvctre_settings_get_use_custom_cpu_ticks() {
    return Settings::values.use_custom_cpu_ticks;
}

void vvctre_settings_set_custom_cpu_ticks(u64 value) {
    Settings::values.custom_cpu_ticks = value;
}

u64 vvctre_settings_get_custom_cpu_ticks() {
    return Settings::values.custom_cpu_ticks;
}

void vvctre_settings_set_cpu_clock_percentage(u32 value) {
    Settings::values.cpu_clock_percentage = value;
}

u32 vvctre_settings_get_cpu_clock_percentage() {
    return Settings::values.cpu_clock_percentage;
}

// Audio Settings
void vvctre_settings_set_enable_dsp_lle(bool value) {
    Settings::values.enable_dsp_lle = value;
}

bool vvctre_settings_get_enable_dsp_lle() {
    return Settings::values.enable_dsp_lle;
}

void vvctre_settings_set_enable_dsp_lle_multithread(bool value) {
    Settings::values.enable_dsp_lle_multithread = value;
}

bool vvctre_settings_get_enable_dsp_lle_multithread() {
    return Settings::values.enable_dsp_lle_multithread;
}

void vvctre_settings_set_enable_audio_stretching(bool value) {
    Settings::values.enable_audio_stretching = value;
}

bool vvctre_settings_get_enable_audio_stretching() {
    return Settings::values.enable_audio_stretching;
}

void vvctre_settings_set_audio_volume(float value) {
    Settings::values.audio_volume = value;
}

float vvctre_settings_get_audio_volume() {
    return Settings::values.audio_volume;
}

void vvctre_settings_set_audio_sink_id(const char* value) {
    Settings::values.audio_sink_id = std::string(value);
}

const char* vvctre_settings_get_audio_sink_id() {
    return Settings::values.audio_sink_id.c_str();
}

void vvctre_settings_set_audio_device_id(const char* value) {
    Settings::values.audio_device_id = std::string(value);
}

const char* vvctre_settings_get_audio_device_id() {
    return Settings::values.audio_device_id.c_str();
}

void vvctre_settings_set_microphone_input_type(int value) {
    Settings::values.microphone_input_type = static_cast<Settings::MicrophoneInputType>(value);
}

int vvctre_settings_get_microphone_input_type() {
    return static_cast<int>(Settings::values.microphone_input_type);
}

void vvctre_settings_set_microphone_device(const char* value) {
    Settings::values.microphone_device = std::string(value);
}

const char* vvctre_settings_get_microphone_device() {
    return Settings::values.microphone_device.c_str();
}

// Camera Settings
void vvctre_settings_set_camera_engine(int index, const char* value) {
    Settings::values.camera_engine[index] = std::string(value);
}

const char* vvctre_settings_get_camera_engine(int index) {
    return Settings::values.camera_engine[index].c_str();
}

void vvctre_settings_set_camera_parameter(int index, const char* value) {
    Settings::values.camera_parameter[index] = std::string(value);
}

const char* vvctre_settings_get_camera_parameter(int index) {
    return Settings::values.camera_parameter[index].c_str();
}

void vvctre_settings_set_camera_flip(int index, int value) {
    Settings::values.camera_flip[index] = static_cast<Service::CAM::Flip>(value);
}

int vvctre_settings_get_camera_flip(int index) {
    return static_cast<int>(Settings::values.camera_flip[index]);
}

// System Settings
void vvctre_set_play_coins(u16 value) {
    Service::PTM::Module::SetPlayCoins(value);
}

u16 vvctre_get_play_coins() {
    return Service::PTM::Module::GetPlayCoins();
}

void vvctre_settings_set_username(void* cfg, const char* value) {
    static_cast<Service::CFG::Module*>(cfg)->SetUsername(Common::UTF8ToUTF16(std::string(value)));
}

void vvctre_settings_get_username(void* cfg, char* out) {
    std::strcpy(
        out, Common::UTF16ToUTF8(static_cast<Service::CFG::Module*>(cfg)->GetUsername()).c_str());
}

void vvctre_settings_set_birthday(void* cfg, u8 month, u8 day) {
    static_cast<Service::CFG::Module*>(cfg)->SetBirthday(month, day);
}

void vvctre_settings_get_birthday(void* cfg, u8* month_out, u8* day_out) {
    const auto [month, day] = static_cast<Service::CFG::Module*>(cfg)->GetBirthday();
    *month_out = month;
    *day_out = day;
}

void vvctre_settings_set_system_language(void* cfg, int value) {
    static_cast<Service::CFG::Module*>(cfg)->SetSystemLanguage(
        static_cast<Service::CFG::SystemLanguage>(value));
}

int vvctre_settings_get_system_language(void* cfg) {
    return static_cast<int>(static_cast<Service::CFG::Module*>(cfg)->GetSystemLanguage());
}

void vvctre_settings_set_sound_output_mode(void* cfg, int value) {
    static_cast<Service::CFG::Module*>(cfg)->SetSoundOutputMode(
        static_cast<Service::CFG::SoundOutputMode>(value));
}

int vvctre_settings_get_sound_output_mode(void* cfg) {
    return static_cast<int>(static_cast<Service::CFG::Module*>(cfg)->GetSoundOutputMode());
}

void vvctre_settings_set_country(void* cfg, u8 value) {
    static_cast<Service::CFG::Module*>(cfg)->SetCountry(value);
}

u8 vvctre_settings_get_country(void* cfg) {
    return static_cast<Service::CFG::Module*>(cfg)->GetCountryCode();
}

void vvctre_settings_set_console_id(void* cfg, u32 random_number, u64 console_id) {
    static_cast<Service::CFG::Module*>(cfg)->SetConsoleUniqueId(random_number, console_id);
}

u64 vvctre_settings_get_console_id(void* cfg) {
    return static_cast<Service::CFG::Module*>(cfg)->GetConsoleUniqueId();
}

void vvctre_settings_set_console_model(void* cfg, u8 value) {
    static_cast<Service::CFG::Module*>(cfg)->SetSystemModel(
        static_cast<Service::CFG::SystemModel>(value));
}

u8 vvctre_settings_get_console_model(void* cfg) {
    return static_cast<u8>(static_cast<Service::CFG::Module*>(cfg)->GetSystemModel());
}

void vvctre_settings_set_eula_version(void* cfg, u8 minor, u8 major) {
    static_cast<Service::CFG::Module*>(cfg)->SetEULAVersion(
        Service::CFG::EULAVersion{minor, major});
}

void vvctre_settings_get_eula_version(void* cfg, u8* minor, u8* major) {
    Service::CFG::EULAVersion v = static_cast<Service::CFG::Module*>(cfg)->GetEULAVersion();
    *minor = v.minor;
    *major = v.major;
}

void vvctre_settings_write_config_savegame(void* cfg) {
    static_cast<Service::CFG::Module*>(cfg)->UpdateConfigNANDSavegame();
}

// Graphics Settings
void vvctre_settings_set_use_hardware_renderer(bool value) {
    Settings::values.use_hardware_renderer = value;
}

bool vvctre_settings_get_use_hardware_renderer() {
    return Settings::values.use_hardware_renderer;
}

void vvctre_settings_set_use_hardware_shader(bool value) {
    Settings::values.use_hardware_shader = value;
}

bool vvctre_settings_get_use_hardware_shader() {
    return Settings::values.use_hardware_shader;
}

void vvctre_settings_set_hardware_shader_accurate_multiplication(bool value) {
    Settings::values.hardware_shader_accurate_multiplication = value;
}

bool vvctre_settings_get_hardware_shader_accurate_multiplication() {
    return Settings::values.hardware_shader_accurate_multiplication;
}

void vvctre_settings_set_use_shader_jit(bool value) {
    Settings::values.use_shader_jit = value;
}

bool vvctre_settings_get_use_shader_jit() {
    return Settings::values.use_shader_jit;
}

void vvctre_settings_set_enable_vsync(bool value) {
    Settings::values.enable_vsync = value;
}

bool vvctre_settings_get_enable_vsync() {
    return Settings::values.enable_vsync;
}

void vvctre_settings_set_dump_textures(bool value) {
    Settings::values.dump_textures = value;
}

bool vvctre_settings_get_dump_textures() {
    return Settings::values.dump_textures;
}

void vvctre_settings_set_custom_textures(bool value) {
    Settings::values.custom_textures = value;
}

bool vvctre_settings_get_custom_textures() {
    return Settings::values.custom_textures;
}

void vvctre_settings_set_preload_textures(bool value) {
    Settings::values.preload_textures = value;
}

bool vvctre_settings_get_preload_textures() {
    return Settings::values.preload_textures;
}

void vvctre_settings_set_enable_linear_filtering(bool value) {
    Settings::values.enable_linear_filtering = value;
}

bool vvctre_settings_get_enable_linear_filtering() {
    return Settings::values.enable_linear_filtering;
}

void vvctre_settings_set_sharper_distant_objects(bool value) {
    Settings::values.sharper_distant_objects = value;
}

bool vvctre_settings_get_sharper_distant_objects() {
    return Settings::values.sharper_distant_objects;
}

void vvctre_settings_set_resolution(u16 value) {
    Settings::values.resolution = value;
}

u16 vvctre_settings_get_resolution() {
    return Settings::values.resolution;
}

void vvctre_settings_set_background_color_red(float value) {
    Settings::values.background_color_red = value;
}

float vvctre_settings_get_background_color_red() {
    return Settings::values.background_color_red;
}

void vvctre_settings_set_background_color_green(float value) {
    Settings::values.background_color_green = value;
}

float vvctre_settings_get_background_color_green() {
    return Settings::values.background_color_green;
}

void vvctre_settings_set_background_color_blue(float value) {
    Settings::values.background_color_blue = value;
}

float vvctre_settings_get_background_color_blue() {
    return Settings::values.background_color_blue;
}

void vvctre_settings_set_post_processing_shader(const char* value) {
    Settings::values.post_processing_shader = std::string(value);
}

const char* vvctre_settings_get_post_processing_shader() {
    return Settings::values.post_processing_shader.c_str();
}

void vvctre_settings_set_texture_filter(const char* value) {
    Settings::values.texture_filter = std::string(value);
}

const char* vvctre_settings_get_texture_filter() {
    return Settings::values.texture_filter.c_str();
}

void vvctre_settings_set_render_3d(int value) {
    Settings::values.render_3d = static_cast<Settings::StereoRenderOption>(value);
}

int vvctre_settings_get_render_3d() {
    return static_cast<int>(Settings::values.render_3d);
}

void vvctre_settings_set_factor_3d(u8 value) {
    Settings::values.factor_3d = value;
}

u8 vvctre_settings_get_factor_3d() {
    return Settings::values.factor_3d.load();
}

// Controls Settings
void vvctre_settings_set_button(int index, const char* params) {
    Settings::values.buttons[index] = std::string(params);
}

const char* vvctre_settings_get_button(int index) {
    return Settings::values.buttons[index].c_str();
}

void vvctre_settings_set_analog(int index, const char* params) {
    Settings::values.analogs[index] = std::string(params);
}

const char* vvctre_settings_get_analog(int index) {
    return Settings::values.analogs[index].c_str();
}

void vvctre_settings_set_motion_device(const char* params) {
    Settings::values.motion_device = std::string(params);
}

const char* vvctre_settings_get_motion_device() {
    return Settings::values.motion_device.c_str();
}

void vvctre_settings_set_touch_device(const char* params) {
    Settings::values.touch_device = std::string(params);
}

const char* vvctre_settings_get_touch_device() {
    return Settings::values.touch_device.c_str();
}

void vvctre_settings_set_cemuhookudp_address(const char* value) {
    Settings::values.cemuhookudp_address = std::string(value);
}

const char* vvctre_settings_get_cemuhookudp_address() {
    return Settings::values.cemuhookudp_address.c_str();
}

void vvctre_settings_set_cemuhookudp_port(u16 value) {
    Settings::values.cemuhookudp_port = value;
}

u16 vvctre_settings_get_cemuhookudp_port() {
    return Settings::values.cemuhookudp_port;
}

void vvctre_settings_set_cemuhookudp_pad_index(u8 value) {
    Settings::values.cemuhookudp_pad_index = value;
}

u8 vvctre_settings_get_cemuhookudp_pad_index() {
    return Settings::values.cemuhookudp_pad_index;
}

// Layout Settings
void vvctre_settings_set_layout(int value) {
    Settings::values.layout = static_cast<Settings::Layout>(value);
}

int vvctre_settings_get_layout() {
    return static_cast<int>(Settings::values.layout);
}

void vvctre_settings_set_swap_screens(bool value) {
    Settings::values.swap_screens = value;
}

bool vvctre_settings_get_swap_screens() {
    return Settings::values.swap_screens;
}

void vvctre_settings_set_upright_screens(bool value) {
    Settings::values.upright_screens = value;
}

bool vvctre_settings_get_upright_screens() {
    return Settings::values.upright_screens;
}

void vvctre_settings_set_use_custom_layout(bool value) {
    Settings::values.use_custom_layout = value;
}

bool vvctre_settings_get_use_custom_layout() {
    return Settings::values.use_custom_layout;
}

void vvctre_settings_set_custom_layout_top_left(u16 value) {
    Settings::values.custom_layout_top_left = value;
}

u16 vvctre_settings_get_custom_layout_top_left() {
    return Settings::values.custom_layout_top_left;
}

void vvctre_settings_set_custom_layout_top_top(u16 value) {
    Settings::values.custom_layout_top_top = value;
}

u16 vvctre_settings_get_custom_layout_top_top() {
    return Settings::values.custom_layout_top_top;
}

void vvctre_settings_set_custom_layout_top_right(u16 value) {
    Settings::values.custom_layout_top_right = value;
}

u16 vvctre_settings_get_custom_layout_top_right() {
    return Settings::values.custom_layout_top_right;
}

void vvctre_settings_set_custom_layout_top_bottom(u16 value) {
    Settings::values.custom_layout_top_bottom = value;
}

u16 vvctre_settings_get_custom_layout_top_bottom() {
    return Settings::values.custom_layout_top_bottom;
}

void vvctre_settings_set_custom_layout_bottom_left(u16 value) {
    Settings::values.custom_layout_bottom_left = value;
}

u16 vvctre_settings_get_custom_layout_bottom_left() {
    return Settings::values.custom_layout_bottom_left;
}

void vvctre_settings_set_custom_layout_bottom_top(u16 value) {
    Settings::values.custom_layout_bottom_top = value;
}

u16 vvctre_settings_get_custom_layout_bottom_top() {
    return Settings::values.custom_layout_bottom_top;
}

void vvctre_settings_set_custom_layout_bottom_right(u16 value) {
    Settings::values.custom_layout_bottom_right = value;
}

u16 vvctre_settings_get_custom_layout_bottom_right() {
    return Settings::values.custom_layout_bottom_right;
}

void vvctre_settings_set_custom_layout_bottom_bottom(u16 value) {
    Settings::values.custom_layout_bottom_bottom = value;
}

u16 vvctre_settings_get_custom_layout_bottom_bottom() {
    return Settings::values.custom_layout_bottom_bottom;
}

u32 vvctre_settings_get_layout_width() {
    return VideoCore::g_renderer->GetRenderWindow().GetFramebufferLayout().width;
}

u32 vvctre_settings_get_layout_height() {
    return VideoCore::g_renderer->GetRenderWindow().GetFramebufferLayout().height;
}

// Modules
void vvctre_settings_set_use_lle_module(const char* name, bool value) {
    Settings::values.lle_modules[std::string(name)] = value;
}

bool vvctre_settings_get_use_lle_module(const char* name) {
    return Settings::values.lle_modules[std::string(name)];
}

void* vvctre_get_cfg_module(void* core, void* plugin_manager) {
    PluginManager* pm = static_cast<PluginManager*>(plugin_manager);
    if (pm->cfg == nullptr) {
        return Service::CFG::GetModule(*static_cast<Core::System*>(core)).get();
    } else {
        return pm->cfg;
    }
}

// Hacks Settings
void vvctre_settings_set_enable_priority_boost(bool value) {
    Settings::values.enable_priority_boost = value;
}

bool vvctre_settings_get_enable_priority_boost() {
    return Settings::values.enable_priority_boost;
}

// Multiplayer

void vvctre_settings_set_multiplayer_ip(const char* value) {
    Settings::values.multiplayer_ip = std::string(value);
}

const char* vvctre_settings_get_multiplayer_ip() {
    return Settings::values.multiplayer_ip.c_str();
}

void vvctre_settings_set_multiplayer_port(u16 value) {
    Settings::values.multiplayer_port = value;
}

u16 vvctre_settings_get_multiplayer_port() {
    return Settings::values.multiplayer_port;
}

void vvctre_settings_set_nickname(const char* value) {
    Settings::values.multiplayer_nickname = std::string(value);
}

const char* vvctre_settings_get_nickname() {
    return Settings::values.multiplayer_nickname.c_str();
}

void vvctre_settings_set_multiplayer_password(const char* value) {
    Settings::values.multiplayer_password = std::string(value);
}

const char* vvctre_settings_get_multiplayer_password() {
    return Settings::values.multiplayer_password.c_str();
}

void vvctre_multiplayer_join(void* core) {
    Core::System* system = static_cast<Core::System*>(core);

    system->RoomMember().Join(
        Settings::values.multiplayer_nickname, Service::CFG::GetConsoleIdHash(*system),
        Settings::values.multiplayer_ip.c_str(), Settings::values.multiplayer_port,
        Network::NO_PREFERRED_MAC_ADDRESS, Settings::values.multiplayer_password);
}

void vvctre_multiplayer_leave(void* core) {
    static_cast<Core::System*>(core)->RoomMember().Leave();
}

u8 vvctre_multiplayer_get_state(void* core) {
    return static_cast<u8>(static_cast<Core::System*>(core)->RoomMember().GetState());
}

void vvctre_multiplayer_send_message(void* core, const char* message) {
    static_cast<Core::System*>(core)->RoomMember().SendChatMessage(std::string(message));
}

void vvctre_multiplayer_set_game(void* core, const char* name, u64 id) {
    static_cast<Core::System*>(core)->RoomMember().SendGameInfo(Network::GameInfo{name, id});
}

const char* vvctre_multiplayer_get_nickname(void* core) {
    return static_cast<Core::System*>(core)->RoomMember().GetNickname().c_str();
}

u8 vvctre_multiplayer_get_member_count(void* core) {
    return static_cast<u8>(
        static_cast<Core::System*>(core)->RoomMember().GetMemberInformation().size());
}

const char* vvctre_multiplayer_get_member_nickname(void* core, std::size_t index) {
    return static_cast<Core::System*>(core)
        ->RoomMember()
        .GetMemberInformation()[index]
        .nickname.c_str();
}

u64 vvctre_multiplayer_get_member_game_id(void* core, std::size_t index) {
    return static_cast<Core::System*>(core)
        ->RoomMember()
        .GetMemberInformation()[index]
        .game_info.id;
}

const char* vvctre_multiplayer_get_member_game_name(void* core, std::size_t index) {
    return static_cast<Core::System*>(core)
        ->RoomMember()
        .GetMemberInformation()[index]
        .game_info.name.c_str();
}

void vvctre_multiplayer_get_member_mac_address(void* core, std::size_t index, u8* mac_address) {
    std::memcpy(mac_address,
                static_cast<Core::System*>(core)
                    ->RoomMember()
                    .GetMemberInformation()[index]
                    .mac_address.data(),
                sizeof(Network::MacAddress));
}

const char* vvctre_multiplayer_get_room_name(void* core) {
    return static_cast<Core::System*>(core)->RoomMember().GetRoomInformation().name.c_str();
}

const char* vvctre_multiplayer_get_room_description(void* core) {
    return static_cast<Core::System*>(core)->RoomMember().GetRoomInformation().description.c_str();
}

u8 vvctre_multiplayer_get_room_member_slots(void* core) {
    return static_cast<u8>(
        static_cast<Core::System*>(core)->RoomMember().GetRoomInformation().member_slots);
}

void vvctre_multiplayer_on_chat_message(void* core, void (*callback)(const char* nickname,
                                                                     const char* message)) {
    static_cast<Core::System*>(core)->RoomMember().BindOnChatMessageReceived(
        [=](const Network::ChatEntry& entry) {
            callback(entry.nickname.c_str(), entry.message.c_str());
        });
}

void vvctre_multiplayer_on_status_message(void* core,
                                          void (*callback)(u8 type, const char* nickname)) {
    static_cast<Core::System*>(core)->RoomMember().BindOnStatusMessageReceived(
        [=](const Network::StatusMessageEntry& entry) {
            callback(static_cast<u8>(entry.type), entry.nickname.c_str());
        });
}

void vvctre_multiplayer_on_error(void* core, void (*callback)(u8 error)) {
    static_cast<Core::System*>(core)->RoomMember().BindOnError(
        [=](const Network::RoomMember::Error& error) { callback(static_cast<u8>(error)); });
}

void vvctre_multiplayer_on_information_change(void* core, void (*callback)()) {
    static_cast<Core::System*>(core)->RoomMember().BindOnRoomInformationChanged(
        [=](const Network::RoomInformation&) { callback(); });
}

void vvctre_multiplayer_on_state_change(void* core, void (*callback)()) {
    static_cast<Core::System*>(core)->RoomMember().BindOnRoomInformationChanged(
        [=](const Network::RoomInformation&) { callback(); });
}

void vvctre_multiplayer_create_room(const char* ip, u16 port, u32 member_slots) {
    new Network::Room(ip, port, member_slots);
}

// CoreTiming
void* vvctre_coretiming_register_event(void* core, const char* name,
                                       void (*callback)(std::uintptr_t user_data,
                                                        int cycles_late)) {
    return static_cast<Core::System*>(core)->CoreTiming().RegisterEvent(
        std::string(name), [callback](std::uintptr_t user_data, int cycles_late) {
            callback(user_data, cycles_late);
        });
}

void vvctre_coretiming_remove_event(void* core, const void* event) {
    static_cast<Core::System*>(core)->CoreTiming().RemoveEvent(
        static_cast<const Core::TimingEventType*>(event));
}

void vvctre_coretiming_remove_normal_and_threadsafe_event(void* core, const void* event) {
    static_cast<Core::System*>(core)->CoreTiming().RemoveNormalAndThreadsafeEvent(
        static_cast<const Core::TimingEventType*>(event));
}

void vvctre_coretiming_schedule_event(void* core, s64 cycles_into_future, const void* event,
                                      std::uintptr_t user_data) {
    static_cast<Core::System*>(core)->CoreTiming().ScheduleEvent(
        cycles_into_future, static_cast<const Core::TimingEventType*>(event), user_data);
}

void vvctre_coretiming_schedule_event_threadsafe(void* core, s64 cycles_into_future,
                                                 const void* event, std::uintptr_t user_data) {
    static_cast<Core::System*>(core)->CoreTiming().ScheduleEventThreadsafe(
        cycles_into_future, static_cast<const Core::TimingEventType*>(event), user_data);
}

void vvctre_coretiming_unschedule(void* core, const void* event, std::uintptr_t user_data) {
    static_cast<Core::System*>(core)->CoreTiming().UnscheduleEvent(
        static_cast<const Core::TimingEventType*>(event), user_data);
}

u64 vvctre_coretiming_get_ticks(void* core) {
    return static_cast<Core::System*>(core)->CoreTiming().GetTicks();
}

u64 vvctre_coretiming_get_idle_ticks(void* core) {
    return static_cast<Core::System*>(core)->CoreTiming().GetIdleTicks();
}

void vvctre_coretiming_add_ticks(void* core, u64 ticks) {
    static_cast<Core::System*>(core)->CoreTiming().AddTicks(ticks);
}

void vvctre_coretiming_advance(void* core) {
    static_cast<Core::System*>(core)->CoreTiming().Advance();
}

void vvctre_coretiming_move_events(void* core) {
    static_cast<Core::System*>(core)->CoreTiming().MoveEvents();
}

void vvctre_coretiming_idle(void* core) {
    static_cast<Core::System*>(core)->CoreTiming().Idle();
}

void vvctre_coretiming_force_exception_check(void* core, s64 cycles) {
    static_cast<Core::System*>(core)->CoreTiming().ForceExceptionCheck(cycles);
}

s64 vvctre_coretiming_get_global_time_us(void* core) {
    return static_cast<Core::System*>(core)->CoreTiming().GetGlobalTimeUs().count();
}

s64 vvctre_coretiming_get_downcount(void* core) {
    return static_cast<Core::System*>(core)->CoreTiming().GetDowncount();
}

// Other
const char* vvctre_get_version() {
    return VVCTRE_STRDUP(
        fmt::format("{}.{}.{}", vvctre_version_major, vvctre_version_minor, vvctre_version_patch)
            .c_str());
}

u8 vvctre_get_version_major() {
    return vvctre_version_major;
}

u8 vvctre_get_version_minor() {
    return vvctre_version_minor;
}

u8 vvctre_get_version_patch() {
    return vvctre_version_patch;
}

void vvctre_log_trace(const char* line) {
    LOG_TRACE(Plugins, "{}", line);
}

void vvctre_log_debug(const char* line) {
    LOG_DEBUG(Plugins, "{}", line);
}

void vvctre_log_info(const char* line) {
    LOG_INFO(Plugins, "{}", line);
}

void vvctre_log_warning(const char* line) {
    LOG_WARNING(Plugins, "{}", line);
}

void vvctre_log_error(const char* line) {
    LOG_ERROR(Plugins, "{}", line);
}

void vvctre_log_critical(const char* line) {
    LOG_CRITICAL(Plugins, "{}", line);
}

void vvctre_swap_buffers() {
    VideoCore::g_renderer->SwapBuffers();
}

void* vvctre_get_opengl_function(const char* name) {
    return SDL_GL_GetProcAddress(name);
}

float vvctre_get_fps() {
    return ImGui::GetIO().Framerate;
}

float vvctre_get_frametime() {
    return ImGui::GetIO().DeltaTime;
}

int vvctre_get_frame_count() {
    return ImGui::GetFrameCount();
}

void vvctre_get_fatal_error(void* out) {
    std::memcpy(out, &Service::ERR::errinfo, sizeof(Service::ERR::ErrInfo));
}

void vvctre_set_show_fatal_error_messages(void* plugin_manager, bool show) {
    static_cast<PluginManager*>(plugin_manager)->show_fatal_error_messages = show;
}

bool vvctre_get_show_fatal_error_messages(void* plugin_manager) {
    return static_cast<PluginManager*>(plugin_manager)->show_fatal_error_messages;
}

std::unordered_map<std::string, void*> PluginManager::function_map = {
    // File
    {"vvctre_load_file", (void*)&vvctre_load_file},
    {"vvctre_install_cia", (void*)&vvctre_install_cia},
    {"vvctre_load_amiibo", (void*)&vvctre_load_amiibo},
    {"vvctre_load_amiibo_from_memory", (void*)&vvctre_load_amiibo_from_memory},
    {"vvctre_load_amiibo_decrypted", (void*)&vvctre_load_amiibo_decrypted},
    {"vvctre_load_amiibo_from_memory_decrypted", (void*)&vvctre_load_amiibo_from_memory_decrypted},
    {"vvctre_get_amiibo_data", (void*)&vvctre_get_amiibo_data},
    {"vvctre_remove_amiibo", (void*)&vvctre_remove_amiibo},
    {"vvctre_get_program_id", (void*)&vvctre_get_program_id},
    {"vvctre_get_process_name", (void*)&vvctre_get_process_name},
    // Emulation
    {"vvctre_restart", (void*)&vvctre_restart},
    {"vvctre_set_paused", (void*)&vvctre_set_paused},
    {"vvctre_get_paused", (void*)&vvctre_get_paused},
    {"vvctre_emulation_running", (void*)&vvctre_emulation_running},
    // Memory
    {"vvctre_read_u8", (void*)&vvctre_read_u8},
    {"vvctre_write_u8", (void*)&vvctre_write_u8},
    {"vvctre_read_u16", (void*)&vvctre_read_u16},
    {"vvctre_write_u16", (void*)&vvctre_write_u16},
    {"vvctre_read_u32", (void*)&vvctre_read_u32},
    {"vvctre_write_u32", (void*)&vvctre_write_u32},
    {"vvctre_read_u64", (void*)&vvctre_read_u64},
    {"vvctre_write_u64", (void*)&vvctre_write_u64},
    {"vvctre_invalidate_cache_range", (void*)&vvctre_invalidate_cache_range},
    // Debugging
    {"vvctre_set_pc", (void*)&vvctre_set_pc},
    {"vvctre_get_pc", (void*)&vvctre_get_pc},
    {"vvctre_set_register", (void*)&vvctre_set_register},
    {"vvctre_get_register", (void*)&vvctre_get_register},
    {"vvctre_set_vfp_register", (void*)&vvctre_set_vfp_register},
    {"vvctre_get_vfp_register", (void*)&vvctre_get_vfp_register},
    {"vvctre_set_vfp_system_register", (void*)&vvctre_set_vfp_system_register},
    {"vvctre_get_vfp_system_register", (void*)&vvctre_get_vfp_system_register},
    {"vvctre_set_cp15_register", (void*)&vvctre_set_cp15_register},
    {"vvctre_get_cp15_register", (void*)&vvctre_get_cp15_register},
    {"vvctre_ipc_recorder_set_enabled", (void*)&vvctre_ipc_recorder_set_enabled},
    {"vvctre_ipc_recorder_get_enabled", (void*)&vvctre_ipc_recorder_get_enabled},
    {"vvctre_ipc_recorder_bind_callback", (void*)&vvctre_ipc_recorder_bind_callback},
    {"vvctre_get_service_name_by_port_id", (void*)&vvctre_get_service_name_by_port_id},
    // Cheats
    {"vvctre_cheat_count", (void*)&vvctre_cheat_count},
    {"vvctre_get_cheat", (void*)&vvctre_get_cheat},
    {"vvctre_get_cheat_name", (void*)&vvctre_get_cheat_name},
    {"vvctre_get_cheat_comments", (void*)&vvctre_get_cheat_comments},
    {"vvctre_get_cheat_type", (void*)&vvctre_get_cheat_type},
    {"vvctre_get_cheat_code", (void*)&vvctre_get_cheat_code},
    {"vvctre_set_cheat_enabled", (void*)&vvctre_set_cheat_enabled},
    {"vvctre_get_cheat_enabled", (void*)&vvctre_get_cheat_enabled},
    {"vvctre_add_gateway_cheat", (void*)&vvctre_add_gateway_cheat},
    {"vvctre_remove_cheat", (void*)&vvctre_remove_cheat},
    {"vvctre_update_gateway_cheat", (void*)&vvctre_update_gateway_cheat},
    {"vvctre_load_cheats_from_file", (void*)&vvctre_load_cheats_from_file},
    {"vvctre_save_cheats_to_file", (void*)&vvctre_save_cheats_to_file},
    // Camera
    {"vvctre_reload_camera_images", (void*)&vvctre_reload_camera_images},
    // GUI
    {"vvctre_gui_push_item_width", (void*)&vvctre_gui_push_item_width},
    {"vvctre_gui_pop_item_width", (void*)&vvctre_gui_pop_item_width},
    {"vvctre_gui_get_content_region_max", (void*)&vvctre_gui_get_content_region_max},
    {"vvctre_gui_get_content_region_avail", (void*)&vvctre_gui_get_content_region_avail},
    {"vvctre_gui_get_window_content_region_min", (void*)&vvctre_gui_get_window_content_region_min},
    {"vvctre_gui_get_window_content_region_max", (void*)&vvctre_gui_get_window_content_region_max},
    {"vvctre_gui_get_window_content_region_width",
     (void*)&vvctre_gui_get_window_content_region_width},
    {"vvctre_gui_get_scroll_x", (void*)&vvctre_gui_get_scroll_x},
    {"vvctre_gui_get_scroll_y", (void*)&vvctre_gui_get_scroll_y},
    {"vvctre_gui_get_scroll_max_x", (void*)&vvctre_gui_get_scroll_max_x},
    {"vvctre_gui_get_scroll_max_y", (void*)&vvctre_gui_get_scroll_max_y},
    {"vvctre_gui_set_scroll_x", (void*)&vvctre_gui_set_scroll_x},
    {"vvctre_gui_set_scroll_y", (void*)&vvctre_gui_set_scroll_y},
    {"vvctre_gui_set_scroll_here_x", (void*)&vvctre_gui_set_scroll_here_x},
    {"vvctre_gui_set_scroll_here_y", (void*)&vvctre_gui_set_scroll_here_y},
    {"vvctre_gui_set_scroll_from_pos_x", (void*)&vvctre_gui_set_scroll_from_pos_x},
    {"vvctre_gui_set_scroll_from_pos_y", (void*)&vvctre_gui_set_scroll_from_pos_y},
    {"vvctre_gui_set_next_item_width", (void*)&vvctre_gui_set_next_item_width},
    {"vvctre_gui_calc_item_width", (void*)&vvctre_gui_calc_item_width},
    {"vvctre_gui_push_text_wrap_pos", (void*)&vvctre_gui_push_text_wrap_pos},
    {"vvctre_gui_pop_text_wrap_pos", (void*)&vvctre_gui_pop_text_wrap_pos},
    {"vvctre_gui_push_allow_keyboard_focus", (void*)&vvctre_gui_push_allow_keyboard_focus},
    {"vvctre_gui_pop_allow_keyboard_focus", (void*)&vvctre_gui_pop_allow_keyboard_focus},
    {"vvctre_gui_push_font", (void*)&vvctre_gui_push_font},
    {"vvctre_gui_pop_font", (void*)&vvctre_gui_pop_font},
    {"vvctre_gui_push_button_repeat", (void*)&vvctre_gui_push_button_repeat},
    {"vvctre_gui_pop_button_repeat", (void*)&vvctre_gui_pop_button_repeat},
    {"vvctre_gui_push_style_color", (void*)&vvctre_gui_push_style_color},
    {"vvctre_gui_pop_style_color", (void*)&vvctre_gui_pop_style_color},
    {"vvctre_gui_push_style_var_float", (void*)&vvctre_gui_push_style_var_float},
    {"vvctre_gui_push_style_var_2floats", (void*)&vvctre_gui_push_style_var_2floats},
    {"vvctre_gui_pop_style_var", (void*)&vvctre_gui_pop_style_var},
    {"vvctre_gui_same_line", (void*)&vvctre_gui_same_line},
    {"vvctre_gui_new_line", (void*)&vvctre_gui_new_line},
    {"vvctre_gui_bullet", (void*)&vvctre_gui_bullet},
    {"vvctre_gui_indent", (void*)&vvctre_gui_indent},
    {"vvctre_gui_unindent", (void*)&vvctre_gui_unindent},
    {"vvctre_gui_begin_group", (void*)&vvctre_gui_begin_group},
    {"vvctre_gui_end_group", (void*)&vvctre_gui_end_group},
    {"vvctre_gui_get_cursor_pos", (void*)&vvctre_gui_get_cursor_pos},
    {"vvctre_gui_get_cursor_pos_x", (void*)&vvctre_gui_get_cursor_pos_x},
    {"vvctre_gui_get_cursor_pos_y", (void*)&vvctre_gui_get_cursor_pos_y},
    {"vvctre_gui_set_cursor_pos", (void*)&vvctre_gui_set_cursor_pos},
    {"vvctre_gui_set_cursor_pos_x", (void*)&vvctre_gui_set_cursor_pos_x},
    {"vvctre_gui_set_cursor_pos_y", (void*)&vvctre_gui_set_cursor_pos_y},
    {"vvctre_gui_get_cursor_start_pos", (void*)&vvctre_gui_get_cursor_start_pos},
    {"vvctre_gui_get_cursor_screen_pos", (void*)&vvctre_gui_get_cursor_start_pos},
    {"vvctre_gui_set_cursor_screen_pos", (void*)&vvctre_gui_set_cursor_screen_pos},
    {"vvctre_gui_align_text_to_frame_padding", (void*)&vvctre_gui_align_text_to_frame_padding},
    {"vvctre_gui_get_text_line_height", (void*)&vvctre_gui_get_text_line_height},
    {"vvctre_gui_get_text_line_height_with_spacing",
     (void*)&vvctre_gui_get_text_line_height_with_spacing},
    {"vvctre_gui_get_frame_height", (void*)&vvctre_gui_get_frame_height},
    {"vvctre_gui_get_frame_height_with_spacing", (void*)&vvctre_gui_get_frame_height_with_spacing},
    {"vvctre_gui_push_id_string", (void*)&vvctre_gui_push_id_string},
    {"vvctre_gui_push_id_string_with_begin_and_end",
     (void*)&vvctre_gui_push_id_string_with_begin_and_end},
    {"vvctre_gui_push_id_void", (void*)&vvctre_gui_push_id_void},
    {"vvctre_gui_push_id_int", (void*)&vvctre_gui_push_id_int},
    {"vvctre_gui_pop_id", (void*)&vvctre_gui_pop_id},
    {"vvctre_gui_spacing", (void*)&vvctre_gui_spacing},
    {"vvctre_gui_separator", (void*)&vvctre_gui_separator},
    {"vvctre_gui_dummy", (void*)&vvctre_gui_dummy},
    {"vvctre_gui_tooltip", (void*)&vvctre_gui_tooltip},
    {"vvctre_gui_begin_tooltip", (void*)&vvctre_gui_begin_tooltip},
    {"vvctre_gui_is_item_hovered", (void*)&vvctre_gui_is_item_hovered},
    {"vvctre_gui_is_item_focused", (void*)&vvctre_gui_is_item_focused},
    {"vvctre_gui_is_item_clicked", (void*)&vvctre_gui_is_item_clicked},
    {"vvctre_gui_is_item_visible", (void*)&vvctre_gui_is_item_visible},
    {"vvctre_gui_is_item_edited", (void*)&vvctre_gui_is_item_edited},
    {"vvctre_gui_is_item_activated", (void*)&vvctre_gui_is_item_activated},
    {"vvctre_gui_is_item_deactivated", (void*)&vvctre_gui_is_item_deactivated},
    {"vvctre_gui_is_item_deactivated_after_edit",
     (void*)&vvctre_gui_is_item_deactivated_after_edit},
    {"vvctre_gui_is_item_toggled_open", (void*)&vvctre_gui_is_item_toggled_open},
    {"vvctre_gui_is_any_item_hovered", (void*)&vvctre_gui_is_any_item_hovered},
    {"vvctre_gui_is_any_item_active", (void*)&vvctre_gui_is_any_item_active},
    {"vvctre_gui_is_any_item_focused", (void*)&vvctre_gui_is_any_item_focused},
    {"vvctre_gui_get_item_rect_min", (void*)&vvctre_gui_get_item_rect_min},
    {"vvctre_gui_get_item_rect_max", (void*)&vvctre_gui_get_item_rect_max},
    {"vvctre_gui_get_item_rect_size", (void*)&vvctre_gui_get_item_rect_size},
    {"vvctre_gui_set_item_allow_overlap", (void*)&vvctre_gui_set_item_allow_overlap},
    {"vvctre_gui_end_tooltip", (void*)&vvctre_gui_end_tooltip},
    {"vvctre_gui_text", (void*)&vvctre_gui_text},
    {"vvctre_gui_text_ex", (void*)&vvctre_gui_text_ex},
    {"vvctre_gui_text_colored", (void*)&vvctre_gui_text_colored},
    {"vvctre_gui_button", (void*)&vvctre_gui_button},
    {"vvctre_gui_small_button", (void*)&vvctre_gui_small_button},
    {"vvctre_gui_color_button", (void*)&vvctre_gui_color_button},
    {"vvctre_gui_color_button_ex", (void*)&vvctre_gui_color_button_ex},
    {"vvctre_gui_invisible_button", (void*)&vvctre_gui_invisible_button},
    {"vvctre_gui_radio_button", (void*)&vvctre_gui_radio_button},
    {"vvctre_gui_image_button", (void*)&vvctre_gui_image_button},
    {"vvctre_gui_checkbox", (void*)&vvctre_gui_checkbox},
    {"vvctre_gui_begin", (void*)&vvctre_gui_begin},
    {"vvctre_gui_begin_overlay", (void*)&vvctre_gui_begin_overlay},
    {"vvctre_gui_begin_auto_resize", (void*)&vvctre_gui_begin_auto_resize},
    {"vvctre_gui_begin_child", (void*)&vvctre_gui_begin_child},
    {"vvctre_gui_begin_child_frame", (void*)&vvctre_gui_begin_child_frame},
    {"vvctre_gui_begin_popup", (void*)&vvctre_gui_begin_popup},
    {"vvctre_gui_begin_popup_modal", (void*)&vvctre_gui_begin_popup_modal},
    {"vvctre_gui_begin_popup_context_item", (void*)&vvctre_gui_begin_popup_context_item},
    {"vvctre_gui_begin_popup_context_window", (void*)&vvctre_gui_begin_popup_context_window},
    {"vvctre_gui_begin_popup_context_void", (void*)&vvctre_gui_begin_popup_context_void},
    {"vvctre_gui_begin_ex", (void*)&vvctre_gui_begin_ex},
    {"vvctre_gui_end", (void*)&vvctre_gui_end},
    {"vvctre_gui_end_child", (void*)&vvctre_gui_end_child},
    {"vvctre_gui_end_child_frame", (void*)&vvctre_gui_end_child_frame},
    {"vvctre_gui_end_popup", (void*)&vvctre_gui_end_popup},
    {"vvctre_gui_open_popup", (void*)&vvctre_gui_open_popup},
    {"vvctre_gui_open_popup_on_item_click", (void*)&vvctre_gui_open_popup_on_item_click},
    {"vvctre_gui_close_current_popup", (void*)&vvctre_gui_close_current_popup},
    {"vvctre_gui_is_popup_open", (void*)&vvctre_gui_is_popup_open},
    {"vvctre_gui_begin_menu", (void*)&vvctre_gui_begin_menu},
    {"vvctre_gui_end_menu", (void*)&vvctre_gui_end_menu},
    {"vvctre_gui_begin_tab", (void*)&vvctre_gui_begin_tab},
    {"vvctre_gui_begin_tab_ex", (void*)&vvctre_gui_begin_tab_ex},
    {"vvctre_gui_begin_tab_bar", (void*)&vvctre_gui_begin_tab_bar},
    {"vvctre_gui_end_tab_bar", (void*)&vvctre_gui_end_tab_bar},
    {"vvctre_gui_set_tab_closed", (void*)&vvctre_gui_set_tab_closed},
    {"vvctre_gui_end_tab", (void*)&vvctre_gui_end_tab},
    {"vvctre_gui_menu_item", (void*)&vvctre_gui_menu_item},
    {"vvctre_gui_menu_item_with_check_mark", (void*)&vvctre_gui_menu_item_with_check_mark},
    {"vvctre_gui_plot_lines", (void*)&vvctre_gui_plot_lines},
    {"vvctre_gui_plot_lines_getter", (void*)&vvctre_gui_plot_lines_getter},
    {"vvctre_gui_plot_histogram", (void*)&vvctre_gui_plot_histogram},
    {"vvctre_gui_plot_histogram_getter", (void*)&vvctre_gui_plot_histogram_getter},
    {"vvctre_gui_begin_listbox", (void*)&vvctre_gui_begin_listbox},
    {"vvctre_gui_end_listbox", (void*)&vvctre_gui_end_listbox},
    {"vvctre_gui_begin_combo_box", (void*)&vvctre_gui_begin_combo_box},
    {"vvctre_gui_end_combo_box", (void*)&vvctre_gui_end_combo_box},
    {"vvctre_gui_selectable", (void*)&vvctre_gui_selectable},
    {"vvctre_gui_selectable_with_selected", (void*)&vvctre_gui_selectable_with_selected},
    {"vvctre_gui_text_input", (void*)&vvctre_gui_text_input},
    {"vvctre_gui_text_input_multiline", (void*)&vvctre_gui_text_input_multiline},
    {"vvctre_gui_text_input_with_hint", (void*)&vvctre_gui_text_input_with_hint},
    {"vvctre_gui_text_input_ex", (void*)&vvctre_gui_text_input_ex},
    {"vvctre_gui_text_input_multiline_ex", (void*)&vvctre_gui_text_input_multiline_ex},
    {"vvctre_gui_text_input_with_hint_ex", (void*)&vvctre_gui_text_input_with_hint_ex},
    {"vvctre_gui_u8_input", (void*)&vvctre_gui_u8_input},
    {"vvctre_gui_u8_input_ex", (void*)&vvctre_gui_u8_input_ex},
    {"vvctre_gui_u8_inputs", (void*)&vvctre_gui_u8_inputs},
    {"vvctre_gui_u16_input", (void*)&vvctre_gui_u16_input},
    {"vvctre_gui_u16_input_ex", (void*)&vvctre_gui_u16_input_ex},
    {"vvctre_gui_u16_inputs", (void*)&vvctre_gui_u16_inputs},
    {"vvctre_gui_u32_input", (void*)&vvctre_gui_u32_input},
    {"vvctre_gui_u32_input_ex", (void*)&vvctre_gui_u32_input_ex},
    {"vvctre_gui_u32_inputs", (void*)&vvctre_gui_u32_inputs},
    {"vvctre_gui_u64_input", (void*)&vvctre_gui_u64_input},
    {"vvctre_gui_u64_input_ex", (void*)&vvctre_gui_u64_input_ex},
    {"vvctre_gui_u64_inputs", (void*)&vvctre_gui_u64_inputs},
    {"vvctre_gui_s8_input", (void*)&vvctre_gui_s8_input},
    {"vvctre_gui_s8_input_ex", (void*)&vvctre_gui_s8_input_ex},
    {"vvctre_gui_s8_inputs", (void*)&vvctre_gui_s8_inputs},
    {"vvctre_gui_s16_input", (void*)&vvctre_gui_s16_input},
    {"vvctre_gui_s16_input_ex", (void*)&vvctre_gui_s16_input_ex},
    {"vvctre_gui_s16_inputs", (void*)&vvctre_gui_s16_inputs},
    {"vvctre_gui_int_input", (void*)&vvctre_gui_int_input},
    {"vvctre_gui_int_input_ex", (void*)&vvctre_gui_int_input_ex},
    {"vvctre_gui_int_inputs", (void*)&vvctre_gui_int_inputs},
    {"vvctre_gui_s64_input", (void*)&vvctre_gui_s64_input},
    {"vvctre_gui_s64_input_ex", (void*)&vvctre_gui_s64_input_ex},
    {"vvctre_gui_s64_inputs", (void*)&vvctre_gui_s64_inputs},
    {"vvctre_gui_float_input", (void*)&vvctre_gui_float_input},
    {"vvctre_gui_float_input_ex", (void*)&vvctre_gui_float_input_ex},
    {"vvctre_gui_float_inputs", (void*)&vvctre_gui_float_inputs},
    {"vvctre_gui_double_input", (void*)&vvctre_gui_double_input},
    {"vvctre_gui_double_input_ex", (void*)&vvctre_gui_double_input_ex},
    {"vvctre_gui_double_inputs", (void*)&vvctre_gui_double_inputs},
    {"vvctre_gui_color_edit", (void*)&vvctre_gui_color_edit},
    {"vvctre_gui_color_picker", (void*)&vvctre_gui_color_picker},
    {"vvctre_gui_color_picker_ex", (void*)&vvctre_gui_color_picker_ex},
    {"vvctre_gui_progress_bar", (void*)&vvctre_gui_progress_bar},
    {"vvctre_gui_progress_bar_ex", (void*)&vvctre_gui_progress_bar_ex},
    {"vvctre_gui_slider_u8", (void*)&vvctre_gui_slider_u8},
    {"vvctre_gui_slider_u8_ex", (void*)&vvctre_gui_slider_u8_ex},
    {"vvctre_gui_sliders_u8", (void*)&vvctre_gui_sliders_u8},
    {"vvctre_gui_slider_u16", (void*)&vvctre_gui_slider_u16},
    {"vvctre_gui_slider_u16_ex", (void*)&vvctre_gui_slider_u16_ex},
    {"vvctre_gui_sliders_u16", (void*)&vvctre_gui_sliders_u16},
    {"vvctre_gui_slider_u32", (void*)&vvctre_gui_slider_u32},
    {"vvctre_gui_slider_u32_ex", (void*)&vvctre_gui_slider_u32_ex},
    {"vvctre_gui_sliders_u32", (void*)&vvctre_gui_sliders_u32},
    {"vvctre_gui_slider_u64", (void*)&vvctre_gui_slider_u64},
    {"vvctre_gui_slider_u64_ex", (void*)&vvctre_gui_slider_u64_ex},
    {"vvctre_gui_sliders_u64", (void*)&vvctre_gui_sliders_u64},
    {"vvctre_gui_slider_s8", (void*)&vvctre_gui_slider_s8},
    {"vvctre_gui_slider_s8_ex", (void*)&vvctre_gui_slider_s8_ex},
    {"vvctre_gui_sliders_s8", (void*)&vvctre_gui_sliders_s8},
    {"vvctre_gui_slider_s16", (void*)&vvctre_gui_slider_s16},
    {"vvctre_gui_slider_s16_ex", (void*)&vvctre_gui_slider_s16_ex},
    {"vvctre_gui_sliders_s16", (void*)&vvctre_gui_sliders_s16},
    {"vvctre_gui_slider_s32", (void*)&vvctre_gui_slider_s32},
    {"vvctre_gui_slider_s32_ex", (void*)&vvctre_gui_slider_s32_ex},
    {"vvctre_gui_sliders_s32", (void*)&vvctre_gui_sliders_s32},
    {"vvctre_gui_slider_s64", (void*)&vvctre_gui_slider_s64},
    {"vvctre_gui_slider_s64_ex", (void*)&vvctre_gui_slider_s64_ex},
    {"vvctre_gui_sliders_s64", (void*)&vvctre_gui_sliders_s64},
    {"vvctre_gui_slider_float", (void*)&vvctre_gui_slider_float},
    {"vvctre_gui_slider_float_ex", (void*)&vvctre_gui_slider_float_ex},
    {"vvctre_gui_sliders_float", (void*)&vvctre_gui_sliders_float},
    {"vvctre_gui_slider_double", (void*)&vvctre_gui_slider_double},
    {"vvctre_gui_slider_double_ex", (void*)&vvctre_gui_slider_double_ex},
    {"vvctre_gui_sliders_double", (void*)&vvctre_gui_sliders_double},
    {"vvctre_gui_slider_angle", (void*)&vvctre_gui_slider_angle},
    {"vvctre_gui_vertical_slider_u8", (void*)&vvctre_gui_vertical_slider_u8},
    {"vvctre_gui_vertical_slider_u16", (void*)&vvctre_gui_vertical_slider_u16},
    {"vvctre_gui_vertical_slider_u32", (void*)&vvctre_gui_vertical_slider_u32},
    {"vvctre_gui_vertical_slider_u64", (void*)&vvctre_gui_vertical_slider_u64},
    {"vvctre_gui_vertical_slider_s8", (void*)&vvctre_gui_vertical_slider_s8},
    {"vvctre_gui_vertical_slider_s16", (void*)&vvctre_gui_vertical_slider_s16},
    {"vvctre_gui_vertical_slider_s32", (void*)&vvctre_gui_vertical_slider_s32},
    {"vvctre_gui_vertical_slider_s64", (void*)&vvctre_gui_vertical_slider_s64},
    {"vvctre_gui_vertical_slider_float", (void*)&vvctre_gui_vertical_slider_float},
    {"vvctre_gui_vertical_slider_double", (void*)&vvctre_gui_vertical_slider_double},
    {"vvctre_gui_drag_u8", (void*)&vvctre_gui_drag_u8},
    {"vvctre_gui_drags_u8", (void*)&vvctre_gui_drags_u8},
    {"vvctre_gui_drag_u16", (void*)&vvctre_gui_drag_u16},
    {"vvctre_gui_drags_u16", (void*)&vvctre_gui_drags_u16},
    {"vvctre_gui_drag_u32", (void*)&vvctre_gui_drag_u32},
    {"vvctre_gui_drags_u32", (void*)&vvctre_gui_drags_u32},
    {"vvctre_gui_drag_u64", (void*)&vvctre_gui_drag_u64},
    {"vvctre_gui_drags_u64", (void*)&vvctre_gui_drags_u64},
    {"vvctre_gui_drag_s8", (void*)&vvctre_gui_drag_s8},
    {"vvctre_gui_drags_s8", (void*)&vvctre_gui_drags_s8},
    {"vvctre_gui_drag_s16", (void*)&vvctre_gui_drag_s16},
    {"vvctre_gui_drags_s16", (void*)&vvctre_gui_drags_s16},
    {"vvctre_gui_drag_s32", (void*)&vvctre_gui_drag_s32},
    {"vvctre_gui_drags_s32", (void*)&vvctre_gui_drags_s32},
    {"vvctre_gui_drag_s64", (void*)&vvctre_gui_drag_s64},
    {"vvctre_gui_drags_s64", (void*)&vvctre_gui_drags_s64},
    {"vvctre_gui_drag_float", (void*)&vvctre_gui_drag_float},
    {"vvctre_gui_drags_float", (void*)&vvctre_gui_drags_float},
    {"vvctre_gui_drag_double", (void*)&vvctre_gui_drag_double},
    {"vvctre_gui_drags_double", (void*)&vvctre_gui_drags_double},
    {"vvctre_gui_image", (void*)&vvctre_gui_image},
    {"vvctre_gui_columns", (void*)&vvctre_gui_columns},
    {"vvctre_gui_next_column", (void*)&vvctre_gui_next_column},
    {"vvctre_gui_get_column_index", (void*)&vvctre_gui_get_column_index},
    {"vvctre_gui_get_column_width", (void*)&vvctre_gui_get_column_width},
    {"vvctre_gui_set_column_width", (void*)&vvctre_gui_set_column_width},
    {"vvctre_gui_get_column_offset", (void*)&vvctre_gui_get_column_offset},
    {"vvctre_gui_set_column_offset", (void*)&vvctre_gui_set_column_offset},
    {"vvctre_gui_get_columns_count", (void*)&vvctre_gui_get_columns_count},
    {"vvctre_gui_tree_node_string", (void*)&vvctre_gui_tree_node_string},
    {"vvctre_gui_tree_push_string", (void*)&vvctre_gui_tree_push_string},
    {"vvctre_gui_tree_push_void", (void*)&vvctre_gui_tree_push_void},
    {"vvctre_gui_tree_pop", (void*)&vvctre_gui_tree_pop},
    {"vvctre_gui_get_tree_node_to_label_spacing",
     (void*)&vvctre_gui_get_tree_node_to_label_spacing},
    {"vvctre_gui_collapsing_header", (void*)&vvctre_gui_collapsing_header},
    {"vvctre_gui_set_next_item_open", (void*)&vvctre_gui_set_next_item_open},
    {"vvctre_gui_set_color", (void*)&vvctre_gui_set_color},
    {"vvctre_gui_get_color", (void*)&vvctre_gui_get_color},
    {"vvctre_gui_set_font", (void*)&vvctre_gui_set_font},
    {"vvctre_gui_set_font_and_get_pointer", (void*)&vvctre_gui_set_font_and_get_pointer},
    {"vvctre_gui_is_window_appearing", (void*)&vvctre_gui_is_window_appearing},
    {"vvctre_gui_is_window_collapsed", (void*)&vvctre_gui_is_window_collapsed},
    {"vvctre_gui_is_window_focused", (void*)&vvctre_gui_is_window_focused},
    {"vvctre_gui_is_window_hovered", (void*)&vvctre_gui_is_window_hovered},
    {"vvctre_gui_get_window_pos", (void*)&vvctre_gui_get_window_pos},
    {"vvctre_gui_get_window_size", (void*)&vvctre_gui_get_window_size},
    {"vvctre_gui_set_next_window_pos", (void*)&vvctre_gui_set_next_window_pos},
    {"vvctre_gui_set_next_window_size", (void*)&vvctre_gui_set_next_window_size},
    {"vvctre_gui_set_next_window_size_constraints",
     (void*)&vvctre_gui_set_next_window_size_constraints},
    {"vvctre_gui_set_next_window_content_size", (void*)&vvctre_gui_set_next_window_content_size},
    {"vvctre_gui_set_next_window_collapsed", (void*)&vvctre_gui_set_next_window_collapsed},
    {"vvctre_gui_set_next_window_focus", (void*)&vvctre_gui_set_next_window_focus},
    {"vvctre_gui_set_next_window_bg_alpha", (void*)&vvctre_gui_set_next_window_bg_alpha},
    {"vvctre_gui_set_window_pos", (void*)&vvctre_gui_set_window_pos},
    {"vvctre_gui_set_window_size", (void*)&vvctre_gui_set_window_size},
    {"vvctre_gui_set_window_collapsed", (void*)&vvctre_gui_set_window_collapsed},
    {"vvctre_gui_set_window_focus", (void*)&vvctre_gui_set_window_focus},
    {"vvctre_gui_set_window_font_scale", (void*)&vvctre_gui_set_window_font_scale},
    {"vvctre_gui_set_window_pos_named", (void*)&vvctre_gui_set_window_pos_named},
    {"vvctre_gui_set_window_size_named", (void*)&vvctre_gui_set_window_size_named},
    {"vvctre_gui_set_window_collapsed_named", (void*)&vvctre_gui_set_window_collapsed_named},
    {"vvctre_gui_set_window_focus_named", (void*)&vvctre_gui_set_window_focus_named},
    {"vvctre_gui_is_key_down", (void*)&vvctre_gui_is_key_down},
    {"vvctre_gui_is_key_pressed", (void*)&vvctre_gui_is_key_pressed},
    {"vvctre_gui_is_key_released", (void*)&vvctre_gui_is_key_released},
    {"vvctre_gui_get_key_pressed_amount", (void*)&vvctre_gui_get_key_pressed_amount},
    {"vvctre_gui_capture_keyboard_from_app", (void*)&vvctre_gui_capture_keyboard_from_app},
    {"vvctre_gui_is_mouse_down", (void*)&vvctre_gui_is_mouse_down},
    {"vvctre_gui_is_mouse_clicked", (void*)&vvctre_gui_is_mouse_clicked},
    {"vvctre_gui_is_mouse_released", (void*)&vvctre_gui_is_mouse_released},
    {"vvctre_gui_is_mouse_double_clicked", (void*)&vvctre_gui_is_mouse_double_clicked},
    {"vvctre_gui_is_mouse_hovering_rect", (void*)&vvctre_gui_is_mouse_hovering_rect},
    {"vvctre_gui_is_mouse_pos_valid", (void*)&vvctre_gui_is_mouse_pos_valid},
    {"vvctre_gui_is_any_mouse_down", (void*)&vvctre_gui_is_any_mouse_down},
    {"vvctre_gui_get_mouse_pos", (void*)&vvctre_gui_get_mouse_pos},
    {"vvctre_gui_get_mouse_pos_on_opening_current_popup",
     (void*)&vvctre_gui_get_mouse_pos_on_opening_current_popup},
    {"vvctre_gui_is_mouse_dragging", (void*)&vvctre_gui_is_mouse_dragging},
    {"vvctre_gui_get_mouse_drag_delta", (void*)&vvctre_gui_get_mouse_drag_delta},
    {"vvctre_gui_reset_mouse_drag_delta", (void*)&vvctre_gui_reset_mouse_drag_delta},
    {"vvctre_gui_get_mouse_cursor", (void*)&vvctre_gui_get_mouse_cursor},
    {"vvctre_gui_set_mouse_cursor", (void*)&vvctre_gui_set_mouse_cursor},
    {"vvctre_gui_capture_mouse_from_app", (void*)&vvctre_gui_capture_mouse_from_app},
    {"vvctre_gui_style_set_alpha", (void*)&vvctre_gui_style_set_alpha},
    {"vvctre_gui_style_set_window_padding", (void*)&vvctre_gui_style_set_window_padding},
    {"vvctre_gui_style_set_window_rounding", (void*)&vvctre_gui_style_set_window_rounding},
    {"vvctre_gui_style_set_window_border_size", (void*)&vvctre_gui_style_set_window_border_size},
    {"vvctre_gui_style_set_window_min_size", (void*)&vvctre_gui_style_set_window_min_size},
    {"vvctre_gui_style_set_window_title_align", (void*)&vvctre_gui_style_set_window_title_align},
    {"vvctre_gui_style_set_child_rounding", (void*)&vvctre_gui_style_set_child_rounding},
    {"vvctre_gui_style_set_child_border_size", (void*)&vvctre_gui_style_set_child_border_size},
    {"vvctre_gui_style_set_popup_rounding", (void*)&vvctre_gui_style_set_popup_rounding},
    {"vvctre_gui_style_set_popup_border_size", (void*)&vvctre_gui_style_set_popup_border_size},
    {"vvctre_gui_style_set_frame_padding", (void*)&vvctre_gui_style_set_frame_padding},
    {"vvctre_gui_style_set_frame_rounding", (void*)&vvctre_gui_style_set_frame_rounding},
    {"vvctre_gui_style_set_frame_border_size", (void*)&vvctre_gui_style_set_frame_border_size},
    {"vvctre_gui_style_set_item_spacing", (void*)&vvctre_gui_style_set_item_spacing},
    {"vvctre_gui_style_set_item_inner_spacing", (void*)&vvctre_gui_style_set_item_inner_spacing},
    {"vvctre_gui_style_set_indent_spacing", (void*)&vvctre_gui_style_set_indent_spacing},
    {"vvctre_gui_style_set_scrollbar_size", (void*)&vvctre_gui_style_set_scrollbar_size},
    {"vvctre_gui_style_set_scrollbar_rounding", (void*)&vvctre_gui_style_set_scrollbar_rounding},
    {"vvctre_gui_style_set_grab_min_size", (void*)&vvctre_gui_style_set_grab_min_size},
    {"vvctre_gui_style_set_grab_rounding", (void*)&vvctre_gui_style_set_grab_rounding},
    {"vvctre_gui_style_set_tab_rounding", (void*)&vvctre_gui_style_set_tab_rounding},
    {"vvctre_gui_style_set_button_text_align", (void*)&vvctre_gui_style_set_button_text_align},
    {"vvctre_gui_style_set_selectable_text_align",
     (void*)&vvctre_gui_style_set_selectable_text_align},
    {"vvctre_gui_style_get_alpha", (void*)&vvctre_gui_style_get_alpha},
    {"vvctre_gui_style_get_window_padding", (void*)&vvctre_gui_style_get_window_padding},
    {"vvctre_gui_style_get_window_rounding", (void*)&vvctre_gui_style_get_window_rounding},
    {"vvctre_gui_style_get_window_border_size", (void*)&vvctre_gui_style_get_window_border_size},
    {"vvctre_gui_style_get_window_min_size", (void*)&vvctre_gui_style_get_window_min_size},
    {"vvctre_gui_style_get_window_title_align", (void*)&vvctre_gui_style_get_window_title_align},
    {"vvctre_gui_style_get_child_rounding", (void*)&vvctre_gui_style_get_child_rounding},
    {"vvctre_gui_style_get_child_border_size", (void*)&vvctre_gui_style_get_child_border_size},
    {"vvctre_gui_style_get_popup_rounding", (void*)&vvctre_gui_style_get_popup_rounding},
    {"vvctre_gui_style_get_popup_border_size", (void*)&vvctre_gui_style_get_popup_border_size},
    {"vvctre_gui_style_get_frame_padding", (void*)&vvctre_gui_style_get_frame_padding},
    {"vvctre_gui_style_get_frame_rounding", (void*)&vvctre_gui_style_get_frame_rounding},
    {"vvctre_gui_style_get_frame_border_size", (void*)&vvctre_gui_style_get_frame_border_size},
    {"vvctre_gui_style_get_item_spacing", (void*)&vvctre_gui_style_get_item_spacing},
    {"vvctre_gui_style_get_item_inner_spacing", (void*)&vvctre_gui_style_get_item_inner_spacing},
    {"vvctre_gui_style_get_indent_spacing", (void*)&vvctre_gui_style_get_indent_spacing},
    {"vvctre_gui_style_get_scrollbar_size", (void*)&vvctre_gui_style_get_scrollbar_size},
    {"vvctre_gui_style_get_scrollbar_rounding", (void*)&vvctre_gui_style_get_scrollbar_rounding},
    {"vvctre_gui_style_get_grab_min_size", (void*)&vvctre_gui_style_get_grab_min_size},
    {"vvctre_gui_style_get_grab_rounding", (void*)&vvctre_gui_style_get_grab_rounding},
    {"vvctre_gui_style_get_tab_rounding", (void*)&vvctre_gui_style_get_tab_rounding},
    {"vvctre_gui_style_get_button_text_align", (void*)&vvctre_gui_style_get_button_text_align},
    {"vvctre_gui_style_get_selectable_text_align",
     (void*)&vvctre_gui_style_get_selectable_text_align},
    {"vvctre_get_dear_imgui_version", (void*)&vvctre_get_dear_imgui_version},
    // OS window
    {"vvctre_set_os_window_size", (void*)&vvctre_set_os_window_size},
    {"vvctre_get_os_window_size", (void*)&vvctre_get_os_window_size},
    {"vvctre_set_os_window_minimum_size", (void*)&vvctre_set_os_window_minimum_size},
    {"vvctre_get_os_window_minimum_size", (void*)&vvctre_get_os_window_minimum_size},
    {"vvctre_set_os_window_maximum_size", (void*)&vvctre_set_os_window_maximum_size},
    {"vvctre_get_os_window_maximum_size", (void*)&vvctre_get_os_window_maximum_size},
    {"vvctre_set_os_window_position", (void*)&vvctre_set_os_window_position},
    {"vvctre_get_os_window_position", (void*)&vvctre_get_os_window_position},
    {"vvctre_set_os_window_title", (void*)&vvctre_set_os_window_title},
    {"vvctre_get_os_window_title", (void*)&vvctre_get_os_window_title},
    // Button devices
    {"vvctre_button_device_new", (void*)&vvctre_button_device_new},
    {"vvctre_button_device_delete", (void*)&vvctre_button_device_delete},
    {"vvctre_button_device_get_state", (void*)&vvctre_button_device_get_state},
    // TAS
    {"vvctre_movie_prepare_for_playback", (void*)&vvctre_movie_prepare_for_playback},
    {"vvctre_movie_prepare_for_recording", (void*)&vvctre_movie_prepare_for_recording},
    {"vvctre_movie_play", (void*)&vvctre_movie_play},
    {"vvctre_movie_record", (void*)&vvctre_movie_record},
    {"vvctre_movie_is_playing", (void*)&vvctre_movie_is_playing},
    {"vvctre_movie_is_recording", (void*)&vvctre_movie_is_recording},
    {"vvctre_movie_stop", (void*)&vvctre_movie_stop},
    {"vvctre_set_frame_advancing_enabled", (void*)&vvctre_set_frame_advancing_enabled},
    {"vvctre_get_frame_advancing_enabled", (void*)&vvctre_get_frame_advancing_enabled},
    {"vvctre_advance_frame", (void*)&vvctre_advance_frame},
    // Remote control
    {"vvctre_set_custom_pad_state", (void*)&vvctre_set_custom_pad_state},
    {"vvctre_use_real_pad_state", (void*)&vvctre_use_real_pad_state},
    {"vvctre_get_pad_state", (void*)&vvctre_get_pad_state},
    {"vvctre_set_custom_circle_pad_state", (void*)&vvctre_set_custom_circle_pad_state},
    {"vvctre_use_real_circle_pad_state", (void*)&vvctre_use_real_circle_pad_state},
    {"vvctre_get_circle_pad_state", (void*)&vvctre_get_circle_pad_state},
    {"vvctre_set_custom_circle_pad_pro_state", (void*)&vvctre_set_custom_circle_pad_pro_state},
    {"vvctre_use_real_circle_pad_pro_state", (void*)&vvctre_use_real_circle_pad_pro_state},
    {"vvctre_get_circle_pad_pro_state", (void*)&vvctre_get_circle_pad_pro_state},
    {"vvctre_set_custom_touch_state", (void*)&vvctre_set_custom_touch_state},
    {"vvctre_use_real_touch_state", (void*)&vvctre_use_real_touch_state},
    {"vvctre_get_touch_state", (void*)&vvctre_get_touch_state},
    {"vvctre_set_custom_motion_state", (void*)&vvctre_set_custom_motion_state},
    {"vvctre_use_real_motion_state", (void*)&vvctre_use_real_motion_state},
    {"vvctre_get_motion_state", (void*)&vvctre_get_motion_state},
    {"vvctre_screenshot", (void*)&vvctre_screenshot},
    {"vvctre_screenshot_bottom_screen", (void*)&vvctre_screenshot_bottom_screen},
    {"vvctre_screenshot_default_layout", (void*)&vvctre_screenshot_default_layout},
    // Settings
    {"vvctre_settings_apply", (void*)&vvctre_settings_apply},
    // Start Settings
    {"vvctre_settings_set_file_path", (void*)&vvctre_settings_set_file_path},
    {"vvctre_settings_get_file_path", (void*)&vvctre_settings_get_file_path},
    {"vvctre_settings_set_play_movie", (void*)&vvctre_settings_set_play_movie},
    {"vvctre_settings_get_play_movie", (void*)&vvctre_settings_get_play_movie},
    {"vvctre_settings_set_record_movie", (void*)&vvctre_settings_set_record_movie},
    {"vvctre_settings_get_record_movie", (void*)&vvctre_settings_get_record_movie},
    {"vvctre_settings_set_region_value", (void*)&vvctre_settings_set_region_value},
    {"vvctre_settings_get_region_value", (void*)&vvctre_settings_get_region_value},
    {"vvctre_settings_set_log_filter", (void*)&vvctre_settings_set_log_filter},
    {"vvctre_settings_get_log_filter", (void*)&vvctre_settings_get_log_filter},
    {"vvctre_settings_set_initial_clock", (void*)&vvctre_settings_set_initial_clock},
    {"vvctre_settings_get_initial_clock", (void*)&vvctre_settings_get_initial_clock},
    {"vvctre_settings_set_unix_timestamp", (void*)&vvctre_settings_set_unix_timestamp},
    {"vvctre_settings_get_unix_timestamp", (void*)&vvctre_settings_get_unix_timestamp},
    {"vvctre_settings_set_use_virtual_sd", (void*)&vvctre_settings_set_use_virtual_sd},
    {"vvctre_settings_get_use_virtual_sd", (void*)&vvctre_settings_get_use_virtual_sd},
    {"vvctre_settings_set_record_frame_times", (void*)&vvctre_settings_set_record_frame_times},
    {"vvctre_settings_get_record_frame_times", (void*)&vvctre_settings_get_record_frame_times},
    {"vvctre_settings_enable_gdbstub", (void*)&vvctre_settings_enable_gdbstub},
    {"vvctre_settings_disable_gdbstub", (void*)&vvctre_settings_disable_gdbstub},
    {"vvctre_settings_is_gdb_stub_enabled", (void*)&vvctre_settings_is_gdb_stub_enabled},
    {"vvctre_settings_get_gdb_stub_port", (void*)&vvctre_settings_get_gdb_stub_port},
    // General Settings
    {"vvctre_settings_set_use_cpu_jit", (void*)&vvctre_settings_set_use_cpu_jit},
    {"vvctre_settings_get_use_cpu_jit", (void*)&vvctre_settings_get_use_cpu_jit},
    {"vvctre_settings_set_limit_speed", (void*)&vvctre_settings_set_limit_speed},
    {"vvctre_settings_get_limit_speed", (void*)&vvctre_settings_get_limit_speed},
    {"vvctre_settings_set_speed_limit", (void*)&vvctre_settings_set_speed_limit},
    {"vvctre_settings_get_speed_limit", (void*)&vvctre_settings_get_speed_limit},
    {"vvctre_settings_set_use_custom_cpu_ticks", (void*)&vvctre_settings_set_use_custom_cpu_ticks},
    {"vvctre_settings_get_use_custom_cpu_ticks", (void*)&vvctre_settings_get_use_custom_cpu_ticks},
    {"vvctre_settings_set_custom_cpu_ticks", (void*)&vvctre_settings_set_custom_cpu_ticks},
    {"vvctre_settings_get_custom_cpu_ticks", (void*)&vvctre_settings_get_custom_cpu_ticks},
    {"vvctre_settings_set_cpu_clock_percentage", (void*)&vvctre_settings_set_cpu_clock_percentage},
    {"vvctre_settings_get_cpu_clock_percentage", (void*)&vvctre_settings_get_cpu_clock_percentage},
    // Audio Settings
    {"vvctre_settings_set_enable_dsp_lle", (void*)&vvctre_settings_set_enable_dsp_lle},
    {"vvctre_settings_get_enable_dsp_lle", (void*)&vvctre_settings_get_enable_dsp_lle},
    {"vvctre_settings_set_enable_dsp_lle_multithread",
     (void*)&vvctre_settings_set_enable_dsp_lle_multithread},
    {"vvctre_settings_get_enable_dsp_lle_multithread",
     (void*)&vvctre_settings_get_enable_dsp_lle_multithread},
    {"vvctre_settings_set_enable_audio_stretching",
     (void*)&vvctre_settings_set_enable_audio_stretching},
    {"vvctre_settings_get_enable_audio_stretching",
     (void*)&vvctre_settings_get_enable_audio_stretching},
    {"vvctre_settings_set_audio_volume", (void*)&vvctre_settings_set_audio_volume},
    {"vvctre_settings_get_audio_volume", (void*)&vvctre_settings_get_audio_volume},
    {"vvctre_settings_set_audio_sink_id", (void*)&vvctre_settings_set_audio_sink_id},
    {"vvctre_settings_get_audio_sink_id", (void*)&vvctre_settings_get_audio_sink_id},
    {"vvctre_settings_set_audio_device_id", (void*)&vvctre_settings_set_audio_device_id},
    {"vvctre_settings_get_audio_device_id", (void*)&vvctre_settings_get_audio_device_id},
    {"vvctre_settings_set_microphone_input_type",
     (void*)&vvctre_settings_set_microphone_input_type},
    {"vvctre_settings_get_microphone_input_type",
     (void*)&vvctre_settings_get_microphone_input_type},
    {"vvctre_settings_set_microphone_device", (void*)&vvctre_settings_set_microphone_device},
    {"vvctre_settings_get_microphone_device", (void*)&vvctre_settings_get_microphone_device},
    // Camera Settings
    {"vvctre_settings_set_camera_engine", (void*)&vvctre_settings_set_camera_engine},
    {"vvctre_settings_get_camera_engine", (void*)&vvctre_settings_get_camera_engine},
    {"vvctre_settings_set_camera_parameter", (void*)&vvctre_settings_set_camera_parameter},
    {"vvctre_settings_get_camera_parameter", (void*)&vvctre_settings_get_camera_parameter},
    {"vvctre_settings_set_camera_flip", (void*)&vvctre_settings_set_camera_flip},
    {"vvctre_settings_get_camera_flip", (void*)&vvctre_settings_get_camera_flip},
    // System Settings
    {"vvctre_set_play_coins", (void*)&vvctre_set_play_coins},
    {"vvctre_get_play_coins", (void*)&vvctre_get_play_coins},
    {"vvctre_settings_set_username", (void*)&vvctre_settings_set_username},
    {"vvctre_settings_get_username", (void*)&vvctre_settings_get_username},
    {"vvctre_settings_set_birthday", (void*)&vvctre_settings_set_birthday},
    {"vvctre_settings_get_birthday", (void*)&vvctre_settings_get_birthday},
    {"vvctre_settings_set_system_language", (void*)&vvctre_settings_set_system_language},
    {"vvctre_settings_get_system_language", (void*)&vvctre_settings_get_system_language},
    {"vvctre_settings_set_sound_output_mode", (void*)&vvctre_settings_set_sound_output_mode},
    {"vvctre_settings_get_sound_output_mode", (void*)&vvctre_settings_get_sound_output_mode},
    {"vvctre_settings_set_country", (void*)&vvctre_settings_set_country},
    {"vvctre_settings_get_country", (void*)&vvctre_settings_get_country},
    {"vvctre_settings_set_console_id", (void*)&vvctre_settings_set_console_id},
    {"vvctre_settings_get_console_id", (void*)&vvctre_settings_get_console_id},
    {"vvctre_settings_set_console_model", (void*)&vvctre_settings_set_console_model},
    {"vvctre_settings_get_console_model", (void*)&vvctre_settings_get_console_model},
    {"vvctre_settings_set_eula_version", (void*)&vvctre_settings_set_eula_version},
    {"vvctre_settings_get_eula_version", (void*)&vvctre_settings_get_eula_version},
    {"vvctre_settings_write_config_savegame", (void*)&vvctre_settings_write_config_savegame},
    // Graphics Settings
    {"vvctre_settings_set_use_hardware_renderer",
     (void*)&vvctre_settings_set_use_hardware_renderer},
    {"vvctre_settings_get_use_hardware_renderer",
     (void*)&vvctre_settings_get_use_hardware_renderer},
    {"vvctre_settings_set_use_hardware_shader", (void*)&vvctre_settings_set_use_hardware_shader},
    {"vvctre_settings_get_use_hardware_shader", (void*)&vvctre_settings_get_use_hardware_shader},
    {"vvctre_settings_set_hardware_shader_accurate_multiplication",
     (void*)&vvctre_settings_set_hardware_shader_accurate_multiplication},
    {"vvctre_settings_get_hardware_shader_accurate_multiplication",
     (void*)&vvctre_settings_get_hardware_shader_accurate_multiplication},
    {"vvctre_settings_set_use_shader_jit", (void*)&vvctre_settings_set_use_shader_jit},
    {"vvctre_settings_get_use_shader_jit", (void*)&vvctre_settings_get_use_shader_jit},
    {"vvctre_settings_set_enable_vsync", (void*)&vvctre_settings_set_enable_vsync},
    {"vvctre_settings_get_enable_vsync", (void*)&vvctre_settings_get_enable_vsync},
    {"vvctre_settings_set_dump_textures", (void*)&vvctre_settings_set_dump_textures},
    {"vvctre_settings_get_dump_textures", (void*)&vvctre_settings_get_dump_textures},
    {"vvctre_settings_set_custom_textures", (void*)&vvctre_settings_set_custom_textures},
    {"vvctre_settings_get_custom_textures", (void*)&vvctre_settings_get_custom_textures},
    {"vvctre_settings_set_preload_textures", (void*)&vvctre_settings_set_preload_textures},
    {"vvctre_settings_get_preload_textures", (void*)&vvctre_settings_get_preload_textures},
    {"vvctre_settings_set_enable_linear_filtering",
     (void*)&vvctre_settings_set_enable_linear_filtering},
    {"vvctre_settings_get_enable_linear_filtering",
     (void*)&vvctre_settings_get_enable_linear_filtering},
    {"vvctre_settings_set_sharper_distant_objects",
     (void*)&vvctre_settings_set_sharper_distant_objects},
    {"vvctre_settings_get_sharper_distant_objects",
     (void*)&vvctre_settings_get_sharper_distant_objects},
    {"vvctre_settings_set_resolution", (void*)&vvctre_settings_set_resolution},
    {"vvctre_settings_get_resolution", (void*)&vvctre_settings_get_resolution},
    {"vvctre_settings_set_background_color_red", (void*)&vvctre_settings_set_background_color_red},
    {"vvctre_settings_get_background_color_red", (void*)&vvctre_settings_get_background_color_red},
    {"vvctre_settings_set_background_color_green",
     (void*)&vvctre_settings_set_background_color_green},
    {"vvctre_settings_get_background_color_green",
     (void*)&vvctre_settings_get_background_color_green},
    {"vvctre_settings_set_background_color_blue",
     (void*)&vvctre_settings_set_background_color_blue},
    {"vvctre_settings_get_background_color_blue",
     (void*)&vvctre_settings_get_background_color_blue},
    {"vvctre_settings_set_post_processing_shader",
     (void*)&vvctre_settings_set_post_processing_shader},
    {"vvctre_settings_get_post_processing_shader",
     (void*)&vvctre_settings_get_post_processing_shader},
    {"vvctre_settings_set_texture_filter", (void*)&vvctre_settings_set_texture_filter},
    {"vvctre_settings_get_texture_filter", (void*)&vvctre_settings_get_texture_filter},
    {"vvctre_settings_set_render_3d", (void*)&vvctre_settings_set_render_3d},
    {"vvctre_settings_get_render_3d", (void*)&vvctre_settings_get_render_3d},
    {"vvctre_settings_set_factor_3d", (void*)&vvctre_settings_set_factor_3d},
    {"vvctre_settings_get_factor_3d", (void*)&vvctre_settings_get_factor_3d},
    // Controls Settings
    {"vvctre_settings_set_button", (void*)&vvctre_settings_set_button},
    {"vvctre_settings_get_button", (void*)&vvctre_settings_get_button},
    {"vvctre_settings_set_analog", (void*)&vvctre_settings_set_analog},
    {"vvctre_settings_get_analog", (void*)&vvctre_settings_get_analog},
    {"vvctre_settings_set_motion_device", (void*)&vvctre_settings_set_motion_device},
    {"vvctre_settings_get_motion_device", (void*)&vvctre_settings_get_motion_device},
    {"vvctre_settings_set_touch_device", (void*)&vvctre_settings_set_touch_device},
    {"vvctre_settings_get_touch_device", (void*)&vvctre_settings_get_touch_device},
    {"vvctre_settings_set_cemuhookudp_address", (void*)&vvctre_settings_set_cemuhookudp_address},
    {"vvctre_settings_get_cemuhookudp_address", (void*)&vvctre_settings_get_cemuhookudp_address},
    {"vvctre_settings_set_cemuhookudp_port", (void*)&vvctre_settings_set_cemuhookudp_port},
    {"vvctre_settings_get_cemuhookudp_port", (void*)&vvctre_settings_get_cemuhookudp_port},
    {"vvctre_settings_set_cemuhookudp_pad_index",
     (void*)&vvctre_settings_set_cemuhookudp_pad_index},
    {"vvctre_settings_get_cemuhookudp_pad_index",
     (void*)&vvctre_settings_get_cemuhookudp_pad_index},
    // Layout Settings
    {"vvctre_settings_set_layout", (void*)&vvctre_settings_set_layout},
    {"vvctre_settings_get_layout", (void*)&vvctre_settings_get_layout},
    {"vvctre_settings_set_swap_screens", (void*)&vvctre_settings_set_swap_screens},
    {"vvctre_settings_get_swap_screens", (void*)&vvctre_settings_get_swap_screens},
    {"vvctre_settings_set_upright_screens", (void*)&vvctre_settings_set_upright_screens},
    {"vvctre_settings_get_upright_screens", (void*)&vvctre_settings_get_upright_screens},
    {"vvctre_settings_set_use_custom_layout", (void*)&vvctre_settings_set_use_custom_layout},
    {"vvctre_settings_get_use_custom_layout", (void*)&vvctre_settings_get_use_custom_layout},
    {"vvctre_settings_set_custom_layout_top_left",
     (void*)&vvctre_settings_set_custom_layout_top_left},
    {"vvctre_settings_get_custom_layout_top_left",
     (void*)&vvctre_settings_get_custom_layout_top_left},
    {"vvctre_settings_set_custom_layout_top_top",
     (void*)&vvctre_settings_set_custom_layout_top_top},
    {"vvctre_settings_get_custom_layout_top_top",
     (void*)&vvctre_settings_get_custom_layout_top_top},
    {"vvctre_settings_set_custom_layout_top_right",
     (void*)&vvctre_settings_set_custom_layout_top_right},
    {"vvctre_settings_get_custom_layout_top_right",
     (void*)&vvctre_settings_get_custom_layout_top_right},
    {"vvctre_settings_set_custom_layout_top_bottom",
     (void*)&vvctre_settings_set_custom_layout_top_bottom},
    {"vvctre_settings_get_custom_layout_top_bottom",
     (void*)&vvctre_settings_get_custom_layout_top_bottom},
    {"vvctre_settings_set_custom_layout_bottom_left",
     (void*)&vvctre_settings_set_custom_layout_bottom_left},
    {"vvctre_settings_get_custom_layout_bottom_left",
     (void*)&vvctre_settings_get_custom_layout_bottom_left},
    {"vvctre_settings_set_custom_layout_bottom_top",
     (void*)&vvctre_settings_set_custom_layout_bottom_top},
    {"vvctre_settings_get_custom_layout_bottom_top",
     (void*)&vvctre_settings_get_custom_layout_bottom_top},
    {"vvctre_settings_set_custom_layout_bottom_right",
     (void*)&vvctre_settings_set_custom_layout_bottom_right},
    {"vvctre_settings_get_custom_layout_bottom_right",
     (void*)&vvctre_settings_get_custom_layout_bottom_right},
    {"vvctre_settings_set_custom_layout_bottom_bottom",
     (void*)&vvctre_settings_set_custom_layout_bottom_bottom},
    {"vvctre_settings_get_custom_layout_bottom_bottom",
     (void*)&vvctre_settings_get_custom_layout_bottom_bottom},
    {"vvctre_settings_get_layout_width", (void*)&vvctre_settings_get_layout_width},
    {"vvctre_settings_get_layout_height", (void*)&vvctre_settings_get_layout_height},
    // Hacks Settings
    {"vvctre_settings_set_enable_priority_boost",
     (void*)&vvctre_settings_set_enable_priority_boost},
    {"vvctre_settings_get_enable_priority_boost",
     (void*)&vvctre_settings_get_enable_priority_boost},
    // Modules
    {"vvctre_settings_set_use_lle_module", (void*)&vvctre_settings_set_use_lle_module},
    {"vvctre_settings_get_use_lle_module", (void*)&vvctre_settings_get_use_lle_module},
    {"vvctre_get_cfg_module", (void*)&vvctre_get_cfg_module},
    // Multiplayer
    {"vvctre_settings_set_multiplayer_ip", (void*)&vvctre_settings_set_multiplayer_ip},
    {"vvctre_settings_get_multiplayer_ip", (void*)&vvctre_settings_get_multiplayer_ip},
    {"vvctre_settings_set_multiplayer_port", (void*)&vvctre_settings_set_multiplayer_port},
    {"vvctre_settings_get_multiplayer_port", (void*)&vvctre_settings_get_multiplayer_port},
    {"vvctre_settings_set_nickname", (void*)&vvctre_settings_set_nickname},
    {"vvctre_settings_get_nickname", (void*)&vvctre_settings_get_nickname},
    {"vvctre_settings_set_multiplayer_password", (void*)&vvctre_settings_set_multiplayer_password},
    {"vvctre_settings_get_multiplayer_password", (void*)&vvctre_settings_get_multiplayer_password},
    {"vvctre_multiplayer_join", (void*)&vvctre_multiplayer_join},
    {"vvctre_multiplayer_leave", (void*)&vvctre_multiplayer_leave},
    {"vvctre_multiplayer_get_state", (void*)&vvctre_multiplayer_get_state},
    {"vvctre_multiplayer_send_message", (void*)&vvctre_multiplayer_send_message},
    {"vvctre_multiplayer_set_game", (void*)&vvctre_multiplayer_set_game},
    {"vvctre_multiplayer_get_nickname", (void*)&vvctre_multiplayer_get_nickname},
    {"vvctre_multiplayer_get_member_count", (void*)&vvctre_multiplayer_get_member_count},
    {"vvctre_multiplayer_get_member_nickname", (void*)&vvctre_multiplayer_get_member_nickname},
    {"vvctre_multiplayer_get_member_game_id", (void*)&vvctre_multiplayer_get_member_game_id},
    {"vvctre_multiplayer_get_member_game_name", (void*)&vvctre_multiplayer_get_member_game_name},
    {"vvctre_multiplayer_get_member_mac_address",
     (void*)&vvctre_multiplayer_get_member_mac_address},
    {"vvctre_multiplayer_get_room_name", (void*)&vvctre_multiplayer_get_room_name},
    {"vvctre_multiplayer_get_room_description", (void*)&vvctre_multiplayer_get_room_description},
    {"vvctre_multiplayer_get_room_member_slots", (void*)&vvctre_multiplayer_get_room_member_slots},
    {"vvctre_multiplayer_on_chat_message", (void*)&vvctre_multiplayer_on_chat_message},
    {"vvctre_multiplayer_on_status_message", (void*)&vvctre_multiplayer_on_status_message},
    {"vvctre_multiplayer_on_error", (void*)&vvctre_multiplayer_on_error},
    {"vvctre_multiplayer_on_information_change", (void*)&vvctre_multiplayer_on_information_change},
    {"vvctre_multiplayer_on_state_change", (void*)&vvctre_multiplayer_on_state_change},
    {"vvctre_multiplayer_create_room", (void*)&vvctre_multiplayer_create_room},
    // CoreTiming
    {"vvctre_coretiming_register_event", (void*)&vvctre_coretiming_register_event},
    {"vvctre_coretiming_remove_event", (void*)&vvctre_coretiming_remove_event},
    {"vvctre_coretiming_remove_normal_and_threadsafe_event",
     (void*)&vvctre_coretiming_remove_normal_and_threadsafe_event},
    {"vvctre_coretiming_schedule_event", (void*)&vvctre_coretiming_schedule_event},
    {"vvctre_coretiming_schedule_event_threadsafe",
     (void*)&vvctre_coretiming_schedule_event_threadsafe},
    {"vvctre_coretiming_unschedule", (void*)&vvctre_coretiming_unschedule},
    {"vvctre_coretiming_get_ticks", (void*)&vvctre_coretiming_get_ticks},
    {"vvctre_coretiming_get_idle_ticks", (void*)&vvctre_coretiming_get_idle_ticks},
    {"vvctre_coretiming_add_ticks", (void*)&vvctre_coretiming_add_ticks},
    {"vvctre_coretiming_advance", (void*)&vvctre_coretiming_advance},
    {"vvctre_coretiming_move_events", (void*)&vvctre_coretiming_move_events},
    {"vvctre_coretiming_idle", (void*)&vvctre_coretiming_idle},
    {"vvctre_coretiming_force_exception_check", (void*)&vvctre_coretiming_force_exception_check},
    {"vvctre_coretiming_get_global_time_us", (void*)&vvctre_coretiming_get_global_time_us},
    {"vvctre_coretiming_get_downcount", (void*)&vvctre_coretiming_get_downcount},
    // Other
    {"vvctre_get_version", (void*)&vvctre_get_version},
    {"vvctre_get_version_major", (void*)&vvctre_get_version_major},
    {"vvctre_get_version_minor", (void*)&vvctre_get_version_minor},
    {"vvctre_get_version_patch", (void*)&vvctre_get_version_patch},
    {"vvctre_log_trace", (void*)&vvctre_log_trace},
    {"vvctre_log_debug", (void*)&vvctre_log_debug},
    {"vvctre_log_info", (void*)&vvctre_log_info},
    {"vvctre_log_warning", (void*)&vvctre_log_warning},
    {"vvctre_log_error", (void*)&vvctre_log_error},
    {"vvctre_log_critical", (void*)&vvctre_log_critical},
    {"vvctre_swap_buffers", (void*)&vvctre_swap_buffers},
    {"vvctre_get_opengl_function", (void*)&vvctre_get_opengl_function},
    {"vvctre_get_fps", (void*)&vvctre_get_fps},
    {"vvctre_get_frametime", (void*)&vvctre_get_frametime},
    {"vvctre_get_frame_count", (void*)&vvctre_get_frame_count},
    {"vvctre_get_fatal_error", (void*)&vvctre_get_fatal_error},
    {"vvctre_set_show_fatal_error_messages", (void*)&vvctre_set_show_fatal_error_messages},
    {"vvctre_get_show_fatal_error_messages", (void*)&vvctre_get_show_fatal_error_messages},
};
