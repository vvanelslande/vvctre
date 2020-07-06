// Copyright 2020 Valentin Vanelslande
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

const fs = require("fs");

const match = process.env.ISSUE_BODY.match(
  new RegExp(
    [
      "^<!--\r\n",
      "If you want to enable something, change `\\[ ]` to `\\[x]`\r\n",
      "If you want to disable something, change `\\[x]` to `\\[ ]`\r\n",
      "-->\r\n\r\n",

      "## Start\r\n\r\n",

      "File: (?<startFile>.+)\r\n",
      "Play Movie: (?<startPlayMovie>.+)\r\n",
      "Record Movie: (?<startRecordMovie>.+)\r\n",
      "Region: (?<startRegion>Auto-select|Japan|USA|Europe|Australia|China|Korea|Taiwan)\r\n",
      "Log Filter: (?:\\\\?)(?<startLogFilter>.+)\r\n",
      "Initial Time: (?<startInitialTime_Type>System|Unix Timestamp) <!-- If this is Unix Timestamp, add the number after this -->(?: )?(?<startInitialTime_Timestamp>.*)?\r\n\r\n",

      "- \\[(?<startUseVirtualSdCard>[x ])] Use Virtual SD Card\r\n",
      "- \\[(?<startStartInFullscreenMode>[x ])] Start in Fullscreen Mode\r\n",
      "- \\[(?<startRecordFrameTimes>[x ])] Record Frame Times\r\n",
      "- \\[(?<startEnableGdbStub>[x ])] Enable GDB Stub <!-- If this is enabled, add `Port: <port>` after this -->(?: Port: )?(?<startGdbStubPort>\\d+)?\r\n\r\n",

      "## General\r\n\r\n",

      "- \\[(?<generalUseCpuJit>[x ])] Use CPU JIT\r\n",
      "- \\[(?<generalLimitSpeed>[x ])] Limit Speed <!-- Remove the To % if this is disabled -->(?: To )?(?<generalSpeedLimit>\\d+)?(?:%)?\r\n",
      "- \\[(?<generalUseCustomCpuTicks>[x ])] Custom CPU Ticks <!-- If this is enabled, add the number after this -->(?: )?(?<generalCustomCpuTicks>\\d+)?\r\n\r\n",

      "CPU Clock Percentage: (?<generalCpuClockPercentage>\\d+)(?:%)?\r\n\r\n",

      "## Audio\r\n\r\n",

      "- \\[(?<audioEnableDspLle>[x ])] Enable DSP LLE\r\n",
      "  - \\[(?<audioDspLleUseMultipleThreads>[x ])] Use multiple threads\r\n\r\n",

      "Volume: (?<audioVolume>.+)\r\n",
      "Sink: (?<audioSink>auto|cubeb|sdl2|null)\r\n",
      "Device: (?<audioDevice>.+)\r\n",
      "Microphone Input Type: (?<audioMicrophoneInputType>Disabled|Real Device|Static Noise)\r\n\r\n",

      "<!-- If Microphone Input Type is Real Device, add Microphone Device after this -->(?: ?(?<audioMicrophoneDevice>.+))?\r\n\r\n",

      "## Camera\r\n\r\n",

      "Inner Camera Engine: (?<cameraInnerCameraEngine>blank|image)\r\n\r\n",

      "<!-- If Inner Camera Engine is image, add Inner Camera Parameter after this -->(?: ?(?<cameraInnerCameraParameter>.+))?\r\n\r\n",

      "Outer Left Camera Engine: (?<cameraOuterLeftCameraEngine>blank|image)\r\n\r\n",

      "<!-- If Outer Left Camera Engine is image, add Outer Left Camera Parameter after this -->(?: ?(?<cameraOuterLeftCameraParameter>.+))?\r\n\r\n",

      "Outer Right Camera Engine: (?<cameraOuterRightCameraEngine>blank|image)\r\n\r\n",

      "<!-- If Outer Right Camera Engine is image, add Outer Right Camera Parameter after this -->(?: ?(?<cameraOuterRightCameraParameter>.+))?\r\n\r\n",

      "## Graphics\r\n\r\n",

      "- \\[(?<graphicsUseHardwareRenderer>[x ])] Use Hardware Renderer\r\n",
      "  - \\[(?<graphicsUseHardwareShader>[x ])] Use Hardware Shader\r\n",
      "    - \\[(?<graphicsAccurateMultiplication>[x ])] Accurate Multiplication\r\n",
      "- \\[(?<graphicsUseShaderJit>[x ])] Use Shader JIT\r\n",
      "- \\[(?<graphicsEnableVsync>[x ])] Enable VSync\r\n",
      "- \\[(?<graphicsDumpTextures>[x ])] Dump Textures\r\n",
      "- \\[(?<graphicsUseCustomTextures>[x ])] Use Custom Textures\r\n",
      "- \\[(?<graphicsPreloadCustomTextures>[x ])] Preload Custom Textures\r\n",
      "- \\[(?<graphicsEnableLinearFiltering>[x ])] Enable Linear Filtering\r\n",
      "- \\[(?<graphicsSharperDistantObjects>[x ])] Sharper Distant Objects\r\n\r\n",

      "Resolution: (?<graphicsResolution>\\d+)\r\n",
      "Background Color: (?:#)(?<graphicsBackgroundColor>\\S\\S\\S\\S\\S\\S)\r\n",
      "Post Processing Shader: (?<graphicsPostProcessingShader>.+)\r\n",
      "Texture Filter: (?<graphicsTextureFilter>none|Anime4K Ultrafast|Bicubic|ScaleForce|xBRZ freescale)\r\n",
      "3D: (?<graphics3dLeft>Off|Side by Side|Anaglyph|Interlaced) (?<graphics3dRight>\\d+)(?:%)?\r\n\r\n",

      "## Layout\r\n\r\n",

      "Layout: (?<layoutLayout>Default|Single Screen|Large Screen|Side by Side|Medium Screen)\r\n\r\n",

      "- \\[(?<layoutUseCustomLayout>[x ])] Use Custom Layout\r\n",
      "- \\[(?<layoutSwapScreens>[x ])] Swap Screens\r\n",
      "- \\[(?<layoutUprightScreens>[x ])] Upright Screens\r\n\r\n",

      "Top Left \\(Custom Layout\\): (?<layoutTopLeft>\\d+)\r\n",
      "Top Top \\(Custom Layout\\): (?<layoutTopTop>\\d+)\r\n",
      "Top Right \\(Custom Layout\\): (?<layoutTopRight>\\d+)\r\n",
      "Top Bottom \\(Custom Layout\\): (?<layoutTopBottom>\\d+)\r\n",
      "Bottom Left \\(Custom Layout\\): (?<layoutBottomLeft>\\d+)\r\n",
      "Bottom Top \\(Custom Layout\\): (?<layoutBottomTop>\\d+)\r\n",
      "Bottom Right \\(Custom Layout\\): (?<layoutBottomRight>\\d+)\r\n",
      "Bottom Bottom \\(Custom Layout\\): (?<layoutBottomBottom>\\d+)\r\n\r\n",

      "## LLE\r\n\r\n",

      "- \\[(?<lleSpi>[x ])] SPI\r\n",
      "- \\[(?<lleGpio>[x ])] GPIO\r\n",
      "- \\[(?<lleMp>[x ])] MP\r\n",
      "- \\[(?<lleCdc>[x ])] CDC\r\n",
      "- \\[(?<lleHttp>[x ])] HTTP\r\n",
      "- \\[(?<lleCsnd>[x ])] CSND\r\n",
      "- \\[(?<lleNs>[x ])] NS\r\n",
      "- \\[(?<lleNfc>[x ])] NFC\r\n",
      "- \\[(?<llePtm>[x ])] PTM\r\n",
      "- \\[(?<lleNews>[x ])] NEWS\r\n",
      "- \\[(?<lleNdm>[x ])] NDM\r\n",
      "- \\[(?<lleMic>[x ])] MIC\r\n",
      "- \\[(?<lleI2c>[x ])] I2C\r\n",
      "- \\[(?<lleIr>[x ])] IR\r\n",
      "- \\[(?<llePdn>[x ])] PDN\r\n",
      "- \\[(?<lleNim>[x ])] NIM\r\n",
      "- \\[(?<lleHid>[x ])] HID\r\n",
      "- \\[(?<lleGsp>[x ])] GSP\r\n",
      "- \\[(?<lleFrd>[x ])] FRD\r\n",
      "- \\[(?<lleCfg>[x ])] CFG\r\n",
      "- \\[(?<llePs>[x ])] PS\r\n",
      "- \\[(?<lleCecd>[x ])] CECD\r\n",
      "- \\[(?<lleDsp>[x ])] DSP\r\n",
      "- \\[(?<lleCam>[x ])] CAM\r\n",
      "- \\[(?<lleMcu>[x ])] MCU\r\n",
      "- \\[(?<lleSsl>[x ])] SSL\r\n",
      "- \\[(?<lleBoss>[x ])] BOSS\r\n",
      "- \\[(?<lleAct>[x ])] ACT\r\n",
      "- \\[(?<lleAc>[x ])] AC\r\n",
      "- \\[(?<lleAm>[x ])] AM\r\n",
      "- \\[(?<lleErr>[x ])] ERR\r\n",
      "- \\[(?<llePxi>[x ])] PXI\r\n",
      "- \\[(?<lleNwm>[x ])] NWM\r\n",
      "- \\[(?<lleDlp>[x ])] DLP\r\n",
      "- \\[(?<lleLdr>[x ])] LDR\r\n",
      "- \\[(?<llePm>[x ])] PM\r\n",
      "- \\[(?<lleSoc>[x ])] SOC\r\n",
      "- \\[(?<lleFs>[x ])] FS\r\n\r\n",

      "## Hacks\r\n\r\n",

      "- \\[(?<hacksPriorityBoost>[x ])] Priority Boost\r\n\r\n",

      "## Multiplayer\r\n\r\n",

      "IP: (?<multiplayerIp>.+)\r\n",
      "Port: (?<multiplayerPort>\\d+)\r\n",
      "Nickname: (?<multiplayerNickname>.+)\r\n",
      "Password: (?<multiplayerPassword>.+)(?:\r\n)?$",
    ].join("")
  )
);

