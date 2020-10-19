// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

const fs = require('fs')

module.exports = () => {
  const match = process.env.COMMENT_BODY.match(
    /^Type: Window Position\r\n\r\nX: (\d+)\r\nY: (\d+)/
  )

  const code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stddef.h>

static const char* required_function_names[] = { "vvctre_set_os_window_position", NULL };

typedef void (*vvctre_set_os_window_position_t)(void* plugin_manager, int x, int y);

static vvctre_set_os_window_position_t vvctre_set_os_window_position;

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
    vvctre_set_os_window_position = (vvctre_set_os_window_position_t)required_functions[0];
    g_plugin_manager = plugin_manager;
}

VVCTRE_PLUGIN_EXPORT void InitialSettingsOpening() {
    vvctre_set_os_window_position(g_plugin_manager, ${match[1]}, ${match[2]});
}

VVCTRE_PLUGIN_EXPORT void EmulationStarting() {
  vvctre_set_os_window_position(g_plugin_manager, ${match[1]}, ${match[2]});
}
`

  fs.writeFileSync('plugin.c', code)
}
