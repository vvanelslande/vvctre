---
name: Custom Default Settings Plugin Request
about: Custom Default Settings Plugin Request
title: Custom Default Settings Plugin Request
labels: Custom Default Settings Plugin Request
assignees: ""
---

<!--
If you want to enable something, change `[ ]` to `[x]`
If you want to disable something, change `[x]` to `[ ]`
-->

## Start

File: Empty
Play Movie: Empty
Record Movie: Empty
Region: Auto-select
Log Filter: \*:Info
Initial Time: System <!-- If this is Unix Timestamp, add the number after this -->

- [x] Use Virtual SD Card
- [ ] Start in Fullscreen Mode
- [ ] Record Frame Times
- [ ] Enable GDB Stub <!-- If this is enabled, add `Port: <port>` after this --->

## General

- [x] Use CPU JIT
- [x] Limit Speed <!-- Remove the To % if this is disabled --> To 100%
- [ ] Custom CPU Ticks <!-- If this is enabled, add the number after this -->

CPU Clock Percentage: 100%

## Audio

- [ ] Enable DSP LLE

Volume: 1.000
Sink: auto
Device: auto
Microphone Input Type: Disabled

<!-- If Microphone Input Type is Real Device, add Microphone Device after this -->

## Camera

Inner Camera Engine: blank

<!-- If Inner Camera Engine is image, add Inner Camera Parameter after this -->

Outer Left Camera Engine: blank

<!-- If Outer Left Camera Engine is image, add Outer Left Camera Parameter after this -->

Outer Right Camera Engine: blank

<!-- If Outer Right Camera Engine is image, add Outer Right Camera Parameter after this -->

## Graphics

- [x] Use Hardware Renderer
  - [x] Use Hardware Shader
    - [ ] Accurate Multiplication
- [x] Use Shader JIT
- [ ] Dump Textures
- [ ] Use Custom Textures
- [ ] Preload Custom Textures
- [x] Enable Linear Filtering
- [ ] Sharper Distant Objects

Resolution: 1
Background Color: #000000
Post Processing Shader: none (builtin)
Texture Filter: none
3D: Off 0%

## Layout

Layout: Default

- [ ] Use Custom Layout
- [ ] Swap Screens
- [ ] Upright Screens

<!--
If Use Custom Layout is enabled, add:

Top Left
Top Top
Top Right
Top Bottom
Bottom Left
Bottom Top
Bottom Right
Bottom Bottom
-->

## LLE

- [ ] SPI
- [ ] MP
- [ ] GPIO
- [ ] CDC
- [ ] HTTP
- [ ] CSND
- [ ] NS
- [ ] NFC
- [ ] PTM
- [ ] NEWS
- [ ] NDM
- [ ] MIC
- [ ] I2C
- [ ] IR
- [ ] PDN
- [ ] NIM
- [ ] HID
- [ ] GSP
- [ ] FRD
- [ ] CFG
- [ ] PS
- [ ] CECD
- [ ] DSP
- [ ] CAM
- [ ] MCU
- [ ] SSL
- [ ] BOSS
- [ ] ACT
- [ ] AC
- [ ] AM
- [ ] ERR
- [ ] PXI
- [ ] NWM
- [ ] DLP
- [ ] LDR
- [ ] PM
- [ ] SOC
- [ ] FS

## Hacks

- [x] Priority Boost

## Multiplayer

IP: Empty
Port: 24872
Nickname: Empty
Password: Empty