const names = [];
const types = [];
const calls = [];

let somethingChanged = false;

if (match.groups.startFile !== "Empty") {
  names.push("vvctre_settings_set_file_path");
  types.push(["void", "const char* value"]);
  calls.push(`vvctre_settings_set_file_path("${match.groups.startFile}");`);
  somethingChanged = true;
}

if (match.groups.startPlayMovie !== "Empty") {
  names.push("vvctre_settings_set_play_movie");
  types.push(["void", "const char* value"]);
  calls.push(
    `vvctre_settings_set_play_movie("${match.groups.startPlayMovie}");`
  );
  somethingChanged = true;
}

if (match.groups.startRecordMovie !== "Empty") {
  names.push("vvctre_settings_set_record_movie");
  types.push(["void", "const char* value"]);
  calls.push(
    `vvctre_settings_set_record_movie("${match.groups.startRecordMovie}");`
  );
  somethingChanged = true;
}

if (match.groups.startRegion !== "Auto-select") {
  names.push("vvctre_settings_set_region_value");
  types.push(["void", "int value"]);
  calls.push(
    `vvctre_settings_set_region_value(${
      {
        Japan: 0,
        USA: 1,
        Europe: 2,
        Australia: 3,
        China: 4,
        Korea: 5,
        Taiwan: 6,
      }[match.groups.startRegion]
    });`
  );
  somethingChanged = true;
}

