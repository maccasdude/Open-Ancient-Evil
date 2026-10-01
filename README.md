# Open AncientEvil

An open source port of **Ancient Evil** v1.11 (Silver Lightning Software,
1998) for Windows and Linux, made by decompiling the software-renderer build
(`RPG.EXE`) and its `DDW16.DLL` graphics library by hand into portable C++.
DirectDraw, DirectSound, DirectInput, MCI CD audio and the Win32 window are
replaced by SDL2. The game logic follows the original function by function;
each function carries the address it came from (`// 0x4291e0`).

On top of the original it adds a widescreen 1080p mode with full-resolution
3D characters, 3dfx-style coloured lighting with a smooth light model, an
Options screen with key rebinding and Diablo-style and WASD control schemes,
quick saves, skippable dialogue and many fixes. See [CHANGELOG.md](CHANGELOG.md).

**No game data is included.** You need your own copy of Ancient Evil.

## Download

Builds for Windows, Ubuntu/Debian and Arch/CachyOS are on the Releases page:

| Package | Contents |
|---|---|
| `OpenAncientEvil-<version>-windows-x64.zip` | `ancientevil.exe` (self-contained): copy it into your game folder and run it |
| `OpenAncientEvil-<version>-linux-x64.tar.gz` | Linux binary, source, `make install` |
| `OpenAncientEvil-<version>-arch.tar.gz` | PKGBUILD for Arch / CachyOS (`makepkg -si`) and a prebuilt binary |
| `OpenAncientEvil-<version>-src.tar.gz` | source only |

`make dist` (or `tools/package.sh`) makes the same packages locally in `dist/`.

## Enhanced mode (default)

By default the port runs an enhanced 1080p version:

- **Widescreen view.** The game draws a 960x540 picture instead of 640x480,
  shown at 1920x1080 (each game pixel is 2x2 screen pixels, so the art stays
  crisp). You see 160 pixels more of the dungeon at each side and 60 more at
  the top. The status bar sits on the bottom edge with stone panels either
  side, and menus and dialogs stay centred.
- **Full-resolution 3D characters.** The player, monsters and other 3D models
  are drawn a second time at 1920x1080 with smoothed (bilinear) textures and
  merged over the picture, so they are sharper than the scenery. When a
  screen draws its background back over a model (the class choice, for
  one), the model's hi-res pixels go with it.
- **3dfx lighting.** The lighting follows the 3dfx build (`RPG3DFX.EXE`)
  rather than the software one: light has colour. Torches and wall lights
  are warm (1.0, 0.87, 0.75), a carried torch is warm and the light spell
  white, warps glow magenta and some tiles red, and fireballs throw red
  light and other missiles blue. Floors, walls and animated tiles blend the
  light smoothly between their corners, models are lit by the colour of
  their tile times the facing of each face, walls in front of the player
  are see-through at the 3dfx opacity (one third) instead of dithered
  (lit by the floor in front of them where that is brighter, so they do
  not show up as dark boxes),
  translucent models blend 50/50, and lightning is drawn with antialiased,
  half transparent lines. The game logic still uses the software build's
  light levels, so nothing plays differently. Smooth light gradients are
  dithered so they do not band, the hi-res models stay sharp behind
  see-through walls, and their smoothed textures do not bleed across the
  seams of the texture sheet. The drawn light also eases between the
  torch's flicker steps instead of flashing at the edge of its pool
  ("Smooth light flicker" in Options, Display).
- **Smooth light model** (the default; "Light model" in Options, Display,
  switches back to the 3dfx per-tile light). The 3dfx build lights whole
  tiles, cuts distances to whole tiles and stops the light dead at the end
  of its range, which gives pools of light stepped, diamond shaped rims
  that sit half a tile off. The smooth model works the light out at every
  tile corner from the real distance to each light, with a falloff that is
  bright near the flame and fades out softly with no rim; walls shade a
  corner only as far as they hide the floor around it, bright light rolls
  off instead of clipping, flames flicker gently instead of jumping, and
  models are lit by the light at their exact position. Shadows are
  strongest near their light and fade out with the distance from it
  (instead of a fixed half darkness that vanished at a set range), fade in
  right next to a light instead of spreading out all round, and are no
  longer stretched off to infinity when a light is close. Walls are lit
  from the same soft light, and ease between light levels, so a wall at
  the edge of the light, or one whose lit side changes as you walk past,
  no longer flashes. The game logic still uses the original light levels.

