// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

const url = new URL(location.href)

if (url.searchParams.has('code')) {
  fetch('https://vvctre.dynv6.net:8831/pm11api/code-to-access-token', {
    headers: {
      'Content-Type': 'text/plain'
    },
    body: url.searchParams.get('code'),
    method: 'POST'
  })
    .then(response => response.text())
    .then(token => {
      if (!token) {
        location.href =
          'https://github.com/login/oauth/authorize?client_id=1df52b4366a6b5d52011&scope=public_repo,workflow'
        return
      }
      localStorage.setItem(
        'code_and_builds_user_github_account_plugin_maker_github_token',
        token
      )
      location.href =
        '/vvctre/plugin-maker/code-and-builds-user-github-account/'
    })
} else if (
  localStorage.getItem(
    'code_and_builds_user_github_account_plugin_maker_github_token'
  ) === null
) {
  location.href =
    'https://github.com/login/oauth/authorize?client_id=1df52b4366a6b5d52011&scope=public_repo,workflow'
}

const type = document.querySelector('#type')

type.addEventListener('change', () => {
  switch (type.options[type.selectedIndex].value) {
    case 'custom_default_settings': {
      document.querySelector('#custom_default_settings_div').style.display =
        'block'
      document.querySelector('#button_to_touch_div').style.display = 'none'
      document.querySelector('#window_size_div').style.display = 'none'
      document.querySelector('#window_position_div').style.display = 'none'
      document.querySelector('#log_file_div').style.display = 'none'
      break
    }
    case 'button_to_touch': {
      document.querySelector('#custom_default_settings_div').style.display =
        'none'
      document.querySelector('#button_to_touch_div').style.display = 'block'
      document.querySelector('#window_size_div').style.display = 'none'
      document.querySelector('#window_position_div').style.display = 'none'
      document.querySelector('#log_file_div').style.display = 'none'
      break
    }
    case 'window_size': {
      document.querySelector('#custom_default_settings_div').style.display =
        'none'
      document.querySelector('#button_to_touch_div').style.display = 'none'
      document.querySelector('#window_size_div').style.display = 'block'
      document.querySelector('#window_position_div').style.display = 'none'
      document.querySelector('#log_file_div').style.display = 'none'
      break
    }
    case 'window_position': {
      document.querySelector('#custom_default_settings_div').style.display =
        'none'
      document.querySelector('#button_to_touch_div').style.display = 'none'
      document.querySelector('#window_size_div').style.display = 'none'
      document.querySelector('#window_position_div').style.display = 'block'
      document.querySelector('#log_file_div').style.display = 'none'
      break
    }
    case 'log_file': {
      document.querySelector('#custom_default_settings_div').style.display =
        'none'
      document.querySelector('#button_to_touch_div').style.display = 'none'
      document.querySelector('#window_size_div').style.display = 'none'
      document.querySelector('#window_position_div').style.display = 'none'
      document.querySelector('#log_file_div').style.display = 'block'
      break
    }
    default: {
      break
    }
  }
})

