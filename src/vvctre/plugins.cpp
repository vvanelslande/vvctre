// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstdlib>
#include <utility>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <asl/Directory.h>
#include <asl/JSON.h>
#include <asl/Process.h>
#include <fmt/format.h>
#include <imgui.h>
#include "common/common_funcs.h"
#include "common/file_util.h"
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
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/sm/sm.h"
#include "core/memory.h"
#include "core/movie.h"
#include "core/settings.h"
#include "network/network.h"
#include "network/room_member.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
#include "vvctre/common.h"
#include "vvctre/function_logger.h"
#include "vvctre/plugins.h"

PluginManager::PluginManager(Core::System& core, SDL_Window* window) : window(window) {
    asl::Array<asl::File> files = asl::Directory(asl::Process::myDir())
                                      .files(
#ifdef _WIN32
                                          "*.dll"
#else
                                          "*.so"
#endif
                                      );
    foreach (asl::File& file, files) {
        if (file.name() != "SDL2.dll") {
            void* handle = SDL_LoadObject(*file.path());
            if (handle == NULL) {
                fmt::print("Plugin {} failed to load: {}\n", *file.name(), SDL_GetError());
            } else {
                void* get_required_function_count =
                    SDL_LoadFunction(handle, "GetRequiredFunctionCount");
                void* get_required_function_names =
                    SDL_LoadFunction(handle, "GetRequiredFunctionNames");
                void* plugin_loaded = SDL_LoadFunction(handle, "PluginLoaded");
                if (get_required_function_count == nullptr ||
                    get_required_function_names == nullptr || plugin_loaded == nullptr) {
                    fmt::print("Plugin {} failed to load: {}\n", *file.name(), SDL_GetError());
                    SDL_UnloadObject(handle);
                } else {
                    Plugin plugin;
                    plugin.handle = handle;
                    plugin.before_drawing_fps =
                        (decltype(Plugin::before_drawing_fps))SDL_LoadFunction(handle,
                                                                               "BeforeDrawingFPS");
                    plugin.add_menu =
                        (decltype(Plugin::add_menu))SDL_LoadFunction(handle, "AddMenu");
                    plugin.add_tab = (decltype(Plugin::add_tab))SDL_LoadFunction(handle, "AddTab");
                    plugin.after_swap_window =
                        (decltype(Plugin::after_swap_window))SDL_LoadFunction(handle,
                                                                              "AfterSwapWindow");
                    plugin.screenshot_callback = (decltype(
                        Plugin::screenshot_callback))SDL_LoadFunction(handle, "ScreenshotCallback");
                    void* log = SDL_LoadFunction(handle, "Log");
                    if (log != nullptr) {
                        Log::AddBackend(std::make_unique<Log::FunctionLogger>(
                            (decltype(Log::FunctionLogger::function))log,
                            fmt::format("Plugin {}", *file.name())));
                    }

                    int count = ((int (*)())get_required_function_count)();
                    const char** required_function_names =
                        ((const char** (*)())get_required_function_names)();
                    std::vector<void*> required_functions(count);
                    for (int i = 0; i < count; ++i) {
                        required_functions[i] =
                            function_map[std::string(required_function_names[i])];
                    }
                    ((void (*)(void* core, void* plugin_manager, void* functions[]))plugin_loaded)(
                        static_cast<void*>(&core), static_cast<void*>(this),
                        required_functions.data());

                    plugins.push_back(std::move(plugin));
                    fmt::print("Plugin {} loaded\n", *file.name());
                }
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

void PluginManager::EmulationStarting() {
    for (const auto& plugin : plugins) {
        void* emulation_starting = SDL_LoadFunction(plugin.handle, "EmulationStarting");
        if (emulation_starting != nullptr) {
            ((void (*)())emulation_starting)();
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

void vvctre_invalidate_cache_range(void* core, u32 address, size_t length) {
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
    return static_cast<Core::System*>(core)->ServiceManager().GetServiceNameByPortId(port).c_str();
}

// Cheats
int vvctre_cheat_count(void* core) {
    return static_cast<int>(static_cast<Core::System*>(core)->CheatEngine().GetCheats().size());
}

const char* vvctre_get_cheat(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->ToString().c_str();
}

const char* vvctre_get_cheat_name(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetName().c_str();
}

const char* vvctre_get_cheat_comments(void* core, int index) {
    return static_cast<Core::System*>(core)
        ->CheatEngine()
        .GetCheats()[index]
        ->GetComments()
        .c_str();
}

const char* vvctre_get_cheat_type(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetType().c_str();
}

const char* vvctre_get_cheat_code(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetCode().c_str();
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
        ImGui::SetTooltip("%s", text);
    }
}

void vvctre_gui_begin_tooltip() {
    ImGui::BeginTooltip();
}

bool vvctre_gui_is_item_hovered(int flags) {
    return ImGui::IsItemHovered(flags);
}

bool vvctre_gui_is_item_deactivated_after_edit() {
    return ImGui::IsItemDeactivatedAfterEdit();
}

void vvctre_gui_end_tooltip() {
    ImGui::EndTooltip();
}

void vvctre_gui_text(const char* text) {
    ImGui::TextUnformatted(text);
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
                             int flags) {
    return ImGui::ColorButton(tooltip, ImVec4(red, green, blue, alpha), flags);
}

bool vvctre_gui_invisible_button(const char* id, float width, float height) {
    return ImGui::InvisibleButton(id, ImVec2(width, height));
}

bool vvctre_gui_radio_button(const char* label, bool active) {
    return ImGui::RadioButton(label, active);
}

bool vvctre_gui_checkbox(const char* label, bool* checked) {
    return ImGui::Checkbox(label, checked);
}

bool vvctre_gui_begin(const char* name) {
    return ImGui::Begin(name);
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

void vvctre_gui_end() {
    ImGui::End();
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

void vvctre_gui_end_tab() {
    ImGui::EndTabItem();
}

bool vvctre_gui_menu_item(const char* label) {
    return ImGui::MenuItem(label);
}

bool vvctre_gui_menu_item_with_check_mark(const char* label, bool* checked) {
    return ImGui::MenuItem(label, nullptr, checked);
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

bool vvctre_gui_text_input(const char* label, char* buffer, size_t buffer_size) {
    return ImGui::InputText(label, buffer, buffer_size);
}

bool vvctre_gui_text_input_multiline(const char* label, char* buffer, size_t buffer_size) {
    return ImGui::InputTextMultiline(label, buffer, buffer_size);
}

bool vvctre_gui_text_input_with_hint(const char* label, const char* hint, char* buffer,
                                     size_t buffer_size) {
    return ImGui::InputTextWithHint(label, hint, buffer, buffer_size);
}

bool vvctre_gui_u8_input(const char* label, u8* value) {
    return ImGui::InputScalar(label, ImGuiDataType_U8, value);
}

bool vvctre_gui_u16_input(const char* label, u16* value) {
    return ImGui::InputScalar(label, ImGuiDataType_U16, value);
}

bool vvctre_gui_u32_input(const char* label, u32* value) {
    return ImGui::InputScalar(label, ImGuiDataType_U32, value);
}

bool vvctre_gui_u64_input(const char* label, u64* value) {
    return ImGui::InputScalar(label, ImGuiDataType_U64, value);
}

bool vvctre_gui_s8_input(const char* label, s8* value) {
    return ImGui::InputScalar(label, ImGuiDataType_S8, value);
}

bool vvctre_gui_s16_input(const char* label, s16* value) {
    return ImGui::InputScalar(label, ImGuiDataType_S16, value);
}

bool vvctre_gui_int_input(const char* label, int* value, int step, int step_fast) {
    return ImGui::InputInt(label, value, step, step_fast);
}

bool vvctre_gui_s64_input(const char* label, s64* value) {
    return ImGui::InputScalar(label, ImGuiDataType_S64, value);
}

bool vvctre_gui_float_input(const char* label, float* value, float step, float step_fast) {
    return ImGui::InputFloat(label, value, step, step_fast);
}

bool vvctre_gui_double_input(const char* label, double* value, double step, double step_fast) {
    return ImGui::InputDouble(label, value, step, step_fast);
}

bool vvctre_gui_color_edit(const char* label, float* color, int flags) {
    return ImGui::ColorEdit4(label, color, flags);
}

bool vvctre_gui_color_picker(const char* label, float* color, int flags) {
    return ImGui::ColorPicker4(label, color, flags);
}

void vvctre_gui_progress_bar(float value, const char* overlay) {
    ImGui::ProgressBar(value, ImVec2(-1, 0), overlay);
}

bool vvctre_gui_slider_u8(const char* label, u8* value, const u8 minimum, const u8 maximum,
                          const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_U8, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_u16(const char* label, u16* value, const u16 minimum, const u16 maximum,
                           const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_U16, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_u32(const char* label, u32* value, const u32 minimum, const u32 maximum,
                           const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_U32, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_u64(const char* label, u64* value, const u64 minimum, const u64 maximum,
                           const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_U64, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_s8(const char* label, s8* value, const s8 minimum, const s8 maximum,
                          const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_S8, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_s16(const char* label, s16* value, const s16 minimum, const s16 maximum,
                           const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_S16, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_s32(const char* label, s32* value, const s32 minimum, const s32 maximum,
                           const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_S32, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_s64(const char* label, s64* value, const s64 minimum, const s64 maximum,
                           const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_U64, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_float(const char* label, float* value, const float minimum,
                             const float maximum, const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_Float, value, &minimum, &maximum, format);
}

bool vvctre_gui_slider_double(const char* label, double* value, const double minimum,
                              const double maximum, const char* format) {
    return ImGui::SliderScalar(label, ImGuiDataType_Double, value, &minimum, &maximum, format);
}

void vvctre_gui_set_color(int index, float r, float g, float b, float a) {
    ImGui::GetStyle().Colors[index] = ImVec4(r, g, b, a);
}

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

void vvctre_settings_set_start_in_fullscreen_mode(bool value) {
    Settings::values.start_in_fullscreen_mode = value;
}

bool vvctre_settings_get_start_in_fullscreen_mode() {
    return Settings::values.start_in_fullscreen_mode;
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
    static_cast<Service::CFG::Module*>(cfg)->SetCountryCode(value);
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
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        room_member->Join(Settings::values.multiplayer_nickname,
                          Service::CFG::GetConsoleIdHash(*static_cast<Core::System*>(core)),
                          Settings::values.multiplayer_ip.c_str(),
                          Settings::values.multiplayer_port, Network::NoPreferredMac,
                          Settings::values.multiplayer_password);
    }
}

void vvctre_multiplayer_leave(void* /* core, currently unused */) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        room_member->Leave();
    }
}

u8 vvctre_multiplayer_get_state(void* /* core, currently unused */) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        return static_cast<u8>(room_member->GetState());
    }

    return static_cast<u8>(Network::RoomMember::State::Uninitialized);
}

void vvctre_multiplayer_send_message(void* /* core, currently unused */, const char* message) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        room_member->SendChatMessage(std::string(message));
    }
}

void vvctre_multiplayer_set_game(void* /* core, currently unused */, const char* name, u64 id) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        room_member->SendGameInfo(Network::GameInfo{name, id});
    }
}

u8 vvctre_multiplayer_get_member_count(void* /* core, currently unused */) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        return static_cast<u8>(room_member->GetMemberInformation().size());
    }

    return 0;
}

const char* vvctre_multiplayer_get_member_nickname(void* /* core, currently unused */,
                                                   std::size_t index) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        return room_member->GetMemberInformation()[index].nickname.c_str();
    }

    return nullptr;
}

u64 vvctre_multiplayer_get_member_game_id(void* /* core, currently unused */, std::size_t index) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        return room_member->GetMemberInformation()[index].game_info.id;
    }

    return 0;
}

const char* vvctre_multiplayer_get_member_game_name(void* /* core, currently unused */,
                                                    std::size_t index) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        return room_member->GetMemberInformation()[index].game_info.name.c_str();
    }

    return nullptr;
}

