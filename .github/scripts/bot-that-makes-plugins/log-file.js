// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

const fs = require('fs')

module.exports = () => {
  const match = process.env.COMMENT_BODY.match(/^Type: Log File\r\n\r\n(.+)/)

  const code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
  
#include <fstream>
#include <iostream>

#ifdef _WIN32
#define VVCTRE_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define VVCTRE_PLUGIN_EXPORT extern "C"
#endif

static std::ofstream file;

VVCTRE_PLUGIN_EXPORT int GetRequiredFunctionCount() {
    return 0;
}

VVCTRE_PLUGIN_EXPORT const char** GetRequiredFunctionNames() {
    return nullptr;
}

VVCTRE_PLUGIN_EXPORT void PluginLoaded(void* core, void* plugin_manager,
                                       void* required_functions[]) {
    file.open("${match[1].replace(/\\/g, '\\\\')}", std::ofstream::trunc);
}

VVCTRE_PLUGIN_EXPORT void Log(const char* line) {
    file << line << std::endl;
}
`

  fs.writeFileSync('plugin.cpp', code)
}