if (match.groups.startLogFilter !== "*:Info") {
  names.push("vvctre_settings_set_log_filter");
  types.push(["void", "const char* value"]);
  calls.push(
    `vvctre_settings_set_log_filter("${match.groups.startLogFilter}");`
  );
  somethingChanged = true;
}

if (
  match.groups.startInitialTime_Type === "Unix Timestamp" &&
  typeof match.groups.startInitialTime_Timestamp === "string"
) {
  names.push("vvctre_settings_set_initial_clock");
  names.push("vvctre_settings_set_unix_timestamp");
  types.push(["void", "int value"]);
  types.push("void", "u64 value");
  calls.push("vvctre_settings_set_initial_clock(1);");
  calls.push(
    `vvctre_settings_set_unix_timestamp(${match.groups.startInitialTime_Timestamp});`
  );
  somethingChanged = true;
}

if (match.groups.startUseVirtualSdCard === " ") {
  names.push("vvctre_settings_set_use_virtual_sd");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_use_virtual_sd(false);");
  somethingChanged = true;
}

if (match.groups.startStartInFullscreenMode === "x") {
  names.push("vvctre_settings_set_start_in_fullscreen_mode");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_start_in_fullscreen_mode(true);");
  somethingChanged = true;
}

if (match.groups.startRecordFrameTimes === "x") {
  names.push("vvctre_settings_set_record_frame_times");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_record_frame_times(true);");
  somethingChanged = true;
}

