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
- `dri` -- ALD archive utility

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

The decompiled source files will be generated in the `src` directory. Edit them as you like.

Once you've finished editing the source files, you can compile them back to the `ADISK.DAT` using the following command:
```
sys3c -p src/sys3c.cfg -o foo
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

## Supported Games

|         Game        |   Status    | Notes |
| ------------------- | ----------- | ----- |
| bunkasai            |             |       |
| crescent            |             |       |
| dps                 |             |       |
| dps_sg_fahren       |             |       |
| dps_sg_katei        |             |       |
| dps_sg_nobunaga     |             |       |
| dps_sg2_antique     |             |       |
| dps_sg2_ikenai      |             |       |
| dps_sg2_akai        |             |       |
| dps_sg3_rabbit      |             |       |
| dps_sg3_shinkon     |             |       |
| dps_sg3_sotsugyou   |             |       |
| fukei               |             |       |
| intruder            | Supported   |       |
| tengu               | Supported   |       |
| toushin_hint        |             |       |
| little_vampire      | Supported   |       |
| little_vampire_eng  |             |       |
| yakata              |             |       |
| gakuen              |             |       |
| gakuen_eng          | Supported   |       |
| ayumi_fd            | Supported   |       |
| ayumi_hint          |             |       |
| ayumi_proto         |             |       |
| dalk_hint           |             |       |
| drstop              | Supported   |       |
| prog_fd             | Supported   |       |
| rance3_hint         |             |       |
| sdps_maria          | Supported   |       |
| sdps_tono           | Supported   |       |
| sdps_kaizoku        | Supported   |       |
| yakata2             | Supported   |       |
| ambivalenz_fd       | Supported   |       |
| ambivalenz_cd       | Supported   |       |
| dps_all             | Supported   |       |
| funnybee_cd         | Supported   |       |
| funnybee_fd         | Supported   |       |
| onlyyou             | Supported   |       |
| onlyyou_demo        |             |       |
| prog_cd             | Supported   |       |
| prog_omake          | Supported   |       |
| rance41             | Supported   |       |
| rance41_eng         | Supported   |       |
| rance42             | Supported   |       |
| rance42_eng         | Supported   |       |
| ayumi_cd            | Supported   |       |
| ayumi_live_256      |             |       |
| ayumi_live_full     |             |       |
| yakata3_cd          | Supported   |       |
| yakata3_fd          | Supported   |       |
| hashirionna2        | Supported   |       |
| toushin2_gd         |             |       |
| toushin2_sp         |             |       |
| otome               | Supported   |       |
| ningyo              |             |       |
| mugen               | Supported   |       |
