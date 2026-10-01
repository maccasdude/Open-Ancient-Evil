// Program entry: the Linux replacement for the CRT start-up and WinMain.
//
//   ancientevil [--game-dir DIR] [--save-dir DIR] [--classic]
//
// The default is the enhanced mode: a 960x540 widescreen view (shown at
// 1920x1080) with the 3D characters drawn at full resolution. --classic (or
// AE_CLASSIC=1) gives the original 640x480 picture.
//
// DIR defaults to $AE_GAME_DIR, else the first of: the current directory,
// ~/.local/share/ancientevil, /usr/share/ancientevil, /opt/ancientevil that
// holds the installed game (GAMEDAT, WAV, LEVELS...). Saves and PREFS.CFG go
// to the save directory, which defaults to the game directory like the
// original (or ~/.local/share/ancientevil/saves when that is read-only).
#include "game.h"
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#ifdef _WIN32
#include <SDL2/SDL.h>   // (SDL_main, SDL_GetBasePath)
#endif

// Start-up errors: stderr, and on Windows (no console) a message box too.
static void StartError(const std::string &msg)
{
    fprintf(stderr, "%s", msg.c_str());
#ifdef _WIN32
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Open AncientEvil", msg.c_str(), nullptr);
#endif
}

static void Usage()
{
    fprintf(stderr, "usage: ancientevil [--game-dir DIR] [--save-dir DIR] [--classic]\n");
    exit(2);
}

int main(int argc, char **argv)
{
    const char *gameDir = getenv("AE_GAME_DIR");
    const char *saveDir = getenv("AE_SAVE_DIR");
    int mode = -1;   // --classic / --enhanced (default: the saved setting)
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--game-dir") && i + 1 < argc) gameDir = argv[++i];
        else if (!strcmp(argv[i], "--save-dir") && i + 1 < argc) saveDir = argv[++i];
        else if (!strcmp(argv[i], "--classic")) mode = 0;
        else if (!strcmp(argv[i], "--enhanced")) mode = 1;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) Usage();
        else if (argv[i][0] != '-' && !gameDir) gameDir = argv[i];
        else Usage();
    }
    // Without --game-dir / AE_GAME_DIR: the current folder, then the usual
    // places for a packaged install.
#ifdef _WIN32
    // Windows: the folder of the exe (put it in the game folder), then
    // %APPDATA%\ancientevil
    std::string dataHome = getenv("APPDATA") && *getenv("APPDATA") ? std::string(getenv("APPDATA")) : std::string(".");
    std::string userDir = dataHome + "/ancientevil";
    std::string exeDir = ".";
    if (char *b = SDL_GetBasePath()) {
        exeDir = b;
        SDL_free(b);
    }
    const std::string cands[] = {".", exeDir, userDir};
#else
    std::string dataHome = getenv("XDG_DATA_HOME") && *getenv("XDG_DATA_HOME")
                               ? std::string(getenv("XDG_DATA_HOME"))
                               : std::string(getenv("HOME") ? getenv("HOME") : ".") + "/.local/share";
    std::string userDir = dataHome + "/ancientevil";
    const std::string cands[] = {".", userDir, "/usr/share/ancientevil", "/opt/ancientevil"};
#endif
    static std::string found;
    if (!gameDir) {
        for (const std::string &c : cands)
            if (fileio_has_game(c.c_str())) {
                found = c;
                break;
            }
        if (found.empty()) {
            std::string m = "The game data was not found. Looked in:\n";
            for (const std::string &c : cands) m += "  " + c + "\n";
            m += "\nCopy the installed game (the folder with RPG.EXE and the GAMEDAT, LEVELS\n"
                 "and WAV folders) to " + userDir + ", put the program in that folder,\n"
                 "or use --game-dir DIR.\n";
            StartError(m);
            return 1;
        }
        gameDir = found.c_str();
    }
    // saves beside the game like the original, unless that folder is read-only
    static std::string saves;
    if (!saveDir && access(gameDir, W_OK) != 0) {
        saves = userDir + "/saves";
        plat_mkdir(dataHome.c_str());
        plat_mkdir(userDir.c_str());
        saveDir = saves.c_str();
    }
    fileio_init(gameDir, saveDir ? saveDir : gameDir);
    plat_install_crash_handler();
    SettingsInit();   // ancientevil.cfg and the AE_* overrides
    if (mode >= 0) gSettings.enhanced = mode;
    if (gSettings.enhanced) {
        gLayout.physW = 960;
        gLayout.physH = 540;
        gLayout.ox = 160;   // the original screen is centred for the menus; in the
        gLayout.oy = 30;    // game it moves to the bottom edge (SetScreenOriginY)
        gLayout.hiresScale = gSettings.hiresModels ? 2 : 1;
    }
    plat_configure_output(gLayout.physW * gLayout.hiresScale, gLayout.physH * gLayout.hiresScale, gLayout.hiresScale);
    struct stat st;
    if (stat(fileio_resolve("GAMEDAT\\ARMS.CST", false).c_str(), &st) != 0) {
        StartError(std::string("The game data was not found in '") + gameDir + "'.\n"
                   "Point --game-dir (or AE_GAME_DIR) at the installed game: the folder\n"
                   "that holds RPG.EXE and the GAMEDAT, LEVELS and WAV folders.\n");
        return 1;
    }
    // Port: the game files it cannot run without (an incomplete install,
    // e.g. a minimum install that left them on the CD, crashed later on)
    static const char *kNeeded[] = {
        "action.spr", "aesmall.chr", "arms.cst", "bar.spr", "blood.cst", "bolt-00.omt", "bolt.omt", "bolt.tex",
        "cblood.cst", "cgm.omt", "cgm.tex", "cgr.omt", "cgr.tex", "cgt.omt", "cgt.tex", "cgw.omt", "cgw.tex",
        "chest.cst", "faces.cst", "fireball.omt", "fireball.tex", "horde.omt", "horde.tex", "invitems.cst",
        "items.cst", "lightnin.omt", "lightnin.tex", "magmiss.omt", "magmiss.tex", "map.cst", "mouse.cst",
        "mwall.cst", "rat.omt", "rat.tex", "rats.cst", "rpg1.chr", "runes.cst", "screen.cst", "screen2.cst",
        "servant.amt", "servant.omt", "servant.tex", "spellfx.cst", "summon.omt", "summon.tex", "sundry1.cst",
        "trap.cst", "witems.cst", "witems.spr", "player\\amtlist.txt"};
    std::string missing;
    int nMissing = 0;
    for (const char *f : kNeeded) {
        std::string path = std::string("GAMEDAT\\") + f;
        if (stat(fileio_resolve(path.c_str(), false).c_str(), &st) != 0) {
            if (nMissing < 12) missing += std::string("  GAMEDAT\\") + f + "\n";
            nMissing++;
        }
    }
    plat_init(argc, argv);
    if (nMissing) {
        if (nMissing > 12) missing += "  ...\n";
        std::string m = "Some files of the game are missing from '" + std::string(gameDir) + "':\n\n" + missing +
                        "\nCopy them from your Ancient Evil CD or from a full install of the game\n"
                        "(the CD's GAMEDAT folder). The game may crash without them.";
        plat_message_box(m.c_str(), "Open AncientEvil");
    }
    SettingsApplyLive();
    plat_set_abs_mouse_hook(MouseSetPosition);
    RunStaticInitializers();
    int r = GameMain();
    plat_shutdown();
    return r;
}
