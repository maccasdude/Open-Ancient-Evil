Open AncientEvil for Windows
============================

This is an open source port of Ancient Evil (Silver Lightning Software,
1998). The game data is NOT included: you need your own copy of the game.

1. Copy ancientevil.exe into the folder of your installed Ancient Evil
   (the folder with RPG.EXE and the GAMEDAT, LEVELS and WAV folders).
2. Double-click ancientevil.exe.

Saves go into that folder like the original game. If the folder is
read-only, they go to %APPDATA%\ancientevil\saves instead.

You can also keep the exe elsewhere and start it with
    ancientevil.exe --game-dir "C:\path\to\AncientEvil"
or copy the game into %APPDATA%\ancientevil.

Speech, cut-scenes and CD music: copy the game CD into a folder named "cd"
inside the game folder (CD music as audio files, e.g. Track02.ogg, in
cd\music or a "music" folder next to the game data).

Options (display, controls, key bindings, lighting) are in the Options
screen on the title screen and in the in-game menu (Esc). Everything else
is in README.md.

Windows SmartScreen may warn about an unknown publisher the first time,
because the exe is not code-signed: choose "More info" then "Run anyway".
