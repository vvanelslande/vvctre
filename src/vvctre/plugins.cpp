// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include <vector>
#include <fmt/format.h>
#include <imgui.h>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/cheats/gateway_cheat.h"
#include "core/core.h"
#include "vvctre/common.h"
#include "vvctre/plugins.h"

#ifdef _WIN32
#include <windows.h>
#endif

bool has_suffix(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

PluginManager::PluginManager(void* core) {
#ifdef _WIN32
    FileUtil::FSTEntry parent;
    FileUtil::ScanDirectoryTree(FileUtil::GetExeDirectory(), parent);
    for (const auto& entry : parent.children) {
        if (!entry.isDirectory && entry.virtualName != "SDL2.dll" &&
            has_suffix(entry.virtualName, ".dll")) {
            HMODULE handle = LoadLibraryA(entry.virtualName.c_str());
            if (handle == NULL) {
                fmt::print("Plugin {} failed to load: {}\n", entry.virtualName, GetLastErrorMsg());
            } else {
                PluginImportedFunctions::PluginLoaded f =
                    (PluginImportedFunctions::PluginLoaded)GetProcAddress(handle, "PluginLoaded");
                if (f == nullptr) {
                    fmt::print("Plugin {} failed to load: PluginLoaded "
                               "function not found\n",
                               entry.virtualName);
                } else {
                    Plugin plugin;
                    plugin.handle = handle;
                    plugin.before_drawing_fps =
                        (PluginImportedFunctions::BeforeDrawingFPS)GetProcAddress(
                            handle, "BeforeDrawingFPS");
                    plugin.add_menu =
                        (PluginImportedFunctions::AddMenu)GetProcAddress(handle, "AddMenu");
                    plugin.after_swap_window =
                        (PluginImportedFunctions::AfterSwapWindow)GetProcAddress(handle,
                                                                                 "AfterSwapWindow");
                    plugins.push_back(std::move(plugin));

                    f(core, static_cast<void*>(this));

                    fmt::print("Plugin {} loaded\n", entry.virtualName);
                }
            }
        }
    }
#endif
}

PluginManager::~PluginManager() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::EmulatorClosing f =
            (PluginImportedFunctions::EmulatorClosing)GetProcAddress(plugin.handle,
                                                                     "EmulatorClosing");
        if (f != nullptr) {
            f();
        }
        FreeLibrary(plugin.handle);
    }
#endif
}

void PluginManager::InitialSettingsOpening() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::InitialSettingsOpening f =
            (PluginImportedFunctions::InitialSettingsOpening)GetProcAddress(
                plugin.handle, "InitialSettingsOpening");
        if (f != nullptr) {
            f();
        }
    }
#endif
}

void PluginManager::InitialSettingsOkPressed() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::InitialSettingsOkPressed f =
            (PluginImportedFunctions::InitialSettingsOkPressed)GetProcAddress(
                plugin.handle, "InitialSettingsOkPressed");
        if (f != nullptr) {
            f();
        }
    }
#endif
}

void PluginManager::BeforeLoading() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::BeforeLoading f =
            (PluginImportedFunctions::BeforeLoading)GetProcAddress(plugin.handle, "BeforeLoading");
        if (f != nullptr) {
            f();
        }
    }
#endif
}

void PluginManager::EmulationStarting() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::EmulationStarting f =
            (PluginImportedFunctions::EmulationStarting)GetProcAddress(plugin.handle,
                                                                       "EmulationStarting");
        if (f != nullptr) {
            f();
        }
    }
#endif
}

void PluginManager::EmulatorClosing() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::EmulatorClosing f =
            (PluginImportedFunctions::EmulatorClosing)GetProcAddress(plugin.handle,
                                                                     "EmulatorClosing");
        if (f != nullptr) {
            f();
        }
    }
#endif
}

void PluginManager::FatalError() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::FatalError f =
            (PluginImportedFunctions::FatalError)GetProcAddress(plugin.handle, "FatalError");
        if (f != nullptr) {
            f();
        }
    }
#endif
}

