// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

const fs = require('fs')

module.exports = () => {
  const match = process.env.COMMENT_BODY.match(
    /^Type: Button To Touch\r\n\r\nX: (\d+)\r\nY: (\d+)\r\nParams: `(.+)`/
  )

  const code = `// Copyright 2020 Valentin Vanelslande
  // Licensed under GPLv2 or any later version
  // Refer to the license.txt file included.
  
  #include <stdbool.h>
  #include <stddef.h>
  
  static const char* required_function_names[] = {
      "vvctre_button_device_new",
      "vvctre_button_device_get_state",
      "vvctre_set_custom_touch_state",
      "vvctre_use_real_touch_state",
  };
  
  typedef void* (*vvctre_button_device_new_t)(void* plugin_manager, const char* params);
  typedef bool* (*vvctre_button_device_get_state_t)(void* device);
  typedef void (*vvctre_set_custom_touch_state_t)(void* core, float x, float y, bool pressed);
  typedef void (*vvctre_use_real_touch_state_t)(void* core);
  
  static vvctre_button_device_new_t vvctre_button_device_new;
  static vvctre_button_device_get_state_t vvctre_button_device_get_state;
  static vvctre_set_custom_touch_state_t vvctre_set_custom_touch_state;
  static vvctre_use_real_touch_state_t vvctre_use_real_touch_state;
  
  static void* g_core = NULL;
  static void* g_device = NULL;
  
  #ifdef _WIN32
  #define VVCTRE_PLUGIN_EXPORT __declspec(dllexport)
  #else
  #define VVCTRE_PLUGIN_EXPORT
  #endif
  
  VVCTRE_PLUGIN_EXPORT int GetRequiredFunctionCount() {
      return 4;
  }
  
  VVCTRE_PLUGIN_EXPORT const char** GetRequiredFunctionNames() {
      return required_function_names;
  }
  
  VVCTRE_PLUGIN_EXPORT void PluginLoaded(void* core, void* plugin_manager,
                                         void* required_functions[]) {
      vvctre_button_device_new = (vvctre_button_device_new_t)required_functions[0];
      vvctre_button_device_get_state = (vvctre_button_device_get_state_t)required_functions[1];
      vvctre_set_custom_touch_state = (vvctre_set_custom_touch_state_t)required_functions[2];
      vvctre_use_real_touch_state = (vvctre_use_real_touch_state_t)required_functions[3];
  
      g_core = core;
      g_device = vvctre_button_device_new(plugin_manager, "${match[3]}");
  }
  
  VVCTRE_PLUGIN_EXPORT void AfterSwapWindow() {
      static bool was_pressed = false;
      const bool pressed = vvctre_button_device_get_state(g_device);
  
      if (was_pressed && !pressed) {
          vvctre_use_real_touch_state(g_core);
          was_pressed = false;
      } else if (!was_pressed && pressed) {
          vvctre_set_custom_touch_state(g_core, ${match[1] / 319}, ${
    match[2] / 239
  }, true);
          was_pressed = true;
      }
  }
  `

  fs.writeFileSync('plugin.c', code)
}
