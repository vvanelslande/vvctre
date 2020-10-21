// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

const fs = require('fs')
const getRegexes = require('../common/custom-default-settings-regexes')

module.exports = () => {
  let matches = 0
  const names = []
  const types = []
  const calls = []

  getRegexes(names, types, calls).forEach(test => {
    if (test.regex.test(process.env.COMMENT_BODY)) {
      const match = process.env.COMMENT_BODY.match(test.regex)
      test.call(match)

      ++matches
    }
  })

  const code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

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

${
  matches === 1
    ? `static const char* required_function_name = "${names[0]}";`
    : `static const char* required_function_names[] = {\n${names
        .map(name => `    "${name}",`)
        .join('\n')}\n};`
}

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
    return ${
      matches === 1 ? '&required_function_name' : 'required_function_names'
    };
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
`

  fs.writeFileSync('plugin.c', code)
}
