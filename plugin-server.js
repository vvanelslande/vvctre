// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

/**
 * Only POST is allowed
 * You can use vvctre-plugin-server.glitch.me if you don't want to self-host.
 *
 * /customdefaultsettings
 *    Request body: See https://vvanelslande.github.io/vvctre/custom-default-settings-plugin-bot-and-server-line-examples/
 *    Response body: code
 *
 * /buttontotouch
 *    Request body: JSON with params, x, and y (example: { "params": "engine:null", "x": 100, "y": 100 })
 *    Response body: code
 *
 * /windowposition
 *    Request body: JSON with x and y (example: { "x": 100, "y": 100 })
 *    Response body: code
 *
 * /windowsize
 *    Request body: JSON with width and height (example: { "width": 100, "height": 100 })
 *    Response body: code
 *
 * /logfile
 *    Request body: File path
 *    Response body: code
 */

const http = require('http')
const fs = require('fs')
const path = require('path')

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
        case '/customdefaultsettings': {
          let matches = 0
          const names = []
          const types = []
          const calls = []

          const regexes = [
            {
              regex: /^start.file (.+)$/m,
              call: match => {
                names.push('vvctre_settings_set_file_path')
                types.push(['void', 'const char* value'])
                calls.push(
                  `vvctre_settings_set_file_path("${match[1].replace(
                    /\\/g,
                    '\\\\'
                  )}");`
                )
              }
            },
            {
              regex: /^start.play_movie (.+)$/m,
              call: match => {
                names.push('vvctre_settings_set_play_movie')
                types.push(['void', 'const char* value'])
                calls.push(`vvctre_settings_set_play_movie("${match[1]}");`)
              }
            },
            {
              regex: /^start.record_movie (.+)$/m,
              call: match => {
                names.push('vvctre_settings_set_record_movie')
                types.push(['void', 'const char* value'])
                calls.push(`vvctre_settings_set_record_movie("${match[1]}");`)
              }
            },
            {
              regex: /^start.region (Japan|USA|Europe|Australia|China|Korea|Taiwan)$/m,
              call: match => {
                names.push('vvctre_settings_set_region_value')
                types.push(['void', 'int value'])
                calls.push(
                  `vvctre_settings_set_region_value(${
                    {
                      Japan: 0,
                      USA: 1,
                      Europe: 2,
                      Australia: 3,
                      China: 4,
                      Korea: 5,
                      Taiwan: 6
                    }[match[1]]
                  });`
                )
              }
            },
            {
              regex: /^start.log_filter (.+)$/m,
              call: match => {
                names.push('vvctre_settings_set_log_filter')
                types.push(['void', 'const char* value'])
                calls.push(`vvctre_settings_set_log_filter("${match[1]}");`)
              }
            },
            {
              regex: /^start.initial_time Unix Timestamp$/m,
              call: () => {
                names.push('vvctre_settings_set_initial_clock')
                types.push(['void', 'int value'])
                calls.push(`vvctre_settings_set_initial_clock(1);`)
              }
            },
            {
              regex: /^start.unix_timestamp (\d+)$/m,
              call: match => {
                names.push('vvctre_settings_set_unix_timestamp')
                types.push('void', 'u64 value')
                calls.push(`vvctre_settings_set_unix_timestamp(${match[1]});`)
              }
            },
            {
              regex: /^start.use_virtual_sd_card disable$/m,
              call: () => {
                names.push('vvctre_settings_set_use_virtual_sd')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_use_virtual_sd(false);')
              }
            },
            {
              regex: /^start.record_frame_times enable$/m,
              call: () => {
                names.push('vvctre_settings_set_record_frame_times')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_record_frame_times(true);')
              }
            },
            {
              regex: /^start.gdb_stub enable (\d+)$/m,
              call: match => {
                names.push('vvctre_settings_enable_gdbstub')
                types.push(['void', 'u16 port'])
                calls.push(`vvctre_settings_enable_gdbstub(${match[1]});`)
              }
            },
            {
              regex: /^general.cpu_jit disable$/m,
              call: () => {
                names.push('vvctre_settings_set_use_cpu_jit')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_use_cpu_jit(false);')
              }
            },
            {
              regex: /^general.limit_speed disable$/m,
              call: () => {
                names.push('vvctre_settings_set_limit_speed')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_limit_speed(false);')
              }
            },
            {
              regex: /^general.enable_custom_cpu_ticks enable$/m,
              call: () => {
                names.push('vvctre_settings_set_use_custom_cpu_ticks')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_use_custom_cpu_ticks(true);')
              }
            },
            {
              regex: /^general.speed_limit (\d+)%?$/m,
              call: match => {
                names.push('vvctre_settings_set_speed_limit')
                types.push(['void', 'u16 value'])
                calls.push(`vvctre_settings_set_speed_limit(${match[1]});`)
              }
            },
            {
              regex: /^general.custom_cpu_ticks (\d+)$/m,
              call: match => {
                names.push('vvctre_settings_set_custom_cpu_ticks')
                types.push(['void', 'u64 value'])
                calls.push(`vvctre_settings_set_custom_cpu_ticks(${match[1]});`)
              }
            },
            {
              regex: /^general.cpu_clock_percentage (\d+)%?$/m,
              call: match => {
                names.push('vvctre_settings_set_cpu_clock_percentage')
                types.push(['void', 'u32 value'])
                calls.push(
                  `vvctre_settings_set_cpu_clock_percentage(${match[1]});`
                )
              }
            },
            {
              regex: /^audio.dsp_lle enable$/m,
              call: () => {
                names.push('vvctre_settings_set_enable_dsp_lle')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_enable_dsp_lle(true);')
              }
            },
            {
              regex: /^audio.dsp_lle_multiple_threads enable$/m,
              call: () => {
                names.push('vvctre_settings_set_enable_dsp_lle_multithread')
                types.push(['void', 'bool value'])
                calls.push(
                  'vvctre_settings_set_enable_dsp_lle_multithread(true);'
                )
              }
            },
            {
              regex: /^audio.stretching disable$/m,
              call: () => {
                names.push('vvctre_settings_set_enable_audio_stretching')
                types.push(['void', 'bool value'])
                calls.push(
                  'vvctre_settings_set_enable_audio_stretching(false);'
                )
              }
            },
            {
              regex: /^audio.volume (\d*\.?\d+f?)$/m,
              call: match => {
                names.push('vvctre_settings_set_audio_volume')
                types.push(['void', 'float value'])
                calls.push(`vvctre_settings_set_audio_volume(${match[1]});`)
              }
            },
            {
              regex: /^audio.sink (cubeb|sdl2|null)$/m,
              call: match => {
                names.push('vvctre_settings_set_audio_sink_id')
                types.push(['void', 'const char* value'])
                calls.push(`vvctre_settings_set_audio_sink_id("${match[1]}");`)
              }
            },
            {
              regex: /^audio.device (.+)$/m,
              call: match => {
                names.push('vvctre_settings_set_audio_device_id')
                types.push(['void', 'const char* value'])
                calls.push(
                  `vvctre_settings_set_audio_device_id("${match[1]}");`
                )
              }
            },
            {
              regex: /^audio.microphone_input_type (Real Device|Static Noise)$/m,
              call: match => {
                names.push('vvctre_settings_set_microphone_input_type')
                types.push(['void', 'int value'])
                calls.push(
                  `vvctre_settings_set_microphone_input_type(${
                    { 'Real Device': 1, 'Static Noise': 2 }[match[1]]
                  });`
                )
              }
            },
            {
              regex: /^audio.microphone_real_device_backend (Cubeb|SDL2|Null)$/m,
              call: match => {
                names.push('vvctre_settings_set_microphone_real_device_backend')
                types.push(['void', 'u8 value'])
                calls.push(
                  `vvctre_settings_set_microphone_real_device_backend(${
                    { Cubeb: 1, SDL2: 2, Null: 3 }[match[1]]
                  });`
                )
              }
            },
            {
              regex: /^audio.microphone_device (.+)$/m,
              call: match => {
                names.push('vvctre_settings_set_microphone_device')
                types.push(['void', 'const char* value'])
                calls.push(
                  `vvctre_settings_set_microphone_device("${match[1]}");`
                )
              }
            },
            {
              regex: /^camera.inner_engine image/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_camera_engine')) {
                  names.push('vvctre_settings_set_camera_engine')
                  types.push(['void', 'int index, const char* value'])
                }
                calls.push(`vvctre_settings_set_camera_engine(1, "image");`)
              }
            },
            {
              regex: /^camera.inner_parameter (.+)$/m,
              call: match => {
                if (!names.includes('vvctre_settings_set_camera_parameter')) {
                  names.push('vvctre_settings_set_camera_parameter')
                  types.push(['void', 'int index, const char* value'])
                }
                calls.push(
                  `vvctre_settings_set_camera_parameter(1, "${match[1]}");`
                )
              }
            },
            {
              regex: /^camera.outer_left_engine image$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_camera_engine')) {
                  names.push('vvctre_settings_set_camera_engine')
                  types.push(['void', 'int index, const char* value'])
                }
                calls.push(`vvctre_settings_set_camera_engine(2, "image");`)
              }
            },
            {
              regex: /^camera.outer_left_parameter (.+)$/m,
              call: match => {
                if (!names.includes('vvctre_settings_set_camera_parameter')) {
                  names.push('vvctre_settings_set_camera_parameter')
                  types.push(['void', 'int index, const char* value'])
                }
                calls.push(
                  `vvctre_settings_set_camera_parameter(2, "${match[1]}");`
                )
              }
            },
            {
              regex: /^camera.outer_right_engine image$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_camera_engine')) {
                  names.push('vvctre_settings_set_camera_engine')
                  types.push(['void', 'int index, const char* value'])
                }
                calls.push(`vvctre_settings_set_camera_engine(0, "image");`)
              }
            },
            {
              regex: /^camera.outer_right_parameter (.+)$/m,
              call: match => {
                if (!names.includes('vvctre_settings_set_camera_parameter')) {
                  names.push('vvctre_settings_set_camera_parameter')
                  types.push(['void', 'int index, const char* value'])
                }
                calls.push(
                  `vvctre_settings_set_camera_parameter(0, "${match[1]}");`
                )
              }
            },
            {
              regex: /^system.play_coins (\d+)$/m,
              call: match => {
                if (!names.includes('vvctre_set_play_coins')) {
                  names.push('vvctre_set_play_coins')
                  types.push(['void', 'u16 value'])
                }
                calls.push(`vvctre_set_play_coins(${match[1]});`)
              }
            },
            {
              regex: /^graphics.hardware_renderer disable$/m,
              call: () => {
                names.push('vvctre_settings_set_use_hardware_renderer')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_use_hardware_renderer(false);')
              }
            },
            {
              regex: /^graphics.hardware_shader disable$/m,
              call: () => {
                names.push('vvctre_settings_set_use_hardware_shader')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_use_hardware_shader(false);')
              }
            },
            {
              regex: /^graphics.hardware_shader_accurate_multiplication enable$/m,
              call: () => {
                names.push(
                  'vvctre_settings_set_hardware_shader_accurate_multiplication'
                )
                types.push(['void', 'bool value'])
                calls.push(
                  'vvctre_settings_set_hardware_shader_accurate_multiplication(true);'
                )
              }
            },
            {
              regex: /^graphics.shader_jit disable$/m,
              call: () => {
                names.push('vvctre_settings_set_use_shader_jit')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_use_shader_jit(false);')
              }
            },
            {
              regex: /^graphics.vsync enable$/m,
              call: () => {
                names.push('vvctre_settings_set_enable_vsync')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_enable_vsync(true);')
              }
            },
            {
              regex: /^graphics.dump_textures enable$/m,
              call: () => {
                names.push('vvctre_settings_set_dump_textures')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_dump_textures(true);')
              }
            },
            {
              regex: /^graphics.custom_textures enable$/m,
              call: () => {
                names.push('vvctre_settings_set_custom_textures')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_custom_textures(true);')
              }
            },
            {
              regex: /^graphics.preload_custom_textures enable$/m,
              call: () => {
                names.push('vvctre_settings_set_preload_textures')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_preload_textures(true);')
              }
            },
            {
              regex: /^graphics.linear_filtering disable$/m,
              call: () => {
                names.push('vvctre_settings_set_enable_linear_filtering')
                types.push(['void', 'bool value'])
                calls.push(
                  'vvctre_settings_set_enable_linear_filtering(false);'
                )
              }
            },
            {
              regex: /^graphics.sharper_distant_objects enable$/m,
              call: () => {
                names.push('vvctre_settings_set_sharper_distant_objects')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_sharper_distant_objects(true);')
              }
            },
            {
              regex: /^graphics.background_color (?:#)(\S\S\S\S\S\S)$/m,
              call: match => {
                names.push('vvctre_settings_set_background_color_red')
                names.push('vvctre_settings_set_background_color_green')
                names.push('vvctre_settings_set_background_color_blue')
                types.push(['void', 'float value'])
                types.push(['void', 'float value'])
                types.push(['void', 'float value'])
                calls.push(
                  `vvctre_settings_set_background_color_red(${
                    Number.parseInt(match[1].slice(0, 2), 16) / 255
                  });`
                )
                calls.push(
                  `vvctre_settings_set_background_color_green(${
                    Number.parseInt(match[1].slice(2, 4), 16) / 255
                  });`
                )
                calls.push(
                  `vvctre_settings_set_background_color_blue(${
                    Number.parseInt(match[1].slice(4, 6), 16) / 255
                  });`
                )
              }
            },
            {
              regex: /^graphics.resolution (\d+|Window Size)$/m,
              call: match => {
                names.push('vvctre_settings_set_resolution')
                types.push(['void', 'u16 value'])
                calls.push(
                  match[1] === 'Window Size'
                    ? 'vvctre_settings_set_resolution(0);'
                    : `vvctre_settings_set_resolution(${match[1]});`
                )
              }
            },
            {
              regex: /^graphics.post_processing_shader (.+)$/m,
              call: match => {
                names.push('vvctre_settings_set_post_processing_shader')
                types.push(['void', 'const char* value'])
                calls.push(
                  `vvctre_settings_set_post_processing_shader("${match[1]}");`
                )
              }
            },
            {
              regex: /^graphics.texture_filter (Anime4K Ultrafast|Bicubic|ScaleForce|xBRZ freescale)$/m,
              call: match => {
                names.push('vvctre_settings_set_texture_filter')
                types.push(['void', 'const char* value'])
                calls.push(`vvctre_settings_set_texture_filter("${match[1]}");`)
              }
            },
            {
              regex: /^graphics.3d_mode (Side by Side|Anaglyph|Interlaced)$/m,
              call: match => {
                names.push('vvctre_settings_set_render_3d')
                types.push(['void', 'int value'])
                calls.push(
                  `vvctre_settings_set_render_3d(${
                    {
                      Off: 0,
                      'Side by Side': 1,
                      Anaglyph: 2,
                      Interlaced: 3
                    }[match[1]]
                  });`
                )
              }
            },
            {
              regex: /^graphics.3d_factor (\d+)%?$/m,
              call: match => {
                names.push('vvctre_settings_set_factor_3d')
                types.push(['void', 'u8 value'])
                calls.push(`vvctre_settings_set_factor_3d(${match[1]});`)
              }
            },
            {
              regex: /^layout.layout (Single Screen|Large Screen|Side by Side|Medium Screen)$/m,
              call: match => {
                names.push('vvctre_settings_set_layout')
                types.push(['void', 'int value'])
                calls.push(
                  `vvctre_settings_set_layout(${
                    {
                      'Single Screen': 1,
                      'Large Screen': 2,
                      'Side by Side': 3,
                      'Medium Screen': 4
                    }[match[1]]
                  });`
                )
              }
            },
            {
              regex: /^layout.use_custom_layout enable$/m,
              call: () => {
                names.push('vvctre_settings_set_use_custom_layout')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_use_custom_layout(true);')
              }
            },
            {
              regex: /^layout.swap_screens enable$/m,
              call: () => {
                names.push('vvctre_settings_set_swap_screens')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_swap_screens(true);')
              }
            },
            {
              regex: /^layout.upright_screens enable$/m,
              call: () => {
                names.push('vvctre_settings_set_upright_screens')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_upright_screens(true);')
              }
            },
            {
              regex: /^layout.top_left (\d+)$/m,
              call: match => {
                names.push('vvctre_settings_set_custom_layout_top_left')
                types.push(['void', 'u16 value'])
                calls.push(
                  `vvctre_settings_set_custom_layout_top_left(${match[1]});`
                )
              }
            },
            {
              regex: /^layout.top_top (\d+)$/m,
              call: match => {
                names.push('vvctre_settings_set_custom_layout_top_top')
                types.push(['void', 'u16 value'])
                calls.push(
                  `vvctre_settings_set_custom_layout_top_top(${match[1]});`
                )
              }
            },
            {
              regex: /^layout.top_right (\d+)$/m,
              call: match => {
                names.push('vvctre_settings_set_custom_layout_top_right')
                types.push(['void', 'u16 value'])
                calls.push(
                  `vvctre_settings_set_custom_layout_top_right(${match[1]});`
                )
              }
            },
            {
              regex: /^layout.top_bottom (\d+)$/m,
              call: match => {
                names.push('vvctre_settings_set_custom_layout_top_bottom')
                types.push(['void', 'u16 value'])
                calls.push(
                  `vvctre_settings_set_custom_layout_top_bottom(${match[1]});`
                )
              }
            },
            {
              regex: /^layout.bottom_left (\d+)$/m,
              call: match => {
                names.push('vvctre_settings_set_custom_layout_bottom_left')
                types.push(['void', 'u16 value'])
                calls.push(
                  `vvctre_settings_set_custom_layout_bottom_left(${match[1]});`
                )
              }
            },
            {
              regex: /^layout.bottom_top (\d+)$/m,
              call: match => {
                names.push('vvctre_settings_set_custom_layout_bottom_top')
                types.push(['void', 'u16 value'])
                calls.push(
                  `vvctre_settings_set_custom_layout_bottom_top(${match[1]});`
                )
              }
            },
            {
              regex: /^layout.bottom_right (\d+)$/m,
              call: match => {
                names.push('vvctre_settings_set_custom_layout_bottom_right')
                types.push(['void', 'u16 value'])
                calls.push(
                  `vvctre_settings_set_custom_layout_bottom_right(${match[1]});`
                )
              }
            },
            {
              regex: /^layout.bottom_bottom (\d+)$/m,
              call: match => {
                names.push('vvctre_settings_set_custom_layout_bottom_bottom')
                types.push(['void', 'u16 value'])
                calls.push(
                  `vvctre_settings_set_custom_layout_bottom_bottom(${match[1]});`
                )
              }
            },
            {
              regex: /^multiplayer.ip (.+)$/m,
              call: match => {
                names.push('vvctre_settings_set_multiplayer_ip')
                types.push(['void', 'const char* value'])
                calls.push(`vvctre_settings_set_multiplayer_ip("${match[1]}");`)
              }
            },
            {
              regex: /^multiplayer.port (\d+)$/m,
              call: match => {
                names.push('vvctre_settings_set_multiplayer_port')
                types.push(['void', 'u16 value'])
                calls.push(`vvctre_settings_set_multiplayer_port(${match[1]});`)
              }
            },
            {
              regex: /^multiplayer.nickname (.+)$/m,
              call: match => {
                names.push('vvctre_settings_set_nickname')
                types.push(['void', 'const char* value'])
                calls.push(`vvctre_settings_set_nickname("${match[1]}");`)
              }
            },
            {
              regex: /^lle.spi enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("SPI", true);')
              }
            },
            {
              regex: /^lle.gpio enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("GPIO", true);')
              }
            },
            {
              regex: /^lle.mp enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("MP", true);')
              }
            },
            {
              regex: /^lle.cdc enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("CDC", true);')
              }
            },
            {
              regex: /^lle.http enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("HTTP", true);')
              }
            },
            {
              regex: /^lle.csnd enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("CSND", true);')
              }
            },
            {
              regex: /^lle.ns enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("NS", true);')
              }
            },
            {
              regex: /^lle.nfc enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("NFC", true);')
              }
            },
            {
              regex: /^lle.ptm enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("PTM", true);')
              }
            },
            {
              regex: /^lle.news enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("NEWS", true);')
              }
            },
            {
              regex: /^lle.ndm enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("NDM", true);')
              }
            },
            {
              regex: /^lle.mic enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("MIC", true);')
              }
            },
            {
              regex: /^lle.i2c enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("I2C", true);')
              }
            },
            {
              regex: /^lle.ir enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("IR", true);')
              }
            },
            {
              regex: /^lle.pdn enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("PDN", true);')
              }
            },
            {
              regex: /^lle.nim enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("NIM", true);')
              }
            },
            {
              regex: /^lle.hid enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("HID", true);')
              }
            },
            {
              regex: /^lle.gsp enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("GSP", true);')
              }
            },
            {
              regex: /^lle.frd enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("FRD", true);')
              }
            },
            {
              regex: /^lle.cfg enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("CFG", true);')
              }
            },
            {
              regex: /^lle.ps enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("PS", true);')
              }
            },
            {
              regex: /^lle.cecd enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("CECD", true);')
              }
            },
            {
              regex: /^lle.dsp enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("DSP", true);')
              }
            },
            {
              regex: /^lle.cam enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("CAM", true);')
              }
            },
            {
              regex: /^lle.mcu enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("MCU", true);')
              }
            },
            {
              regex: /^lle.ssl enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("SSL", true);')
              }
            },
            {
              regex: /^lle.boss enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("BOSS", true);')
              }
            },
            {
              regex: /^lle.act enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("ACT", true);')
              }
            },
            {
              regex: /^lle.ac enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("AC", true);')
              }
            },
            {
              regex: /^lle.am enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("AM", true);')
              }
            },
            {
              regex: /^lle.err enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("ERR", true);')
              }
            },
            {
              regex: /^lle.pxi enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("PXI", true);')
              }
            },
            {
              regex: /^lle.nwm enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("NWM", true);')
              }
            },
            {
              regex: /^lle.dlp enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("DLP", true);')
              }
            },
            {
              regex: /^lle.ldr enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("LDR", true);')
              }
            },
            {
              regex: /^lle.pm enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("PM", true);')
              }
            },
            {
              regex: /^lle.soc enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("SOC", true);')
              }
            },
            {
              regex: /^lle.fs enable$/m,
              call: () => {
                if (!names.includes('vvctre_settings_set_use_lle_module')) {
                  names.push('vvctre_settings_set_use_lle_module')
                  types.push(['void', 'const char* name, bool value'])
                }
                calls.push('vvctre_settings_set_use_lle_module("FS", true);')
              }
            },
            {
              regex: /^hacks.priority_boost disable$/m,
              call: () => {
                names.push('vvctre_settings_set_enable_priority_boost')
                types.push(['void', 'bool value'])
                calls.push('vvctre_settings_set_enable_priority_boost(false);')
              }
            }
          ]

          for (const regex of regexes) {
            if (regex.regex.test(body)) {
              const match = body.match(regex.regex)
              regex.call(match)

              ++matches
            }
          }

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