void PluginManager::BeforeDrawingFPS() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        if (plugin.before_drawing_fps != nullptr) {
            plugin.before_drawing_fps();
        }
    }
#endif
}

void PluginManager::AddMenus() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        if (plugin.add_menu != nullptr) {
            plugin.add_menu();
        }
    }
#endif
}

void PluginManager::AfterSwapWindow() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        if (plugin.after_swap_window != nullptr) {
            plugin.after_swap_window();
        }
    }
#endif
}

// Exports

#ifdef _WIN32
#define VVCTRE_PLUGIN_FUNCTION extern "C" __declspec(dllexport)
#else
#define VVCTRE_PLUGIN_FUNCTION
#endif

// File, Emulation
VVCTRE_PLUGIN_FUNCTION void vvctre_load_file(void* core, const char* path) {
    static_cast<Core::System*>(core)->SetResetFilePath(std::string(path));
    static_cast<Core::System*>(core)->RequestReset();
}

VVCTRE_PLUGIN_FUNCTION void vvctre_restart(void* core) {
    static_cast<Core::System*>(core)->Reset();
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_paused(void* plugin_manager, bool paused) {
    static_cast<PluginManager*>(plugin_manager)->paused = paused;
}

// Debugging
VVCTRE_PLUGIN_FUNCTION void vvctre_set_pc(void* core, u32 addr) {
    static_cast<Core::System*>(core)->CPU().SetPC(addr);
}

VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_pc(void* core) {
    return static_cast<Core::System*>(core)->CPU().GetPC();
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetReg(index, value);
}

VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetReg(index);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_vfp_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetVFPReg(index, value);
}

VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_vfp_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetVFPReg(index);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_vfp_system_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetVFPSystemReg(static_cast<VFPSystemRegister>(index),
                                                            value);
}

VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_vfp_system_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetVFPSystemReg(
        static_cast<VFPSystemRegister>(index));
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_cp15_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetCP15Register(static_cast<CP15Register>(index),
                                                            value);
}

VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_cp15_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetCP15Register(
        static_cast<CP15Register>(index));
}

// Cheats
VVCTRE_PLUGIN_FUNCTION int vvctre_cheat_count(void* core) {
    return static_cast<int>(static_cast<Core::System*>(core)->CheatEngine().GetCheats().size());
}

VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->ToString().c_str();
}

VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat_name(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetName().c_str();
}

VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat_comments(void* core, int index) {
    return static_cast<Core::System*>(core)
        ->CheatEngine()
        .GetCheats()[index]
        ->GetComments()
        .c_str();
}

VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat_type(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetType().c_str();
}

VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat_code(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetCode().c_str();
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_cheat_enabled(void* core, int index, bool enabled) {
    static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->SetEnabled(enabled);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_add_gateway_cheat(void* core, const char* name, const char* code,
                                                     const char* comments) {
    static_cast<Core::System*>(core)->CheatEngine().AddCheat(std::make_shared<Cheats::GatewayCheat>(
        std::string(name), std::string(code), std::string(comments)));
}

VVCTRE_PLUGIN_FUNCTION void vvctre_remove_cheat(void* core, int index) {
    static_cast<Core::System*>(core)->CheatEngine().RemoveCheat(index);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_update_gateway_cheat(void* core, int index, const char* name,
                                                        const char* code, const char* comments) {
    static_cast<Core::System*>(core)->CheatEngine().UpdateCheat(
        index, std::make_shared<Cheats::GatewayCheat>(std::string(name), std::string(code),
                                                      std::string(comments)));
}

// Other
VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_version() {
    return vvctre_version.c_str();
}

// GUI
VVCTRE_PLUGIN_FUNCTION void vvctre_gui_text(const char* text) {
    ImGui::Text(text);
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_button(const char* text) {
    return ImGui::Button(text);
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_begin(const char* name) {
    return ImGui::Begin(name);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_gui_end() {
    ImGui::End();
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_begin_menu(const char* name) {
    return ImGui::BeginMenu(name);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_gui_end_menu() {
    ImGui::EndMenu();
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_menu_item(const char* name) {
    return ImGui::MenuItem(name);
}
