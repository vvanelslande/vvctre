// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

module.exports = function getRegexes(names, types, calls) {
  return [
    {
      regex: /^start.file (.+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_file_path");
        types.push(["void", "const char* value"]);
        calls.push(
          `vvctre_settings_set_file_path("${match[1].replace(/\\/g, "\\\\")}");`
        );
      },
    },
    {
      regex: /^start.play_movie (.+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_play_movie");
        types.push(["void", "const char* value"]);
        calls.push(`vvctre_settings_set_play_movie("${match[1]}");`);
      },
    },
    {
      regex: /^start.record_movie (.+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_record_movie");
        types.push(["void", "const char* value"]);
        calls.push(`vvctre_settings_set_record_movie("${match[1]}");`);
      },
    },
    {
      regex: /^start.region (Auto-select|Japan|USA|Europe|Australia|China|Korea|Taiwan)$/m,
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
      regex: /^start.log_filter (.+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_log_filter");
        types.push(["void", "const char* value"]);
        calls.push(`vvctre_settings_set_log_filter("${match[1]}");`);
      },
    },
    {
      regex: /^start.initial_time (System|Unix Timestamp)$/m,
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
      regex: /^start.unix_timestamp (\d+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_unix_timestamp");
        types.push("void", "u64 value");
        calls.push(`vvctre_settings_set_unix_timestamp(${match[1]});`);
      },
    },
    {
      regex: /^start.use_virtual_sd_card disable$/m,
      call: () => {
        names.push("vvctre_settings_set_use_virtual_sd");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_use_virtual_sd(false);");
      },
    },
    {
      regex: /^start.record_frame_times enable$/m,
      call: () => {
        names.push("vvctre_settings_set_record_frame_times");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_record_frame_times(true);");
      },
    },
    {
      regex: /^start.gdb_stub enable (\d+)$/m,
      call: (match) => {
        names.push("vvctre_settings_enable_gdbstub");
        types.push(["void", "u16 port"]);
        calls.push(`vvctre_settings_enable_gdbstub(${match[1]});`);
      },
    },
    {
      regex: /^general.cpu_jit disable$/m,
      call: (match) => {
        names.push("vvctre_settings_set_use_cpu_jit");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_use_cpu_jit(false);");
      },
    },
    {
      regex: /^general.limit_speed disable$/m,
      call: () => {
        names.push("vvctre_settings_set_limit_speed");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_limit_speed(false);");
      },
    },
    {
      regex: /^general.enable_custom_cpu_ticks enable$/m,
      call: (match) => {
        names.push("vvctre_settings_set_use_custom_cpu_ticks");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_use_custom_cpu_ticks(true);");
      },
    },
    {
      regex: /^general.speed_limit (\d+)%?$/m,
      call: (match) => {
        names.push("vvctre_settings_set_speed_limit");
        types.push(["void", "u16 value"]);
        calls.push(`vvctre_settings_set_speed_limit(${match[1]});`);
      },
    },
    {
      regex: /^general.custom_cpu_ticks (\d+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_custom_cpu_ticks");
        types.push(["void", "u64 value"]);
        calls.push(`vvctre_settings_set_custom_cpu_ticks(${match[1]});`);
      },
    },
    {
      regex: /^general.cpu_clock_percentage (\d+)%?$/m,
      call: (match) => {
        names.push("vvctre_settings_set_cpu_clock_percentage");
        types.push(["void", "u32 value"]);
        calls.push(`vvctre_settings_set_cpu_clock_percentage(${match[1]});`);
      },
    },
    {
      regex: /^audio.dsp_lle enable$/m,
      call: (match) => {
        names.push("vvctre_settings_set_enable_dsp_lle");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_enable_dsp_lle(true);");
      },
    },
    {
      regex: /^audio.dsp_lle_multiple_threads enable$/m,
      call: (match) => {
        names.push("vvctre_settings_set_enable_dsp_lle_multithread");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_enable_dsp_lle_multithread(true);");
      },
    },
    {
      regex: /^audio.volume (.+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_audio_volume");
        types.push(["void", "float value"]);
        calls.push(`vvctre_settings_set_audio_volume(${match[1]});`);
      },
    },
    {
      regex: /^audio.sink (.+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_audio_sink_id");
        types.push(["void", "const char* value"]);
        calls.push(`vvctre_settings_set_audio_sink_id("${match[1]}");`);
      },
    },
    {
      regex: /^audio.device (.+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_audio_device_id");
        types.push(["void", "const char* value"]);
        calls.push(`vvctre_settings_set_audio_device_id("${match[1]}");`);
      },
    },
    {
      regex: /^audio.microphone_input_type (Disabled|Real Device|Static Noise)$/m,
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
      regex: /^audio.microphone_device (.+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_microphone_device");
        types.push(["void", "const char* value"]);
        calls.push(`vvctre_settings_set_microphone_device("${match[1]}");`);
      },
    },
    {
      regex: /^camera.inner_engine (blank|image)$/m,
      call: (match) => {
        if (!names.includes("vvctre_settings_set_camera_engine")) {
          names.push("vvctre_settings_set_camera_engine");
          types.push(["void", "int index, const char* value"]);
        }
        calls.push(`vvctre_settings_set_camera_engine(1, "${match[1]}");`);
      },
    },
    {
      regex: /^camera.inner_parameter (.+)$/m,
      call: (match) => {
        if (!names.includes("vvctre_settings_set_camera_parameter")) {
          names.push("vvctre_settings_set_camera_parameter");
          types.push(["void", "int index, const char* value"]);
        }
        calls.push(`vvctre_settings_set_camera_parameter(1, "${match[1]}");`);
      },
    },
    {
      regex: /^camera.outer_left_engine (blank|image)$/m,
      call: (match) => {
        if (!names.includes("vvctre_settings_set_camera_engine")) {
          names.push("vvctre_settings_set_camera_engine");
          types.push(["void", "int index, const char* value"]);
        }
        calls.push(`vvctre_settings_set_camera_engine(2, "${match[1]}");`);
      },
    },
    {
      regex: /^camera.outer_left_parameter (.+)$/m,
      call: (match) => {
        if (!names.includes("vvctre_settings_set_camera_parameter")) {
          names.push("vvctre_settings_set_camera_parameter");
          types.push(["void", "int index, const char* value"]);
        }
        calls.push(`vvctre_settings_set_camera_parameter(2, "${match[1]}");`);
      },
    },
    {
      regex: /^camera.outer_right_parameter (blank|image)$/m,
      call: (match) => {
        if (!names.includes("vvctre_settings_set_camera_engine")) {
          names.push("vvctre_settings_set_camera_engine");
          types.push(["void", "int index, const char* value"]);
        }
        calls.push(`vvctre_settings_set_camera_engine(0, "${match[1]}");`);
      },
    },
    {
      regex: /^camera.outer_right_parameter (.+)$/m,
      call: (match) => {
        if (!names.includes("vvctre_settings_set_camera_parameter")) {
          names.push("vvctre_settings_set_camera_parameter");
          types.push(["void", "int index, const char* value"]);
        }
        calls.push(`vvctre_settings_set_camera_parameter(0, "${match[1]}");`);
      },
    },
    {
      regex: /^system.play_coins (\d+)$/m,
      call: (match) => {
        if (!names.includes("vvctre_set_play_coins")) {
          names.push("vvctre_set_play_coins");
          types.push(["void", "u16 value"]);
        }
        calls.push(`vvctre_set_play_coins(${match[1]});`);
      },
    },
    {
      regex: /^graphics.hardware_renderer disable$/m,
      call: () => {
        names.push("vvctre_settings_set_use_hardware_renderer");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_use_hardware_renderer(false);");
      },
    },
    {
      regex: /^graphics.hardware_shader disable$/m,
      call: () => {
        names.push("vvctre_settings_set_use_hardware_shader");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_use_hardware_shader(false);");
      },
    },
    {
      regex: /^graphics.hardware_shader_accurate_multiplication enable$/m,
      call: () => {
        names.push(
          "vvctre_settings_set_hardware_shader_accurate_multiplication"
        );
        types.push(["void", "bool value"]);
        calls.push(
          "vvctre_settings_set_hardware_shader_accurate_multiplication(true);"
        );
      },
    },
    {
      regex: /^graphics.shader_jit disable$/m,
      call: () => {
        names.push("vvctre_settings_set_use_shader_jit");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_use_shader_jit(false);");
      },
    },
    {
      regex: /^graphics.vsync enable$/m,
      call: () => {
        names.push("vvctre_settings_set_enable_vsync");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_enable_vsync(true);");
      },
    },
    {
      regex: /^graphics.dump_textures enable$/m,
      call: () => {
        names.push("vvctre_settings_set_dump_textures");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_dump_textures(true);");
      },
    },
    {
      regex: /^graphics.custom_textures enable$/m,
      call: () => {
        names.push("vvctre_settings_set_custom_textures");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_custom_textures(true);");
      },
    },
    {
      regex: /^graphics.preload_custom_textures enable$/m,
      call: () => {
        names.push("vvctre_settings_set_preload_textures");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_preload_textures(true);");
      },
    },
    {
      regex: /^graphics.linear_filtering disable$/m,
      call: () => {
        names.push("vvctre_settings_set_enable_linear_filtering");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_enable_linear_filtering(false);");
      },
    },
    {
      regex: /^graphics.sharper_distant_objects enable$/m,
      call: () => {
        names.push("vvctre_settings_set_sharper_distant_objects");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_sharper_distant_objects(true);");
      },
    },
    {
      regex: /^graphics.background_color (?:#)(\S\S\S\S\S\S)$/m,
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
      regex: /^graphics.resolution (\d+|Window Size)$/m,
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
      regex: /^graphics.post_processing_shader (.+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_post_processing_shader");
        types.push(["void", "const char* value"]);
        calls.push(
          `vvctre_settings_set_post_processing_shader("${match[1]}");`
        );
      },
    },
    {
      regex: /^graphics.texture_filter (none|Anime4K Ultrafast|Bicubic|ScaleForce|xBRZ freescale)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_texture_filter");
        types.push(["void", "const char* value"]);
        calls.push(`vvctre_settings_set_texture_filter("${match[1]}");`);
      },
    },
    {
      regex: /^graphics.3d_mode (Off|Side by Side|Anaglyph|Interlaced)$/m,
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
      regex: /^graphics.3d_factor (\d+)%?$/m,
      call: (match) => {
        names.push("vvctre_settings_set_factor_3d");
        types.push(["void", "u8 value"]);
        calls.push(`vvctre_settings_set_factor_3d(${match[1]});`);
      },
    },
    {
      regex: /^layout.layout (Default|Single Screen|Large Screen|Side by Side|Medium Screen)$/m,
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
      regex: /^layout.use_custom_layout enable$/m,
      call: () => {
        names.push("vvctre_settings_set_use_custom_layout");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_use_custom_layout(true);");
      },
    },
    {
      regex: /^layout.swap_screens enable$/m,
      call: () => {
        names.push("vvctre_settings_set_swap_screens");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_swap_screens(true);");
      },
    },
    {
      regex: /^layout.upright_screens enable$/m,
      call: () => {
        names.push("vvctre_settings_set_upright_screens");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_upright_screens(true);");
      },
    },
    {
      regex: /^layout.top_left (\d+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_custom_layout_top_left");
        types.push(["void", "u16 value"]);
        calls.push(`vvctre_settings_set_custom_layout_top_left(${match[1]});`);
      },
    },
    {
      regex: /^layout.top_top (\d+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_custom_layout_top_top");
        types.push(["void", "u16 value"]);
        calls.push(`vvctre_settings_set_custom_layout_top_top(${match[1]});`);
      },
    },
    {
      regex: /^layout.top_right (\d+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_custom_layout_top_right");
        types.push(["void", "u16 value"]);
        calls.push(`vvctre_settings_set_custom_layout_top_right(${match[1]});`);
      },
    },
    {
      regex: /^layout.top_bottom (\d+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_custom_layout_top_bottom");
        types.push(["void", "u16 value"]);
        calls.push(
          `vvctre_settings_set_custom_layout_top_bottom(${match[1]});`
        );
      },
    },
    {
      regex: /^layout.bottom_left (\d+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_custom_layout_bottom_left");
        types.push(["void", "u16 value"]);
        calls.push(
          `vvctre_settings_set_custom_layout_bottom_left(${match[1]});`
        );
      },
    },
    {
      regex: /^layout.bottom_top (\d+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_custom_layout_bottom_top");
        types.push(["void", "u16 value"]);
        calls.push(
          `vvctre_settings_set_custom_layout_bottom_top(${match[1]});`
        );
      },
    },
    {
      regex: /^layout.bottom_right (\d+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_custom_layout_bottom_right");
        types.push(["void", "u16 value"]);
        calls.push(
          `vvctre_settings_set_custom_layout_bottom_right(${match[1]});`
        );
      },
    },
    {
      regex: /^layout.bottom_bottom (\d+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_custom_layout_bottom_bottom");
        types.push(["void", "u16 value"]);
        calls.push(
          `vvctre_settings_set_custom_layout_bottom_bottom(${match[1]});`
        );
      },
    },
    {
      regex: /^multiplayer.ip (.+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_multiplayer_ip");
        types.push(["void", "const char* value"]);
        calls.push(`vvctre_settings_set_multiplayer_ip("${match[1]}");`);
      },
    },
    {
      regex: /^multiplayer.port (\d+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_multiplayer_port");
        types.push(["void", "u16 value"]);
        calls.push(`vvctre_settings_set_multiplayer_port(${match[1]});`);
      },
    },
    {
      regex: /^multiplayer.nickname (.+)$/m,
      call: (match) => {
        names.push("vvctre_settings_set_nickname");
        types.push(["void", "const char* value"]);
        calls.push(`vvctre_settings_set_nickname("${match[1]}");`);
      },
    },
    {
      regex: /^lle.spi enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("SPI", true);');
      },
    },
    {
      regex: /^lle.gpio enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("GPIO", true);');
      },
    },
    {
      regex: /^lle.mp enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("MP", true);');
      },
    },
    {
      regex: /^lle.cdc enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("CDC", true);');
      },
    },
    {
      regex: /^lle.http enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("HTTP", true);');
      },
    },
    {
      regex: /^lle.csnd enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("CSND", true);');
      },
    },
    {
      regex: /^lle.ns enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("NS", true);');
      },
    },
    {
      regex: /^lle.nfc enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("NFC", true);');
      },
    },
    {
      regex: /^lle.ptm enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("PTM", true);');
      },
    },
    {
      regex: /^lle.news enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("NEWS", true);');
      },
    },
    {
      regex: /^lle.ndm enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("NDM", true);');
      },
    },
    {
      regex: /^lle.mic enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("MIC", true);');
      },
    },
    {
      regex: /^lle.i2c enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("I2C", true);');
      },
    },
    {
      regex: /^lle.ir enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("IR", true);');
      },
    },
    {
      regex: /^lle.pdn enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("PDN", true);');
      },
    },
    {
      regex: /^lle.nim enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("NIM", true);');
      },
    },
    {
      regex: /^lle.hid enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("HID", true);');
      },
    },
    {
      regex: /^lle.gsp enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("GSP", true);');
      },
    },
    {
      regex: /^lle.frd enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("FRD", true);');
      },
    },
    {
      regex: /^lle.cfg enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("CFG", true);');
      },
    },
    {
      regex: /^lle.ps enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("PS", true);');
      },
    },
    {
      regex: /^lle.cecd enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("CECD", true);');
      },
    },
    {
      regex: /^lle.dsp enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("DSP", true);');
      },
    },
    {
      regex: /^lle.cam enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("CAM", true);');
      },
    },
    {
      regex: /^lle.mcu enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("MCU", true);');
      },
    },
    {
      regex: /^lle.ssl enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("SSL", true);');
      },
    },
    {
      regex: /^lle.boss enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("BOSS", true);');
      },
    },
    {
      regex: /^lle.act enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("ACT", true);');
      },
    },
    {
      regex: /^lle.ac enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("AC", true);');
      },
    },
    {
      regex: /^lle.am enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("AM", true);');
      },
    },
    {
      regex: /^lle.err enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("ERR", true);');
      },
    },
    {
      regex: /^lle.pxi enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("PXI", true);');
      },
    },
    {
      regex: /^lle.nwm enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("NWM", true);');
      },
    },
    {
      regex: /^lle.dlp enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("DLP", true);');
      },
    },
    {
      regex: /^lle.ldr enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("LDR", true);');
      },
    },
    {
      regex: /^lle.pm enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("PM", true);');
      },
    },
    {
      regex: /^lle.soc enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("SOC", true);');
      },
    },
    {
      regex: /^lle.fs enable$/m,
      call: () => {
        if (!names.includes("vvctre_settings_set_use_lle_module")) {
          names.push("vvctre_settings_set_use_lle_module");
          types.push(["void", "const char* name, bool value"]);
        }
        calls.push('vvctre_settings_set_use_lle_module("FS", true);');
      },
    },
    {
      regex: /^hacks.priority_boost disable$/m,
      call: () => {
        names.push("vvctre_settings_set_enable_priority_boost");
        types.push(["void", "bool value"]);
        calls.push("vvctre_settings_set_enable_priority_boost(false);");
      },
    },
  ];
};