const char* vvctre_multiplayer_get_room_name(void* /* core, currently unused */) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        return room_member->GetRoomInformation().name.c_str();
    }

    return nullptr;
}

const char* vvctre_multiplayer_get_room_description(void* /* core, currently unused */) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        return room_member->GetRoomInformation().description.c_str();
    }

    return nullptr;
}

u8 vvctre_multiplayer_get_room_member_slots(void* /* core, currently unused */) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        return static_cast<u8>(room_member->GetRoomInformation().member_slots);
    }

    return 0;
}

void vvctre_multiplayer_on_chat_message(void* /* core, currently unused */,
                                        void (*callback)(const char* nickname,
                                                         const char* message)) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        room_member->BindOnChatMessageReceived([=](const Network::ChatEntry& entry) {
            callback(entry.nickname.c_str(), entry.message.c_str());
        });
    }
}

void vvctre_multiplayer_on_status_message(void* /* core, currently unused */,
                                          void (*callback)(u8 type, const char* nickname)) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        room_member->BindOnStatusMessageReceived([=](const Network::StatusMessageEntry& entry) {
            callback(static_cast<u8>(entry.type), entry.nickname.c_str());
        });
    }
}

void vvctre_multiplayer_on_error(void* /* core, currently unused */, void (*callback)(u8 error)) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        room_member->BindOnError(
            [=](const Network::RoomMember::Error& error) { callback(static_cast<u8>(error)); });
    }
}

