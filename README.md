# System 1-3 Compiler and Decompiler
This is a companion tool for [system3-sdl2](https://github.com/kichikuou/system3-sdl2).
It can decompile and compile the scenario files of System 1-3 games that are
supported by system3-sdl2.

## Download
Prebuilt Windows executables are available from the
[release page](https://github.com/kichikuou/sys3c/releases). The 64-bit version
supports Windows 10 or later. For older Windows, please use the 32-bit version.

## Build from Source
First install the dependencies (corresponding Debian package in parentheses):
- meson (`meson`)
- asciidoctor (`asciidoctor`) [optional, for generating manual pages]

Then build the executables with meson.
```
meson build
ninja -C build
```
This creates the following executables under the `build` directory:
- `sys3c` -- System 1-3 compiler
- `sys3dc` -- System 1-3 decompiler
- `dri` -- DAT archive utility

Optionally you can install these executables to your system, by:
```
sudo ninja -C build install
```
If you want to install them in a custom directory, specify `--prefix=` when
running meson.

## Basic Workflow
Here are the steps for decompiling a game, editing the source, and compiling back to the scenario file.

### Windows

1. Copy all files in the sys3c archive to the game directory (the directory containing `ADISK.DAT`).
2. Run `decompile.bat`. The decompiled source files will be generated in the `src` directory.
3. Edit the source files in the `src` directory as you like.
4. Run `compile.bat`. The scenario files in the game directory will be updated.

### Other Platforms

First, in the game directory (the directory containing `ADISK.DAT`), run `sys3dc` with the following command:
```
sys3dc . --outdir=src
```

The decompiled source files will be generated in the `src` directory. Edit them as you like.

Once you've finished editing the source files, you can compile them back to the
`DAT` files using the following command:
```
sys3c --project=src/sys3c.cfg --outdir=.
```

## Unicode mode
Using the "Unicode mode" of [system3-sdl2](https://github.com/kichikuou/system3-sdl2),
you can translate games into any languages that are not supported by original
System 1-3. See [docs/unicode.adoc](docs/unicode.adoc) for more information.

## Command Documentations
Here are the manuals for the commands:
- [docs/sys3c.adoc](docs/sys3c.adoc)
- [docs/sys3dc.adoc](docs/sys3dc.adoc)
- [docs/dri.adoc](docs/dri.adoc)