document.querySelector('#makePlugin').addEventListener('click', async () => {
  document.querySelector('#makingPlugin').style.display = 'block'
  document.querySelector('#everything').style.display = 'none'

  let code = ''

  switch (type.options[type.selectedIndex].value) {
    case 'custom_default_settings': {
      const names = []
      const types = []
      const calls = []
      const regexes = getCdsRegexes(names, types, calls)

      const custom_default_settings_lines = document.querySelector(
        '#custom_default_settings_lines'
      )

      const validLines = custom_default_settings_lines.value
        .split('\n')
        .filter(line => regexes.some(regex => regex.regex.test(line)))

      if (validLines.length === 0) {
        alert('All the lines are invalid or the lines input is empty')
        document.querySelector('#makingPlugin').style.display = 'none'
        document.querySelector('#everything').style.display = 'block'
        return
      }

      const validLinesJoined = validLines.join('\n')

      custom_default_settings_lines.value = validLinesJoined

      for (const regex of regexes) {
        if (regex.regex.test(validLinesJoined)) {
          const matches = validLinesJoined.match(regex.regex)
          regex.call(matches)
        }
      }

      code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common_types.h"

#ifdef _WIN32
#define VVCTRE_PLUGIN_EXPORT __declspec(dllexport)
#else
#define VVCTRE_PLUGIN_EXPORT
#endif

${
  names.length === 1
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
      names.length === 1 ? '&required_function_name' : 'required_function_names'
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

      break
    }
    case 'button_to_touch': {
      code = `// Copyright 2020 Valentin Vanelslande
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
    g_device = vvctre_button_device_new(plugin_manager, "${
      document.querySelector('#button_to_touch_params').value
    }");
}

VVCTRE_PLUGIN_EXPORT void AfterSwapWindow() {
    static bool was_pressed = false;
    const bool pressed = vvctre_button_device_get_state(g_device);

    if (was_pressed && !pressed) {
        vvctre_use_real_touch_state(g_core);
        was_pressed = false;
    } else if (!was_pressed && pressed) {
        vvctre_set_custom_touch_state(g_core, ${
          Number(document.querySelector('#button_to_touch_x').value) / 319
        }, ${
        Number(document.querySelector('#button_to_touch_y').value) / 239
      }, true);
        was_pressed = true;
    }
}
`

      break
    }
    case 'window_size': {
      const width = document.querySelector('#window_size_width').value
      const height = document.querySelector('#window_size_height').value

      code = `// Copyright 2020 Valentin Vanelslande
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
    vvctre_set_os_window_size(g_plugin_manager, ${width}, ${height});
}

VVCTRE_PLUGIN_EXPORT void EmulationStarting() {
    vvctre_set_os_window_size(g_plugin_manager, ${width}, ${height});
}
`

      break
    }
    case 'window_position': {
      const x = document.querySelector('#window_position_x').value
      const y = document.querySelector('#window_position_y').value

      code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stddef.h>

static const char* required_function_name = "vvctre_set_os_window_position";

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
    return &required_function_name;
}

VVCTRE_PLUGIN_EXPORT void PluginLoaded(void* core, void* plugin_manager,
                                       void* required_functions[]) {
    vvctre_set_os_window_position = (vvctre_set_os_window_position_t)required_functions[0];
    g_plugin_manager = plugin_manager;
}

VVCTRE_PLUGIN_EXPORT void InitialSettingsOpening() {
    vvctre_set_os_window_position(g_plugin_manager, ${x}, ${y});
}

VVCTRE_PLUGIN_EXPORT void EmulationStarting() {
    vvctre_set_os_window_position(g_plugin_manager, ${x}, ${y});
}
`

      break
    }
    case 'log_file': {
      code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stdio.h>
#include <stddef.h>

#ifdef _WIN32
#define VVCTRE_PLUGIN_EXPORT __declspec(dllexport)
#else
#define VVCTRE_PLUGIN_EXPORT
#endif

static FILE* fp = NULL;

VVCTRE_PLUGIN_EXPORT int GetRequiredFunctionCount() {
    return 0;
}

VVCTRE_PLUGIN_EXPORT const char** GetRequiredFunctionNames() {
    return NULL;
}

VVCTRE_PLUGIN_EXPORT void PluginLoaded(void* core, void* plugin_manager,
                                       void* required_functions[]) {
    fp = fopen("${document
      .querySelector('#log_file_file_path')
      .value.replace(/\\/g, '\\\\')}", "w");
}

VVCTRE_PLUGIN_EXPORT void Log(const char* line) {
    fprintf(fp, "%s\\n", line);
}

VVCTRE_PLUGIN_EXPORT void EmulatorClosing() {
    fclose(fp);
    fp = NULL;
}
`

      break
    }
  }

  if (code) {
    const userResponse = await fetch('https://api.github.com/user', {
      headers: {
        Authorization: `token ${localStorage.getItem(
          'code_and_builds_user_github_account_plugin_maker_github_token'
        )}`
      }
    })

    const userJson = await userResponse.json()

    const generateResponse = await fetch(
      'https://api.github.com/repos/vvanelslande/vvctre-plugin-template-for-plugin-maker-1-1/generate',
      {
        headers: {
          Accept: 'application/vnd.github.baptiste-preview+json',
          Authorization: `token ${localStorage.getItem(
            'code_and_builds_user_github_account_plugin_maker_github_token'
          )}`,
          'Content-Type': 'text/plain'
        },
        body: JSON.stringify({
          owner: userJson.login,
          name: document.querySelector('#repository_name').value
        }),
        method: 'POST'
      }
    )

    if (!generateResponse.ok) {
      alert(
        `A repository called ${
          document.querySelector('#repository_name').value
        } already exists or localStorage.code_and_builds_user_github_account_plugin_maker_github_token is invalid`
      )
      document.querySelector('#makingPlugin').style.display = 'none'
      document.querySelector('#everything').style.display = 'block'
      return
    }

    await new Promise(resolve => {
      setTimeout(async function f() {
        try {
          await fetch(
            `https://api.github.com/repos/${userJson.login}/${
              document.querySelector('#repository_name').value
            }`,
            {
              headers: {
                Authorization: `token ${localStorage.getItem(
                  'code_and_builds_user_github_account_plugin_maker_github_token'
                )}`
              }
            }
          )
          resolve()
        } catch {
          setTimeout(f, 10000)
        }
      }, 10000)
    })

    await fetch(
      `https://api.github.com/repos/${userJson.login}/${
        document.querySelector('#repository_name').value
      }/contents/plugin.c`,
      {
        headers: {
          Authorization: `token ${localStorage.getItem(
            'code_and_builds_user_github_account_plugin_maker_github_token'
          )}`,
          'Content-Type': 'text/plain'
        },
        body: JSON.stringify({
          content: btoa(code),
          message: 'Add code',
          branch: 'master'
        }),
        method: 'PUT'
      }
    )

    await fetch(
      `https://api.github.com/repos/${userJson.login}/${
        document.querySelector('#repository_name').value
      }/actions/workflows/build.yml/dispatches`,
      {
        headers: {
          Authorization: `token ${localStorage.getItem(
            'code_and_builds_user_github_account_plugin_maker_github_token'
          )}`,
          'Content-Type': 'text/plain'
        },
        body: JSON.stringify({
          ref: 'master'
        }),
        method: 'POST'
      }
    )

    setTimeout(async function f() {
      const response = await fetch(
        `https://api.github.com/repos/${userJson.login}/${
          document.querySelector('#repository_name').value
        }/releases/latest`,
        {
          headers: {
            Authorization: `token ${localStorage.getItem(
              'code_and_builds_user_github_account_plugin_maker_github_token'
            )}`
          }
        }
      )

      if (response.ok) {
        const json = await response.json()
        if (json.assets.length === 4) {
          location.href = json.html_url
        } else {
          setTimeout(f, 5000)
        }
      } else {
        setTimeout(f, 5000)
      }
    }, 60000)
  }
})