if (
  match.groups.startEnableGdbStub === "x" &&
  typeof match.groups.startGdbStubPort === "string"
) {
  names.push("vvctre_settings_enable_gdbstub");
  types.push(["void", "u16 port"]);
  calls.push(
    `vvctre_settings_enable_gdbstub(${match.groups.startGdbStubPort});`
  );
  somethingChanged = true;
}

if (match.groups.generalUseCpuJit === " ") {
  names.push("vvctre_settings_set_use_cpu_jit");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_use_cpu_jit(false);");
  somethingChanged = true;
}

if (match.groups.generalLimitSpeed === " ") {
  names.push("vvctre_settings_set_limit_speed");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_limit_speed(false);");
  somethingChanged = true;
}

if (
  typeof match.groups.generalSpeedLimit === "string" &&
  match.groups.generalSpeedLimit !== "100"
) {
  names.push("vvctre_settings_set_speed_limit");
  types.push(["void", "u16 value"]);
  calls.push(
    `vvctre_settings_set_speed_limit(${match.groups.generalSpeedLimit});`
  );
  somethingChanged = true;
}

if (match.groups.generalUseCustomCpuTicks === "x") {
  names.push("vvctre_settings_set_use_custom_cpu_ticks");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_use_custom_cpu_ticks(true);");
  somethingChanged = true;
}

if (typeof match.groups.generalCustomCpuTicks !== "undefined") {
  names.push("vvctre_settings_set_custom_cpu_ticks");
  types.push(["void", "u64 value"]);
  calls.push(
    `vvctre_settings_set_custom_cpu_ticks(${match.groups.generalCustomCpuTicks});`
  );
  somethingChanged = true;
}

if (match.groups.generalCpuClockPercentage !== "100") {
  names.push("vvctre_settings_set_cpu_clock_percentage");
  types.push(["void", "u32 value"]);
  calls.push(
    `vvctre_settings_set_cpu_clock_percentage(${match.groups.generalCpuClockPercentage});`
  );
  somethingChanged = true;
}

if (match.groups.audioEnableDspLle === "x") {
  names.push("vvctre_settings_set_enable_dsp_lle");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_enable_dsp_lle(true);");
  somethingChanged = true;
}

if (match.groups.audioDspLleUseMultipleThreads === "x") {
  names.push("vvctre_settings_set_enable_dsp_lle_multithread");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_enable_dsp_lle_multithread(true);");
  somethingChanged = true;
}

if (match.groups.audioVolume !== "1.000") {
  names.push("vvctre_settings_set_audio_volume");
  types.push(["void", "float value"]);
  calls.push(`vvctre_settings_set_audio_volume(${match.groups.audioVolume}f);`);
  somethingChanged = true;
}

if (match.groups.audioSink !== "auto") {
  names.push("vvctre_settings_set_audio_sink_id");
  types.push(["void", "const char* value"]);
  calls.push(`vvctre_settings_set_audio_sink_id("${match.groups.audioSink}");`);
  somethingChanged = true;
}

if (match.groups.audioDevice !== "auto") {
  names.push("vvctre_settings_set_audio_device_id");
  types.push(["void", "const char* value"]);
  calls.push(
    `vvctre_settings_set_audio_device_id("${match.groups.audioDevice}");`
  );
  somethingChanged = true;
}