`--classic` (or `AE_CLASSIC=1`) gives the original 640x480 picture instead.
`AE_NO_HIRES=1` keeps the widescreen view but draws the models at the normal
resolution; `AE_NO_FILTER=1` turns off the texture smoothing; `AE_NO_COLOR=1`
gives the software build's white lighting.

## Timing

Both builds pace the game the same way (the 3dfx build has identical
timers): each frame waits until `GetTickCount` has advanced 49 ms (menus
50 ms); the game clock is a 40 ms multimedia timer (25 Hz); ambient sounds
run on a 50 ms timer and the animated cursor on a 66 ms one.

`GetTickCount` only moves in steps of the system tick, so the real frame
rate depended on Windows: on Windows 95/98, which the game was made for,
the tick is the 18.2 Hz PC timer (54.925 ms), so the game ran at 18.2 frames
per second; Windows NT 4 (10 ms ticks) gave 20 fps. The port emulates that
clock, so the default is the Windows 95/98 speed, on a fixed schedule that
does not drift with drawing time. `AE_TICK_MS` picks another tick:

| `AE_TICK_MS` | Frame rate | As on |
|---|---|---|
| (default) 54.925 | 18.2 fps | Windows 95/98 |
| 10 | 20 fps | Windows NT 4 |
| 15.625 | 16 fps | Windows 2000/XP and later |
| 1 | about 18.7 fps, varies with the machine | a 1 ms clock |

The timers run on their own threads against fixed schedules (drift-free,
like `timeSetEvent` periodic timers). `AE_TIMING=1` prints the measured
frame rate, jitter and game clock rate every 5 seconds.

For a 1080p monitor, full screen (Alt+Enter, or `AE_FULLSCREEN=1`) shows the
enhanced mode pixel for pixel.

## Options

The title screen and the in-game menu (Esc) have an **Options** screen with
four pages:

- **Display**: enhanced or classic mode, hi-res models, texture smoothing,
  3dfx coloured lighting, light model, smooth light flicker, shadows, gamma, fullscreen, window size and
  scaling (sharp, sharp in integer steps, or smooth).
- **Game**: game speed (the Windows the timing copies, see Timing), always
  run, hints, pausing while the window is in the background.
- **Audio**: music and sound volume.
- **Controls**: the control scheme, mouse speed, and key bindings.

Use the mouse (left click changes a value, right click changes it back,
click on a slider to set it) or the keys (Up/Down, Left/Right, Enter, Tab
for the next page, Esc to go back). The in-game menu keeps Save, Restore,
Spell hot keys and Abort; the other old entries moved into Options. Mode and
hi-res model changes take effect on the next start; everything else is
immediate.

The port's settings are saved in `ancientevil.cfg` in the save folder; the
game's own ones stay in `PREFS.CFG` as in the original. The `AE_*`
environment variables and `--classic` / `--enhanced` still override the saved
settings for one run.

### Control schemes

- **Classic**: the original. Left click walks and uses things, right click
  does the current action (weapon, spell or jump, chosen with A, C and J),
  the arrow keys walk and turn.
- **Diablo style**: hold the left button to walk towards the cursor; click a
  monster to walk up and fight it (hold the button to keep fighting; with a
  bow you shoot from where you stand); click an item, door, lever or chest to
  walk over and use it; click a friendly character to talk; right click casts
  the current spell; Shift+click attacks where you stand; J (held) charges a
  jump towards the cursor.
- **WASD + mouse aim**: W walks towards the cursor, S backs away from it
  still facing it, A/D circle round it, and the direction follows the mouse
  as you move (Options, Controls, "WASD moves" switches to plain screen
  directions: up, left, down, right). The arrow keys do the same. Standing,
  you face the cursor; left click attacks
  towards it (or picks up / opens / uses what is under it); right click
  casts; Space (held) charges a jump. Shift runs.

In the Diablo and WASD schemes the view follows the player and stays
centred ("Camera follows" in Options, Controls turns it off).

Every scheme has its own key bindings (Options, Controls, Key bindings...):
click a key to change it (a key used elsewhere swaps over), right click
clears it, and each action can have two keys. The WASD scheme moves the
commands off the movement keys:

| Action | Classic / Diablo | WASD |
|---|---|---|
| Weapon / spell / jump mode | A / C / J (jump mode classic only) | Z / X / - |
| Equipment | Space | I or Tab |
| Map | Tab | M |
| Memorise spells | M | B |
| Use item | U | E |
| Drink elixir | E | Q |
| Drop / identify / search | D / I / S | G / N / R |
| Open doors | O | F |
| Centre the view | . | C |

New in all schemes: **F5** quick saves (into save slot 10, named
"Quicksave") and **F8** loads it. Keys 0-9 (spell hot keys) and Esc (the
menu) stay fixed. The hints over the status bar buttons name the keys bound
now (or "Unbound") and describe the mouse of the scheme in use.

Input fixes: keys are read from the key presses themselves (SDL's text
input dropped letters while Ctrl or Alt was held and can lose keys behind an
input method such as IBus); every waiting key is handled each frame (the
original took one message per frame, and a key press is three); keys and
mouse buttons are let go when the window loses the focus, so none stays
held down; a quick tap of a held action (face, jump) is seen by every check
in the frame; and a command key pressed while walking stops the walk and
is acted on at once instead of waiting until the player stands still.

Conversations: a click, Space, Enter or Esc skips the line being spoken
(the next line or the topic list comes up at once). The falling and smash
cut-scenes can be skipped with Esc or a click.

Esc opens the menu at once, also while walking (the original read no keys
until the player stood still, so the menu came up late or not at all when
moving): a walk stops, and an attack, spell or jump finishes first.

## Building

Requirements: a C++17 compiler, make and the SDL2 development files.
FFmpeg (libavformat, libavcodec, libswscale, libswresample, libavutil) is
optional; with it the cut-scene movies and the CD music tracks play.

    # Arch / CachyOS
    sudo pacman -S --needed base-devel sdl2 ffmpeg

    # Debian / Ubuntu
    sudo apt install build-essential libsdl2-dev
    sudo apt install libavformat-dev libavcodec-dev libswscale-dev libswresample-dev   # optional

    make            # uses FFmpeg when pkg-config finds it
    make NO_FFMPEG=1

The result is a single binary, `ancientevil`. `make install` (PREFIX,
DESTDIR) installs it with a desktop entry.

### Windows

The Windows build is cross-compiled on Linux with mingw-w64:

    sudo apt install mingw-w64 curl xz-utils make
    tools/build-windows.sh            # or --no-ffmpeg

It downloads the SDL2 mingw package and builds a small static FFmpeg (just
the decoders the game needs) the first time, and leaves a self-contained
`build-win/OpenAncientEvil/ancientevil.exe`. On Windows the game looks for
the data in the current folder, the folder of the exe and
`%APPDATA%\ancientevil`; the simplest is to put the exe into the game folder.

### Releases (GitHub Actions)

`.github/workflows/build.yml` builds all three packages on every push (the
files are kept as workflow artifacts). Pushing a version tag makes a GitHub
Release with them attached:

    git tag v1.0.0
    git push origin v1.0.0

### Arch Linux, CachyOS and other Arch based systems

The `arch` folder of the release has a PKGBUILD and the source tarball:

    cd arch
    makepkg -si          # builds against your SDL2 and FFmpeg, installs with pacman

Then put the game where the installed binary looks for it:

    mkdir -p ~/.local/share/ancientevil
    cp -r /path/to/AncientEvil/. ~/.local/share/ancientevil/
    ancientevil          # or "Open AncientEvil" in the applications menu

Without `--game-dir` the game looks in the current folder,
`~/.local/share/ancientevil`, `/usr/share/ancientevil` and
`/opt/ancientevil`. Saves go next to the game, or to
`~/.local/share/ancientevil/saves` when the game folder is read-only. The
CD folder may be called `cd` or `CD`.

## Running

Point the game at the installed game folder (the one with `RPG.EXE`,
`GAMEDAT`, `LEVELS` and `WAV`). File name case does not matter.

    ./ancientevil --game-dir /path/to/AncientEvil
    ./ancientevil --game-dir /path/to/AncientEvil --classic
    ./ancientevil --game-dir /path/to/AncientEvil --save-dir ~/.local/share/ancientevil