void vvctre_multiplayer_on_information_change(void* /* core, currently unused */,
                                              void (*callback)()) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        room_member->BindOnRoomInformationChanged(
            [=](const Network::RoomInformation&) { callback(); });
    }
}

void vvctre_multiplayer_on_state_change(void* /* core, currently unused */, void (*callback)()) {
    if (std::shared_ptr<Network::RoomMember> room_member = Network::GetRoomMember().lock()) {
        room_member->BindOnRoomInformationChanged(
            [=](const Network::RoomInformation&) { callback(); });
    }
}

// Other
const char* vvctre_get_version() {
    return fmt::format("{}.{}.{}", vvctre_version_major, vvctre_version_minor, vvctre_version_patch)
        .c_str();
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

std::unordered_map<std::string, void*> PluginManager::function_map = {
    // File
    {"vvctre_load_file", (void*)&vvctre_load_file},
    {"vvctre_install_cia", (void*)&vvctre_install_cia},
    {"vvctre_load_amiibo", (void*)&vvctre_load_amiibo},
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
    // Camera
    {"vvctre_reload_camera_images", (void*)&vvctre_reload_camera_images},
    // GUI
    {"vvctre_gui_push_item_width", (void*)&vvctre_gui_push_item_width},
    {"vvctre_gui_pop_item_width", (void*)&vvctre_gui_pop_item_width},
    {"vvctre_gui_same_line", (void*)&vvctre_gui_same_line},
    {"vvctre_gui_new_line", (void*)&vvctre_gui_new_line},
    {"vvctre_gui_bullet", (void*)&vvctre_gui_bullet},
    {"vvctre_gui_indent", (void*)&vvctre_gui_indent},
    {"vvctre_gui_unindent", (void*)&vvctre_gui_unindent},
    {"vvctre_gui_spacing", (void*)&vvctre_gui_spacing},
    {"vvctre_gui_separator", (void*)&vvctre_gui_separator},
    {"vvctre_gui_dummy", (void*)&vvctre_gui_dummy},
    {"vvctre_gui_tooltip", (void*)&vvctre_gui_tooltip},
    {"vvctre_gui_begin_tooltip", (void*)&vvctre_gui_begin_tooltip},
    {"vvctre_gui_is_item_hovered", (void*)&vvctre_gui_is_item_hovered},
    {"vvctre_gui_is_item_deactivated_after_edit",
     (void*)&vvctre_gui_is_item_deactivated_after_edit},
    {"vvctre_gui_end_tooltip", (void*)&vvctre_gui_end_tooltip},
    {"vvctre_gui_text", (void*)&vvctre_gui_text},
    {"vvctre_gui_text_colored", (void*)&vvctre_gui_text_colored},
    {"vvctre_gui_button", (void*)&vvctre_gui_button},
    {"vvctre_gui_small_button", (void*)&vvctre_gui_small_button},
    {"vvctre_gui_color_button", (void*)&vvctre_gui_color_button},
    {"vvctre_gui_invisible_button", (void*)&vvctre_gui_invisible_button},
    {"vvctre_gui_radio_button", (void*)&vvctre_gui_radio_button},
    {"vvctre_gui_checkbox", (void*)&vvctre_gui_checkbox},
    {"vvctre_gui_begin", (void*)&vvctre_gui_begin},
    {"vvctre_gui_begin_overlay", (void*)&vvctre_gui_begin_overlay},
    {"vvctre_gui_begin_auto_resize", (void*)&vvctre_gui_begin_auto_resize},
    {"vvctre_gui_end", (void*)&vvctre_gui_end},
    {"vvctre_gui_begin_menu", (void*)&vvctre_gui_begin_menu},
    {"vvctre_gui_end_menu", (void*)&vvctre_gui_end_menu},
    {"vvctre_gui_begin_tab", (void*)&vvctre_gui_begin_tab},
    {"vvctre_gui_end_tab", (void*)&vvctre_gui_end_tab},
    {"vvctre_gui_menu_item", (void*)&vvctre_gui_menu_item},
    {"vvctre_gui_menu_item_with_check_mark", (void*)&vvctre_gui_menu_item_with_check_mark},
    {"vvctre_gui_begin_listbox", (void*)&vvctre_gui_begin_listbox},
    {"vvctre_gui_end_listbox", (void*)&vvctre_gui_end_listbox},
    {"vvctre_gui_begin_combo_box", (void*)&vvctre_gui_begin_combo_box},
    {"vvctre_gui_end_combo_box", (void*)&vvctre_gui_end_combo_box},
    {"vvctre_gui_selectable", (void*)&vvctre_gui_selectable},
    {"vvctre_gui_selectable_with_selected", (void*)&vvctre_gui_selectable_with_selected},
    {"vvctre_gui_text_input", (void*)&vvctre_gui_text_input},
    {"vvctre_gui_text_input_multiline", (void*)&vvctre_gui_text_input_multiline},
    {"vvctre_gui_text_input_with_hint", (void*)&vvctre_gui_text_input_with_hint},
    {"vvctre_gui_u8_input", (void*)&vvctre_gui_u8_input},
    {"vvctre_gui_u16_input", (void*)&vvctre_gui_u16_input},
    {"vvctre_gui_u32_input", (void*)&vvctre_gui_u32_input},
    {"vvctre_gui_u64_input", (void*)&vvctre_gui_u64_input},
    {"vvctre_gui_s8_input", (void*)&vvctre_gui_s8_input},
    {"vvctre_gui_s16_input", (void*)&vvctre_gui_s16_input},
    {"vvctre_gui_int_input", (void*)&vvctre_gui_int_input},
    {"vvctre_gui_s64_input", (void*)&vvctre_gui_s64_input},
    {"vvctre_gui_float_input", (void*)&vvctre_gui_float_input},
    {"vvctre_gui_double_input", (void*)&vvctre_gui_double_input},
    {"vvctre_gui_color_edit", (void*)&vvctre_gui_color_edit},
    {"vvctre_gui_color_picker", (void*)&vvctre_gui_color_picker},
    {"vvctre_gui_progress_bar", (void*)&vvctre_gui_progress_bar},
    {"vvctre_gui_slider_u8", (void*)&vvctre_gui_slider_u8},
    {"vvctre_gui_slider_u16", (void*)&vvctre_gui_slider_u16},
    {"vvctre_gui_slider_u32", (void*)&vvctre_gui_slider_u32},
    {"vvctre_gui_slider_u64", (void*)&vvctre_gui_slider_u64},
    {"vvctre_gui_slider_s8", (void*)&vvctre_gui_slider_s8},
    {"vvctre_gui_slider_s16", (void*)&vvctre_gui_slider_s16},
    {"vvctre_gui_slider_s32", (void*)&vvctre_gui_slider_s32},
    {"vvctre_gui_slider_s64", (void*)&vvctre_gui_slider_s64},
    {"vvctre_gui_slider_float", (void*)&vvctre_gui_slider_float},
    {"vvctre_gui_slider_double", (void*)&vvctre_gui_slider_double},
    {"vvctre_gui_set_color", (void*)&vvctre_gui_set_color},
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
    {"vvctre_settings_set_start_in_fullscreen_mode",
     (void*)&vvctre_settings_set_start_in_fullscreen_mode},
    {"vvctre_settings_get_start_in_fullscreen_mode",
     (void*)&vvctre_settings_get_start_in_fullscreen_mode},
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
    // Modules
    {"vvctre_settings_set_use_lle_module", (void*)&vvctre_settings_set_use_lle_module},
    {"vvctre_settings_get_use_lle_module", (void*)&vvctre_settings_get_use_lle_module},
    {"vvctre_get_cfg_module", (void*)&vvctre_get_cfg_module},
    // Hacks Settings
    {"vvctre_settings_set_enable_priority_boost",
     (void*)&vvctre_settings_set_enable_priority_boost},
    {"vvctre_settings_get_enable_priority_boost",
     (void*)&vvctre_settings_get_enable_priority_boost},
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
    {"vvctre_multiplayer_get_member_count", (void*)&vvctre_multiplayer_get_member_count},
    {"vvctre_multiplayer_get_member_nickname", (void*)&vvctre_multiplayer_get_member_nickname},
    {"vvctre_multiplayer_get_member_game_id", (void*)&vvctre_multiplayer_get_member_game_id},
    {"vvctre_multiplayer_get_member_game_name", (void*)&vvctre_multiplayer_get_member_game_name},
    {"vvctre_multiplayer_get_room_name", (void*)&vvctre_multiplayer_get_room_name},
    {"vvctre_multiplayer_get_room_description", (void*)&vvctre_multiplayer_get_room_description},
    {"vvctre_multiplayer_get_room_member_slots", (void*)&vvctre_multiplayer_get_room_member_slots},
    {"vvctre_multiplayer_on_chat_message", (void*)&vvctre_multiplayer_on_chat_message},
    {"vvctre_multiplayer_on_status_message", (void*)&vvctre_multiplayer_on_status_message},
    {"vvctre_multiplayer_on_error", (void*)&vvctre_multiplayer_on_error},
    {"vvctre_multiplayer_on_information_change", (void*)&vvctre_multiplayer_on_information_change},
    {"vvctre_multiplayer_on_state_change", (void*)&vvctre_multiplayer_on_state_change},
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
};