if (match.groups.audioDevice !== "auto") {
  names.push("vvctre_settings_set_audio_device_id");
  types.push(["void", "const char* value"]);
  calls.push(
    `vvctre_settings_set_audio_device_id("${match.groups.audioDevice}");`
  );
  somethingChanged = true;
}

if (match.groups.audioMicrophoneInputType !== "Disabled") {
  names.push("vvctre_settings_set_microphone_input_type");
  types.push(["void", "int value"]);
  calls.push(
    `vvctre_settings_set_microphone_input_type(${
      {
        "Real Device": 1,
        "Static Noise": 2,
      }[match.groups.audioMicrophoneInputType]
    });`
  );
  somethingChanged = true;
}

if (typeof match.groups.audioMicrophoneDevice === "string") {
  names.push("vvctre_settings_set_microphone_device");
  types.push(["void", "const char* value"]);
  calls.push(
    `vvctre_settings_set_microphone_device("${match.groups.audioMicrophoneDevice}");`
  );
  somethingChanged = true;
}

if (match.groups.cameraInnerCameraEngine === "image") {
  names.push("vvctre_settings_set_camera_engine");
  types.push(["void", "int index, const char* value"]);
  calls.push('vvctre_settings_set_camera_engine(1, "image");');
  somethingChanged = true;
}

if (typeof match.groups.cameraInnerCameraParameter === "string") {
  names.push("vvctre_settings_set_camera_parameter");
  types.push(["void", "int index, const char* value"]);
  calls.push(
    `vvctre_settings_set_camera_parameter(1, "${match.groups.cameraInnerCameraParameter}");`
  );
  somethingChanged = true;
}

if (match.groups.cameraOuterLeftCameraEngine === "image") {
  names.push("vvctre_settings_set_camera_engine");
  types.push(["void", "int index, const char* value"]);
  calls.push('vvctre_settings_set_camera_engine(2, "image");');
  somethingChanged = true;
}

if (typeof match.groups.cameraOuterLeftCameraParameter === "string") {
  names.push("vvctre_settings_set_camera_parameter");
  types.push(["void", "int index, const char* value"]);
  calls.push(
    `vvctre_settings_set_camera_parameter(2, "${match.groups.cameraOuterLeftCameraParameter}");`
  );
  somethingChanged = true;
}

if (match.groups.cameraOuterRightCameraEngine === "image") {
  names.push("vvctre_settings_set_camera_engine");
  types.push(["void", "int index, const char* value"]);
  calls.push('vvctre_settings_set_camera_engine(0, "image");');
  somethingChanged = true;
}

if (typeof match.groups.cameraOuterRightCameraParameter === "string") {
  names.push("vvctre_settings_set_camera_parameter");
  types.push(["void", "int index, const char* value"]);
  calls.push(
    `vvctre_settings_set_camera_parameter(0, "${match.groups.cameraOuterRightCameraParameter}");`
  );
  somethingChanged = true;
}

if (match.groups.graphicsUseHardwareRenderer === " ") {
  names.push("vvctre_settings_set_use_hardware_renderer");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_use_hardware_renderer(false);");
  somethingChanged = true;
}

if (match.groups.graphicsUseHardwareShader === " ") {
  names.push("vvctre_settings_set_use_hardware_shader");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_use_hardware_shader(false);");
  somethingChanged = true;
}

if (match.groups.graphicsAccurateMultiplication === "x") {
  names.push("vvctre_settings_set_hardware_shader_accurate_multiplication");
  types.push(["void", "bool value"]);
  calls.push(
    "vvctre_settings_set_hardware_shader_accurate_multiplication(true);"
  );
  somethingChanged = true;
}

if (match.groups.graphicsUseShaderJit === " ") {
  names.push("vvctre_settings_set_use_shader_jit");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_use_shader_jit(false);");
  somethingChanged = true;
}

if (match.groups.graphicsEnableVsync === "x") {
  names.push("vvctre_settings_set_enable_vsync");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_enable_vsync(true);");
  somethingChanged = true;
}

if (match.groups.graphicsDumpTextures === "x") {
  names.push("vvctre_settings_set_dump_textures");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_dump_textures(true);");
  somethingChanged = true;
}

