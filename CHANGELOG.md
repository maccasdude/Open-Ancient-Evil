# Changelog

## 1.0.0 - 2026-10-01

First release as **Open AncientEvil**.

### New
- Windows build: a self-contained `ancientevil.exe` (SDL2 and a small FFmpeg
  linked in), cross-compiled with mingw-w64 (`tools/build-windows.sh`).
- GitHub Actions build the Windows, Linux and Arch packages and attach them
  to a release when a version tag is pushed.
- MIT license for the port's code.
- `AE_DEBUG_POS=x,y` debugging aid (start on a given tile).

### Fixed
- Menus ignore the click that opened them: the pick-up menu of a pile of
  items no longer takes whatever item was under the cursor.
- Parchments: the close box (and other small dark parts of the art) were
  see-through and showed the status bar through them.

## Earlier work (the Ancient Evil for Linux builds)

### Dialogue and cut-scenes
- A click, Space, Enter or Esc skips the spoken line; keys and clicks from
  before the line do not skip it.
- The falling and smash cut-scenes can be skipped with Esc or a click.
- Without the speech files, lines wait for a click or key instead of
  flashing past.

### Input
- Keys are read from the key presses themselves (SDL text input dropped
  letters while Ctrl or Alt was held and could lose keys behind IBus).
- Every waiting key is handled each frame (the original took one message per
  frame, and a key press is three).
- Keys and mouse buttons are let go when the window loses the focus.
- Quick taps of held actions (jump, face) are seen by every check.
- Command keys work while walking; Esc opens the menu at once.
- Mouse clicks can no longer be lost to the cursor thread.

### Lighting
- Smooth light model (default; "Light model" in Options): falloff from the
  real distance with no hard rim, walls soften the light's edge, bright
  light rolls off, gentle flame flicker, models lit at their exact position.
- Walls use the same soft light and ease between levels (no flashing).
- Shadows fade with the distance from their light, fade in next to a light
  and no longer stretch to infinity.
- "Smooth light flicker" option.
- 3dfx-style coloured light: warm torches, magenta warps, red fireballs,
  blue missiles.

### Options and controls
- Options screen (Display, Game, Audio, Controls) in the game's own style,
  saved in `ancientevil.cfg`.
- Key rebinding per control scheme, two keys per action; the status bar
  hints show the bound keys.
- Control schemes: Classic, Diablo style, WASD + mouse aim; the camera
  follows the player in the new schemes.
- F5 quick save, F8 quick load.
- Game speed setting (the Windows 95/98, NT 4 or 2000/XP timing).

### Enhanced mode
- 960x540 widescreen view shown at 1920x1080; fullscreen, window scaling,
  sharp or smooth scaling.
- 3D characters at full resolution with smoothed textures.
- `--classic` keeps the original 640x480 software picture.

### Fixes
- Weapon icon shows the sword again (the port's table had a skull).
- 3D models no longer leave outlines when a screen paints over them.
- Brief frames with a missing floor; walls clipped to 640x400 in widescreen;
  scenery missing around the player with the following camera.
- Sleeping fades fully to black.
- First-level key had the glass hammer's name and icon; the first lever
  could not be used; text of some objects did not render; squares with no
  light on the floor.
- Out-of-range reads of the original are guarded; widescreen clipping of
  text and dialogs.

### Packaging
- Linux tarball with `make install` and a desktop entry.
- Arch / CachyOS PKGBUILD and a prebuilt binary.
