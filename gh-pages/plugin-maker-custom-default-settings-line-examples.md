---
title: Plugin Maker Custom Default Settings Line Examples
redirect_from:
  - /custom-default-settings-plugin-bot-and-plugin-maker-line-examples/
---

| Tab         | Setting(s)                                                               | Example(s)                                                         |
| ----------- | ------------------------------------------------------------------------ | ------------------------------------------------------------------ |
| Start       | File                                                                     | `start.file file.3ds`                                              |
| Start       | Play Movie                                                               | `start.play_movie file.vcm`                                        |
| Start       | Record Movie                                                             | `start.record_movie file.vcm`                                      |
| Start       | Region                                                                   | `start.region USA`                                                 |
| Start       | Log Filter                                                               | `start.log_filter *:Critical`                                      |
| Start       | Initial Time                                                             | `start.initial_time Unix Timestamp`                                |
| Start       | Unix Timestamp                                                           | `start.unix_timestamp 123`                                         |
| Start       | Use Virtual SD Card                                                      | `start.use_virtual_sd_card disable`                                |
| Start       | Record Frame Times                                                       | `start.record_frame_times enable`                                  |
| Start       | Enable GDB Stub<br>GDB Stub Port                                         | `start.gdb_stub enable 12345`                                      |
| General     | Use CPU JIT                                                              | `general.cpu_jit disable`                                          |
| General     | Enable Core 2                                                            | `general.core_2 enable`                                            |
| General     | Limit Speed                                                              | `general.limit_speed disable`                                      |
| General     | Enable Custom CPU Ticks                                                  | `general.enable_custom_cpu_ticks enable`                           |
| General     | Speed Limit                                                              | `general.speed_limit 1000`                                         |
| General     | Custom CPU Ticks                                                         | `general.custom_cpu_ticks 21000`                                   |
| General     | CPU Clock Percentage                                                     | `general.cpu_clock_percentage 82`                                  |
| Audio       | Enable DSP LLE                                                           | `audio.dsp_lle enable`                                             |
| Audio       | Enable DSP LLE -> Use multiple threads                                   | `audio.dsp_lle_multiple_threads enable`                            |
| Audio       | Output -> Enable Stretching                                              | `audio.stretching disable`                                         |
| Audio       | Output -> Volume                                                         | `audio.volume 0.5f`                                                |
| Audio       | Output -> Sink                                                           | `audio.sink sdl2`                                                  |
| Audio       | Output -> Device                                                         | `audio.device auto`                                                |
| Audio       | Microphone -> Source                                                     | `audio.microphone_input_type Static Noise`                         |
| Audio       | Microphone -> Backend                                                    | `audio.microphone_real_device_backend SDL2`                        |
| Audio       | Microphone -> Device                                                     | `audio.microphone_device auto`                                     |
| Camera      | Inner -> Engine                                                          | `camera.inner_engine image`                                        |
| Camera      | Inner -> Parameter                                                       | `camera.inner_parameter inner.png`                                 |
| Camera      | Outer Left -> Engine                                                     | `camera.outer_left_engine image`                                   |
| Camera      | Outer Left -> Parameter                                                  | `camera.outer_left_parameter outer_left.png`                       |
| Camera      | Outer Right -> Engine                                                    | `camera.outer_right_engine image`                                  |
| Camera      | Outer Right -> Parameter                                                 | `camera.outer_right_parameter outer_right.png`                     |
| System      | Play Coins -> Play Coins                                                 | `system.play_coins 300`                                            |
| Graphics    | Use Hardware Renderer                                                    | `graphics.hardware_renderer disable`                               |
| Graphics    | Use Hardware Renderer -> Use Hardware Shader                             | `graphics.hardware_shader disable`                                 |
| Graphics    | Use Hardware Renderer -> Use Hardware Shader -> Accurate Multiplication  | `graphics.hardware_shader_accurate_multiplication enable`          |
| Graphics    | Use Hardware Renderer -> Use Hardware Shader -> Enable Disk Shader Cache | `graphics.enable_disk_shader_cache enable`                         |
| Graphics    | Use Shader JIT                                                           | `graphics.shader_jit disable`                                      |
| Graphics    | Enable VSync                                                             | `graphics.vsync enable`                                            |
| Graphics    | Dump Textures                                                            | `graphics.dump_textures enable`                                    |
| Graphics    | Use Custom Textures                                                      | `graphics.custom_textures enable`                                  |
| Graphics    | Preload Custom Textures                                                  | `graphics.preload_custom_textures enable`                          |
| Graphics    | Enable Linear Filtering                                                  | `graphics.linear_filtering disable`                                |
| Graphics    | Sharper Distant Objects                                                  | `graphics.sharper_distant_objects enable`                          |
| Graphics    | Background Color                                                         | `graphics.background_color #001122`                                |
| Graphics    | Resolution                                                               | 1. `graphics.resolution 2`<br>2. `graphics.resolution Window Size` |
| Graphics    | Post Processing Shader                                                   | `graphics.post_processing_shader shader`                           |
| Graphics    | Texture Filter                                                           | `graphics.texture_filter xBRZ freescale`                           |
| Graphics    | 3D Mode                                                                  | `graphics.3d_mode Side by Side`                                    |
| Graphics    | 3D Factor                                                                | `graphics.3d_factor 100`                                           |
| Layout      | Layout                                                                   | `layout.layout Medium Screen`                                      |
| Layout      | Use Custom Layout                                                        | `layout.use_custom_layout enable`                                  |
| Layout      | Swap Screens                                                             | `layout.swap_screens enable`                                       |
| Layout      | Upright Screens                                                          | `layout.upright_screens enable`                                    |
| Layout      | Top Screen -> Left                                                       | `layout.top_left 123`                                              |
| Layout      | Top Screen -> Top                                                        | `layout.top_top 123`                                               |
| Layout      | Top Screen -> Right                                                      | `layout.top_right 123`                                             |
| Layout      | Top Screen -> Bottom                                                     | `layout.top_bottom 123`                                            |
| Layout      | Bottom Screen -> Left                                                    | `layout.bottom_left 123`                                           |
| Layout      | Bottom Screen -> Top                                                     | `layout.bottom_top 123`                                            |
| Layout      | Bottom Screen -> Right                                                   | `layout.bottom_right 123`                                          |
| Layout      | Bottom Screen -> Bottom                                                  | `layout.bottom_bottom 123`                                         |
| Multiplayer | IP                                                                       | `multiplayer.ip 127.0.0.1`                                         |
| Multiplayer | Port                                                                     | `multiplayer.port 12345`                                           |
| Multiplayer | Nickname                                                                 | `multiplayer.nickname vvctre`                                      |
| LLE Modules | FS                                                                       | `lle.fs enable`                                                    |
| LLE Modules | PM                                                                       | `lle.pm enable`                                                    |
| LLE Modules | LDR                                                                      | `lle.ldr enable`                                                   |
| LLE Modules | PXI                                                                      | `lle.pxi enable`                                                   |
| LLE Modules | ERR                                                                      | `lle.err enable`                                                   |
| LLE Modules | AC                                                                       | `lle.ac enable`                                                    |
| LLE Modules | ACT                                                                      | `lle.act enable`                                                   |
| LLE Modules | AM                                                                       | `lle.am enable`                                                    |
| LLE Modules | BOSS                                                                     | `lle.boss enable`                                                  |
| LLE Modules | CAM                                                                      | `lle.cam enable`                                                   |
| LLE Modules | CECD                                                                     | `lle.cecd enable`                                                  |
| LLE Modules | CFG                                                                      | `lle.cfg enable`                                                   |
| LLE Modules | DLP                                                                      | `lle.dlp enable`                                                   |
| LLE Modules | DSP                                                                      | `lle.dsp enable`                                                   |
| LLE Modules | FRD                                                                      | `lle.frd enable`                                                   |
| LLE Modules | GSP                                                                      | `lle.gsp enable`                                                   |
| LLE Modules | HID                                                                      | `lle.hid enable`                                                   |
| LLE Modules | IR                                                                       | `lle.ir enable`                                                    |
| LLE Modules | MIC                                                                      | `lle.mic enable`                                                   |
| LLE Modules | NDM                                                                      | `lle.ndm enable`                                                   |
| LLE Modules | NEWS                                                                     | `lle.news enable`                                                  |
| LLE Modules | NFC                                                                      | `lle.nfc enable`                                                   |
| LLE Modules | NIM                                                                      | `lle.nim enable`                                                   |
| LLE Modules | NS                                                                       | `lle.ns enable`                                                    |
| LLE Modules | NWM                                                                      | `lle.nwm enable`                                                   |
| LLE Modules | PTM                                                                      | `lle.ptm enable`                                                   |
| LLE Modules | CSND                                                                     | `lle.csnd enable`                                                  |
| LLE Modules | HTTP                                                                     | `lle.http enable`                                                  |
| LLE Modules | SOC                                                                      | `lle.soc enable`                                                   |
| LLE Modules | SSL                                                                      | `lle.ssl enable`                                                   |
| LLE Modules | PS                                                                       | `lle.ps enable`                                                    |
| LLE Modules | MCU                                                                      | `lle.mcu enable`                                                   |
| LLE Modules | CDC                                                                      | `lle.cdc enable`                                                   |
| LLE Modules | GPIO                                                                     | `lle.gpio enable`                                                  |
| LLE Modules | I2C                                                                      | `lle.i2c enable`                                                   |
| LLE Modules | MP                                                                       | `lle.mp enable`                                                    |
| LLE Modules | PDN                                                                      | `lle.pdn enable`                                                   |
| LLE Modules | SPI                                                                      | `lle.spi enable`                                                   |
