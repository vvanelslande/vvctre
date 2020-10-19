// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

const custom_default_settings = require('./custom-default-settings')
const button_to_touch = require('./button-to-touch')
const window_position = require('./window-position')
const window_size = require('./window-size')

if (/^Type: Custom Default Settings\r\n\r\n/.test(process.env.COMMENT_BODY)) {
  custom_default_settings()
  console.log('custom-default-settings')
} else if (
  /^Type: Button To Touch\r\n\r\nX: (\d+)\r\nY: (\d+)\r\nParams: `(.+)`/.test(
    process.env.COMMENT_BODY
  )
) {
  button_to_touch()
  console.log('button-to-touch')
} else if (
  /^Type: Window Position\r\n\r\nX: (\d+)\r\nY: (\d+)/.test(
    process.env.COMMENT_BODY
  )
) {
  window_position()
  console.log('window-position')
} else if (
  /^Type: Window Size\r\n\r\nWidth: (\d+)\r\nHeight: (\d+)/.test(
    process.env.COMMENT_BODY
  )
) {
  window_size()
  console.log('window-size')
} else {
  process.exit(1)
}
