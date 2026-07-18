# Changelog

## 0.5.0 - 2026-07-18
- Added support for original (MSX2) Gakuen Senki.
- Added automatic detection of Rance 4.1 and 4.2 English Patch 2.0.
- The decompiler now reads the game ID from `system3.ini` when it cannot detect
  the game from the `ADISK.DAT` file.

## 0.4.1 - 2026-02-07
- decompiler: Exit with code 2 when the game cannot be determined.
- dri: Fixed `dri create <drifile> <file>...` always creating an empty archive.

## 0.4.0 - 2025-04-26
- Rewrote the translation guide document.
- compiler: Renamed `quoted_strings` config to `allow_ascii`.
- compiler: Now `encoding = utf-8` implies `allow_ascii = true`.
- compiler: Fixed compile error for Only You and Funny Bee in Unicode mode.

## 0.3.1 - 2025-04-20
- Added special filename handling for `rance2_hint` and `prog_omake` (#1)
- Fixed typo in game_id (`ranec2` -> `rance2`)
- decompiler: Escapes gaiji (user-defined characters)
- decompiler: Fixed `-G` option

## 0.3.0 - 2025-01-10
- Supported more games.

## 0.2.0 - 2025-01-01
- decompiler: Now you can specify a game folder to decompile all the scripts in it.
- compiler: Added `--outdir` option to specify the output directory.
- Added Windows batch files to run the decompiler and compiler.
- New supported game: Rance 4 Option Disk

## 0.1.0 - 2024-12-28
- First public release.
