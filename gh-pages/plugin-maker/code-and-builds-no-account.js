// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

let makingPlugin = false
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
  document.querySelector('#notMakingPlugin').style.display = 'none'

  let url = ''
  let body = ''

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
        document.querySelector('#notMakingPlugin').style.display = 'block'
        return
      }

      const validLinesJoined = validLines.join('\n')

      custom_default_settings_lines.value = validLinesJoined

      url =
        'https://d42fcfc3-and-611c56b1.vvanelslande.dynv6.net:30317/make-custom-default-settings-plugin'

      body = validLinesJoined

      break
    }
    case 'button_to_touch': {
      url =
        'https://d42fcfc3-and-611c56b1.vvanelslande.dynv6.net:30317/make-button-to-touch-plugin'

      body = JSON.stringify({
        x: Number(document.querySelector('#button_to_touch_x').value),
        y: Number(document.querySelector('#button_to_touch_y').value),
        params: document.querySelector('#button_to_touch_params').value
      })

      break
    }
    case 'window_size': {
      url =
        'https://d42fcfc3-and-611c56b1.vvanelslande.dynv6.net:30317/make-window-size-plugin'

      body = JSON.stringify({
        width: Number(document.querySelector('#window_size_width').value),
        height: Number(document.querySelector('#window_size_height').value)
      })

      break
    }
    case 'window_position': {
      url =
        'https://d42fcfc3-and-611c56b1.vvanelslande.dynv6.net:30317/make-window-position-plugin'

      body = JSON.stringify({
        x: Number(document.querySelector('#window_position_x').value),
        y: Number(document.querySelector('#window_position_y').value)
      })

      break
    }
    case 'log_file': {
      url =
        'https://d42fcfc3-and-611c56b1.vvanelslande.dynv6.net:30317/make-log-file-plugin'
      body = document.querySelector('#log_file_file_path').value

      break
    }
  }

  const response = await fetch(url, {
    headers: {
      'Content-Type': 'text/plain'
    },
    body,
    method: 'POST'
  })

  const blob = await response.blob()

  saveAs(blob, 'plugin.zip')

  document.querySelector('#makingPlugin').style.display = 'none'
  document.querySelector('#notMakingPlugin').style.display = 'block'
})
