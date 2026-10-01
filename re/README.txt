src/game/globals.h and globals.cpp are generated from structs.def and
globals.def by gen_globals.py, which reads the initial data out of the
original RPG.EXE (expected at ../orig/rpg.exe relative to this folder).
Needs Python 3 with pefile and capstone. Run: python3 gen_globals.py
