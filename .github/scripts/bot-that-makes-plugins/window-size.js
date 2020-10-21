// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

const fs = require('fs')

module.exports = () => {
  const match = process.env.COMMENT_BODY.match(
    /^Type: Window Size\r\n\r\nWidth: (\d+)\r\nHeight: (\d+)/
  )

  const code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stddef.h>

static const char* required_function_name = "vvctre_set_os_window_size";

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
    return &required_function_name;
}

VVCTRE_PLUGIN_EXPORT void PluginLoaded(void* core, void* plugin_manager,
                                       void* required_functions[]) {
    vvctre_set_os_window_size = (vvctre_set_os_window_size_t)required_functions[0];
    g_plugin_manager = plugin_manager;
}

VVCTRE_PLUGIN_EXPORT void InitialSettingsOpening() {
    vvctre_set_os_window_size(g_plugin_manager, ${match[1]}, ${match[2]});
}

VVCTRE_PLUGIN_EXPORT void EmulationStarting() {
  vvctre_set_os_window_size(g_plugin_manager, ${match[1]}, ${match[2]});
}
`

  fs.writeFileSync('plugin.c', code)
}