if (match.groups.graphicsUseCustomTextures === "x") {
  names.push("vvctre_settings_set_custom_textures");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_custom_textures(true);");
  somethingChanged = true;
}

if (match.groups.graphicsPreloadCustomTextures === "x") {
  names.push("vvctre_settings_set_hardware_shader_accurate_multiplication");
  types.push(["void", "bool value"]);
  calls.push(
    "vvctre_settings_set_hardware_shader_accurate_multiplication(true);"
  );
  somethingChanged = true;
}

if (match.groups.graphicsEnableLinearFiltering === " ") {
  names.push("vvctre_settings_set_enable_linear_filtering");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_enable_linear_filtering(false);");
  somethingChanged = true;
}

if (match.groups.graphicsSharperDistantObjects === "x") {
  names.push("vvctre_settings_set_sharper_distant_objects");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_sharper_distant_objects(true);");
  somethingChanged = true;
}

if (match.groups.graphicsResolution !== "1") {
  names.push("vvctre_settings_set_resolution");
  types.push(["void", "u16 value"]);
  calls.push(
    `vvctre_settings_set_resolution(${match.groups.graphicsResolution});`
  );
  somethingChanged = true;
}

if (match.groups.graphicsBackgroundColor !== "000000") {
  names.push("vvctre_settings_set_background_color_red");
  names.push("vvctre_settings_set_background_color_green");
  names.push("vvctre_settings_set_background_color_blue");
  types.push(["void", "float value"]);
  types.push(["void", "float value"]);
  types.push(["void", "float value"]);
  calls.push(
    `vvctre_settings_set_background_color_red(${
      Number.parseInt(match.groups.graphicsBackgroundColor.slice(0, 2), 16) /
      255
    });`
  );
  calls.push(
    `vvctre_settings_set_background_color_green(${
      Number.parseInt(match.groups.graphicsBackgroundColor.slice(2, 4), 16) /
      255
    });`
  );
  calls.push(
    `vvctre_settings_set_background_color_blue(${
      Number.parseInt(match.groups.graphicsBackgroundColor.slice(4, 6), 16) /
      255
    });`
  );
  somethingChanged = true;
}

if (match.groups.graphicsPostProcessingShader !== "none (builtin)") {
  names.push("vvctre_settings_set_post_processing_shader");
  types.push(["void", "const char* value"]);
  calls.push(
    `vvctre_settings_set_post_processing_shader("${match.groups.graphicsPostProcessingShader}");`
  );
  somethingChanged = true;
}

if (match.groups.graphicsTextureFilter !== "none") {
  names.push("vvctre_settings_set_texture_filter");
  types.push(["void", "const char* value"]);
  calls.push(
    `vvctre_settings_set_texture_filter("${match.groups.graphicsTextureFilter}");`
  );
  somethingChanged = true;
}

if (match.groups.graphics3dLeft !== "Off") {
  names.push("vvctre_settings_set_render_3d");
  types.push(["void", "int value"]);
  calls.push(
    `vvctre_settings_set_render_3d(${
      {
        "Side by Side": 1,
        Anaglyph: 2,
        Interlaced: 3,
      }[match.groups.graphics3dLeft]
    });`
  );
  somethingChanged = true;
}

if (match.groups.graphics3dRight !== "0") {
  names.push("vvctre_settings_set_factor_3d");
  types.push(["void", "u8 value"]);
  calls.push(`vvctre_settings_set_factor_3d(${match.groups.graphics3dRight});`);
  somethingChanged = true;
}

if (match.groups.layoutLayout !== "Default") {
  names.push("vvctre_settings_set_layout");
  types.push(["void", "int value"]);
  calls.push(
    `vvctre_settings_set_layout(${
      {
        "Single Screen": 1,
        "Large Screen": 2,
        "Side by Side": 3,
        "Medium Screen": 4,
      }[match.groups.layoutLayout]
    });`
  );
  somethingChanged = true;
}

if (match.groups.layoutUseCustomLayout === "x") {
  names.push("vvctre_settings_set_use_custom_layout");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_use_custom_layout(true);");
  somethingChanged = true;
}

