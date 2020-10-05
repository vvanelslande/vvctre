// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

const http = require('http')
const fs = require('fs')
const path = require('path')
const getRegexes = require('./.github/scripts/common/custom-default-settings-plugin-request-regexes')

const server = http
  .createServer((req, res) => {
    if (req.method !== 'POST') {
      res.writeHead(405, {
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'POST',
        'Access-Control-Allow-Headers': '*'
      })
      res.end()
      return
    }

    let body = ''

    req.on('data', chunk => {
      body += chunk.toString()
    })

    req.on('end', () => {
      switch (req.url) {
        case '/customdefaultsettings':
          let matches = 0
          const names = []
          const types = []
          const calls = []

          getRegexes(names, types, calls).forEach(test => {
            if (test.regex.test(body)) {
              const match = body.match(test.regex)
              test.call(match)

              ++matches
            }
          })

          if (matches === 0) {
            res.writeHead(400, {
              'Content-Type': 'text/plain',
              'Access-Control-Allow-Origin': '*',
              'Access-Control-Allow-Methods': 'POST',
              'Access-Control-Allow-Headers': '*'
            })
            res.write('No matches')
            res.end()
            return
          }

          if (matches === 1) {
            names.push(null)
          }

          const code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// License file text is after InitialSettingsOpening

#include <stdbool.h>
#include <stddef.h>

/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    common_types.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-02-11
 * @brief   Common types used throughout the project
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include <stdint.h>

typedef uint8_t u8;   ///< 8-bit unsigned byte
typedef uint16_t u16; ///< 16-bit unsigned short
typedef uint32_t u32; ///< 32-bit unsigned word
typedef uint64_t u64; ///< 64-bit unsigned int

typedef int8_t s8;   ///< 8-bit signed byte
typedef int16_t s16; ///< 16-bit signed short
typedef int32_t s32; ///< 32-bit signed word
typedef int64_t s64; ///< 64-bit signed int

typedef float f32;  ///< 32-bit floating point
typedef double f64; ///< 64-bit floating point

typedef u32 VAddr; ///< Represents a pointer in the userspace virtual address space.
typedef u32 PAddr; ///< Represents a pointer in the ARM11 physical address space.

////////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#define VVCTRE_PLUGIN_EXPORT __declspec(dllexport)
#else
#define VVCTRE_PLUGIN_EXPORT
#endif

static const char* required_function_names[] = {
${names
  .map(name => (name === null ? '    NULL,' : `    "${name}",`))
  .join('\n')}
};

${names
  .filter(Boolean)
  .map(
    (name, index) =>
      `typedef ${types[index][0]} (*${name}_t)(${types[index][1]});\nstatic ${name}_t ${name};`
  )
  .join('\n')}

VVCTRE_PLUGIN_EXPORT int GetRequiredFunctionCount() {
    return ${names.length};
}

VVCTRE_PLUGIN_EXPORT const char** GetRequiredFunctionNames() {
    return required_function_names;
}
    
VVCTRE_PLUGIN_EXPORT void PluginLoaded(void* core, void* plugin_manager, void* required_functions[]) {
${names
  .filter(Boolean)
  .map(
    (name, index) => `    ${name} = (${name}_t)required_functions[${index}];`
  )
  .join('\n')}
}

VVCTRE_PLUGIN_EXPORT void InitialSettingsOpening() {
${calls.map(call => `    ${call}`).join('\n')}
}

/*
License:

${fs.readFileSync(path.resolve('license.txt'))}
*/
`

          res.writeHead(200, {
            'Content-Type': 'text/x-c',
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'POST',
            'Access-Control-Allow-Headers': '*'
          })
          res.write(code)
          res.end()

          break

        case '/buttontotouch':
          try {
            const json = JSON.parse(body)

            const code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// License file text is after AfterSwapWindow

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
    g_device = vvctre_button_device_new(plugin_manager, "${json.params}");
}

VVCTRE_PLUGIN_EXPORT void AfterSwapWindow() {
    static bool was_pressed = false;
    const bool pressed = vvctre_button_device_get_state(g_device);

    if (was_pressed && !pressed) {
        vvctre_use_real_touch_state(g_core);
        was_pressed = false;
    } else if (!was_pressed && pressed) {
        vvctre_set_custom_touch_state(g_core, ${json.x / 319}, ${
              json.y / 239
            }, true);
        was_pressed = true;
    }
}

/*
License:

${fs.readFileSync(path.resolve('license.txt'))}
*/
`

            res.writeHead(200, {
              'Content-Type': 'text/x-c',
              'Access-Control-Allow-Origin': '*',
              'Access-Control-Allow-Methods': 'POST',
              'Access-Control-Allow-Headers': '*'
            })
            res.write(code)
            res.end()
          } catch {
            res.writeHead(400, {
              'Access-Control-Allow-Origin': '*',
              'Access-Control-Allow-Methods': 'POST',
              'Access-Control-Allow-Headers': '*'
            })
            res.end()
            return
          }

          break

        case '/windowposition':
          try {
            const json = JSON.parse(body)

            const code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// License file text is after EmulationStarting

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
    vvctre_set_os_window_position(g_plugin_manager, ${json.x}, ${json.y});
}

VVCTRE_PLUGIN_EXPORT void EmulationStarting() {
  vvctre_set_os_window_position(g_plugin_manager, ${json.x}, ${json.y});
}

/*
License:

${fs.readFileSync(path.resolve('license.txt'))}
*/
`

            res.writeHead(200, {
              'Content-Type': 'text/x-c',
              'Access-Control-Allow-Origin': '*',
              'Access-Control-Allow-Methods': 'POST',
              'Access-Control-Allow-Headers': '*'
            })
            res.write(code)
            res.end()
          } catch {
            res.writeHead(400, {
              'Access-Control-Allow-Origin': '*',
              'Access-Control-Allow-Methods': 'POST',
              'Access-Control-Allow-Headers': '*'
            })
            res.end()
            return
          }

          break

        case '/windowsize':
          try {
            const json = JSON.parse(body)

            const code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// License file text is after EmulationStarting

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
    vvctre_set_os_window_size(g_plugin_manager, ${json.width}, ${json.height});
}

VVCTRE_PLUGIN_EXPORT void EmulationStarting() {
  vvctre_set_os_window_size(g_plugin_manager, ${json.width}, ${json.height});
}

/*
License:

${fs.readFileSync(path.resolve('license.txt'))}
*/
`

            res.writeHead(200, {
              'Content-Type': 'text/x-c',
              'Access-Control-Allow-Origin': '*',
              'Access-Control-Allow-Methods': 'POST',
              'Access-Control-Allow-Headers': '*'
            })
            res.write(code)
            res.end()
          } catch {
            res.writeHead(400, {
              'Access-Control-Allow-Origin': '*',
              'Access-Control-Allow-Methods': 'POST',
              'Access-Control-Allow-Headers': '*'
            })
            res.end()
            return
          }

          break

        default:
          res.writeHead(400, {
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'POST',
            'Access-Control-Allow-Headers': '*'
          })
          res.end()
          break
      }
    })
  })
  .listen(process.env.PORT ? process.env.PORT : 0, () => {
    console.log('Port:', server.address().port)
  })
