---
title: Requirements
redirect_from:
  - /
---

OpenGL: 3.3+  
OS: 64-bit Windows 7+ or Linux

If you use Windows:

- [Microsoft Visual C++ 2015-2019 Redistributable (x64)](https://aka.ms/vs/16/release/vc_redist.x64.exe)
- For AAC on N and KN: [Media Feature Pack](https://support.microsoft.com/en-us/help/3145500/media-feature-pack-list-for-windows-n-editions)

If you use Linux:

- SDL2
- libpng
- For dialogs: Zenity, Matedialog, Qarma, or KDialog

If your GPU doesn't support OpenGL 3.3, you can use:

- Windows: [https://github.com/pal1000/mesa-dist-win](https://github.com/pal1000/mesa-dist-win)
- Linux: This command: `export LIBGL_ALWAYS_SOFTWARE=1`
