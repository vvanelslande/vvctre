// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

if (process.env.ISSUE_BODY === "WIDTHxHEIGHT\r\n") {
  console.log("no changes");
  process.exit(1);
}

if (process.env.ISSUE_BODY === "400x480\r\n") {
  console.log("it's the default window size");
  process.exit(1);
}

const fs = require("fs");

const match = process.env.ISSUE_BODY.match(
  /^(?<width>\d+)x(?<height>\d+)(?:\r\n)?$/
);

let code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stddef.h>

static const char* required_function_names[] = { "vvctre_set_os_window_size", NULL };

typedef void (*vvctre_set_os_window_size_t)(void* plugin_manager, int width, int height);

static vvctre_set_os_window_size_t vvctre_set_os_window_size;

static void* g_plugin_manager = NULL;

#ifdef _WIN32
#define VVCTRE_PLUGIN_EXPORT __declspec(dllexport)
#else
#define VVCTRE_PLUGIN_EXPORT
#endif

VVCTRE_PLUGIN_EXPORT int GetRequiredFunctionCount() {
    return 1;
}

VVCTRE_PLUGIN_EXPORT const char** GetRequiredFunctionNames() {
    return required_function_names;
}

VVCTRE_PLUGIN_EXPORT void PluginLoaded(void* core, void* plugin_manager,
                                       void* required_functions[]) {
    vvctre_set_os_window_size = (vvctre_set_os_window_size_t)required_functions[0];
    g_plugin_manager = plugin_manager;
}

VVCTRE_PLUGIN_EXPORT void InitialSettingsOpening() {
    vvctre_set_os_window_size(g_plugin_manager, ${match.groups.width}, ${match.groups.height});
}

VVCTRE_PLUGIN_EXPORT void EmulationStarting() {
  vvctre_set_os_window_size(g_plugin_manager, ${match.groups.width}, ${match.groups.height});
}
`;

fs.writeFileSync("plugin.c", code);