/*
License:

${fs.readFileSync(path.resolve(__dirname, 'license.txt'))}
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
        }

        case '/buttontotouch': {
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

${fs.readFileSync(path.resolve(__dirname, 'license.txt'))}
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
        }

        case '/windowposition': {
          try {
            const json = JSON.parse(body)

            const code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// License file text is after EmulationStarting

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
    vvctre_set_os_window_position(g_plugin_manager, ${json.x}, ${json.y});
}

VVCTRE_PLUGIN_EXPORT void EmulationStarting() {
    vvctre_set_os_window_position(g_plugin_manager, ${json.x}, ${json.y});
}

/*
License:

${fs.readFileSync(path.resolve(__dirname, 'license.txt'))}
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
        }

        case '/windowsize': {
          try {
            const json = JSON.parse(body)

            const code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// License file text is after EmulationStarting

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
    vvctre_set_os_window_size(g_plugin_manager, ${json.width}, ${json.height});
}

VVCTRE_PLUGIN_EXPORT void EmulationStarting() {
    vvctre_set_os_window_size(g_plugin_manager, ${json.width}, ${json.height});
}

/*
License:

${fs.readFileSync(path.resolve(__dirname, 'license.txt'))}
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
        }

        case '/logfile': {
          const code = `// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// License file text is after the Log function

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
    file.open("${body.replace(/\\/g, '\\\\')}", std::ofstream::trunc);
}

VVCTRE_PLUGIN_EXPORT void Log(const char* line) {
    file << line << std::endl;
}
  
/*
License:

${fs.readFileSync(path.resolve(__dirname, 'license.txt'))}
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
        }

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