if (match.groups.layoutSwapScreens === "x") {
  names.push("vvctre_settings_set_swap_screens");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_swap_screens(true);");
  somethingChanged = true;
}

if (match.groups.layoutUprightScreens === "x") {
  names.push("vvctre_settings_set_upright_screens");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_upright_screens(true);");
  somethingChanged = true;
}

if (match.groups.layoutTopLeft !== "0") {
  names.push("vvctre_settings_set_custom_layout_top_left");
  types.push(["void", "u16 value"]);
  calls.push(
    `vvctre_settings_set_custom_layout_top_left(${match.groups.layoutTopLeft});`
  );
  somethingChanged = true;
}

if (match.groups.layoutTopTop !== "0") {
  names.push("vvctre_settings_set_custom_layout_top_top");
  types.push(["void", "u16 value"]);
  calls.push(
    `vvctre_settings_set_custom_layout_top_top(${match.groups.layoutTopTop});`
  );
  somethingChanged = true;
}

if (match.groups.layoutTopRight !== "400") {
  names.push("vvctre_settings_set_custom_layout_top_right");
  types.push(["void", "u16 value"]);
  calls.push(
    `vvctre_settings_set_custom_layout_top_right(${match.groups.layoutTopRight});`
  );
  somethingChanged = true;
}

if (match.groups.layoutTopBottom !== "240") {
  names.push("vvctre_settings_set_custom_layout_top_bottom");
  types.push(["void", "u16 value"]);
  calls.push(
    `vvctre_settings_set_custom_layout_top_bottom(${match.groups.layoutTopBottom});`
  );
  somethingChanged = true;
}

if (match.groups.layoutBottomLeft !== "40") {
  names.push("vvctre_settings_set_custom_layout_bottom_left");
  types.push(["void", "u16 value"]);
  calls.push(
    `vvctre_settings_set_custom_layout_bottom_left(${match.groups.layoutBottomLeft});`
  );
  somethingChanged = true;
}

if (match.groups.layoutBottomTop !== "240") {
  names.push("vvctre_settings_set_custom_layout_bottom_top");
  types.push(["void", "u16 value"]);
  calls.push(
    `vvctre_settings_set_custom_layout_bottom_top(${match.groups.layoutBottomTop});`
  );
  somethingChanged = true;
}

if (match.groups.layoutBottomRight !== "360") {
  names.push("vvctre_settings_set_custom_layout_bottom_right");
  types.push(["void", "u16 value"]);
  calls.push(
    `vvctre_settings_set_custom_layout_bottom_right(${match.groups.layoutBottomRight});`
  );
  somethingChanged = true;
}

if (match.groups.layoutBottomBottom !== "480") {
  names.push("vvctre_settings_set_custom_layout_bottom_bottom");
  types.push(["void", "u16 value"]);
  calls.push(
    `vvctre_settings_set_custom_layout_bottom_bottom(${match.groups.layoutBottomBottom});`
  );
  somethingChanged = true;
}

if (match.groups.lleSpi === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("SPI", true);');
  somethingChanged = true;
}

if (match.groups.lleGpio === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("GPIO", true);');
  somethingChanged = true;
}

if (match.groups.lleMp === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("MP", true);');
  somethingChanged = true;
}

if (match.groups.lleCdc === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("CDC", true);');
  somethingChanged = true;
}

if (match.groups.lleHttp === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("HTTP", true);');
  somethingChanged = true;
}

if (match.groups.lleCsnd === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("CSND", true);');
  somethingChanged = true;
}

if (match.groups.lleNs === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("NS", true);');
  somethingChanged = true;
}

if (match.groups.lleNfc === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("NFC", true);');
  somethingChanged = true;
}

if (match.groups.llePtm === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("PTM", true);');
  somethingChanged = true;
}

if (match.groups.lleNews === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("NEWS", true);');
  somethingChanged = true;
}

if (match.groups.lleNdm === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("NDM", true);');
  somethingChanged = true;
}

if (match.groups.lleMic === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("MIC", true);');
  somethingChanged = true;
}

if (match.groups.lleI2c === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("I2C", true);');
  somethingChanged = true;
}