Save games (`SAVE0.AE` .. `SAVE9.AE`) and `PREFS.CFG` go to the save folder,
which defaults to the game folder like the original. Saves are written in
the original file format (the 32-bit memory layout is rebuilt field by
field), so they are meant to be interchangeable with the Windows version.

Speech, movies and CD music live on the game CD. Copy the CD contents to a
`cd` folder inside the game folder, or set `AE_CD_DIR`. Without them the game
runs silently in those places. (The original would not start without its
CD.) Without the speech, conversations wait for a click or key after each
thing a character says, since the speech is what kept the text on screen.

## Controls

As in the original: the mouse does almost everything. Keys include
Space (equipment), Tab (map), A / C / J (attack, cast, jump mode),
M (memorise spells), K (spell hot keys), 0-9 (hot keys), S (search),
O (open doors), E (elixir), H (health), D / U / I (drop, use, identify),
F9 (mouse speed), F11 / F12 (gamma), Backspace (status bar), Esc (options).
Alt+Enter toggles full screen.

## Environment variables

| Variable | Effect |
|---|---|
| `AE_GAME_DIR`, `AE_SAVE_DIR`, `AE_CD_DIR` | folders, as the options above |
| `AE_SCALE=n` | window size as a multiple of the game picture (default 2) |
| `AE_CLASSIC=1` | original 640x480 picture |
| `AE_NO_HIRES=1`, `AE_NO_FILTER=1`, `AE_NO_COLOR=1` | enhanced mode without hi-res models / texture smoothing / coloured light |
| `AE_LIGHT_MODEL=0` / `1` | 3dfx per-tile light / smooth light model |
| `AE_TICK_MS=n` | system tick of the emulated `GetTickCount` (see Timing) |
| `AE_TIMING=1` | print frame and clock rates |
| `AE_FULLSCREEN=1` | start in full screen |
| `AE_SMOOTH=1` | linear instead of nearest-neighbour scaling |
| `AE_NOSOUND=1` | no audio device |
| `AE_NO_PAUSE_ON_FOCUS=1` | keep running when the window loses focus |
| `AE_SCHEME=classic/diablo/wasd` | control scheme for this run |

Testing aids (not part of the original game):

| Variable | Effect |
|---|---|
| `AE_INPUT_SCRIPT=file` | replay timed input; see `src/platform/platform.cpp` |
| `AE_SHOT_EVERY=n`, `AE_SHOT_DIR=dir` | write every n-th frame as a PPM |
| `AE_DEBUG_LEVEL=n` | a new game starts on level n |
| `AE_DEBUG_POS=x,y` | a new game starts with the player on tile x,y |

With `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` the game runs headless.

## Source layout

    src/game/      the decompiled game (one file per address range)
    src/engine/    DDW16.DLL: DirectDrawWindow, PCX, sprites, fonts, shading
    src/platform/  SDL2 video, input, timers, audio mixer, file access
    src/game/globals.*  generated from the reverse-engineering notes
                        (structs.def, globals.def) with the original data

## Known differences

- The classic mode renders exactly the original 640x480 16-bit picture and
  SDL scales it to the window.
- In the enhanced mode full-screen pictures (title, book pages, map) keep
  their 640x480 size with black or dimmed borders, and the scenery sprites
  are the original art, doubled.
- Timers, sound and the mouse cursor run on threads instead of multimedia
  timers and DirectInput; the rates match the original's constants.
- A few out-of-bounds reads in the original (harmless there) are guarded,
  and are marked in the code.
- Parchments: the black inside the close box (and other small dark
  patches of the art) was see-through, showing the status bar through the
  box; only the black round the parchment and its burnt holes are now.
- Menus (the pick-up menu of a pile of items, and all others) ignore the
  click that opened them: an entry is chosen only by a click made after
  the menu came up.
- The status bar shows a sword for swords again: the weapon icon table in
  the port had the skull (an icon of a spell) where the original has the
  sword.

## Legal

Open AncientEvil is an unofficial fan project, not affiliated with or
endorsed by Silver Lightning Software or any rights holder of Ancient Evil.
The port's own code is under the MIT license (see `LICENSE`). Ancient Evil
itself, its art, sound, levels and text are not included and remain the
property of their owners; you need a copy of the original game to play.
`src/game/globals.cpp` holds the initial values of the original program's
global data (tables and message strings), generated from `RPG.EXE` by
`re/gen_globals.py`, so the decompiled code can run.

