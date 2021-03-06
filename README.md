Spout (dos-like)
================

Spout is a simple caveflying game. The aim is to get as high as
possible, avoiding or destroying obstacles.

![game screenshot](./screenshot.png)

Controls
--------

Left:      Rotate left  
Right:     Rotate right  
Space:     Thrust  
Esc:       Pause  
Shift-Esc: Quit

History
-------

Spout was originally written for a handheld by kuni, and soon
afterwards was ported to Windows using cygwin and sdl and released
under the MIT license.

In 2004 a 'unix version' was released, which mostly just slapped
autotools into the windows version and infringed the license.

This is a [Dos-Like](https://mattiasgustavsson.itch.io/dos-like) version, based on [Nick's new unix version](https://njw.me.uk/spout/), based on the original Windows code by kuni.

Demo
----
http://mooses.nl/misc/spout-doslike



Compiling
---------

To compile on Windows (using clang 13):

```
clang -target x86_64-pc-windows-gnu spout.c ./vendor/dos.c -lgdi32 -luser32 -lwinmm -o spout.exe
```

To compile for the web:

```
wasm\node wasm\wajicup.js -template template.html spout.c vendor/dos.c spout.html
```

A WebAssembly build environment is required. You can download it (for Windows) here: [github.com/mattiasgustavsson/dos-like/releases/tag/wasm-env](https://github.com/mattiasgustavsson/dos-like/releases/tag/wasm-env).
Unzip it so that the `wasm` folder in the zip file is at your repository root. 

The wasm build environment is a compact distribution of [node](https://nodejs.org/en/download/), [clang/wasm-ld](https://releases.llvm.org/download.html),
[WAjic](https://github.com/schellingb/wajic) and [wasm system libraries](https://github.com/emscripten-core/emscripten/tree/main/system).