if (match.groups.lleIr === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("IR", true);');
  somethingChanged = true;
}

if (match.groups.llePdn === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("PDN", true);');
  somethingChanged = true;
}

if (match.groups.lleNim === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("NIM", true);');
  somethingChanged = true;
}

if (match.groups.lleHid === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("HID", true);');
  somethingChanged = true;
}

if (match.groups.lleGsp === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("GSP", true);');
  somethingChanged = true;
}

if (match.groups.lleFrd === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("FRD", true);');
  somethingChanged = true;
}

if (match.groups.lleCfg === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("CFG", true);');
  somethingChanged = true;
}

if (match.groups.llePs === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("PS", true);');
  somethingChanged = true;
}

if (match.groups.lleCecd === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("CECD", true);');
  somethingChanged = true;
}

if (match.groups.lleDsp === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("DSP", true);');
  somethingChanged = true;
}

if (match.groups.lleCam === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("CAM", true);');
  somethingChanged = true;
}

if (match.groups.lleMcu === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("MCU", true);');
  somethingChanged = true;
}

if (match.groups.lleSsl === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("SSL", true);');
  somethingChanged = true;
}

if (match.groups.lleBoss === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("BOSS", true);');
  somethingChanged = true;
}

if (match.groups.lleAct === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("ACT", true);');
  somethingChanged = true;
}

if (match.groups.lleAc === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("AC", true);');
  somethingChanged = true;
}

if (match.groups.lleAm === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("AM", true);');
  somethingChanged = true;
}

if (match.groups.lleErr === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("ERR", true);');
  somethingChanged = true;
}

if (match.groups.llePxi === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("PXI", true);');
  somethingChanged = true;
}

if (match.groups.lleNwm === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("NWM", true);');
  somethingChanged = true;
}

if (match.groups.lleDlp === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("DLP", true);');
  somethingChanged = true;
}

if (match.groups.lleLdr === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("LDR", true);');
  somethingChanged = true;
}

if (match.groups.llePm === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("PM", true);');
  somethingChanged = true;
}

if (match.groups.lleSoc === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("SOC", true);');
  somethingChanged = true;
}

if (match.groups.lleFs === "x") {
  names.push("vvctre_settings_set_use_lle_module");
  types.push(["void", "const char* name, bool value"]);
  calls.push('vvctre_settings_set_use_lle_module("FS", true);');
  somethingChanged = true;
}

if (match.groups.hacksPriorityBoost === " ") {
  names.push("vvctre_settings_set_enable_priority_boost");
  types.push(["void", "bool value"]);
  calls.push("vvctre_settings_set_enable_priority_boost(false);");
  somethingChanged = true;
}

if (match.groups.multiplayerIp !== "Empty") {
  names.push("vvctre_settings_set_multiplayer_ip");
  types.push(["void", "const char* value"]);
  calls.push(
    `vvctre_settings_set_multiplayer_ip("${match.groups.multiplayerIp}");`
  );
  somethingChanged = true;
}

if (match.groups.multiplayerPort !== "24872") {
  names.push("vvctre_settings_set_multiplayer_port");
  types.push(["void", "u16 value"]);
  calls.push(
    `vvctre_settings_set_multiplayer_port(${match.groups.multiplayerPort});`
  );
  somethingChanged = true;
}

if (match.groups.multiplayerNickname !== "Empty") {
  names.push("vvctre_settings_set_nickname");
  types.push(["void", "const char* value"]);
  calls.push(
    `vvctre_settings_set_nickname("${match.groups.multiplayerNickname}");`
  );
  somethingChanged = true;
}

if (match.groups.multiplayerPassword !== "Empty") {
  names.push("vvctre_settings_set_multiplayer_password");
  types.push(["void", "const char* value"]);
  calls.push(
    `vvctre_settings_set_multiplayer_password("${match.groups.multiplayerPassword}");`
  );
  somethingChanged = true;
}

if (!somethingChanged) {
  process.exit(1);
}

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
${names.map((name) => `    "${name}",\n`).join("")}
    NULL,
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
}`;

fs.writeFileSync("plugin.c", code);
