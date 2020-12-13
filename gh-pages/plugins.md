---
title: Plugins
redirect_from:
  - /Plugins
---

# Plugin makers

Code and builds:

- [This]({{ site.baseurl }}/plugin-maker/code-and-builds-no-account/) (doesn't need a account, uses my vvctre API server)
- [This]({{ site.baseurl }}/plugin-maker/code-and-builds-user-github-account/) (needs a GitHub account)
- vvctre bot (it's in my Discord server)
- vvctre API (my server: [https://vvctre.dynv6.net:8831](https://vvctre.dynv6.net:8831))
  - Custom default settings:  
    &nbsp;&nbsp;Path: /make-custom-default-settings-plugin
    &nbsp;&nbsp;Method: POST  
    &nbsp;&nbsp;Body: Lines
  - Button to touch:  
    &nbsp;&nbsp;Path: /make-button-to-touch-plugin  
    &nbsp;&nbsp;Method: POST  
    &nbsp;&nbsp;Body: JSON object with `x`, `y`, and `params`
  - Window size:  
    &nbsp;&nbsp;Path: /make-window-size-plugin  
    &nbsp;&nbsp;Method: POST  
    &nbsp;&nbsp;Body: JSON object with `width` and `height`
  - Window position:  
    &nbsp;&nbsp;Path: /make-window-position-plugin  
    &nbsp;&nbsp;Method: POST  
    &nbsp;&nbsp;Body: JSON object with `x` and `y`
  - Log file:  
    &nbsp;&nbsp;Path: /make-log-file-plugin  
    &nbsp;&nbsp;Method: POST  
    &nbsp;&nbsp;Body: File path

Code only:

- [GUI]({{ site.baseurl }}/plugin-maker/code-only/)
- [CLI](https://github.com/vvanelslande/vvctre-cli-code-only-plugin-maker)

# Plugin list

## GitHub

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-quick-settings](https://github.com/vvanelslande/vvctre-plugin-quick-settings)

Adds a quick settings window

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-quick-settings/releases/download/2.0.2/vvctre-plugin-quick-settings-2.0.2-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-quick-settings/releases/download/2.0.2/vvctre-plugin-quick-settings-2.0.2-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-quick-settings/releases/download/2.0.2/vvctre-plugin-quick-settings-2.0.2-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-quick-settings/releases/download/2.0.2/vvctre-plugin-quick-settings-2.0.2-linux.zip)

### [tbsp](https://github.com/tbsp) / [vvctre-plugin-pause](https://github.com/tbsp/vvctre-plugin-pause)

A pause button for vvctre (F8)

[Download Windows 7z](https://github.com/tbsp/vvctre-plugin-pause/releases/download/1.0.2/vvctre-plugin-pause-1.0.2-Windows.7z) \| [Download Linux 7z](https://github.com/tbsp/vvctre-plugin-pause/releases/download/1.0.2/vvctre-plugin-pause-1.0.2-Linux.7z)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-default-multiplayer-settings-file](https://github.com/vvanelslande/vvctre-plugin-default-multiplayer-settings-file)

Loads multiplayer settings from multiplayer-settings.json (it needs to be in vvctre's folder) when vvctre starts (everything is optional) and joins the room when emulation is starting for the first time (it's disabled if default-multiplayer-settings.json doesn't have `join_room_when_emulation_is_starting_for_the_first_time`)

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-default-multiplayer-settings-file/releases/download/1.0.2/vvctre-plugin-default-multiplayer-settings-file-1.0.2-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-default-multiplayer-settings-file/releases/download/1.0.2/vvctre-plugin-default-multiplayer-settings-file-1.0.2-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-default-multiplayer-settings-file/releases/download/1.0.2/vvctre-plugin-default-multiplayer-settings-file-1.0.2-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-default-multiplayer-settings-file/releases/download/1.0.2/vvctre-plugin-default-multiplayer-settings-file-1.0.2-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-enable-core-2](https://github.com/vvanelslande/vvctre-plugin-enable-core-2)

Enables core 2 when vvctre starts

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-enable-core-2/releases/download/1.0.0/vvctre-plugin-enable-core-2-1.0.0-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-enable-core-2/releases/download/1.0.0/vvctre-plugin-enable-core-2-1.0.0-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-enable-core-2/releases/download/1.0.0/vvctre-plugin-enable-core-2-1.0.0-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-enable-core-2/releases/download/1.0.0/vvctre-plugin-enable-core-2-1.0.0-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-default-start-paths-file](https://github.com/vvanelslande/vvctre-plugin-default-start-paths-file)

Loads Start tab paths (file, play movie, and record movie) from default-start-paths.json (it needs to be in vvctre's folder) when vvctre starts

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-default-start-paths-file/releases/download/1.0.0/vvctre-plugin-default-start-paths-file-1.0.0-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-default-start-paths-file/releases/download/1.0.0/vvctre-plugin-default-start-paths-file-1.0.0-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-default-start-paths-file/releases/download/1.0.0/vvctre-plugin-default-start-paths-file-1.0.0-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-default-start-paths-file/releases/download/1.0.0/vvctre-plugin-default-start-paths-file-1.0.0-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-enable-disk-shader-cache](https://github.com/vvanelslande/vvctre-plugin-enable-disk-shader-cache)

Enables Graphics -> Use Hardware Renderer -> Use Hardware Shader -> Enable Disk Shader Cache when vvctre starts

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-enable-disk-shader-cache/releases/download/1.0.0/vvctre-plugin-enable-disk-shader-cache-1.0.0-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-enable-disk-shader-cache/releases/download/1.0.0/vvctre-plugin-enable-disk-shader-cache-1.0.0-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-enable-disk-shader-cache/releases/download/1.0.0/vvctre-plugin-enable-disk-shader-cache-1.0.0-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-enable-disk-shader-cache/releases/download/1.0.0/vvctre-plugin-enable-disk-shader-cache-1.0.0-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-make-all-settings-in-quick-settings-persistent](https://github.com/vvanelslande/vvctre-plugin-make-all-settings-in-quick-settings-persistent)

Loads all settings in Quick Settings from settings.json (it needs to be in vvctre's folder) when vvctre is starting and saves all settings in Quick Settings to settings.json when vvctre is closing. Only loads and saves settings that are in settings.json.

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-make-all-settings-in-quick-settings-persistent/releases/download/1.0.0/vvctre-plugin-make-all-settings-in-quick-settings-persistent-1.0.0-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-make-all-settings-in-quick-settings-persistent/releases/download/1.0.0/vvctre-plugin-make-all-settings-in-quick-settings-persistent-1.0.0-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-make-settings-persistent/releases/download/1.0.0/vvctre-plugin-make-all-settings-in-quick-settings-persistent-1.0.0-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-make-settings-persistent/releases/download/1.0.0/vvctre-plugin-make-all-settings-in-quick-settings-persistent-1.0.0-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-skip-hle-nwm-wlan-comm-id-check](https://github.com/vvanelslande/vvctre-plugin-skip-hle-nwm-wlan-comm-id-check)

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-skip-hle-nwm-wlan-comm-id-check/releases/download/1.0.1/vvctre-plugin-skip-hle-nwm-wlan-comm-id-check-1.0.1-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-skip-hle-nwm-wlan-comm-id-check/releases/download/1.0.1/vvctre-plugin-skip-hle-nwm-wlan-comm-id-check-1.0.1-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-skip-hle-nwm-wlan-comm-id-check/releases/download/1.0.1/vvctre-plugin-skip-hle-nwm-wlan-comm-id-check-1.0.1-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-skip-hle-nwm-wlan-comm-id-check/releases/download/1.0.1/vvctre-plugin-skip-hle-nwm-wlan-comm-id-check-1.0.1-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-default-window-size-and-position-file](https://github.com/vvanelslande/vvctre-plugin-default-window-size-and-position-file)

Loads window size and position from default-window-size-and-position.json (it needs to be in vvctre's folder) (it needs to be in vvctre's folder) when vvctre starts (size and position can be disabled)

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-default-window-size-and-position-file/releases/download/1.0.0/vvctre-plugin-default-window-size-and-position-file-1.0.0-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-default-window-size-and-position-file/releases/download/1.0.0/vvctre-plugin-default-window-size-and-position-file-1.0.0-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-default-window-size-and-position-file/releases/download/1.0.0/vvctre-plugin-default-window-size-and-position-file-1.0.0-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-default-window-size-and-position-file/releases/download/1.0.0/vvctre-plugin-default-window-size-and-position-file-1.0.0-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-auto-load-controls](https://github.com/vvanelslande/vvctre-plugin-auto-load-controls)

Loads controls.json (it needs to be in vvctre's folder) when vvctre starts

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-auto-load-controls/releases/download/1.0.2/vvctre-plugin-auto-load-controls-1.0.2-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-auto-load-controls/releases/download/1.0.2/vvctre-plugin-auto-load-controls-1.0.2-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-auto-load-controls/releases/download/1.0.2/vvctre-plugin-auto-load-controls-1.0.2-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-auto-load-controls/releases/download/1.0.2/vvctre-plugin-auto-load-controls-1.0.2-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-2ds-switch](https://github.com/vvanelslande/vvctre-plugin-2ds-switch)

Adds a 2DS switch

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-2ds-switch/releases/download/1.0.0/vvctre-plugin-2ds-switch-1.0.0-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-2ds-switch/releases/download/1.0.0/vvctre-plugin-2ds-switch-1.0.0-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-2ds-switch/releases/download/1.0.0/vvctre-plugin-2ds-switch-1.0.0-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-2ds-switch/releases/download/1.0.0/vvctre-plugin-2ds-switch-1.0.0-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vnc4vvctre](https://github.com/vvanelslande/vnc4vvctre)

Adds a VNC server

[Download Windows 7z](https://github.com/vvanelslande/vnc4vvctre/releases/download/1.3.2/vnc4vvctre-1.3.2-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vnc4vvctre/releases/download/1.3.2/vnc4vvctre-1.3.2-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vnc4vvctre/releases/download/1.3.2/vnc4vvctre-1.3.2-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vnc4vvctre/releases/download/1.3.2/vnc4vvctre-1.3.2-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-button-to-touch-json](https://github.com/vvanelslande/vvctre-plugin-button-to-touch-json)

Button to touch plugin that supports more than 1 button. Keyboard code list: [https://www.libsdl.org/tmp/SDL/include/SDL_scancode.h](https://www.libsdl.org/tmp/SDL/include/SDL_scancode.h). Default is ".".

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-button-to-touch-json/releases/download/1.0.0/vvctre-plugin-button-to-touch-json-1.0.0-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-button-to-touch-json/releases/download/1.0.0/vvctre-plugin-button-to-touch-json-1.0.0-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-button-to-touch-json/releases/download/1.0.0/vvctre-plugin-button-to-touch-json-1.0.0-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-button-to-touch-json/releases/download/1.0.0/vvctre-plugin-button-to-touch-json-1.0.0-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-cycle-custom-layouts](https://github.com/vvanelslande/vvctre-plugin-cycle-custom-layouts)

Adds disableable and changeable hotkey/button that cycles custom layouts, and optionally loads the first layout when vvctre starts and emulation starts for the first time. Layouts can have a window size, window position, and a Upright Screens override.

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-cycle-custom-layouts/releases/download/3.2.2/vvctre-plugin-cycle-custom-layouts-3.2.2-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-cycle-custom-layouts/releases/download/3.2.2/vvctre-plugin-cycle-custom-layouts-3.2.2-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-cycle-custom-layouts/releases/download/3.2.2/vvctre-plugin-cycle-custom-layouts-3.2.2-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-cycle-custom-layouts/releases/download/3.2.2/vvctre-plugin-cycle-custom-layouts-3.2.2-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-log-file](https://github.com/vvanelslande/vvctre-plugin-log-file)

Writes the logs to `log.txt` in vvctre's folder

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-log-file/releases/download/2.0.0/vvctre-plugin-log-file-2.0.0-windows.7z) \| [Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-log-file/releases/download/2.0.0/vvctre-plugin-log-file-2.0.0-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-log-file/releases/download/2.0.0/vvctre-plugin-log-file-2.0.0-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-log-file/releases/download/2.0.0/vvctre-plugin-log-file-2.0.0-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-mjpeg-camera](https://github.com/vvanelslande/vvctre-mjpeg-camera)

Server that returns JPEGs from a MJPEG stream and a vvctre plugin that changes the camera settings and reloads the camera images every frame

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-enable-lle-modules-in-file-when-starting](https://github.com/vvanelslande/vvctre-plugin-enable-lle-modules-in-file-when-starting)

Enables the LLE modules in `enable-lle-modules-in-file-when-starting.txt` (it needs to be in vvctre's folder) when vvctre starts

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-enable-lle-modules-in-file-when-starting/releases/download/2.0.1/vvctre-plugin-enable-lle-modules-in-file-when-starting-2.0.1-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-enable-lle-modules-in-file-when-starting/releases/download/2.0.1/vvctre-plugin-enable-lle-modules-in-file-when-starting-2.0.1-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-enable-lle-modules-in-file-when-starting/releases/download/2.0.1/vvctre-plugin-enable-lle-modules-in-file-when-starting-2.0.1-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-enable-lle-modules-in-file-when-starting/releases/download/2.0.1/vvctre-plugin-enable-lle-modules-in-file-when-starting-2.0.1-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-free-hotkeys](https://github.com/vvanelslande/vvctre-plugin-free-hotkeys)

Adds some hotkeys (previously called vvctre-plugin-shortcuts-free)

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-free-hotkeys/releases/download/1.4.1/vvctre-plugin-free-hotkeys-1.4.1-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-free-hotkeys/releases/download/1.4.1/vvctre-plugin-free-hotkeys-1.4.1-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-free-hotkeys/releases/download/1.4.1/vvctre-plugin-free-hotkeys-1.4.1-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-free-hotkeys/releases/download/1.4.1/vvctre-plugin-free-hotkeys-1.4.1-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-get-touch-screen-x-and-y](https://github.com/vvanelslande/vvctre-plugin-get-touch-screen-x-and-y)

Shows touch screen X and Y

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-get-touch-screen-x-and-y/releases/download/1.0.2/vvctre-plugin-get-touch-screen-x-and-y-1.0.2-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-get-touch-screen-x-and-y/releases/download/1.0.2/vvctre-plugin-get-touch-screen-x-and-y-1.0.2-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-get-touch-screen-x-y/releases/download/1.0.2/vvctre-plugin-get-touch-screen-x-y-1.0.2-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-get-touch-screen-x-y/releases/download/1.0.2/vvctre-plugin-get-touch-screen-x-y-1.0.2-linux.zip)

### [vvanelslande](https://github.com/vvanelslande) / [vvctre-plugin-default-layout-settings-file](https://github.com/vvanelslande/vvctre-plugin-default-layout-settings-file)

Loads Layout, Swap Screens, and Upright Screens from default-layout-settings.json (it needs to be in vvctre's folder) when vvctre starts

[Download Windows 7z](https://github.com/vvanelslande/vvctre-plugin-default-layout-settings-file/releases/download/1.0.0/vvctre-plugin-default-layout-settings-file-1.0.0-windows.7z) \| [Download Windows zip](https://github.com/vvanelslande/vvctre-plugin-default-layout-settings-file/releases/download/1.0.0/vvctre-plugin-default-layout-settings-file-1.0.0-windows.zip) \| [Download Linux 7z](https://github.com/vvanelslande/vvctre-plugin-default-layout-settings-file/releases/download/1.0.0/vvctre-plugin-default-layout-settings-file-1.0.0-linux.7z) \| [Download Linux zip](https://github.com/vvanelslande/vvctre-plugin-default-layout-settings-file/releases/download/1.0.0/vvctre-plugin-default-layout-settings-file-1.0.0-linux.zip)

## Patreon tier #2

### Patreon hotkeys

[Download](https://www.patreon.com/posts/patreon-hotkeys-44818963)

Hotkeys:

- Load File
- Load Encrypted Amiibo
- Remove Amiibo
- Restart Emulation
- Install CIA
- Hold to remove speed limit

### Light theme

[Download](https://www.patreon.com/posts/light-theme-2-0-37869489)

### Dear ImGui classic theme

[Download](https://www.patreon.com/posts/dear-imgui-theme-37870192)

# Templates

- [C - Window](https://github.com/vvanelslande/vvctre-window-plugin-template-c)
- [C - Custom Default Settings](https://github.com/vvanelslande/vvctre-custom-default-settings-plugin-template)
- [C++ - Window](https://github.com/vvanelslande/vvctre-window-plugin-template-cpp)
- [C++ - Tab](https://github.com/vvanelslande/vvctre-tab-plugin-template)
- [C++ - Button To Touch](https://github.com/vvanelslande/vvctre-button-to-touch-plugin-template)

# Functions plugins can export

## `int GetRequiredFunctionCount()`

Required  
Returns the number of functions the plugin will use

## `const char** GetRequiredFunctionNames()`

Required  
Returns the names of the functions the plugin will use

## `void PluginLoaded(void* core, void* plugin_manager, void* functions[])`

Required  
You should set the function pointers in this function

## `void InitialSettingsOpening()`

Optional  
Called before Initial Settings opens

## `void InitialSettingsOkPressed()`

Optional  
Called after Initial Settings's OK button is pressed  
Called before loading the file if a file was dropped into `vvctre.exe` or vvctre was started with `./vvctre <file>`

## `void BeforeLoading()`

Optional  
Called before loading the file (only the first time)

## `void EmulationStarting()`

Optional  
Called after loading the file (only the first time)

## `void BeforeLoadingAfterFirstTime()`

Optional  
Called before loading the file (after the first time)

## `void EmulationStartingAfterFirstTime()`

Optional  
Called after loading the file (after the first time)

## `void EmulatorClosing()`

Optional  
Called when the emulator is closing

## `void FatalError()`

Optional  
Called when a fatal error happens

## `void BeforeDrawingFPS()`

Optional  
Called every frame before drawing FPS

## `void AddMenu()`

Optional  
Called every frame (only if the menu is open).  
The menu is added at the end.

## `void AddTab()`

Optional  
Called every frame (only if Initial Settings is open).  
The tab is added at the end.

## `void AfterSwapWindow()`

Optional  
Called after the end of every frame (only if Initial Settings is not open).

## `void ScreenshotCallback(void* data)`

Optional  
Called after a screenshot is made

## `void Log(const char* log)`

Optional  
Called when a line is added to the log

## `bool OverrideWlanCommIdCheck(u32 in_beacon, u32 requested)`

Optional  
Overrides the wlan_comm_id check  
If this returns true, the beacon is added to RecvBeaconBroadcastData's buffer

## `void OverrideOnLoadFailed(u32 result)`

Optional  
Called when a file load error happens  
If a plugin exports this, the file load error messages are disabled

# vvctre functions

## `void vvctre_load_file(void* core, const char* path)`

Loads a file

## `bool vvctre_install_cia(const char* path)`

Installs a CIA

## `bool vvctre_load_amiibo(void* core, const char* path)`

Loads a encrypted amiibo

## `void vvctre_load_amiibo_from_memory(void* core, const u8 data[540])`

Loads a encrypted amiibo from memory

## `bool vvctre_load_amiibo_decrypted(void* core, const char* path)`

Loads a decrypted amiibo

## `void vvctre_load_amiibo_from_memory_decrypted(void* core, const u8 data[540])`

Loads a decrypted amiibo from memory

## `void vvctre_get_amiibo_data(void* core, u8 data[540])`

Get the amiibo data (encrypted)

## `void vvctre_remove_amiibo(void* core)`

Removes the amiibo

## `u64 vvctre_get_program_id(void* core)`

Returns the current process's ID

## `const char* vvctre_get_process_name(void* core)`

Returns the current process's name

## `void vvctre_restart(void* core)`

Restarts emulation

## `void vvctre_set_paused(void* plugin_manager, bool paused)`

Pause/unpause emulation

## `bool vvctre_get_paused(void* plugin_manager)`

Returns whether emulation is paused

## `bool vvctre_emulation_running(void* core)`

Returns whether emulation is running

## `u8 vvctre_read_u8(void* core, VAddr address)`

Reads memory

## `void vvctre_write_u8(void* core, VAddr address, u8 value)`

Writes to memory

## `u16 vvctre_read_u16(void* core, VAddr address)`

Reads memory

## `void vvctre_write_u16(void* core, VAddr address, u16 value)`

Writes to memory

## `u32 vvctre_read_u32(void* core, VAddr address)`

Reads memory

## `void vvctre_write_u32(void* core, VAddr address, u32 value)`

Writes to memory

## `u64 vvctre_read_u64(void* core, VAddr address)`

Reads memory

## `void vvctre_write_u64(void* core, VAddr address, u64 value)`

Writes to memory

## `void vvctre_invalidate_cache_range(void* core, u32 address, std::size_t length)`

Invalidates a range of the running core's cache

## `void vvctre_invalidate_core_1_cache_range(void* core, u32 address, std::size_t length)`

Invalidates a range of core 1's cache

## `void vvctre_invalidate_core_2_cache_range(void* core, u32 address, std::size_t length)`

Invalidates a range of core 2's cache

## `void vvctre_set_pc(void* core, u32 addr)`

Sets the PC of the running core

## `void vvctre_set_core_1_pc(void* core, u32 addr)`

Sets the PC of core 1

## `void vvctre_set_core_2_pc(void* core, u32 addr)`

Sets the PC of core 2

## `u32 vvctre_get_pc(void* core)`

Returns the PC of the running core

## `u32 vvctre_get_core_1_pc(void* core)`

Returns the PC of core 1

## `u32 vvctre_get_core_2_pc(void* core)`

Returns the PC of core 2

## `void vvctre_set_register(void* core, int index, u32 value)`

Sets a register of the running core

## `void vvctre_set_core_1_register(void* core, int index, u32 value)`

Sets a register of core 1

## `void vvctre_set_core_2_register(void* core, int index, u32 value)`

Sets a register of core 2

## `u32 vvctre_get_register(void* core, int index)`

Returns a register of the running core

## `u32 vvctre_get_core_1_register(void* core, int index)`

Returns a register of core 1

## `u32 vvctre_get_core_2_register(void* core, int index)`

Returns a register of core 2

## `void vvctre_set_vfp_register(void* core, int index, u32 value)`

Sets a VFP register of the running core

## `void vvctre_set_core_1_vfp_register(void* core, int index, u32 value)`

Sets a VFP register of core 1

## `void vvctre_set_core_2_vfp_register(void* core, int index, u32 value)`

Sets a VFP register of core 2

## `u32 vvctre_get_vfp_register(void* core, int index)`

Returns a VFP register of the running core

## `u32 vvctre_get_core_1_vfp_register(void* core, int index)`

Returns a VFP register of core 1

## `u32 vvctre_get_core_2_vfp_register(void* core, int index)`

Returns a VFP register of core 2

## `void vvctre_set_vfp_system_register(void* core, int index, u32 value)`

Sets a VFP system register of the running core

## `void vvctre_set_core_1_vfp_system_register(void* core, int index, u32 value)`

Sets a VFP system register of core 1

## `void vvctre_set_core_2_vfp_system_register(void* core, int index, u32 value)`

Sets a VFP system register of core 2

## `u32 vvctre_get_vfp_system_register(void* core, int index)`

Returns a VFP system register of the running core

## `u32 vvctre_get_core_1_vfp_system_register(void* core, int index)`

Returns a VFP system register of core 1

## `u32 vvctre_get_core_2_vfp_system_register(void* core, int index)`

Returns a VFP system register of core 2

## `void vvctre_set_cp15_register(void* core, int index, u32 value)`

Sets a CP15 register of the running core

## `void vvctre_set_core_1_cp15_register(void* core, int index, u32 value)`

Sets a CP15 register of core 1

## `void vvctre_set_core_2_cp15_register(void* core, int index, u32 value)`

Sets a CP15 register of core 2

## `u32 vvctre_get_cp15_register(void* core, int index)`

Returns a CP15 register of the running core

## `u32 vvctre_get_core_1_cp15_register(void* core, int index)`

Returns a CP15 register of core 1

## `u32 vvctre_get_core_2_cp15_register(void* core, int index)`

Returns a CP15 register of core 2

## `void vvctre_ipc_recorder_set_enabled(void* core, bool enabled)`

Sets whether the IPC recorder is enabled

## `bool vvctre_ipc_recorder_get_enabled(void* core)`

Returns whether the IPC recorder is enabled

## `void vvctre_ipc_recorder_bind_callback(void* core, void (*callback)(const char* json))`

Adds a callback to the IPC recorder

## `const char* vvctre_get_service_name_by_port_id(void* core, u32 port)`

Converts a port ID to a name and returns it  
This uses strdup/\_strdup

## `int vvctre_cheat_count(void* core)`

Returns the number of cheats for the current game

## `const char* vvctre_get_cheat(void* core, int index)`

Returns a cheat  
This uses strdup/\_strdup

## `const char* vvctre_get_cheat_name(void* core, int index)`

Returns the name of a cheat  
This uses strdup/\_strdup

## `const char* vvctre_get_cheat_comments(void* core, int index)`

Returns the comments of a cheat  
This uses strdup/\_strdup

## `const char* vvctre_get_cheat_type(void* core, int index)`

Returns the type of a cheat  
This uses strdup/\_strdup

## `const char* vvctre_get_cheat_code(void* core, int index)`

Returns the code of a cheat  
This uses strdup/\_strdup

## `void vvctre_set_cheat_enabled(void* core, int index, bool enabled)`

Sets whether a cheat is enabled

## `bool vvctre_get_cheat_enabled(void* core, int index)`

Returns whether a cheat is enabled

## `void vvctre_add_gateway_cheat(void* core, const char* name, const char* code, const char* comments)`

Adds a Gateway cheat

## `void vvctre_remove_cheat(void* core, int index)`

Removes a cheat

## `void vvctre_update_gateway_cheat(void* core, int index, const char* name, const char* code, const char* comments)`

Updates a Gateway cheat

## `void vvctre_load_cheats_from_file(void* core)`

Loads the cheats from the file

## `void vvctre_save_cheats_to_file(void* core)`

Saves the cheats to the file

## `void vvctre_reload_camera_images(void* core)`

Reloads the camera images

## `void vvctre_gui_push_item_width(float item_width)`

`ImGui::PushItemWidth` wrapper

## `void vvctre_gui_pop_item_width()`

`ImGui::PopItemWidth` wrapper

## `void vvctre_gui_get_content_region_max(float out[2])`

Calls `ImGui::GetContentRegionMax`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `void vvctre_gui_get_content_region_avail(float out[2])`

Calls `ImGui::GetContentRegionAvail`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `void vvctre_gui_get_window_content_region_min(float out[2])`

Calls `ImGui::GetWindowContentRegionMin`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `void vvctre_gui_get_window_content_region_max(float out[2])`

Calls `ImGui::GetWindowContentRegionMax`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `float vvctre_gui_get_window_content_region_width()`

`ImGui::GetWindowContentRegionWidth` wrapper

## `float vvctre_gui_get_scroll_x()`

`ImGui::GetScrollX` wrapper

## `float vvctre_gui_get_scroll_y()`

`ImGui::GetScrollY` wrapper

## `float vvctre_gui_get_scroll_max_x()`

`ImGui::GetScrollMaxX` wrapper

## `float vvctre_gui_get_scroll_max_y()`

`ImGui::GetScrollMaxY` wrapper

## `void vvctre_gui_set_scroll_x(float scroll_x)`

`ImGui::SetScrollX` wrapper

## `void vvctre_gui_set_scroll_y(float scroll_y)`

`ImGui::SetScrollY` wrapper

## `void vvctre_gui_set_scroll_here_x(float center_x_ratio)`

`ImGui::SetScrollHereX` wrapper

## `void vvctre_gui_set_scroll_here_y(float center_y_ratio)`

`ImGui::SetScrollHereY` wrapper

## `void vvctre_gui_set_scroll_from_pos_x(float local_x, float center_x_ratio)`

`ImGui::SetScrollFromPosX` wrapper

## `void vvctre_gui_set_scroll_from_pos_y(float local_y, float center_y_ratio)`

`ImGui::SetScrollFromPosY` wrapper

## `void vvctre_gui_set_next_item_width(float item_width)`

`ImGui::SetNextItemWidth` wrapper

## `float vvctre_gui_calc_item_width()`

`ImGui::CalcItemWidth` wrapper

## `void vvctre_gui_push_text_wrap_pos(float wrap_local_pos_x)`

`ImGui::PushTextWrapPos` wrapper

## `void vvctre_gui_pop_text_wrap_pos()`

`ImGui::PopTextWrapPos` wrapper

## `void vvctre_gui_push_allow_keyboard_focus(bool allow_keyboard_focus)`

`ImGui::PushAllowKeyboardFocus` wrapper

## `void vvctre_gui_pop_allow_keyboard_focus()`

`ImGui::PopAllowKeyboardFocus` wrapper

## `void vvctre_gui_push_button_repeat(bool repeat)`

`ImGui::PushButtonRepeat` wrapper

## `void vvctre_gui_pop_button_repeat()`

`ImGui::PopButtonRepeat` wrapper

## `void vvctre_gui_push_font(void* font)`

`ImGui::PushFont` wrapper

## `void vvctre_gui_pop_font()`

`ImGui::PopFont` wrapper

## `void vvctre_gui_push_style_color(ImGuiCol idx, const float r, const float g, const float b, const float a)`

`ImGui::PushStyleColor` wrapper

## `void vvctre_gui_pop_style_color(int count)`

`ImGui::PopStyleColor` wrapper

## `void vvctre_gui_push_style_var_float(ImGuiStyleVar idx, float val)`

`ImGui::PushStyleVar` wrapper

## `void vvctre_gui_push_style_var_2floats(ImGuiStyleVar idx, float val[2])`

`ImGui::PushStyleVar` wrapper

## `void vvctre_gui_pop_style_var(int count)`

`ImGui::PopStyleVar` wrapper

## `void vvctre_gui_same_line()`

`ImGui::SameLine` wrapper

## `void vvctre_gui_new_line()`

`ImGui::NewLine` wrapper

## `void vvctre_gui_bullet()`

`ImGui::Bullet` wrapper

## `void vvctre_gui_indent()`

`ImGui::Indent` wrapper

## `void vvctre_gui_unindent()`

`ImGui::Unindent` wrapper

## `void vvctre_gui_begin_group()`

`ImGui::BeginGroup` wrapper

## `void vvctre_gui_end_group()`

`ImGui::EndGroup` wrapper

## `void vvctre_gui_get_cursor_pos(float out[2])`

Calls `ImGui::GetCursorPos`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `float vvctre_gui_get_cursor_pos_x()`

`ImGui::GetCursorPosX` wrapper

## `float vvctre_gui_get_cursor_pos_y() `

`ImGui::GetCursorPosY` wrapper

## `void vvctre_gui_set_cursor_pos(float local_x, float local_y)`

`ImGui::SetCursorPos` wrapper

## `void vvctre_gui_set_cursor_pos_x(float local_x)`

`ImGui::SetCursorPosX` wrapper

## `void vvctre_gui_set_cursor_pos_y(float local_y)`

`ImGui::SetCursorPosY` wrapper

## `void vvctre_gui_get_cursor_start_pos(float out[2])`

Calls `ImGui::GetCursorStartPos`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `void vvctre_gui_get_cursor_screen_pos(float out[2])`

Calls `ImGui::GetCursorScreenPos`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `void vvctre_gui_set_cursor_screen_pos(float x, float y)`

`ImGui::SetCursorScreenPos` wrapper

## `void vvctre_gui_align_text_to_frame_padding()`

`ImGui::AlignTextToFramePadding` wrapper

## `float vvctre_gui_get_text_line_height()`

`ImGui::GetTextLineHeight` wrapper

## `float vvctre_gui_get_text_line_height_with_spacing()`

`ImGui::GetTextLineHeightWithSpacing` wrapper

## `float vvctre_gui_get_frame_height()`

`ImGui::GetFrameHeight` wrapper

## `float vvctre_gui_get_frame_height_with_spacing()`

`ImGui::GetFrameHeightWithSpacing` wrapper

## `void vvctre_gui_push_id_string(const char* id)`

`ImGui::PushID` wrapper

## `void vvctre_gui_push_id_string_with_begin_and_end(const char* begin, const char* end)`

`ImGui::PushID` wrapper

## `void vvctre_gui_push_id_void(void* id)`

`ImGui::PushID` wrapper

## `void vvctre_gui_push_id_int(int id)`

`ImGui::PushID` wrapper

## `void vvctre_gui_pop_id()`

`ImGui::PopID` wrapper

## `void vvctre_gui_spacing()`

`ImGui::Spacing` wrapper

## `void vvctre_gui_separator()`

`ImGui::Separator` wrapper

## `void vvctre_gui_dummy(float width, float height)`

`ImGui::Dummy` wrapper

## `void vvctre_gui_tooltip(const char* text)`

If the current item is hovered, sets the tooltip to `text`

## `void vvctre_gui_begin_tooltip()`

`ImGui::BeginTooltip` wrapper

## `bool vvctre_gui_is_item_hovered(ImGuiHoveredFlags flags)`

`ImGui::IsItemHovered` wrapper

## `bool vvctre_gui_is_item_focused()`

`ImGui::IsItemFocused` wrapper

## `bool vvctre_gui_is_item_clicked(ImGuiMouseButton button)`

`ImGui::IsItemClicked` wrapper

## `bool vvctre_gui_is_item_visible()`

`ImGui::IsItemVisible` wrapper

## `bool vvctre_gui_is_item_edited()`

`ImGui::IsItemEdited` wrapper

## `bool vvctre_gui_is_item_activated()`

`ImGui::IsItemActivated` wrapper

## `bool vvctre_gui_is_item_deactivated()`

`ImGui::IsItemDeactivated` wrapper

## `bool vvctre_gui_is_item_deactivated_after_edit()`

`ImGui::IsItemDeactivatedAfterEdit` wrapper

## `bool vvctre_gui_is_item_toggled_open()`

`ImGui::IsItemToggledOpen` wrapper

## `bool vvctre_gui_is_any_item_hovered()`

`ImGui::IsAnyItemHovered` wrapper

## `bool vvctre_gui_is_any_item_active()`

`ImGui::IsAnyItemActive` wrapper

## `bool vvctre_gui_is_any_item_focused()`

`ImGui::IsAnyItemFocused` wrapper

## `void vvctre_gui_get_item_rect_min(float out[2])`

Calls `ImGui::GetItemRectMin`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `void vvctre_gui_get_item_rect_max(float out[2])`

Calls `ImGui::GetItemRectMax`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `void vvctre_gui_get_item_rect_size(float out[2])`

Calls `ImGui::GetItemRectSize`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `void vvctre_gui_set_item_allow_overlap()`

`ImGui::SetItemAllowOverlap` wrapper

## `void vvctre_gui_end_tooltip()`

`ImGui::EndTooltip` wrapper

## `void vvctre_gui_text(const char* text)`

`ImGui::TextUnformatted` wrapper

## `void vvctre_gui_text_ex(const char* text, const char* end)`

`ImGui::TextUnformatted` wrapper

## `void vvctre_gui_text_colored(float red, float green, float blue, float alpha, const char* text)`

Adds colored text

## `bool vvctre_gui_button(const char* label)`

`ImGui::Button` wrapper

## `bool vvctre_gui_small_button(const char* label)`

`ImGui::SmallButton` wrapper

## `bool vvctre_gui_color_button(const char* tooltip, float red, float green, float blue, float alpha, ImGuiColorEditFlags flags)`

`ImGui::ColorButton` wrapper

## `bool vvctre_gui_color_button_ex(const char* tooltip, float red, float green, float blue, float alpha, ImGuiColorEditFlags flags, float width, float height)`

`ImGui::ColorButton` wrapper

## `bool vvctre_gui_invisible_button(const char* id, float width, float height)`

`ImGui::InvisibleButton` wrapper

## `bool vvctre_gui_radio_button(const char* label, bool active)`

`ImGui::RadioButton` wrapper

## `bool vvctre_gui_image_button(void* texture_id, float width, float height, float uv0[2], float uv1[2], int frame_padding, float background_color[4], float tint_color[4])`

`ImGui::ImageButton` wrapper

## `bool vvctre_gui_checkbox(const char* label, bool* checked)`

`ImGui::Checkbox` wrapper

## `bool vvctre_gui_begin(const char* name)`

`ImGui::Begin` wrapper

## `bool vvctre_gui_begin_ex(const char* name, bool* open, ImGuiWindowFlags flags)`

`ImGui::Begin` wrapper

## `bool vvctre_gui_begin_overlay(const char* name, float initial_x, float initial_y)`

Creates a overlay with the specified name and initial position

## `bool vvctre_gui_begin_auto_resize(const char* name)`

`ImGui::Begin` wrapper

## `bool vvctre_gui_begin_child(const char* id, float width, float height, bool border, ImGuiWindowFlags flags)`

`ImGui::BeginChild` wrapper

## `bool vvctre_gui_begin_child_frame(const char* id, float width, float height, ImGuiWindowFlags flags)`

`ImGui::BeginChildFrame` wrapper

## `bool vvctre_gui_begin_popup(const char* id, ImGuiWindowFlags flags)`

`ImGui::BeginPopup` wrapper

## `bool vvctre_gui_begin_popup_modal(const char* name, bool* open, ImGuiWindowFlags flags)`

`ImGui::BeginPopupModal` wrapper

## `bool vvctre_gui_begin_popup_context_item(const char* id, ImGuiPopupFlags flags)`

`ImGui::BeginPopupContextItem` wrapper

## `bool vvctre_gui_begin_popup_context_window(const char* id, ImGuiPopupFlags flags)`

`ImGui::BeginPopupContextWindow` wrapper

## `bool vvctre_gui_begin_popup_context_void(const char* id, ImGuiPopupFlags flags)`

`ImGui::BeginPopupContextVoid` wrapper

## `void vvctre_gui_end()`

`ImGui::End` wrapper

## `void vvctre_gui_end_child()`

`ImGui::EndChild` wrapper

## `void vvctre_gui_end_child_frame()`

`ImGui::EndChildFrame` wrapper

## `void vvctre_gui_end_popup()`

`ImGui::EndPopup` wrapper

## `void vvctre_gui_open_popup(const char* id, ImGuiPopupFlags flags)`

`ImGui::OpenPopup` wrapper

## `void vvctre_gui_open_popup_on_item_click(const char* id, ImGuiPopupFlags flags)`

`ImGui::OpenPopupOnItemClick` wrapper

## `void vvctre_gui_close_current_popup()`

`ImGui::CloseCurrentPopup` wrapper

## `bool vvctre_gui_is_popup_open(const char* id, ImGuiPopupFlags flags)`

`ImGui::IsPopupOpen` wrapper

## `bool vvctre_gui_begin_menu(const char* label)`

`ImGui::BeginMenu` wrapper

## `void vvctre_gui_end_menu()`

`ImGui::EndMenu` wrapper

## `bool vvctre_gui_begin_tab(const char* label)`

`ImGui::BeginTabItem` wrapper

## `bool vvctre_gui_begin_tab_ex(const char* label, bool* open, ImGuiTabItemFlags flags)`

`ImGui::BeginTabItem` wrapper

## `bool vvctre_gui_begin_tab_bar(const char* id, ImGuiTabBarFlags flags)`

`ImGui::BeginTabBar` wrapper

## `void vvctre_gui_end_tab_bar()`

`ImGui::EndTabBar` wrapper

## `void vvctre_gui_set_tab_closed(const char* name)`

`ImGui::SetTabItemClosed` wrapper

## `void vvctre_gui_end_tab()`

`ImGui::EndTabItem` wrapper

## `bool vvctre_gui_menu_item(const char* label)`

`ImGui::MenuItem` wrapper

## `bool vvctre_gui_menu_item_with_check_mark(const char* label, bool* checked)`

`ImGui::MenuItem` wrapper

## `void vvctre_gui_plot_lines(const char* label, const float* values, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, float graph_width, float graph_height, int stride)`

`ImGui::PlotLines` wrapper

## `void vvctre_gui_plot_lines_getter(const char* label, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, float graph_width, float graph_height)`

`ImGui::PlotLines` wrapper

## `void vvctre_gui_plot_histogram(const char* label, const float* values, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, float graph_width, float graph_height, int stride)`

`ImGui::PlotHistogram` wrapper

## `void vvctre_gui_plot_histogram_getter(const char* label, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, float graph_width, float graph_height)`

`ImGui::PlotHistogram` wrapper

## `bool vvctre_gui_begin_listbox(const char* label)`

`ImGui::ListBoxHeader` wrapper

## `void vvctre_gui_end_listbox()`

`ImGui::ListBoxFooter` wrapper

## `bool vvctre_gui_begin_combo_box(const char* label, const char* preview)`

`ImGui::BeginCombo` wrapper

## `void vvctre_gui_end_combo_box()`

`ImGui::EndCombo` wrapper

## `bool vvctre_gui_selectable(const char* label)`

`ImGui::Selectable` wrapper

## `bool vvctre_gui_selectable_with_selected(const char* label, bool* selected)`

`ImGui::Selectable` wrapper

## `bool vvctre_gui_text_input(const char* label, char* buffer, std::size_t buffer_size)`

`ImGui::InputText` wrapper

## `bool vvctre_gui_text_input_multiline(const char* label, char* buffer, std::size_t buffer_size)`

`ImGui::InputTextMultiline` wrapper

## `bool vvctre_gui_text_input_with_hint(const char* label, const char* hint, char* buffer, std::size_t buffer_size)`

`ImGui::InputTextWithHint` wrapper

## `bool vvctre_gui_text_input_ex(const char* label, char* buffer, std::size_t buffer_size, ImGuiInputTextFlags flags)`

`ImGui::InputText` wrapper

## `bool vvctre_gui_text_input_multiline_ex(const char* label, char* buffer, std::size_t buffer_size, float width, float height, ImGuiInputTextFlags flags)`

`ImGui::InputTextMultiline` wrapper

## `bool vvctre_gui_text_input_with_hint_ex(const char* label, const char* hint, char* buffer, std::size_t buffer_size, ImGuiInputTextFlags flags)`

`ImGui::InputTextWithHint` wrapper

## `bool vvctre_gui_u8_input(const char* label, u8* value)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_u8_input_ex(const char* label, u8* value, const u8* step, const u8* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_u8_inputs(const char* label, u8* values, int components, const u8* step, const u8* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalarN` wrapper

## `bool vvctre_gui_u16_input(const char* label, u16* value)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_u16_input_ex(const char* label, u16* value, const u16* step, const u16* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_u16_inputs(const char* label, u16* values, int components, const u16* step, const u16* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalarN` wrapper

## `bool vvctre_gui_u32_input(const char* label, u32* value)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_u32_input_ex(const char* label, u32* value, const u32* step, const u32* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_u32_inputs(const char* label, u32* values, int components, const u32* step, const u32* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalarN` wrapper

## `bool vvctre_gui_u64_input(const char* label, u64* value)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_u64_input_ex(const char* label, u64* value, const u64* step, const u64* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_u64_inputs(const char* label, u64* values, int components, const u64* step, const u64* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalarN` wrapper

## `bool vvctre_gui_s8_input(const char* label, s8* value)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_s8_input_ex(const char* label, s8* value, const s8* step, const s8* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_s8_inputs(const char* label, s8* values, int components, const s8* step, const s8* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalarN` wrapper

## `bool vvctre_gui_s16_input(const char* label, s16* value)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_s16_input_ex(const char* label, s16* value, const s16* step, const s16* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_s16_inputs(const char* label, s16* values, int components, const s16* step, const s16* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalarN` wrapper

## `bool vvctre_gui_int_input(const char* label, int* value, int step, int step_fast)`

`ImGui::InputInt` wrapper

## `bool vvctre_gui_int_input_ex(const char* label, int* value, const int* step, const int* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_int_inputs(const char* label, int* values, int components, const int* step, const int* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalarN` wrapper

## `bool vvctre_gui_s64_input(const char* label, s64* value)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_s64_input_ex(const char* label, s64* value, const s64* step, const s64* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_s64_inputs(const char* label, s64* values, int components, const s64* step, const s64* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalarN` wrapper

## `bool vvctre_gui_float_input(const char* label, float* value, float step, float step_fast)`

`ImGui::InputFloat` wrapper

## `bool vvctre_gui_float_input_ex(const char* label, float* value, const float* step, const float* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_float_inputs(const char* label, float* values, int components, const float* step, const float* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalarN` wrapper

## `bool vvctre_gui_double_input(const char* label, double* value, double step, double step_fast)`

`ImGui::InputDouble` wrapper

## `bool vvctre_gui_double_input_ex(const char* label, double* value, const double* step, const double* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalar` wrapper

## `bool vvctre_gui_double_inputs(const char* label, double* values, int components, const double* step, const double* step_fast, const char* format, ImGuiInputTextFlags flags)`

`ImGui::InputScalarN` wrapper

## `bool vvctre_gui_color_edit(const char* label, float* color, ImGuiColorEditFlags flags)`

`ImGui::ColorEdit4` wrapper

## `bool vvctre_gui_color_picker(const char* label, float* color, ImGuiColorEditFlags flags)`

`ImGui::ColorPicker4` wrapper

## `bool vvctre_gui_color_picker_ex(const char* label, float* color, ImGuiColorEditFlags flags, const float* ref_col)`

`ImGui::ColorPicker4` wrapper

## `void vvctre_gui_progress_bar(float value, const char* overlay)`

`ImGui::ProgressBar` wrapper

## `void vvctre_gui_progress_bar_ex(float value, float width, float height, const char* overlay)`

`ImGui::ProgressBar` wrapper

## `bool vvctre_gui_slider_u8(const char* label, u8* value, const u8 minimum, const u8 maximum, const char* format)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_slider_u8_ex(const char* label, u8* value, const u8 minimum, const u8 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_sliders_u8(const char* label, u8* values, int components, const u8 minimum, const u8 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalarN` wrapper

## `bool vvctre_gui_slider_u16(const char* label, u16* value, const u16 minimum, const u16 maximum, const char* format)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_slider_u16_ex(const char* label, u16* value, const u16 minimum, const u16 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_sliders_u16(const char* label, u16* values, int components, const u16 minimum, const u16 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalarN` wrapper

## `bool vvctre_gui_slider_u32(const char* label, u32* value, const u32 minimum, const u32 maximum, const char* format)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_slider_u32_ex(const char* label, u32* value, const u32 minimum, const u32 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_sliders_u32(const char* label, u32* values, int components, const u32 minimum, const u32 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalarN` wrapper

## `bool vvctre_gui_slider_u64(const char* label, u64* value, const u64 minimum, const u64 maximum, const char* format)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_slider_u64_ex(const char* label, u64* value, const u64 minimum, const u64 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_sliders_u64(const char* label, u64* values, int components, const u64 minimum, const u64 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalarN` wrapper

## `bool vvctre_gui_slider_s8(const char* label, s8* value, const s8 minimum, const s8 maximum, const char* format)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_slider_s8_ex(const char* label, s8* value, const s8 minimum, const s8 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_sliders_s8(const char* label, s8* values, int components, const s8 minimum, const s8 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalarN` wrapper

## `bool vvctre_gui_slider_s16(const char* label, s16* value, const s16 minimum, const s16 maximum, const char* format)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_slider_s16_ex(const char* label, s16* value, const s16 minimum, const s16 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_sliders_s16(const char* label, s16* values, int components, const s16 minimum, const s16 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalarN` wrapper

## `bool vvctre_gui_slider_s32(const char* label, s32* value, const s32 minimum, const s32 maximum, const char* format)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_slider_s32_ex(const char* label, s32* value, const s32 minimum, const s32 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_sliders_s32(const char* label, s32* values, int components, const s32 minimum, const s32 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalarN` wrapper

## `bool vvctre_gui_slider_s64(const char* label, s64* value, const s64 minimum, const s64 maximum, const char* format)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_slider_s64_ex(const char* label, s32* value, const s32 minimum, const s32 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_sliders_s64(const char* label, s64* values, int components, const s64 minimum, const s64 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalarN` wrapper

## `bool vvctre_gui_slider_float(const char* label, float* value, const float minimum, const float maximum, const char* format)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_slider_float_ex(const char* label, float* value, const float minimum, const float maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_sliders_float(const char* label, float* values, int components, const float minimum, const float maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalarN` wrapper

## `bool vvctre_gui_slider_double(const char* label, double* value, const double minimum, const double maximum, const char* format)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_slider_double_ex(const char* label, float* value, const double minimum, const double maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalar` wrapper

## `bool vvctre_gui_sliders_double(const char* label, double* values, int components, const double minimum, const double maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderScalarN` wrapper

## `bool vvctre_gui_slider_angle(const char* label, float* rad, float degrees_min, float degrees_max, const char* format, ImGuiSliderFlags flags)`

`ImGui::SliderAngle` wrapper

## `bool vvctre_gui_vertical_slider_u8(const char* label, float width, float height, u8* value, const u8 minimum, const u8 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::VSliderScalar` wrapper

## `bool vvctre_gui_vertical_slider_u16(const char* label, float width, float height, u16* value, const u16 minimum, const u16 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::VSliderScalar` wrapper

## `bool vvctre_gui_vertical_slider_u32(const char* label, float width, float height, u32* value, const u32 minimum, const u32 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::VSliderScalar` wrapper

## `bool vvctre_gui_vertical_slider_u64(const char* label, float width, float height, u64* value, const u64 minimum, const u64 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::VSliderScalar` wrapper

## `bool vvctre_gui_vertical_slider_s8(const char* label, float width, float height, s8* value, const s8 minimum, const s8 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::VSliderScalar` wrapper

## `bool vvctre_gui_vertical_slider_s16(const char* label, float width, float height, s16* value, const s16 minimum, const s16 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::VSliderScalar` wrapper

## `bool vvctre_gui_vertical_slider_s32(const char* label, float width, float height, s32* value, const s32 minimum, const s32 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::VSliderScalar` wrapper

## `bool vvctre_gui_vertical_slider_s64(const char* label, float width, float height, s64* value, const s64 minimum, const s64 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::VSliderScalar` wrapper

## `bool vvctre_gui_vertical_slider_float(const char* label, float width, float height, float* value, const float minimum, const float maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::VSliderScalar` wrapper

## `bool vvctre_gui_vertical_slider_double(const char* label, float width, float height, double* value, const double minimum, const double maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::VSliderScalar` wrapper

## `bool vvctre_gui_drag_u8(const char* label, u8* value, float speed, const u8 minimum, const u8 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalar` wrapper

## `bool vvctre_gui_drags_u8(const char* label, u8* values, int components, float speed, const u8 minimum, const u8 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalarN` wrapper

## `bool vvctre_gui_drag_u16(const char* label, u16* value, float speed, const u16 minimum, const u16 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalar` wrapper

## `bool vvctre_gui_drags_u16(const char* label, u16* values, int components, float speed, const u16 minimum, const u16 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalarN` wrapper

## `bool vvctre_gui_drag_u32(const char* label, u32* value, float speed, const u32 minimum, const u32 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalar` wrapper

## `bool vvctre_gui_drags_u32(const char* label, u32* values, int components, float speed, const u32 minimum, const u32 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalarN` wrapper

## `bool vvctre_gui_drag_u64(const char* label, u32* value, float speed, const u32 minimum, const u32 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalar` wrapper

## `bool vvctre_gui_drags_u64(const char* label, u64* values, int components, float speed, const u64 minimum, const u64 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalarN` wrapper

## `bool vvctre_gui_drag_s8(const char* label, s8* value, float speed, const s8 minimum, const s8 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalar` wrapper

## `bool vvctre_gui_drags_s8(const char* label, s8* values, int components, float speed, const s8 minimum, const s8 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalarN` wrapper

## `bool vvctre_gui_drag_s16(const char* label, s16* value, float speed, const s16 minimum, const s16 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalar` wrapper

## `bool vvctre_gui_drags_s16(const char* label, s16* values, int components, float speed, const s16 minimum, const s16 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalarN` wrapper

## `bool vvctre_gui_drag_s32(const char* label, s32* value, float speed, const s32 minimum, const s32 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalar` wrapper

## `bool vvctre_gui_drags_s32(const char* label, s32* values, int components, float speed, const s32 minimum, const s32 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalarN` wrapper

## `bool vvctre_gui_drag_s64(const char* label, s64* value, float speed, const s64 minimum, const s64 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalar` wrapper

## `bool vvctre_gui_drags_s64(const char* label, s64* values, int components, float speed, const s64 minimum, const s64 maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalarN` wrapper

## `bool vvctre_gui_drag_float(const char* label, float* value, float speed, const float minimum, const float maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalar` wrapper

## `bool vvctre_gui_drags_float(const char* label, float* values, int components, float speed, const float minimum, const float maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalarN` wrapper

## `bool vvctre_gui_drag_double(const char* label, double* value, float speed, const double minimum, const double maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalar` wrapper

## `bool vvctre_gui_drags_double(const char* label, double* values, int components, float speed, const double minimum, const double maximum, const char* format, ImGuiSliderFlags flags)`

`ImGui::DragScalarN` wrapper

## `void vvctre_gui_image(void* texture_id, float width, float height, float uv0[2], float uv1[2], float tint_color[4], float border_color[4])`

`ImGui::Image` wrapper

## `void vvctre_gui_columns(int count, const char* id, bool border)`

`ImGui::Columns` wrapper

## `void vvctre_gui_next_column()`

`ImGui::GetNextColumn` wrapper

## `int vvctre_gui_get_column_index()`

`ImGui::GetColumnIndex` wrapper

## `float vvctre_gui_get_column_width(int column_index)`

`ImGui::GetColumnWidth` wrapper

## `void vvctre_gui_set_column_width(int column_index, float width)`

`ImGui::SetColumnWidth` wrapper

## `float vvctre_gui_get_column_offset(int column_index)`

`ImGui::GetColumnOffset` wrapper

## `void vvctre_gui_set_column_offset(int column_index, float offset_x)`

`ImGui::SetColumnOffset` wrapper

## `int vvctre_gui_get_columns_count()`

`ImGui::GetColumnsCount` wrapper

## `bool vvctre_gui_begin_table(const char* id, int columns, ImGuiTableFlags flags, float outer_width, float outer_height, float inner_width)`

`ImGui::BeginTable` wrapper

## `void vvctre_gui_end_table()`

`ImGui::EndTable` wrapper

## `void vvctre_gui_table_next_row(ImGuiTableRowFlags flags, float minimum_row_height)`

`ImGui::TableNextRow` wrapper

## `bool vvctre_gui_table_next_column()`

`ImGui::TableNextColumn` wrapper

## `bool vvctre_gui_table_set_column_index(int index)`

`ImGui::TableSetColumnIndex` wrapper

## `int vvctre_gui_table_get_column_index()`

`ImGui::TableGetColumnIndex` wrapper

## `int vvctre_gui_table_get_row_index()`

`ImGui::TableGetRowIndex` wrapper

## `void vvctre_gui_table_setup_column(const char* label, ImGuiTableColumnFlags flags, float initial_width_or_weight, ImU32 user_id)`

`ImGui::TableSetupColumn` wrapper

## `void vvctre_gui_table_setup_scroll_freeze(int columns, int rows)`

`ImGui::TableSetupScrollFreeze` wrapper

## `void vvctre_gui_table_headers_row()`

`ImGui::TableHeadersRow` wrapper

## `void vvctre_gui_table_header(const char* label)`

`ImGui::TableHeader` wrapper

## `int vvctre_gui_table_get_column_count()`

`ImGui::TableGetColumnCount` wrapper

## `const char* vvctre_gui_table_get_column_name(int column)`

`ImGui::TableGetColumnName` wrapper

## `ImGuiTableColumnFlags vvctre_gui_table_get_column_flags(int column)`

`ImGui::TableGetColumnFlags` wrapper

## `ImGuiTableSortSpecs* vvctre_gui_table_get_sort_specs()`

`ImGui::TableGetSortSpecs` wrapper

## `void vvctre_gui_table_set_background_color(ImGuiTableBgTarget target, ImU32 color, int n)`

`ImGui::TableSetBgColor` wrapper

## `bool vvctre_gui_tree_node_string(const char* label, ImGuiTreeNodeFlags flags)`

`ImGui::TreeNodeEx` wrapper

## `void vvctre_gui_tree_push_string(const char* id)`

`ImGui::TreePush` wrapper

## `void vvctre_gui_tree_push_void(const void* id)`

`ImGui::TreePush` wrapper

## `void vvctre_gui_tree_pop()`

`ImGui::TreePop` wrapper

## `float vvctre_gui_get_tree_node_to_label_spacing()`

`ImGui::GetTreeNodeToLabelSpacing` wrapper

## `bool vvctre_gui_collapsing_header(const char* label, ImGuiTreeNodeFlags flags)`

`ImGui::CollapsingHeader` wrapper

## `void vvctre_gui_set_next_item_open(bool is_open)`

`ImGui::SetNextItemOpen` wrapper

## `void vvctre_gui_set_color(ImGuiCol index, float r, float g, float b, float a)`

Sets a GUI color

## `void vvctre_gui_get_color(ImGuiCol index, float color_out[4])`

Get a GUI color

## `void vvctre_gui_set_font(void* data, int data_size, float font_size)`

Sets the GUI font

## `void* vvctre_gui_set_font_and_get_pointer(void* data, int data_size, float font_size)`

Sets the GUI font and returns its pointer

## `bool vvctre_gui_is_window_appearing()`

`ImGui::IsWindowAppearing` wrapper

## `bool vvctre_gui_is_window_collapsed()`

`ImGui::IsWindowCollapsed` wrapper

## `bool vvctre_gui_is_window_focused(ImGuiFocusedFlags flags)`

`ImGui::IsWindowFocused` wrapper

## `bool vvctre_gui_is_window_hovered(ImGuiHoveredFlags flags)`

`ImGui::IsWindowHovered` wrapper

## `void vvctre_gui_get_window_pos(float out[2])`

Calls `ImGui::GetWindowPos`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `void vvctre_gui_get_window_size(float out[2])`

Calls `ImGui::GetWindowSize`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `void vvctre_gui_set_next_window_pos(float x, float y, ImGuiCond condition, float pivot[2])`

`ImGui::SetNextWindowPos` wrapper

## `void vvctre_gui_set_next_window_size(float width, float height, ImGuiCond condition)`

`ImGui::SetNextWindowSize` wrapper

## `void vvctre_gui_set_next_window_size_constraints(float min[2], float max[2])`

`ImGui::SetNextWindowSizeConstraints` wrapper

## `void vvctre_gui_set_next_window_content_size(float width, float height)`

`ImGui::SetNextWindowContentSize` wrapper

## `void vvctre_gui_set_next_window_collapsed(bool collapsed, ImGuiCond condition)`

`ImGui::SetNextWindowCollapsed` wrapper

## `void vvctre_gui_set_next_window_focus()`

`ImGui::SetNextWindowFocus` wrapper

## `void vvctre_gui_set_next_window_bg_alpha(float alpha)`

`ImGui::SetNextWindowBgAlpha` wrapper

## `void vvctre_gui_set_window_pos(float x, float y, ImGuiCond condition)`

`ImGui::SetWindowPos` wrapper

## `void vvctre_gui_set_window_size(float width, float height, ImGuiCond condition)`

`ImGui::SetWindowSize` wrapper

## `void vvctre_gui_set_window_collapsed(bool collapsed, ImGuiCond condition)`

`ImGui::SetWindowCollapsed` wrapper

## `void vvctre_gui_set_window_focus()`

`ImGui::SetWindowFocus` wrapper

## `void vvctre_gui_set_window_font_scale(float scale)`

`ImGui::SetWindowFontScale` wrapper

## `void vvctre_gui_set_window_pos_named(const char* name, float x, float y, ImGuiCond condition)`

`ImGui::SetWindowPos` wrapper

## `void vvctre_gui_set_window_size_named(const char* name, float width, float height, ImGuiCond condition)`

`ImGui::SetWindowSize` wrapper

## `void vvctre_gui_set_window_collapsed_named(const char* name, bool collapsed, ImGuiCond condition)`

`ImGui::SetWindowCollapsed` wrapper

## `void vvctre_gui_set_window_focus_named(const char* name)`

`ImGui::SetWindowFocus` wrapper

## `bool vvctre_gui_is_key_down(int key)`

`ImGui::IsKeyDown` wrapper

## `bool vvctre_gui_is_key_pressed(int key, bool repeat)`

`ImGui::IsKeyPressed` wrapper

## `bool vvctre_gui_is_key_released(int key)`

`ImGui::IsKeyReleased` wrapper

## `int vvctre_gui_get_key_pressed_amount(int key, float repeat_delay, float rate)`

`ImGui::GetKeyPressedAmount` wrapper

## `void vvctre_gui_capture_keyboard_from_app(bool want_capture_keyboard_value)`

`ImGui::CaptureKeyboardFromApp` wrapper

## `bool vvctre_gui_is_mouse_down(ImGuiMouseButton button)`

`ImGui::IsMouseDown` wrapper

## `bool vvctre_gui_is_mouse_clicked(ImGuiMouseButton button, bool repeat)`

`ImGui::IsMouseClicked` wrapper

## `bool vvctre_gui_is_mouse_released(ImGuiMouseButton button)`

`ImGui::IsMouseReleased` wrapper

## `bool vvctre_gui_is_mouse_double_clicked(ImGuiMouseButton button)`

`ImGui::IsMouseDoubleClicked` wrapper

## `bool vvctre_gui_is_mouse_hovering_rect(const float min[2], const float max[2], bool clip)`

`ImGui::IsMouseHoveringRect` wrapper

## `bool vvctre_gui_is_mouse_pos_valid(const float pos[2])`

`ImGui::IsMousePosValid` wrapper

## `bool vvctre_gui_is_any_mouse_down()`

`ImGui::IsAnyMouseDown` wrapper

## `void vvctre_gui_get_mouse_pos(float out[2])`

Calls `ImGui::GetMousePos`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `void vvctre_gui_get_mouse_pos_on_opening_current_popup(float out[2])`

Calls `ImGui::GetMousePosOnOpeningCurrentPopup`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `bool vvctre_gui_is_mouse_dragging(ImGuiMouseButton button, float lock_threshold)`

`ImGui::IsMouseDragging` wrapper

## `void vvctre_gui_get_mouse_drag_delta(ImGuiMouseButton button, float lock_threshold, float out[2])`

Calls `ImGui::GetMouseDragDelta`  
Sets `out[0]` to `x`  
Sets `out[1]` to `y`

## `void vvctre_gui_reset_mouse_drag_delta(ImGuiMouseButton button)`

`ImGui::ResetMouseDragDelta` wrapper

## `ImGuiMouseCursor vvctre_gui_get_mouse_cursor()`

`ImGui::GetMouseCursor` wrapper

## `void vvctre_gui_set_mouse_cursor(ImGuiMouseCursor cursor_type)`

`ImGui::SetMouseCursor` wrapper

## `void vvctre_gui_capture_mouse_from_app(bool want_capture_mouse_value)`

`ImGui::CaptureMouseFromApp` wrapper

## `void vvctre_gui_style_set_alpha(float value)`

Sets alpha

## `void vvctre_gui_style_set_window_padding(float value[2])`

Sets window padding

## `void vvctre_gui_style_set_window_rounding(float value)`

Sets window rounding

## `void vvctre_gui_style_set_window_border_size(float value)`

Sets window border size

## `void vvctre_gui_style_set_window_min_size(float value[2])`

Sets window minimum size

## `void vvctre_gui_style_set_window_title_align(float value[2])`

Sets window title align

## `void vvctre_gui_style_set_window_menu_button_position(ImGuiDir value)`

Sets window menu button position

## `void vvctre_gui_style_set_child_rounding(float value)`

Sets child rounding

## `void vvctre_gui_style_set_child_border_size(float value)`

Sets child border size

## `void vvctre_gui_style_set_popup_rounding(float value)`

Sets popup rounding

## `void vvctre_gui_style_set_popup_border_size(float value)`

Sets popup border size

## `void vvctre_gui_style_set_frame_padding(float value[2])`

Sets frame padding

## `void vvctre_gui_style_set_frame_rounding(float value)`

Sets frame rounding

## `void vvctre_gui_style_set_frame_border_size(float value)`

Sets frame border size

## `void vvctre_gui_style_set_item_spacing(float value[2])`

Sets item spacing

## `void vvctre_gui_style_set_item_inner_spacing(float value[2])`

Sets item inner spacing

## `void vvctre_gui_style_set_cell_padding(float value[2])`

Sets cell padding

## `void vvctre_gui_style_set_touch_extra_padding(float value[2])`

Sets touch extra padding

## `void vvctre_gui_style_set_indent_spacing(float value)`

Sets indent spacing

## `void vvctre_gui_style_set_columns_min_spacing(float value)`

Sets columns minimum spacing

## `void vvctre_gui_style_set_scrollbar_size(float value)`

Sets scrollbar size

## `void vvctre_gui_style_set_scrollbar_rounding(float value)`

Sets scrollbar rounding

## `void vvctre_gui_style_set_grab_min_size(float value)`

Sets grab minimum size

## `void vvctre_gui_style_set_grab_rounding(float value)`

Sets grab rounding

## `void vvctre_gui_style_set_log_slider_deadzone(float value)`

Sets the size in pixels of the dead-zone around zero on logarithmic sliders that cross zero

## `void vvctre_gui_style_set_tab_rounding(float value)`

Sets tab rounding

## `void vvctre_gui_style_set_tab_border_size(float value)`

Sets tab border size

## `void vvctre_gui_style_set_tab_min_width_for_close_button(float value)`

Sets minimum width for close button to appears on an unselected tab when hovered

## `void vvctre_gui_style_set_color_button_position(ImGuiDir value)`

Sets color button position

## `void vvctre_gui_style_set_button_text_align(float value[2])`

Sets button text align

## `void vvctre_gui_style_set_selectable_text_align(float value[2])`

Sets selectable text align

## `void vvctre_gui_style_set_display_window_padding(float value[2])`

Sets [`DisplayWindowPadding`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1486)

## `void vvctre_gui_style_set_display_safe_area_padding(float value[2])`

Sets [`DisplaySafeAreaPadding`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1487)

## `void vvctre_gui_style_set_mouse_cursor_scale(float value)`

Sets [`MouseCursorScale`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1488)

## `void vvctre_gui_style_set_anti_aliased_lines(bool value)`

Sets [`AntiAliasedLines`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1489)

## `void vvctre_gui_style_set_anti_aliased_lines_use_tex(bool value)`

Sets [`AntiAliasedLinesUseTex`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1490)

## `void vvctre_gui_style_set_anti_aliased_fill(bool value)`

Sets [`AntiAliasedFill`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1491)

## `void vvctre_gui_style_set_curve_tessellation_tol(float value)`

Sets [`CurveTessellationTol`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1492)

## `void vvctre_gui_style_set_circle_segment_max_error(float value)`

Sets [`CircleSegmentMaxError`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1493)

## `float vvctre_gui_style_get_alpha()`

Get alpha

## `void vvctre_gui_style_get_window_padding(float value[2])`

Get window padding

## `float vvctre_gui_style_get_window_rounding()`

Get window rounding

## `float vvctre_gui_style_get_window_border_size()`

Get window border size

## `void vvctre_gui_style_get_window_min_size(float value[2])`

Get window minimum size

## `void vvctre_gui_style_get_window_title_align(float value[2])`

Get window title align

## `ImGuiDir vvctre_gui_style_get_window_menu_button_position()`

Get window menu button position

## `float vvctre_gui_style_get_child_rounding()`

Get child rounding

## `float vvctre_gui_style_get_child_border_size()`

Get child border size

## `float vvctre_gui_style_get_popup_rounding()`

Get popup rounding

## `float vvctre_gui_style_get_popup_border_size()`

Get popup border size

## `void vvctre_gui_style_get_frame_padding(float value[2])`

Get frame padding

## `float vvctre_gui_style_get_frame_rounding()`

Get frame rounding

## `float vvctre_gui_style_get_frame_border_size()`

Get frame border size

## `void vvctre_gui_style_get_item_spacing(float value[2])`

Get item spacing

## `void vvctre_gui_style_get_item_inner_spacing(float value[2])`

Get item inner spacing

## `void vvctre_gui_style_get_cell_padding(float value[2])`

Get cell padding

## `void vvctre_gui_style_get_touch_extra_padding(float value[2])`

Get touch extra padding

## `float vvctre_gui_style_get_indent_spacing()`

Get indent spacing

## `float vvctre_gui_style_get_columns_min_spacing()`

Get columns minimum spacing

## `float vvctre_gui_style_get_scrollbar_size()`

Get scrollbar size

## `float vvctre_gui_style_get_scrollbar_rounding()`

Get scrollbar rounding

## `float vvctre_gui_style_get_grab_min_size()`

Get grab minimum size

## `float vvctre_gui_style_get_grab_rounding()`

Get grab rounding

## `float vvctre_gui_style_get_log_slider_deadzone()`

Get the size in pixels of the dead-zone around zero on logarithmic sliders that cross zero

## `float vvctre_gui_style_get_tab_rounding()`

Get tab rounding

## `float vvctre_gui_style_get_tab_border_size()`

Get tab border size

## `float vvctre_gui_style_get_tab_min_width_for_close_button()`

Get minimum width for close button to appears on an unselected tab when hovered

## `ImGuiDir vvctre_gui_style_get_color_button_position()`

Get color button position

## `void vvctre_gui_style_get_button_text_align(float value[2])`

Get button text align

## `void vvctre_gui_style_get_selectable_text_align(float value[2])`

Get selectable text align

## `void vvctre_gui_style_get_display_window_padding(float value[2])`

Get [`DisplayWindowPadding`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1486)

## `void vvctre_gui_style_get_display_safe_area_padding(float value[2])`

Get [`DisplaySafeAreaPadding`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1487)

## `float vvctre_gui_style_get_mouse_cursor_scale()`

Get [`MouseCursorScale`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1488)

## `bool vvctre_gui_style_get_anti_aliased_lines()`

Get [`AntiAliasedLines`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1489)

## `bool vvctre_gui_style_get_anti_aliased_lines_use_tex()`

Get [`AntiAliasedLinesUseTex`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1490)

## `bool vvctre_gui_style_get_anti_aliased_fill()`

Get [`AntiAliasedFill`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1491)

## `float vvctre_gui_style_get_curve_tessellation_tol()`

Get [`CurveTessellationTol`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1492)

## `float vvctre_gui_style_get_circle_segment_max_error()`

Get [`CircleSegmentMaxError`](https://github.com/ocornut/imgui/blob/master/imgui.h#L1493)

## `u64 vvctre_get_dear_imgui_version()`

Returns `IMGUI_VERSION_NUM`

## `void vvctre_set_os_window_size(void* plugin_manager, int width, int height)`

`SDL_SetWindowSize` wrapper

## `void vvctre_get_os_window_size(void* plugin_manager, int* width, int* height)`

`SDL_GetWindowSize` wrapper

## `void vvctre_set_os_window_minimum_size(void* plugin_manager, int width, int height)`

`SDL_SetWindowMinimumSize` wrapper

## `void vvctre_get_os_window_minimum_size(void* plugin_manager, int* width, int* height)`

`SDL_GetWindowMinimumSize` wrapper

## `void vvctre_set_os_window_maximum_size(void* plugin_manager, int width, int height)`

`SDL_SetWindowMaximumSize` wrapper

## `void vvctre_get_os_window_maximum_size(void* plugin_manager, int* width, int* height)`

`SDL_GetWindowMaximumSize` wrapper

## `void vvctre_set_os_window_position(void* plugin_manager, int x, int y)`

`SDL_SetWindowPosition` wrapper

## `void vvctre_get_os_window_position(void* plugin_manager, int* x, int* y)`

`SDL_GetWindowPosition` wrapper

## `void vvctre_set_os_window_title(void* plugin_manager, const char* title)`

`SDL_SetWindowTitle` wrapper

## `const char* vvctre_get_os_window_title(void* plugin_manager)`

`SDL_GetWindowTitle` wrapper

## `void* vvctre_button_device_new(void* plugin_manager, const char* params)`

Creates a button device

## `void vvctre_button_device_delete(void* plugin_manager, void* device)`

Deletes a button device

## `bool vvctre_button_device_get_state(void* device)`

Returns the status of a button device

## `void vvctre_movie_prepare_for_playback(const char* path)`

Prepares to override the clock before playing back movies

## `void vvctre_movie_prepare_for_recording()`

Prepares to override the clock before recording movies

## `void vvctre_movie_play(const char* path)`

Plays a movie  
This doesn't have the hidden feature.

## `void vvctre_movie_record(const char* path)`

Records a movie

## `bool vvctre_movie_is_playing()`

Returns whether a movie is playing

## `bool vvctre_movie_is_recording()`

Returns whether a movie is being recorded

## `void vvctre_movie_stop()`

Stops playing/recording

## `void vvctre_set_frame_advancing_enabled(void* core, bool enabled)`

Sets whether frame advancing is enabled

## `bool vvctre_get_frame_advancing_enabled(void* core)`

Returns whether frame advancing is enabled

## `void vvctre_advance_frame(void* core)`

Advances a frame

## `void vvctre_set_custom_pad_state(void* core, u32 state)`

Sets the pad state

## `void vvctre_use_real_pad_state(void* core)`

Makes vvctre use the real pad state

## `u32 vvctre_get_pad_state(void* core)`

Returns the pad state

## `void vvctre_set_custom_circle_pad_state(void* core, float x, float y)`

Sets the circle pad state  
Range: 0.0 - 1.0

## `void vvctre_use_real_circle_pad_state(void* core)`

Makes vvctre use the real circle pad state

## `void vvctre_get_circle_pad_state(void* core, float* x_out, float* y_out)`

Get the circle pad state  
Range: 0.0 - 1.0

## `void vvctre_set_custom_circle_pad_pro_state(void* core, float x, float y, bool zl, bool zr)`

Sets the circle pad pro state  
Range: 0.0 - 1.0

## `void vvctre_use_real_circle_pad_pro_state(void* core)`

Makes vvctre use the real circle pad pro state

## `void vvctre_get_circle_pad_pro_state(void* core, float* x_out, float* y_out, bool* zl_out, bool* zr_out)`

Get the circle pad pro state  
Range: 0.0 - 1.0

## `void vvctre_set_custom_touch_state(void* core, float x, float y, bool pressed)`

Sets the touch state  
Range: 0.0 - 1.0

## `void vvctre_use_real_touch_state(void* core)`

Makes vvctre use the real touch state

## `bool vvctre_get_touch_state(void* core, float* x_out, float* y_out)`

Get the touch state  
Range: 0.0 - 1.0  
Returns whether the touch screen is pressed

## `void vvctre_set_custom_motion_state(void* core, float accelerometer[3], float gyroscope[3])`

Sets the motion state  
Range: 0.0 - 1.0

## `void vvctre_use_real_motion_state(void* core)`

Makes vvctre use the real motion state

## `void vvctre_get_motion_state(void* core, float accelerometer_out[3], float gyroscope_out[3])`

Get the motion state  
Range: 0.0 - 1.0

## `bool vvctre_screenshot(void* plugin_manager, void* data)`

Takes a screenshot

## `bool vvctre_screenshot_bottom_screen(void* plugin_manager, void* data, u32 width, u32 height)`

Takes a screenshot of the bottom screen

## `bool vvctre_screenshot_default_layout(void* plugin_manager, void* data)`

Takes a screenshot with the default layout

## `void vvctre_settings_apply()`

Applies the settings

## `void vvctre_settings_set_file_path(const char* value)`

Sets the file path

## `const char* vvctre_settings_get_file_path()`

Returns the file path

## `void vvctre_settings_set_play_movie(const char* value)`

Sets the movie file for playing

## `const char* vvctre_settings_get_play_movie()`

Returns the movie file for playing

## `void vvctre_settings_set_record_movie(const char* value)`

Sets the movie file for recording

## `const char* vvctre_settings_get_record_movie()`

Returns the movie file for recording

## `void vvctre_settings_set_region_value(int value)`

Sets the region

## `int vvctre_settings_get_region_value()`

Returns the region

## `void vvctre_settings_set_log_filter(const char* value)`

Sets the log filter

## `const char* vvctre_settings_get_log_filter()`

Returns the log filter

## `void vvctre_settings_set_initial_clock(int value)`

Sets Start -> Initial Time

## `int vvctre_settings_get_initial_clock()`

Returns Start -> Initial Time

## `void vvctre_settings_set_unix_timestamp(u64 value)`

Sets Start -> Initial Time's timestamp

## `u64 vvctre_settings_get_unix_timestamp()`

Returns Start -> Initial Time's timestamp

## `void vvctre_settings_set_use_virtual_sd(bool value)`

Sets Start -> Use Virtual SD Card

## `bool vvctre_settings_get_use_virtual_sd()`

Returns Start -> Use Virtual SD Card

## `void vvctre_settings_set_record_frame_times(bool value)`

Sets Start -> Record Frame Times

## `bool vvctre_settings_get_record_frame_times()`

Returns Start -> Record Frame Times

## `void vvctre_settings_enable_gdbstub(u16 port)`

Enables the GDB stub and sets the port to `port`

## `void vvctre_settings_disable_gdbstub()`

Disables the GDB stub

## `bool vvctre_settings_is_gdb_stub_enabled()`

Returns whether the GDB stub is enabled

## `u16 vvctre_settings_get_gdb_stub_port()`

Returns the GDB stub port

## `void vvctre_settings_set_use_cpu_jit(bool value)`

Sets General -> Use CPU JIT

## `bool vvctre_settings_get_use_cpu_jit()`

Returns General -> Use CPU JIT

## `void vvctre_settings_set_enable_core_2(bool value)`

Sets General -> Enable Core 2

## `bool vvctre_settings_get_enable_core_2()`

Returns General -> Enable Core 2

## `void vvctre_settings_set_limit_speed(bool value)`

Sets General -> Limit Speed

## `bool vvctre_settings_get_limit_speed()`

Returns General -> Limit Speed

## `void vvctre_settings_set_speed_limit(u16 value)`

Sets the speed limit

## `u16 vvctre_settings_get_speed_limit()`

Returns the speed limit

## `void vvctre_settings_set_use_custom_cpu_ticks(bool value)`

Sets whether General -> Enable Custom CPU Ticks is enabled

## `bool vvctre_settings_get_use_custom_cpu_ticks()`

Returns whether General -> Enable Custom CPU Ticks is enabled

## `void vvctre_settings_set_custom_cpu_ticks(u64 value)`

Sets General -> Custom CPU Ticks

## `u64 vvctre_settings_get_custom_cpu_ticks()`

Returns General -> Custom CPU Ticks

## `void vvctre_settings_set_cpu_clock_percentage(u32 value)`

Sets General -> CPU Clock Percentage

## `u32 vvctre_settings_get_cpu_clock_percentage()`

Returns General -> CPU Clock Percentage

## `void vvctre_settings_set_enable_dsp_lle(bool value)`

Sets Audio -> Enable DSP LLE

## `bool vvctre_settings_get_enable_dsp_lle()`

Returns Audio -> Enable DSP LLE

## `void vvctre_settings_set_enable_dsp_lle_multithread(bool value)`

Sets Audio -> DSP LLE -> Use Multiple Threads

## `bool vvctre_settings_get_enable_dsp_lle_multithread()`

Returns Audio -> DSP LLE -> Use Multiple Threads

## `void vvctre_settings_set_enable_audio_stretching(bool value)`

Sets Audio -> Output -> Enable Stretching

## `bool vvctre_settings_get_enable_audio_stretching()`

Returns Audio -> Output -> Enable Stretching

## `void vvctre_settings_set_audio_volume(float value)`

Sets Audio -> Output -> Volume

## `float vvctre_settings_get_audio_volume()`

Returns Audio -> Output -> Volume

## `void vvctre_settings_set_audio_sink_id(const char* value)`

Sets Audio -> Output -> Sink

Valid `value` values:

- cubeb
- sdl2
- null

## `const char* vvctre_settings_get_audio_sink_id()`

Returns Audio -> Output -> Sink

Return values:

- cubeb
- sdl2
- null

## `void vvctre_settings_set_audio_device_id(const char* value)`

Sets Audio -> Output -> Device

## `const char* vvctre_settings_get_audio_device_id()`

Returns Audio -> Output -> Device

## `void vvctre_settings_set_microphone_input_type(MicrophoneInputType value)`

Sets Audio -> Microphone -> Source

Valid `value` values:

- Disabled: 0
- Real Device: 1
- Static Noise: 2

## `MicrophoneInputType vvctre_settings_get_microphone_input_type()`

Returns Audio -> Microphone -> Source

Return values (underlying type: :

- Disabled: 0
- Real Device: 1
- Static Noise: 2

## `void vvctre_settings_set_microphone_device(const char* value)`

Sets Audio -> Microphone -> Device

## `const char* vvctre_settings_get_microphone_device()`

Returns Audio -> Microphone -> Device

## `void vvctre_settings_set_microphone_real_device_backend(MicrophoneRealDeviceBackend value)`

Sets Audio -> Microphone -> Backend

Valid `value` values (underlying type: u8):

- Auto: 0
- Cubeb: 1
- SDL2: 2
- Null: 3

## `MicrophoneRealDeviceBackend vvctre_settings_get_microphone_real_device_backend()`

Returns Audio -> Microphone -> Backend

Return values (underlying type: u8):

- Auto: 0
- Cubeb: 1
- SDL2: 2
- Null: 3

## `void vvctre_settings_set_camera_engine(int index, const char* value)`

Sets a camera's engine

Camera engine list:

- blank
- image

## `const char* vvctre_settings_get_camera_engine(int index)`

Returns a camera's engine

Camera engine list:

- blank
- image

## `void vvctre_settings_set_camera_parameter(int index, const char* value)`

Sets a camera's parameter

## `const char* vvctre_settings_get_camera_parameter(int index)`

Returns a camera's parameter

## `void vvctre_settings_set_camera_flip(CameraIndex index, int value)`

Set a camera's flip

Valid `index` values (underlying type: int):

- Outer Right: 0
- Inner: 1
- Outer Left: 2

Valid `value` values:

- None: 0
- Horizontal: 1
- Vertical: 2
- Reverse: 3

## `int vvctre_settings_get_camera_flip(CameraIndex index)`

Returns a camera's flip

Valid `index` values (underlying type: int):

- Outer Right: 0
- Inner: 1
- Outer Left: 2

Return values:

- None: 0
- Horizontal: 1
- Vertical: 2
- Reverse: 3

## `void vvctre_set_play_coins(u16 value)`

Sets System -> Play Coins

## `u16 vvctre_get_play_coins()`

Returns System -> Play Coins

## `void vvctre_settings_set_username(void* cfg, const char* value)`

Sets System -> Username

## `void vvctre_settings_get_username(void* cfg, char* out)`

Returns System -> Username

## `void vvctre_settings_set_birthday(void* cfg, u8 month, u8 day)`

Sets System -> Birthday

## `void vvctre_settings_get_birthday(void* cfg, u8* month_out, u8* day_out)`

Get System -> Birthday

## `void vvctre_settings_set_system_language(void* cfg, Service::CFG::SystemLanguage value)`

Sets System -> Language  
Language list: [https://www.3dbrew.org/wiki/Config_Savegame#Languages](https://www.3dbrew.org/wiki/Config_Savegame#Languages)  
`value` underlying type: int

## `Service::CFG::SystemLanguage vvctre_settings_get_system_language(void* cfg)`

Returns System -> Language  
Language list: [https://www.3dbrew.org/wiki/Config_Savegame#Languages](https://www.3dbrew.org/wiki/Config_Savegame#Languages)  
Return value underlying type: int

## `void vvctre_settings_set_sound_output_mode(void* cfg, Service::CFG::SoundOutputMode value)`

Sets System -> Sound output mode

Valid `value` values (underlying type: int):

- Mono: 0
- Stereo: 1
- Surround: 2

## `Service::CFG::SoundOutputMode vvctre_settings_get_sound_output_mode(void* cfg)`

Returns System -> Sound output mode

Return values (underlying type: int):

- Mono: 0
- Stereo: 1
- Surround: 2

## `void vvctre_settings_set_country(void* cfg, u8 value)`

Sets System -> Country  
Country list: [https://3dbrew.org/wiki/Country_Code_List](https://3dbrew.org/wiki/Country_Code_List)

## `u8 vvctre_settings_get_country(void* cfg)`

Returns System -> Country  
Country list: [https://3dbrew.org/wiki/Country_Code_List](https://3dbrew.org/wiki/Country_Code_List)

## `void vvctre_settings_set_console_id(void* cfg, u32 random_number, u64 console_id)`

Sets the console ID

## `u64 vvctre_settings_get_console_id(void* cfg)`

Returns the console ID

## `void vvctre_settings_set_console_model(void* cfg, u8 value)`

Sets the console model

Valid `value` values:

- Nintendo 3DS: 0
- Nintendo 3DS XL: 1
- New Nintendo 3DS: 2,
- Nintendo 2DS: 3,
- New Nintendo 3DS XL: 4
- New Nintendo 2DS XL: 5

## `u8 vvctre_settings_get_console_model(void* cfg)`

Returns the console model

Return values:

- Nintendo 3DS: 0
- Nintendo 3DS XL: 1
- New Nintendo 3DS: 2,
- Nintendo 2DS: 3,
- New Nintendo 3DS XL: 4
- New Nintendo 2DS XL: 5

## `void vvctre_settings_set_eula_version(void* cfg, u8 minor, u8 major)`

Sets the EULA version

## `void vvctre_settings_get_eula_version(void* cfg, u8* minor, u8* major)`

Get the EULA version

## `void vvctre_settings_write_config_savegame(void* cfg)`

Writes the config savegame

## `void vvctre_settings_set_use_hardware_renderer(bool value)`

Sets Graphics -> Use Hardware Renderer

## `bool vvctre_settings_get_use_hardware_renderer()`

Returns Graphics -> Use Hardware Renderer

## `void vvctre_settings_set_use_hardware_shader(bool value)`

Sets Graphics -> Use Hardware Renderer -> Use Hardware Shader

## `bool vvctre_settings_get_use_hardware_shader()`

Returns Graphics -> Use Hardware Renderer -> Use Hardware Shader

## `void vvctre_settings_set_hardware_shader_accurate_multiplication(bool value)`

Sets Graphics -> Use Hardware Renderer -> Use Hardware Shader -> Accurate Multiplication

## `bool vvctre_settings_get_hardware_shader_accurate_multiplication()`

Returns Graphics -> Use Hardware Renderer -> Use Hardware Shader -> Accurate Multiplication

## `void vvctre_settings_set_enable_disk_shader_cache(bool value)`

Sets Graphics -> Use Hardware Renderer -> Use Hardware Shader -> Enable Disk Shader Cache

## `bool vvctre_settings_get_enable_disk_shader_cache()`

Returns Graphics -> Use Hardware Renderer -> Use Hardware Shader -> Enable Disk Shader Cache

## `void vvctre_settings_set_use_shader_jit(bool value)`

Sets Graphics -> Use Shader JIT

## `bool vvctre_settings_get_use_shader_jit()`

Returns Graphics -> Use Shader JIT

## `void vvctre_settings_set_enable_vsync(bool value)`

Sets Graphics -> Enable VSync

## `bool vvctre_settings_get_enable_vsync()`

Returns Graphics -> Enable VSync

## `void vvctre_settings_set_dump_textures(bool value)`

Sets Graphics -> Dump Textures

## `bool vvctre_settings_get_dump_textures()`

Returns Graphics -> Dump Textures

## `void vvctre_settings_set_custom_textures(bool value)`

Sets Graphics -> Use Custom Textures

## `bool vvctre_settings_get_custom_textures()`

Returns Graphics -> Use Custom Textures

## `void vvctre_settings_set_preload_textures(bool value)`

Sets Graphics -> Preload Custom Textures

## `bool vvctre_settings_get_preload_textures()`

Return Graphics -> Preload Custom Textures

## `void vvctre_settings_set_enable_linear_filtering(bool value)`

Sets Graphics -> Enable Linear Filtering

## `bool vvctre_settings_get_enable_linear_filtering()`

Returns Graphics -> Enable Linear Filtering

## `void vvctre_settings_set_sharper_distant_objects(bool value)`

Sets Graphics -> Sharper Distant Objects

## `bool vvctre_settings_get_sharper_distant_objects()`

Returns Graphics -> Sharper Distant Objects

## `void vvctre_settings_set_resolution(u16 value)`

Sets Graphics -> Resolution  
0 = Window Size

## `u16 vvctre_settings_get_resolution()`

Returns Graphics -> Resolution  
0 = Window Size

## `void vvctre_settings_set_background_color_red(float value)`

Sets Graphics -> Background Color -> R  
Range: 0.0 - 1.0

## `float vvctre_settings_get_background_color_red()`

Returns Graphics -> Background Color -> R  
Range: 0.0 - 1.0

## `void vvctre_settings_set_background_color_green(float value)`

Sets Graphics -> Background Color -> G  
Range: 0.0 - 1.0

## `float vvctre_settings_get_background_color_green()`

Returns Graphics -> Background Color -> G  
Range: 0.0 - 1.0

## `void vvctre_settings_set_background_color_blue(float value)`

Sets Graphics -> Background Color -> B  
Range: 0.0 - 1.0

## `float vvctre_settings_get_background_color_blue()`

Returns Graphics -> Background Color -> B  
Range: 0.0 - 1.0

## `void vvctre_settings_set_post_processing_shader(const char* value)`

Sets Graphics -> Post Processing Shader

## `const char* vvctre_settings_get_post_processing_shader()`

Returns Graphics -> Post Processing Shader

## `void vvctre_settings_set_texture_filter(const char* value)`

Sets Graphics -> Texture Filter

Valid `value` values:

- none
- Anime4K Ultrafast
- Bicubic
- ScaleForce
- xBRZ freescale

## `const char* vvctre_settings_get_texture_filter()`

Returns Graphics -> Texture Filter

Return values:

- none
- Anime4K Ultrafast
- Bicubic
- ScaleForce
- xBRZ freescale

## `void vvctre_settings_set_render_3d(Settings::StereoRenderOption value)`

Sets Graphics -> 3D Mode

Valid `value` values (underlying type: int):

- Off: 0
- Side by Side: 1
- Anaglyph: 2
- Interlaced: 3
- Reverse Interlaced: 4

## `Settings::StereoRenderOption vvctre_settings_get_render_3d()`

Return Graphics -> 3D Mode

Return values (underlying type: int):

- Off: 0
- Side by Side: 1
- Anaglyph: 2
- Interlaced: 3
- Reverse Interlaced: 4

## `void vvctre_settings_set_factor_3d(u8 value)`

Set Graphics -> 3D Factor

## `u8 vvctre_settings_get_factor_3d()`

Returns Graphics -> 3D Factor

## `void vvctre_settings_set_button(int index, const char* params)`

Sets a button

## `const char* vvctre_settings_get_button(int index)`

Returns a button

## `void vvctre_settings_set_analog(int index, const char* params)`

Sets a analog

## `const char* vvctre_settings_get_analog(int index)`

Returns a analog

## `void vvctre_settings_set_motion_device(const char* params)`

Sets Controls -> Motion

## `const char* vvctre_settings_get_motion_device()`

Returns Controls -> Motion

## `void vvctre_settings_set_touch_device(const char* params)`

Sets Controls -> Touch

## `const char* vvctre_settings_get_touch_device()`

Returns Controls -> Touch

## `void vvctre_settings_set_cemuhookudp_address(const char* value)`

Sets Controls -> CemuhookUDP -> Address

## `const char* vvctre_settings_get_cemuhookudp_address()`

Returns Controls -> CemuhookUDP -> Address

## `void vvctre_settings_set_cemuhookudp_port(u16 value)`

Sets Controls -> CemuhookUDP -> Port

## `u16 vvctre_settings_get_cemuhookudp_port()`

Returns Controls -> CemuhookUDP -> Port

## `void vvctre_settings_set_cemuhookudp_pad_index(u8 value)`

Sets Controls -> CemuhookUDP -> Pad

## `u8 vvctre_settings_get_cemuhookudp_pad_index()`

Returns Controls -> CemuhookUDP -> Pad

## `void vvctre_settings_set_layout(Settings::Layout value)`

Sets Layout -> Layout

Valid `value` values (underlying type: int):

- Default: 0
- Single Screen: 1
- Large Screen: 2
- Side by Side: 3
- Medium Screen: 4

## `Settings::Layout vvctre_settings_get_layout()`

Returns Layout -> Layout

Return values (underlying type: int):

- Default: 0
- Single Screen: 1
- Large Screen: 2
- Side by Side: 3
- Medium Screen: 4

## `void vvctre_settings_set_swap_screens(bool value)`

Sets Layout -> Swap Screens

## `bool vvctre_settings_get_swap_screens()`

Returns Layout -> Swap Screens

## `void vvctre_settings_set_upright_screens(bool value)`

Sets Layout -> Upright Screens

## `bool vvctre_settings_get_upright_screens()`

Returns Layout -> Upright Screens

## `void vvctre_settings_set_use_custom_layout(bool value)`

Sets Layout -> Use Custom Layout

## `bool vvctre_settings_get_use_custom_layout()`

Returns Layout -> Use Custom Layout

## `void vvctre_settings_set_custom_layout_top_left(u16 value)`

Sets Layout -> Top Left

## `u16 vvctre_settings_get_custom_layout_top_left()`

Returns Layout -> Top Left

## `void vvctre_settings_set_custom_layout_top_top(u16 value)`

Sets Layout -> Top Top

## `u16 vvctre_settings_get_custom_layout_top_top()`

Returns Layout -> Top Top

## `void vvctre_settings_set_custom_layout_top_right(u16 value)`

Sets Layout -> Top Right

## `u16 vvctre_settings_get_custom_layout_top_right()`

Returns Layout -> Top Right

## `void vvctre_settings_set_custom_layout_top_bottom(u16 value)`

Sets Layout -> Top Bottom

## `u16 vvctre_settings_get_custom_layout_top_bottom()`

Returns Layout -> Top Bottom

## `void vvctre_settings_set_custom_layout_bottom_left(u16 value)`

Sets Layout -> Bottom Left

## `u16 vvctre_settings_get_custom_layout_bottom_left()`

Returns Layout -> Bottom Left

## `void vvctre_settings_set_custom_layout_bottom_top(u16 value)`

Sets Layout -> Bottom Top

## `u16 vvctre_settings_get_custom_layout_bottom_top()`

Returns Layout -> Bottom Top

## `void vvctre_settings_set_custom_layout_bottom_right(u16 value)`

Sets Layout -> Bottom Right

## `u16 vvctre_settings_get_custom_layout_bottom_right()`

Returns Layout -> Bottom Right

## `void vvctre_settings_set_custom_layout_bottom_bottom(u16 value)`

Sets Layout -> Bottom Bottom

## `u16 vvctre_settings_get_custom_layout_bottom_bottom()`

Returns Layout -> Bottom Bottom

## `u32 vvctre_settings_get_layout_width()`

Returns the layout's width

## `u32 vvctre_settings_get_layout_height()`

Returns the layout's height

## `void vvctre_settings_set_use_lle_module(const char* name, bool value)`

Sets whether that module is LLE

## `bool vvctre_settings_get_use_lle_module(const char* name)`

Returns whether that module is LLE

## `void* vvctre_get_cfg_module(void* core, void* plugin_manager)`

Returns the HLE CFG module

## `void vvctre_settings_set_multiplayer_ip(const char* value)`

Sets the multiplayer IP

## `const char* vvctre_settings_get_multiplayer_ip()`

Returns the multiplayer IP

## `void vvctre_settings_set_multiplayer_port(u16 value)`

Sets the multiplayer port

## `u16 vvctre_settings_get_multiplayer_port()`

Returns the multiplayer port

## `void vvctre_settings_set_nickname(const char* value)`

Sets the multiplayer nickname

## `const char* vvctre_settings_get_nickname()`

Returns the multiplayer

## `void vvctre_settings_set_multiplayer_password(const char* value)`

Sets the multiplayer password

## `const char* vvctre_settings_get_multiplayer_password()`

Returns Multiplayer -> Password

## `void vvctre_multiplayer_join(void* core)`

Connects to the IP:Port in the settings  
The MAC address is random  
Uses the password in the settings/Connect To Citra Room

## `void vvctre_multiplayer_leave(void* core)`

Leaves the room

## `u8 vvctre_multiplayer_get_state(void* core)`

Return the multiplayer state

## `void vvctre_multiplayer_send_message(void* core, const char* message)`

Sends a message

## `void vvctre_multiplayer_set_game(void* core, const char* name, u64 id)`

Sets the game

## `const char* vvctre_multiplayer_get_nickname(void* core)`

Returns your nickname

## `u8 vvctre_multiplayer_get_member_count(void* core)`

Returns the member count

## `const char* vvctre_multiplayer_get_member_nickname(void* core, std::size_t index)`

Returns a member's nickname

## `u64 vvctre_multiplayer_get_member_game_id(void* core, std::size_t index)`

Returns a member's game ID

## `const char* vvctre_multiplayer_get_member_game_name(void* core, std::size_t index)`

Returns a member's game name

## `void vvctre_multiplayer_get_member_mac_address(void* core, std::size_t index, u8* mac_address)`

Copies a member's MAC address to `mac_address`

## `const char* vvctre_multiplayer_get_room_name(void* core)`

Returns the room's name

## `const char* vvctre_multiplayer_get_room_description(void* core)`

Returns the room's description

## `u8 vvctre_multiplayer_get_room_member_slots(void* core)`

Returns the room's member slots

## `void vvctre_multiplayer_on_chat_message(void* core, void(*callback)(const char* nickname, const char* message))`

Adds a chat message callback

## `void vvctre_multiplayer_on_status_message(void* core, void(*callback)(u8 type, const char* nickname))`

Adds a status message callback

## `void vvctre_multiplayer_on_error(void* core, void(*callback)(u8 error))`

Adds a error callback

## `void vvctre_multiplayer_on_information_change(void* core, void(*callback)())`

Adds a information change callback

## `void vvctre_multiplayer_on_state_change(void* core, void(*callback)())`

Adds a state change callback

## `void vvctre_multiplayer_create_room(const char* ip, u16 port, u32 member_slots)`

Creates a room

## `void* vvctre_coretiming_register_event(void* core, const char* name, void (*callback)(std::uintptr_t user_data, int cycles_late))`

Registers a event

## `void vvctre_coretiming_remove_event(void* core, const void* event)`

Removes a event

## `void vvctre_coretiming_schedule_event(void* core, s64 cycles_into_future, const void* event, std::uintptr_t user_data)`

Schedules a event (running core)

## `void vvctre_coretiming_schedule_event_core_1(void* core, s64 cycles_into_future, const void* event, std::uintptr_t user_data)`

Schedules a event (core 1)

## `void vvctre_coretiming_schedule_event_core_2(void* core, s64 cycles_into_future, const void* event, std::uintptr_t user_data)`

Schedules a event (core 2)

## `void vvctre_coretiming_unschedule(void* core, const void* event, std::uintptr_t user_data)`

Unschedules a event

## `u64 vvctre_coretiming_get_ticks(void* core)`

Returns the current core's ticks

## `u64 vvctre_coretiming_get_ticks_core_1(void* core)`

Returns the core 1's ticks

## `u64 vvctre_coretiming_get_ticks_core_2(void* core)`

Returns the core 2's ticks

## `u64 vvctre_coretiming_get_idle_ticks(void* core)`

Returns the current core's idle ticks

## `u64 vvctre_coretiming_get_idle_ticks_core_1(void* core)`

Returns core 1's idle ticks

## `u64 vvctre_coretiming_get_idle_ticks_core_2(void* core)`

Returns the core 2's idle ticks

## `void vvctre_coretiming_add_ticks(void* core, u64 ticks)`

Adds ticks to the current core

## `void vvctre_coretiming_add_ticks_core_1(void* core, u64 ticks)`

Adds ticks to core 1

## `void vvctre_coretiming_add_ticks_core_2(void* core, u64 ticks)`

Adds ticks to core 2

## `void vvctre_coretiming_advance(void* core)`

Advances the current core

## `void vvctre_coretiming_advance_core_1(void* core)`

Advances core 1

## `void vvctre_coretiming_advance_core_2(void* core)`

Advances core 2

## `void vvctre_coretiming_move_events(void* core)`

Moves events from the threadsafe queue to the normal queue (running core)

## `void vvctre_coretiming_move_events_core_1(void* core)`

Moves events from the threadsafe queue to the normal queue (core 1)

## `void vvctre_coretiming_move_events_core_2(void* core)`

Moves events from the threadsafe queue to the normal queue (core 2)

## `void vvctre_coretiming_idle(void* core)`

Adds `downcount` to `idled_cycles` and sets `downcount` to 0 (running core)

## `void vvctre_coretiming_idle_core_1(void* core)`

Adds `downcount` to `idled_cycles` and sets `downcount` to 0 (core 1)

## `void vvctre_coretiming_idle_core_2(void* core)`

Adds `downcount` to `idled_cycles` and sets `downcount` to 0 (core 2)

## `void vvctre_coretiming_force_exception_check(void* core, s64 cycles)`

Forces exception check for the running core

## `void vvctre_coretiming_force_exception_check_core_1(void* core, s64 cycles)`

Forces exception check for core 1

## `void vvctre_coretiming_force_exception_check_core_2(void* core, s64 cycles)`

Forces exception check for core 2

## `s64 vvctre_coretiming_get_global_time_us(void* core)`

Returns the global time in microseconds

## `s64 vvctre_coretiming_get_downcount(void* core)`

Returns `downcount` of the running core

## `s64 vvctre_coretiming_get_downcount_core_1(void* core)`

Returns `downcount` of core 1

## `s64 vvctre_coretiming_get_downcount_core_2(void* core)`

Returns `downcount` of core 2

## `s64 vvctre_coretiming_get_global_ticks(void* core)`

Returns global ticks

## `void vvctre_coretiming_set_next_slice(void* core, s64 max_slice_length)`

Sets the next slice length of the running core

## `void vvctre_coretiming_set_next_slice_core_1(void* core, s64 max_slice_length)`

Sets the next slice length of core 1

## `void vvctre_coretiming_set_next_slice_core_2(void* core, s64 max_slice_length)`

Sets the next slice length of core 2

## `s64 vvctre_coretiming_get_max_slice_length(void* core)`

Returns the maximum slice length of the running core

## `s64 vvctre_coretiming_get_max_slice_length_core_1(void* core)`

Returns the maximum slice length of core 1

## `s64 vvctre_coretiming_get_max_slice_length_core_2(void* core)`

Returns the maximum slice length of core 2

## `char* vvctre_get_version()`

Returns vvctre's version  
This uses strdup/\_strdup

## `u8 vvctre_get_version_major()`

Returns vvctre's major version

## `u8 vvctre_get_version_minor()`

Returns vvctre's minor version

## `u8 vvctre_get_version_patch()`

Returns vvctre's patch version

## `void vvctre_log_trace(const char* line)`

Logs `line` (Trace level)

## `void vvctre_log_debug(const char* line)`

Logs `line` (Debug level)

## `void vvctre_log_info(const char* line)`

Logs `line` (Info level)

## `void vvctre_log_warning(const char* line)`

Logs `line` (Warning level)

## `void vvctre_log_error(const char* line)`

Logs `line` (Error level)

## `void vvctre_log_critical(const char* line)`

Logs `line` (Critical level)

## `void vvctre_swap_buffers()`

Swaps buffers. This can only be used when emulation is running.

## `void* vvctre_get_opengl_function(const char* name)`

`SDL_GL_GetProcAddress` wrapper

## `float vvctre_get_fps()`

Return FPS

## `float vvctre_get_frametime()`

Return the frametime

## `int vvctre_get_frame_count()`

Return the frame count

## `void vvctre_get_fatal_error(void* out)`

Copies the [fatal error info](https://www.3dbrew.org/wiki/ERR:Throw#FatalErrInfo) to `out`

## `void vvctre_set_show_fatal_error_messages(void* plugin_manager, bool show)`

Sets whether fatal error messages are shown

## `bool vvctre_get_show_fatal_error_messages(void* plugin_manager)`

Returns whether fatal error messages are shown
