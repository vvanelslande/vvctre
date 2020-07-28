// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

const fs = require("fs");

if (!process.env.ISSUE_BODY) {
  console.log("empty");
  process.exit(1);
}

const names = [];
const types = [];
const calls = [];

[
  {
    regex: /start.file (.+)/,
    call: (match) => {
      names.push("vvctre_settings_set_file_path");
      types.push(["void", "const char* value"]);
      calls.push(
        `vvctre_settings_set_file_path("${match[1].replace(/\\/g, "\\\\")}");`
      );
    },
  },
  {
    regex: /start.play_movie (.+)/,
    call: (match) => {
      names.push("vvctre_settings_set_play_movie");
      types.push(["void", "const char* value"]);
      calls.push(`vvctre_settings_set_play_movie("${match[1]}");`);
    },
  },
  {
    regex: /start.record_movie (.+)/,
    call: (match) => {
      names.push("vvctre_settings_set_record_movie");
      types.push(["void", "const char* value"]);
      calls.push(`vvctre_settings_set_record_movie("${match[1]}");`);
    },
  },
  {
    regex: /start.region (Auto-select|Japan|USA|Europe|Australia|China|Korea|Taiwan)/,
    call: (match) => {
      names.push("vvctre_settings_set_region_value");
      types.push(["void", "int value"]);
      calls.push(
        `vvctre_settings_set_region_value(${
          {
            "Auto-select": -1,
            Japan: 0,
            USA: 1,
            Europe: 2,
            Australia: 3,
            China: 4,
            Korea: 5,
            Taiwan: 6,
          }[match[1]]
        });`
      );
    },
  },
  {
    regex: /start.log_filter (.+)/,
    call: (match) => {
      names.push("vvctre_settings_set_log_filter");
      types.push(["void", "const char* value"]);
      calls.push(`vvctre_settings_set_log_filter("${match[1]}");`);
    },
  },
  {
    regex: /start.initial_time (System|Unix Timestamp)/,
    call: (match) => {
      names.push("vvctre_settings_set_initial_clock");
      types.push(["void", "int value"]);
      calls.push(
        `vvctre_settings_set_initial_clock(${
          {
            System: 0,
            "Unix Timestamp": 1,
          }[match[1]]
        });`
      );
    },
  },
  {
    regex: /start.unix_timestamp (\d+)/,
    call: (match) => {
      names.push("vvctre_settings_set_unix_timestamp");
      types.push("void", "u64 value");
      calls.push(`vvctre_settings_set_unix_timestamp(${match[1]});`);
    },
  },
  {
    regex: /start.use_virtual_sd_card disable/,
    call: () => {
      names.push("vvctre_settings_set_use_virtual_sd");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_use_virtual_sd(false);");
    },
  },
  {
    regex: /start.record_frame_times enable/,
    call: () => {
      names.push("vvctre_settings_set_record_frame_times");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_record_frame_times(true);");
    },
  },
  {
    regex: /start.gdb_stub enable (\d+)/,
    call: (match) => {
      names.push("vvctre_settings_enable_gdbstub");
      types.push(["void", "u16 port"]);
      calls.push(`vvctre_settings_enable_gdbstub(${match[1]});`);
    },
  },
  {
    regex: /general.cpu_jit disable/,
    call: (match) => {
      names.push("vvctre_settings_set_use_cpu_jit");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_use_cpu_jit(false);");
    },
  },
  {
    regex: /general.limit_speed disable/,
    call: () => {
      names.push("vvctre_settings_set_limit_speed");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_limit_speed(false);");
    },
  },
  {
    regex: /general.enable_custom_cpu_ticks enable/,
    call: (match) => {
      names.push("vvctre_settings_set_use_custom_cpu_ticks");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_use_custom_cpu_ticks(true);");
    },
  },
  {
    regex: /general.speed_limit (\d+)%?/,
    call: (match) => {
      names.push("vvctre_settings_set_speed_limit");
      types.push(["void", "u16 value"]);
      calls.push(`vvctre_settings_set_speed_limit(${match[1]});`);
    },
  },
  {
    regex: /general.custom_cpu_ticks (\d+)/,
    call: (match) => {
      names.push("vvctre_settings_set_custom_cpu_ticks");
      types.push(["void", "u64 value"]);
      calls.push(`vvctre_settings_set_custom_cpu_ticks(${match[1]});`);
    },
  },
  {
    regex: /general.cpu_clock_percentage (\d+)%?/,
    call: (match) => {
      names.push("vvctre_settings_set_cpu_clock_percentage");
      types.push(["void", "u32 value"]);
      calls.push(`vvctre_settings_set_cpu_clock_percentage(${match[1]});`);
    },
  },
  {
    regex: /audio.dsp_lle enable/,
    call: (match) => {
      names.push("vvctre_settings_set_enable_dsp_lle");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_enable_dsp_lle(true);");
    },
  },
  {
    regex: /audio.dsp_lle_multiple_threads enable/,
    call: (match) => {
      names.push("vvctre_settings_set_enable_dsp_lle_multithread");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_enable_dsp_lle_multithread(true);");
    },
  },
  {
    regex: /audio.volume (.+)/,
    call: (match) => {
      names.push("vvctre_settings_set_audio_volume");
      types.push(["void", "float value"]);
      calls.push(`vvctre_settings_set_audio_volume(${match[1]});`);
    },
  },
  {
    regex: /audio.sink (.+)/,
    call: (match) => {
      names.push("vvctre_settings_set_audio_sink_id");
      types.push(["void", "const char* value"]);
      calls.push(`vvctre_settings_set_audio_sink_id("${match[1]}");`);
    },
  },
  {
    regex: /audio.device (.+)/,
    call: (match) => {
      names.push("vvctre_settings_set_audio_device_id");
      types.push(["void", "const char* value"]);
      calls.push(`vvctre_settings_set_audio_device_id("${match[1]}");`);
    },
  },
  {
    regex: /audio.microphone_input_type (Disabled|Real Device|Static Noise)/,
    call: (match) => {
      names.push("vvctre_settings_set_microphone_input_type");
      types.push(["void", "int value"]);
      calls.push(
        `vvctre_settings_set_microphone_input_type(${
          { Disabled: 0, "Real Device": 1, "Static Noise": 2 }[match[1]]
        });`
      );
    },
  },
  {
    regex: /audio.microphone_device (.+)/,
    call: (match) => {
      names.push("vvctre_settings_set_microphone_device");
      types.push(["void", "const char* value"]);
      calls.push(`vvctre_settings_set_microphone_device("${match[1]}");`);
    },
  },
  {
    regex: /camera.inner_engine (blank|image)/,
    call: (match) => {
      if (!names.includes("vvctre_settings_set_camera_engine")) {
        names.push("vvctre_settings_set_camera_engine");
        types.push(["void", "int index, const char* value"]);
      }
      calls.push(`vvctre_settings_set_camera_engine(1, "${match[1]}");`);
    },
  },
  {
    regex: /camera.inner_parameter (.+)/,
    call: (match) => {
      if (!names.includes("vvctre_settings_set_camera_parameter")) {
        names.push("vvctre_settings_set_camera_parameter");
        types.push(["void", "int index, const char* value"]);
      }
      calls.push(`vvctre_settings_set_camera_parameter(1, "${match[1]}");`);
    },
  },
  {
    regex: /camera.outer_left_engine (blank|image)/,
    call: (match) => {
      if (!names.includes("vvctre_settings_set_camera_engine")) {
        names.push("vvctre_settings_set_camera_engine");
        types.push(["void", "int index, const char* value"]);
      }
      calls.push(`vvctre_settings_set_camera_engine(2, "${match[1]}");`);
    },
  },
  {
    regex: /camera.outer_left_parameter (.+)/,
    call: (match) => {
      if (!names.includes("vvctre_settings_set_camera_parameter")) {
        names.push("vvctre_settings_set_camera_parameter");
        types.push(["void", "int index, const char* value"]);
      }
      calls.push(`vvctre_settings_set_camera_parameter(2, "${match[1]}");`);
    },
  },
  {
    regex: /camera.outer_right_parameter (blank|image)/,
    call: (match) => {
      if (!names.includes("vvctre_settings_set_camera_engine")) {
        names.push("vvctre_settings_set_camera_engine");
        types.push(["void", "int index, const char* value"]);
      }
      calls.push(`vvctre_settings_set_camera_engine(0, "${match[1]}");`);
    },
  },
  {
    regex: /camera.outer_right_parameter (.+)/,
    call: (match) => {
      if (!names.includes("vvctre_settings_set_camera_parameter")) {
        names.push("vvctre_settings_set_camera_parameter");
        types.push(["void", "int index, const char* value"]);
      }
      calls.push(`vvctre_settings_set_camera_parameter(0, "${match[1]}");`);
    },
  },
  {
    regex: /system.play_coins (\d+)/,
    call: (match) => {
      if (!names.includes("vvctre_set_play_coins")) {
        names.push("vvctre_set_play_coins");
        types.push(["void", "u16 value"]);
      }
      calls.push(`vvctre_set_play_coins(${match[1]});`);
    },
  },
  {
    regex: /graphics.hardware_renderer disable/,
    call: () => {
      names.push("vvctre_settings_set_use_hardware_renderer");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_use_hardware_renderer(false);");
    },
  },
  {
    regex: /graphics.hardware_shader disable/,
    call: () => {
      names.push("vvctre_settings_set_use_hardware_shader");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_use_hardware_shader(false);");
    },
  },
  {
    regex: /graphics.hardware_shader_accurate_multiplication enable/,
    call: () => {
      names.push("vvctre_settings_set_hardware_shader_accurate_multiplication");
      types.push(["void", "bool value"]);
      calls.push(
        "vvctre_settings_set_hardware_shader_accurate_multiplication(true);"
      );
    },
  },
  {
    regex: /graphics.shader_jit disable/,
    call: () => {
      names.push("vvctre_settings_set_use_shader_jit");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_use_shader_jit(false);");
    },
  },
  {
    regex: /graphics.vsync enable/,
    call: () => {
      names.push("vvctre_settings_set_enable_vsync");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_enable_vsync(true);");
    },
  },
  {
    regex: /graphics.dump_textures enable/,
    call: () => {
      names.push("vvctre_settings_set_dump_textures");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_dump_textures(true);");
    },
  },
  {
    regex: /graphics.custom_textures enable/,
    call: () => {
      names.push("vvctre_settings_set_custom_textures");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_custom_textures(true);");
    },
  },
  {
    regex: /graphics.preload_custom_textures enable/,
    call: () => {
      names.push("vvctre_settings_set_preload_textures");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_preload_textures(true);");
    },
  },
  {
    regex: /graphics.linear_filtering disable/,
    call: () => {
      names.push("vvctre_settings_set_enable_linear_filtering");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_enable_linear_filtering(false);");
    },
  },
  {
    regex: /graphics.sharper_distant_objects enable/,
    call: () => {
      names.push("vvctre_settings_set_sharper_distant_objects");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_sharper_distant_objects(true);");
    },
  },
  {
    regex: /graphics.background_color (?:#)(\S\S\S\S\S\S)/,
    call: (match) => {
      names.push("vvctre_settings_set_background_color_red");
      names.push("vvctre_settings_set_background_color_green");
      names.push("vvctre_settings_set_background_color_blue");
      types.push(["void", "float value"]);
      types.push(["void", "float value"]);
      types.push(["void", "float value"]);
      calls.push(
        `vvctre_settings_set_background_color_red(${
          Number.parseInt(match[1].slice(0, 2), 16) / 255
        });`
      );
      calls.push(
        `vvctre_settings_set_background_color_green(${
          Number.parseInt(match[1].slice(2, 4), 16) / 255
        });`
      );
      calls.push(
        `vvctre_settings_set_background_color_blue(${
          Number.parseInt(match[1].slice(4, 6), 16) / 255
        });`
      );
    },
  },
  {
    regex: /graphics.resolution (\d+|Window Size)/,
    call: (match) => {
      names.push("vvctre_settings_set_resolution");
      types.push(["void", "u16 value"]);
      calls.push(
        match[1] === "Window Size"
          ? "vvctre_settings_set_resolution(0);"
          : `vvctre_settings_set_resolution(${match[1]});`
      );
    },
  },
  {
    regex: /graphics.post_processing_shader (.+)/,
    call: (match) => {
      names.push("vvctre_settings_set_post_processing_shader");
      types.push(["void", "const char* value"]);
      calls.push(`vvctre_settings_set_post_processing_shader("${match[1]}");`);
    },
  },
  {
    regex: /graphics.texture_filter (none|Anime4K Ultrafast|Bicubic|ScaleForce|xBRZ freescale)/,
    call: (match) => {
      names.push("vvctre_settings_set_texture_filter");
      types.push(["void", "const char* value"]);
      calls.push(`vvctre_settings_set_texture_filter("${match[1]}");`);
    },
  },
  {
    regex: /graphics.3d_mode (Off|Side by Side|Anaglyph|Interlaced)/,
    call: (match) => {
      names.push("vvctre_settings_set_render_3d");
      types.push(["void", "int value"]);
      calls.push(
        `vvctre_settings_set_render_3d(${
          {
            Off: 0,
            "Side by Side": 1,
            Anaglyph: 2,
            Interlaced: 3,
          }[match[1]]
        });`
      );
    },
  },
  {
    regex: /graphics.3d_factor (\d+)%?/,
    call: (match) => {
      names.push("vvctre_settings_set_factor_3d");
      types.push(["void", "u8 value"]);
      calls.push(`vvctre_settings_set_factor_3d(${match[1]});`);
    },
  },
  {
    regex: /layout.layout (Default|Single Screen|Large Screen|Side by Side|Medium Screen)/,
    call: (match) => {
      names.push("vvctre_settings_set_layout");
      types.push(["void", "int value"]);
      calls.push(
        `vvctre_settings_set_layout(${
          {
            Default: 0,
            "Single Screen": 1,
            "Large Screen": 2,
            "Side by Side": 3,
            "Medium Screen": 4,
          }[match[1]]
        });`
      );
    },
  },
  {
    regex: /layout.use_custom_layout enable/,
    call: () => {
      names.push("vvctre_settings_set_use_custom_layout");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_use_custom_layout(true);");
    },
  },
  {
    regex: /layout.swap_screens enable/,
    call: () => {
      names.push("vvctre_settings_set_swap_screens");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_swap_screens(true);");
    },
  },
  {
    regex: /layout.upright_screens enable/,
    call: () => {
      names.push("vvctre_settings_set_upright_screens");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_upright_screens(true);");
    },
  },
  {
    regex: /layout.top_left (\d+)/,
    call: (match) => {
      names.push("vvctre_settings_set_custom_layout_top_left");
      types.push(["void", "u16 value"]);
      calls.push(`vvctre_settings_set_custom_layout_top_left(${match[1]});`);
    },
  },
  {
    regex: /layout.top_top (\d+)/,
    call: (match) => {
      names.push("vvctre_settings_set_custom_layout_top_top");
      types.push(["void", "u16 value"]);
      calls.push(`vvctre_settings_set_custom_layout_top_top(${match[1]});`);
    },
  },
  {
    regex: /layout.top_right (\d+)/,
    call: (match) => {
      names.push("vvctre_settings_set_custom_layout_top_right");
      types.push(["void", "u16 value"]);
      calls.push(`vvctre_settings_set_custom_layout_top_right(${match[1]});`);
    },
  },
  {
    regex: /layout.top_bottom (\d+)/,
    call: (match) => {
      names.push("vvctre_settings_set_custom_layout_top_bottom");
      types.push(["void", "u16 value"]);
      calls.push(`vvctre_settings_set_custom_layout_top_bottom(${match[1]});`);
    },
  },
  {
    regex: /layout.bottom_left (\d+)/,
    call: (match) => {
      names.push("vvctre_settings_set_custom_layout_bottom_left");
      types.push(["void", "u16 value"]);
      calls.push(`vvctre_settings_set_custom_layout_bottom_left(${match[1]});`);
    },
  },
  {
    regex: /layout.bottom_top (\d+)/,
    call: (match) => {
      names.push("vvctre_settings_set_custom_layout_bottom_top");
      types.push(["void", "u16 value"]);
      calls.push(`vvctre_settings_set_custom_layout_bottom_top(${match[1]});`);
    },
  },
  {
    regex: /layout.bottom_right (\d+)/,
    call: (match) => {
      names.push("vvctre_settings_set_custom_layout_bottom_right");
      types.push(["void", "u16 value"]);
      calls.push(
        `vvctre_settings_set_custom_layout_bottom_right(${match[1]});`
      );
    },
  },
  {
    regex: /layout.bottom_bottom (\d+)/,
    call: (match) => {
      names.push("vvctre_settings_set_custom_layout_bottom_bottom");
      types.push(["void", "u16 value"]);
      calls.push(
        `vvctre_settings_set_custom_layout_bottom_bottom(${match[1]});`
      );
    },
  },
  {
    regex: /multiplayer.ip (.+)/,
    call: (match) => {
      names.push("vvctre_settings_set_multiplayer_ip");
      types.push(["void", "const char* value"]);
      calls.push(`vvctre_settings_set_multiplayer_ip("${match[1]}");`);
    },
  },
  {
    regex: /multiplayer.port (\d+)/,
    call: (match) => {
      names.push("vvctre_settings_set_multiplayer_port");
      types.push(["void", "u16 value"]);
      calls.push(`vvctre_settings_set_multiplayer_port(${match[1]});`);
    },
  },
  {
    regex: /multiplayer.nickname (.+)/,
    call: (match) => {
      names.push("vvctre_settings_set_nickname");
      types.push(["void", "const char* value"]);
      calls.push(`vvctre_settings_set_nickname("${match[1]}");`);
    },
  },
  {
    regex: /lle.spi enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("SPI", true);');
    },
  },
  {
    regex: /lle.gpio enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("GPIO", true);');
    },
  },
  {
    regex: /lle.mp enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("MP", true);');
    },
  },
  {
    regex: /lle.cdc enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("CDC", true);');
    },
  },
  {
    regex: /lle.http enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("HTTP", true);');
    },
  },
  {
    regex: /lle.csnd enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("CSND", true);');
    },
  },
  {
    regex: /lle.ns enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("NS", true);');
    },
  },
  {
    regex: /lle.nfc enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("NFC", true);');
    },
  },
  {
    regex: /lle.ptm enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("PTM", true);');
    },
  },
  {
    regex: /lle.news enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("NEWS", true);');
    },
  },
  {
    regex: /lle.ndm enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("NDM", true);');
    },
  },
  {
    regex: /lle.mic enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("MIC", true);');
    },
  },
  {
    regex: /lle.i2c enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("I2C", true);');
    },
  },
  {
    regex: /lle.ir enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("IR", true);');
    },
  },
  {
    regex: /lle.pdn enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("PDN", true);');
    },
  },
  {
    regex: /lle.nim enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("NIM", true);');
    },
  },
  {
    regex: /lle.hid enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("HID", true);');
    },
  },
  {
    regex: /lle.gsp enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("GSP", true);');
    },
  },
  {
    regex: /lle.frd enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("FRD", true);');
    },
  },
  {
    regex: /lle.cfg enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("CFG", true);');
    },
  },
  {
    regex: /lle.ps enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("PS", true);');
    },
  },
  {
    regex: /lle.cecd enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("CECD", true);');
    },
  },
  {
    regex: /lle.dsp enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("DSP", true);');
    },
  },
  {
    regex: /lle.cam enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("CAM", true);');
    },
  },
  {
    regex: /lle.mcu enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("MCU", true);');
    },
  },
  {
    regex: /lle.ssl enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("SSL", true);');
    },
  },
  {
    regex: /lle.boss enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("BOSS", true);');
    },
  },
  {
    regex: /lle.act enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("ACT", true);');
    },
  },
  {
    regex: /lle.ac enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("AC", true);');
    },
  },
  {
    regex: /lle.am enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("AM", true);');
    },
  },
  {
    regex: /lle.err enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("ERR", true);');
    },
  },
  {
    regex: /lle.pxi enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("PXI", true);');
    },
  },
  {
    regex: /lle.nwm enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("NWM", true);');
    },
  },
  {
    regex: /lle.dlp enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("DLP", true);');
    },
  },
  {
    regex: /lle.ldr enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("LDR", true);');
    },
  },
  {
    regex: /lle.pm enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("PM", true);');
    },
  },
  {
    regex: /lle.soc enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("SOC", true);');
    },
  },
  {
    regex: /lle.fs enable/,
    call: () => {
      if (!names.includes("vvctre_settings_set_use_lle_module")) {
        names.push("vvctre_settings_set_use_lle_module");
        types.push(["void", "const char* name, bool value"]);
      }
      calls.push('vvctre_settings_set_use_lle_module("FS", true);');
    },
  },
  {
    regex: /hacks.priority_boost disable/,
    call: () => {
      names.push("vvctre_settings_set_enable_priority_boost");
      types.push(["void", "bool value"]);
      calls.push("vvctre_settings_set_enable_priority_boost(false);");
    },
  },
].forEach((test) => {
  if (test.regex.test(process.env.ISSUE_BODY)) {
    const match = process.env.ISSUE_BODY.match(test.regex);
    test.call(match);
  }
});

let code = `// Copyright 2020 Valentin Vanelslande
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

static const char *required_function_names[] = {
${names.map((name) => `    "${name}",\n`).join("")}${(() =>
  names.length === 1 ? "\nNULL, \n" : "")()}
};

${names
  .map(
    (name, index) =>
      `typedef ${types[index][0]} (*${name}_t)(${types[index][1]});\nstatic ${name}_t ${name};`
  )
  .join("\n")}

VVCTRE_PLUGIN_EXPORT int GetRequiredFunctionCount() {
    return ${names.length};
}

VVCTRE_PLUGIN_EXPORT const char** GetRequiredFunctionNames() {
    return required_function_names;
}
    
VVCTRE_PLUGIN_EXPORT void PluginLoaded(void* core, void* plugin_manager, void* required_functions[]) {
${names
  .map(
    (name, index) => `    ${name} = (${name}_t)required_functions[${index}];`
  )
  .join("\n")}
}

VVCTRE_PLUGIN_EXPORT void InitialSettingsOpening() {
${calls.map((call) => `    ${call}`).join("\n")}
}
`;

fs.writeFileSync("plugin.c", code);
