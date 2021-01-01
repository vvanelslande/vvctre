// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

const customDefaultSettingsRegexesAndFunctions = [
  {
    regex: /^start.file (.+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_file_path')
      types.push(['void', 'const char* value'])
      calls.push(
        `vvctre_settings_set_file_path("${matches[1].replace(/\\/g, '\\\\')}");`
      )
    }
  },
  {
    regex: /^start.play_movie (.+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_play_movie')
      types.push(['void', 'const char* value'])
      calls.push(`vvctre_settings_set_play_movie("${matches[1]}");`)
    }
  },
  {
    regex: /^start.record_movie (.+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_record_movie')
      types.push(['void', 'const char* value'])
      calls.push(`vvctre_settings_set_record_movie("${matches[1]}");`)
    }
  },
  {
    regex: /^start.region (Japan|USA|Europe|Australia|China|Korea|Taiwan)$/m,
    f(names, types, calls, matches) {
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
          }[matches[1]]
        });`
      )
    }
  },
  {
    regex: /^start.log_filter (.+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_log_filter')
      types.push(['void', 'const char* value'])
      calls.push(`vvctre_settings_set_log_filter("${matches[1]}");`)
    }
  },
  {
    regex: /^start.initial_time Unix Timestamp$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_initial_clock')
      types.push(['void', 'int value'])
      calls.push(`vvctre_settings_set_initial_clock(1);`)
    }
  },
  {
    regex: /^start.unix_timestamp (\d+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_unix_timestamp')
      types.push('void', 'u64 value')
      calls.push(`vvctre_settings_set_unix_timestamp(${matches[1]});`)
    }
  },
  {
    regex: /^start.use_virtual_sd_card disable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_use_virtual_sd')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_use_virtual_sd(false);')
    }
  },
  {
    regex: /^start.record_frame_times enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_record_frame_times')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_record_frame_times(true);')
    }
  },
  {
    regex: /^start.gdb_stub enable (\d+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_enable_gdbstub')
      types.push(['void', 'u16 port'])
      calls.push(`vvctre_settings_enable_gdbstub(${matches[1]});`)
    }
  },
  {
    regex: /^general.cpu_jit disable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_use_cpu_jit')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_use_cpu_jit(false);')
    }
  },
  {
    regex: /^general.core_2 enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_enable_core_2')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_enable_core_2(true);')
    }
  },
  {
    regex: /^general.limit_speed disable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_limit_speed')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_limit_speed(false);')
    }
  },
  {
    regex: /^general.enable_custom_cpu_ticks enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_use_custom_cpu_ticks')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_use_custom_cpu_ticks(true);')
    }
  },
  {
    regex: /^general.speed_limit (\d+)%?$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_speed_limit')
      types.push(['void', 'u16 value'])
      calls.push(`vvctre_settings_set_speed_limit(${matches[1]});`)
    }
  },
  {
    regex: /^general.custom_cpu_ticks (\d+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_custom_cpu_ticks')
      types.push(['void', 'u64 value'])
      calls.push(`vvctre_settings_set_custom_cpu_ticks(${matches[1]});`)
    }
  },
  {
    regex: /^general.cpu_clock_percentage (\d+)%?$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_cpu_clock_percentage')
      types.push(['void', 'u32 value'])
      calls.push(`vvctre_settings_set_cpu_clock_percentage(${matches[1]});`)
    }
  },
  {
    regex: /^general.core_system_run_default_max_slice_value (-?\d+)%?$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_core_system_run_default_max_slice_value')
      types.push(['void', 's64 value'])
      calls.push(
        `vvctre_settings_set_core_system_run_default_max_slice_value(${matches[1]});`
      )
    }
  },
  {
    regex: /^general.set_slice_length_to_this_in_core_timing_timer_timer (-?\d+)%?$/m,
    f(names, types, calls, matches) {
      names.push(
        'vvctre_settings_set_set_slice_length_to_this_in_core_timing_timer_timer'
      )
      types.push(['void', 's64 value'])
      calls.push(
        `vvctre_settings_set_set_slice_length_to_this_in_core_timing_timer_timer(${matches[1]});`
      )
    }
  },
  {
    regex: /^general.set_downcount_to_this_in_core_timing_timer_timer (-?\d+)%?$/m,
    f(names, types, calls, matches) {
      names.push(
        'vvctre_settings_set_set_downcount_to_this_in_core_timing_timer_timer'
      )
      types.push(['void', 's64 value'])
      calls.push(
        `vvctre_settings_set_set_downcount_to_this_in_core_timing_timer_timer(${matches[1]});`
      )
    }
  },
  {
    regex: /^general.return_this_if_the_event_queue_is_empty_in_core_timing_timer_getmaxslicelength (-?\d+)%?$/m,
    f(names, types, calls, matches) {
      names.push(
        'vvctre_settings_set_return_this_if_the_event_queue_is_empty_in_core_timing_timer_getmaxslicelength'
      )
      types.push(['void', 's64 value'])
      calls.push(
        `vvctre_settings_set_return_this_if_the_event_queue_is_empty_in_core_timing_timer_getmaxslicelength(${matches[1]});`
      )
    }
  },
  {
    regex: /^audio.dsp_lle enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_enable_dsp_lle')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_enable_dsp_lle(true);')
    }
  },
  {
    regex: /^audio.dsp_lle_multiple_threads enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_enable_dsp_lle_multithread')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_enable_dsp_lle_multithread(true);')
    }
  },
  {
    regex: /^audio.stretching disable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_enable_audio_stretching')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_enable_audio_stretching(false);')
    }
  },
  {
    regex: /^audio.volume (\d*\.?\d+f?)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_audio_volume')
      types.push(['void', 'float value'])
      calls.push(`vvctre_settings_set_audio_volume(${matches[1]});`)
    }
  },
  {
    regex: /^audio.sink (cubeb|sdl2|null)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_audio_sink_id')
      types.push(['void', 'const char* value'])
      calls.push(`vvctre_settings_set_audio_sink_id("${matches[1]}");`)
    }
  },
  {
    regex: /^audio.device (.+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_audio_device_id')
      types.push(['void', 'const char* value'])
      calls.push(`vvctre_settings_set_audio_device_id("${matches[1]}");`)
    }
  },
  {
    regex: /^audio.microphone_input_type (Real Device|Static Noise)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_microphone_input_type')
      types.push(['void', 'int value'])
      calls.push(
        `vvctre_settings_set_microphone_input_type(${
          { 'Real Device': 1, 'Static Noise': 2 }[matches[1]]
        });`
      )
    }
  },
  {
    regex: /^audio.microphone_real_device_backend (Cubeb|SDL2|Null)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_microphone_real_device_backend')
      types.push(['void', 'u8 value'])
      calls.push(
        `vvctre_settings_set_microphone_real_device_backend(${
          { Cubeb: 1, SDL2: 2, Null: 3 }[matches[1]]
        });`
      )
    }
  },
  {
    regex: /^audio.microphone_device (.+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_microphone_device')
      types.push(['void', 'const char* value'])
      calls.push(`vvctre_settings_set_microphone_device("${matches[1]}");`)
    }
  },
  {
    regex: /^camera.inner_engine image$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_camera_engine')) {
        names.push('vvctre_settings_set_camera_engine')
        types.push(['void', 'int index, const char* value'])
      }
      calls.push(`vvctre_settings_set_camera_engine(1, "image");`)
    }
  },
  {
    regex: /^camera.inner_parameter (.+)$/m,
    f(names, types, calls, matches) {
      if (!names.includes('vvctre_settings_set_camera_parameter')) {
        names.push('vvctre_settings_set_camera_parameter')
        types.push(['void', 'int index, const char* value'])
      }
      calls.push(`vvctre_settings_set_camera_parameter(1, "${matches[1]}");`)
    }
  },
  {
    regex: /^camera.outer_left_engine image$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_camera_engine')) {
        names.push('vvctre_settings_set_camera_engine')
        types.push(['void', 'int index, const char* value'])
      }
      calls.push(`vvctre_settings_set_camera_engine(2, "image");`)
    }
  },
  {
    regex: /^camera.outer_left_parameter (.+)$/m,
    f(names, types, calls, matches) {
      if (!names.includes('vvctre_settings_set_camera_parameter')) {
        names.push('vvctre_settings_set_camera_parameter')
        types.push(['void', 'int index, const char* value'])
      }
      calls.push(`vvctre_settings_set_camera_parameter(2, "${matches[1]}");`)
    }
  },
  {
    regex: /^camera.outer_right_engine image$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_camera_engine')) {
        names.push('vvctre_settings_set_camera_engine')
        types.push(['void', 'int index, const char* value'])
      }
      calls.push(`vvctre_settings_set_camera_engine(0, "image");`)
    }
  },
  {
    regex: /^camera.outer_right_parameter (.+)$/m,
    f(names, types, calls, matches) {
      if (!names.includes('vvctre_settings_set_camera_parameter')) {
        names.push('vvctre_settings_set_camera_parameter')
        types.push(['void', 'int index, const char* value'])
      }
      calls.push(`vvctre_settings_set_camera_parameter(0, "${matches[1]}");`)
    }
  },
  {
    regex: /^system.play_coins (\d+)$/m,
    f(names, types, calls, matches) {
      if (!names.includes('vvctre_set_play_coins')) {
        names.push('vvctre_set_play_coins')
        types.push(['void', 'u16 value'])
      }
      calls.push(`vvctre_set_play_coins(${matches[1]});`)
    }
  },
  {
    regex: /^graphics.hardware_renderer disable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_use_hardware_renderer')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_use_hardware_renderer(false);')
    }
  },
  {
    regex: /^graphics.hardware_shader disable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_use_hardware_shader')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_use_hardware_shader(false);')
    }
  },
  {
    regex: /^graphics.hardware_shader_accurate_multiplication enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_hardware_shader_accurate_multiplication')
      types.push(['void', 'bool value'])
      calls.push(
        'vvctre_settings_set_hardware_shader_accurate_multiplication(true);'
      )
    }
  },
  {
    regex: /^graphics.enable_disk_shader_cache enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_enable_disk_shader_cache')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_enable_disk_shader_cache(true);')
    }
  },
  {
    regex: /^graphics.shader_jit disable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_use_shader_jit')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_use_shader_jit(false);')
    }
  },
  {
    regex: /^graphics.vsync enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_enable_vsync')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_enable_vsync(true);')
    }
  },
  {
    regex: /^graphics.dump_textures enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_dump_textures')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_dump_textures(true);')
    }
  },
  {
    regex: /^graphics.custom_textures enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_custom_textures')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_custom_textures(true);')
    }
  },
  {
    regex: /^graphics.preload_custom_textures enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_preload_textures')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_preload_textures(true);')
    }
  },
  {
    regex: /^graphics.linear_filtering disable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_enable_linear_filtering')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_enable_linear_filtering(false);')
    }
  },
  {
    regex: /^graphics.sharper_distant_objects enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_sharper_distant_objects')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_sharper_distant_objects(true);')
    }
  },
  {
    regex: /^graphics.background_color (?:#)(\S\S\S\S\S\S)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_background_color_red')
      names.push('vvctre_settings_set_background_color_green')
      names.push('vvctre_settings_set_background_color_blue')
      types.push(['void', 'float value'])
      types.push(['void', 'float value'])
      types.push(['void', 'float value'])
      calls.push(
        `vvctre_settings_set_background_color_red(${
          Number.parseInt(matches[1].slice(0, 2), 16) / 255
        });`
      )
      calls.push(
        `vvctre_settings_set_background_color_green(${
          Number.parseInt(matches[1].slice(2, 4), 16) / 255
        });`
      )
      calls.push(
        `vvctre_settings_set_background_color_blue(${
          Number.parseInt(matches[1].slice(4, 6), 16) / 255
        });`
      )
    }
  },
  {
    regex: /^graphics.resolution (\d+|Window Size)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_resolution')
      types.push(['void', 'u16 value'])
      calls.push(
        matches[1] === 'Window Size'
          ? 'vvctre_settings_set_resolution(0);'
          : `vvctre_settings_set_resolution(${matches[1]});`
      )
    }
  },
  {
    regex: /^graphics.post_processing_shader (.+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_post_processing_shader')
      types.push(['void', 'const char* value'])
      calls.push(`vvctre_settings_set_post_processing_shader("${matches[1]}");`)
    }
  },
  {
    regex: /^graphics.texture_filter (Anime4K Ultrafast|Bicubic|ScaleForce|xBRZ freescale)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_texture_filter')
      types.push(['void', 'const char* value'])
      calls.push(`vvctre_settings_set_texture_filter("${matches[1]}");`)
    }
  },
  {
    regex: /^graphics.3d_mode (Side by Side|Anaglyph|Interlaced)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_render_3d')
      types.push(['void', 'int value'])
      calls.push(
        `vvctre_settings_set_render_3d(${
          {
            Off: 0,
            'Side by Side': 1,
            Anaglyph: 2,
            Interlaced: 3
          }[matches[1]]
        });`
      )
    }
  },
  {
    regex: /^graphics.3d_factor (\d+)%?$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_factor_3d')
      types.push(['void', 'u8 value'])
      calls.push(`vvctre_settings_set_factor_3d(${matches[1]});`)
    }
  },
  {
    regex: /^layout.layout (Single Screen|Large Screen|Side by Side|Medium Screen)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_layout')
      types.push(['void', 'int value'])
      calls.push(
        `vvctre_settings_set_layout(${
          {
            'Single Screen': 1,
            'Large Screen': 2,
            'Side by Side': 3,
            'Medium Screen': 4
          }[matches[1]]
        });`
      )
    }
  },
  {
    regex: /^layout.use_custom_layout enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_use_custom_layout')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_use_custom_layout(true);')
    }
  },
  {
    regex: /^layout.swap_screens enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_swap_screens')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_swap_screens(true);')
    }
  },
  {
    regex: /^layout.upright_screens enable$/m,
    f(names, types, calls) {
      names.push('vvctre_settings_set_upright_screens')
      types.push(['void', 'bool value'])
      calls.push('vvctre_settings_set_upright_screens(true);')
    }
  },
  {
    regex: /^layout.top_left (\d+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_custom_layout_top_left')
      types.push(['void', 'u16 value'])
      calls.push(`vvctre_settings_set_custom_layout_top_left(${matches[1]});`)
    }
  },
  {
    regex: /^layout.top_top (\d+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_custom_layout_top_top')
      types.push(['void', 'u16 value'])
      calls.push(`vvctre_settings_set_custom_layout_top_top(${matches[1]});`)
    }
  },
  {
    regex: /^layout.top_right (\d+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_custom_layout_top_right')
      types.push(['void', 'u16 value'])
      calls.push(`vvctre_settings_set_custom_layout_top_right(${matches[1]});`)
    }
  },
  {
    regex: /^layout.top_bottom (\d+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_custom_layout_top_bottom')
      types.push(['void', 'u16 value'])
      calls.push(`vvctre_settings_set_custom_layout_top_bottom(${matches[1]});`)
    }
  },
  {
    regex: /^layout.bottom_left (\d+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_custom_layout_bottom_left')
      types.push(['void', 'u16 value'])
      calls.push(
        `vvctre_settings_set_custom_layout_bottom_left(${matches[1]});`
      )
    }
  },
  {
    regex: /^layout.bottom_top (\d+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_custom_layout_bottom_top')
      types.push(['void', 'u16 value'])
      calls.push(`vvctre_settings_set_custom_layout_bottom_top(${matches[1]});`)
    }
  },
  {
    regex: /^layout.bottom_right (\d+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_custom_layout_bottom_right')
      types.push(['void', 'u16 value'])
      calls.push(
        `vvctre_settings_set_custom_layout_bottom_right(${matches[1]});`
      )
    }
  },
  {
    regex: /^layout.bottom_bottom (\d+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_custom_layout_bottom_bottom')
      types.push(['void', 'u16 value'])
      calls.push(
        `vvctre_settings_set_custom_layout_bottom_bottom(${matches[1]});`
      )
    }
  },
  {
    regex: /^multiplayer.ip (.+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_multiplayer_ip')
      types.push(['void', 'const char* value'])
      calls.push(`vvctre_settings_set_multiplayer_ip("${matches[1]}");`)
    }
  },
  {
    regex: /^multiplayer.port (\d+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_multiplayer_port')
      types.push(['void', 'u16 value'])
      calls.push(`vvctre_settings_set_multiplayer_port(${matches[1]});`)
    }
  },
  {
    regex: /^multiplayer.nickname (.+)$/m,
    f(names, types, calls, matches) {
      names.push('vvctre_settings_set_nickname')
      types.push(['void', 'const char* value'])
      calls.push(`vvctre_settings_set_nickname("${matches[1]}");`)
    }
  },
  {
    regex: /^lle.spi enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("SPI", true);')
    }
  },
  {
    regex: /^lle.gpio enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("GPIO", true);')
    }
  },
  {
    regex: /^lle.mp enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("MP", true);')
    }
  },
  {
    regex: /^lle.cdc enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("CDC", true);')
    }
  },
  {
    regex: /^lle.http enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("HTTP", true);')
    }
  },
  {
    regex: /^lle.csnd enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("CSND", true);')
    }
  },
  {
    regex: /^lle.ns enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("NS", true);')
    }
  },
  {
    regex: /^lle.nfc enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("NFC", true);')
    }
  },
  {
    regex: /^lle.ptm enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("PTM", true);')
    }
  },
  {
    regex: /^lle.news enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("NEWS", true);')
    }
  },
  {
    regex: /^lle.ndm enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("NDM", true);')
    }
  },
  {
    regex: /^lle.mic enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("MIC", true);')
    }
  },
  {
    regex: /^lle.i2c enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("I2C", true);')
    }
  },
  {
    regex: /^lle.ir enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("IR", true);')
    }
  },
  {
    regex: /^lle.pdn enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("PDN", true);')
    }
  },
  {
    regex: /^lle.nim enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("NIM", true);')
    }
  },
  {
    regex: /^lle.hid enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("HID", true);')
    }
  },
  {
    regex: /^lle.gsp enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("GSP", true);')
    }
  },
  {
    regex: /^lle.frd enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("FRD", true);')
    }
  },
  {
    regex: /^lle.cfg enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("CFG", true);')
    }
  },
  {
    regex: /^lle.ps enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("PS", true);')
    }
  },
  {
    regex: /^lle.cecd enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("CECD", true);')
    }
  },
  {
    regex: /^lle.dsp enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("DSP", true);')
    }
  },
  {
    regex: /^lle.cam enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("CAM", true);')
    }
  },
  {
    regex: /^lle.mcu enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("MCU", true);')
    }
  },
  {
    regex: /^lle.ssl enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("SSL", true);')
    }
  },
  {
    regex: /^lle.boss enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("BOSS", true);')
    }
  },
  {
    regex: /^lle.act enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("ACT", true);')
    }
  },
  {
    regex: /^lle.ac enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("AC", true);')
    }
  },
  {
    regex: /^lle.am enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("AM", true);')
    }
  },
  {
    regex: /^lle.err enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("ERR", true);')
    }
  },
  {
    regex: /^lle.pxi enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("PXI", true);')
    }
  },
  {
    regex: /^lle.nwm enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("NWM", true);')
    }
  },
  {
    regex: /^lle.dlp enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("DLP", true);')
    }
  },
  {
    regex: /^lle.ldr enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("LDR", true);')
    }
  },
  {
    regex: /^lle.pm enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("PM", true);')
    }
  },
  {
    regex: /^lle.soc enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("SOC", true);')
    }
  },
  {
    regex: /^lle.fs enable$/m,
    f(names, types, calls) {
      if (!names.includes('vvctre_settings_set_use_lle_module')) {
        names.push('vvctre_settings_set_use_lle_module')
        types.push(['void', 'const char* name, bool value'])
      }
      calls.push('vvctre_settings_set_use_lle_module("FS", true);')
    }
  }
]
