Open AncientEvil - Arch Linux / CachyOS
==========================================

Recommended: build a proper package (uses your system's SDL2 and FFmpeg,
so the cut-scene movies and CD music work):

    sudo pacman -S --needed base-devel sdl2 ffmpeg
    cd arch
    makepkg -si

This installs /usr/bin/ancientevil and an "Ancient Evil" menu entry.
Remove it again with:  sudo pacman -R ancientevil

Then put your copy of the game where it looks for it:

    mkdir -p ~/.local/share/ancientevil
    cp -r /path/to/AncientEvil/. ~/.local/share/ancientevil/
    ancientevil

(The folder with RPG.EXE, GAMEDAT, LEVELS and WAV. For speech, movies and
music copy the game CD into ~/.local/share/ancientevil/cd - "cd" or "CD".)
You can also run it with --game-dir /path/to/game from anywhere.

Quick alternative without building: prebuilt/ancientevil runs directly
(needs only sdl2: sudo pacman -S sdl2) but has no movies or CD music:

    ./prebuilt/ancientevil --game-dir /path/to/AncientEvil

Everything else (options, controls, enhanced mode) is in the README that
the package installs to /usr/share/doc/ancientevil/README.md.
