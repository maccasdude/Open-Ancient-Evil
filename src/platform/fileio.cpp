#include "fileio.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

static std::string gGameDir = ".";
static std::string gSaveDir = ".";
static std::string gCDDir;      // contents of the game CD (speech, movies), optional

static std::string resolve_in(const std::string &base, const std::vector<std::string> &parts, bool *exists);

int plat_mkdir(const char *path) {
#ifdef _WIN32
    return mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

void fileio_init(const char *gameDir, const char *saveDir) {
    gGameDir = gameDir ? gameDir : ".";
    gSaveDir = saveDir ? saveDir : gGameDir;
    plat_mkdir(gSaveDir.c_str());
    const char *cd = getenv("AE_CD_DIR");
    if (cd) {
        gCDDir = cd;
    } else {
        // a "cd" folder in the game folder, in any letter case
        bool ex;
        gCDDir = resolve_in(gGameDir, std::vector<std::string>{"cd"}, &ex);
    }
}

bool fileio_has_game(const char *dir)
{
    bool ex;
    resolve_in(dir, std::vector<std::string>{"GAMEDAT", "ARMS.CST"}, &ex);
    return ex;
}
const char *fileio_cd_dir() { return gCDDir.c_str(); }
const char *fileio_game_dir() { return gGameDir.c_str(); }
const char *fileio_save_dir() { return gSaveDir.c_str(); }

// Normalise "X:\\dir\\File.EXT" to components; drive letters are dropped.
static std::vector<std::string> split_path(const char *path) {
    std::vector<std::string> parts;
    std::string cur;
    const char *p = path;
    if (p[0] && p[1] == ':') p += 2;
    for (; *p; p++) {
        if (*p == '\\' || *p == '/') {
            if (!cur.empty()) parts.push_back(cur);
            cur.clear();
        } else {
            cur += *p;
        }
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
}

// Case-insensitive lookup of each component below base. Returns the host path;
// missing components keep the requested spelling (lower-cased for new files).
static std::string resolve_in(const std::string &base, const std::vector<std::string> &parts, bool *exists) {
    std::string cur = base;
    *exists = true;
    for (size_t i = 0; i < parts.size(); i++) {
        const std::string &want = parts[i];
        if (want == ".") continue;
        std::string found;
        if (*exists) {
            DIR *d = opendir(cur.c_str());
            if (d) {
                struct dirent *e;
                while ((e = readdir(d))) {
                    if (strcasecmp(e->d_name, want.c_str()) == 0) { found = e->d_name; break; }
                }
                closedir(d);
            }
        }
        if (found.empty()) {
            *exists = false;
            std::string lw = want;
            for (auto &c : lw) c = (char)tolower((unsigned char)c);
            found = lw;
        }
        cur += "/" + found;
    }
    return cur;
}

std::string fileio_resolve(const char *path, bool forWrite) {
    std::vector<std::string> parts = split_path(path);
    bool ex;
    // Paths with a drive letter ("D:\\SPEECH\\...") refer to the game CD:
    // look in the CD directory first, then the game directory.
    if (!forWrite && path[0] && path[1] == ':') {
        std::string s = resolve_in(gCDDir, parts, &ex);
        if (ex) return s;
    }
    if (forWrite) {
        std::string s = resolve_in(gSaveDir, parts, &ex);
        return s;
    }
    std::string s = resolve_in(gSaveDir, parts, &ex);
    if (ex && gSaveDir != gGameDir) return s;
    s = resolve_in(gGameDir, parts, &ex);
    return s;
}

int w_open(const char *path, int wf, int pmode) {
    int fl = 0;
    switch (wf & 3) {
    case W_O_WRONLY: fl = O_WRONLY; break;
    case W_O_RDWR: fl = O_RDWR; break;
    default: fl = O_RDONLY; break;
    }
    if (wf & W_O_APPEND) fl |= O_APPEND;
    if (wf & W_O_CREAT) fl |= O_CREAT;
    if (wf & W_O_TRUNC) fl |= O_TRUNC;
    if (wf & W_O_EXCL) fl |= O_EXCL;
#ifdef _WIN32
    // binary unless asked for text, as the files are read on Linux
    fl |= (wf & W_O_TEXT) ? O_TEXT : O_BINARY;
#endif
    bool wr = (wf & 3) != 0 || (wf & (W_O_CREAT | W_O_TRUNC));
    std::string p = fileio_resolve(path, wr);
    return open(p.c_str(), fl, 0644);
    (void)pmode;
}

int w_read(int fd, void *buf, unsigned n) { return (int)read(fd, buf, n); }
int w_write(int fd, const void *buf, unsigned n) { return (int)write(fd, buf, n); }
int w_close(int fd) { return fd >= 0 ? close(fd) : -1; }
long w_filelength(int fd) {
    struct stat st;
    if (fstat(fd, &st) != 0) return -1;
    return (long)st.st_size;
}

FILE *w_fopen(const char *path, const char *mode) {
    bool wr = strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+');
    std::string p = fileio_resolve(path, wr);
    return fopen(p.c_str(), mode);
}
