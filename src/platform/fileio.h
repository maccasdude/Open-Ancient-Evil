// File access with Windows-style paths (backslashes, drive letters, any case).
#pragma once
#include <stdio.h>
#include <string>

// MSVC _open flags as used by the original binaries.
enum {
    W_O_RDONLY = 0x0000, W_O_WRONLY = 0x0001, W_O_RDWR = 0x0002, W_O_APPEND = 0x0008,
    W_O_CREAT = 0x0100, W_O_TRUNC = 0x0200, W_O_EXCL = 0x0400,
    W_O_TEXT = 0x4000, W_O_BINARY = 0x8000,
};

void fileio_init(const char *gameDir, const char *saveDir);
bool fileio_has_game(const char *dir);   // GAMEDAT\ARMS.CST found there (any letter case)
// Resolve a game path to a host path. forWrite paths go to the save directory.
std::string fileio_resolve(const char *path, bool forWrite);
const char *fileio_game_dir();
const char *fileio_save_dir();
const char *fileio_cd_dir();

int w_open(const char *path, int wflags, int pmode = 0600);
int w_read(int fd, void *buf, unsigned n);
int w_write(int fd, const void *buf, unsigned n);
int w_close(int fd);
long w_filelength(int fd);
FILE *w_fopen(const char *path, const char *mode);
int plat_mkdir(const char *path);   // (0755 on Linux)
