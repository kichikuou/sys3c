# System 1-3 Compiler and Decompiler
This is a companion tool for [system3-sdl2](https://github.com/kichikuou/system3-sdl2).
It can decompile and compile the scenario files of System 1-3 games that are
supported by system3-sdl2.

## Build & Install
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

First, decompile the game by giving the scenario file (`ADISK.DAT`) to `sys3dc`.
```
sys3dc -o src ADISK.DAT
```
If the game includes a `AG00.DAT` file, provide that as well:
```
sys3dc -o src ADISK.DAT AG00.DAT
```

The decompiled source files will be generated in the `src` directory. Edit them as you like.

Once you've finished editing the source files, you can compile them back to the `ADISK.DAT`/`AG00.DAT` using the following command:
```
sys3c -p src/sys3c.cfg
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
