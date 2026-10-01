// Port: the port's own settings (ancientevil.cfg in the save folder), key
// bindings and their translation into the original game's keys.
#include "game.h"
#include "settings.h"
#include "../platform/platform.h"
#include "../platform/fileio.h"
#include <string>

PortSettings gSettings;

// VK codes used below
enum {
    K_BACK = 0x08, K_TAB = 0x09, K_RET = 0x0d, K_SHIFT = 0x10, K_CTRL = 0x11, K_ALT = 0x12,
    K_ESC = 0x1b, K_SPACE = 0x20, K_LEFT = 0x25, K_UP = 0x26, K_RIGHT = 0x27, K_DOWN = 0x28,
    K_F5 = 0x74, K_F8 = 0x77, K_F9 = 0x78, K_F11 = 0x7a, K_F12 = 0x7b, K_PERIOD = 0xbe,
};

#define ALL 7
#define CD 3     // classic and Diablo
#define W 4      // WASD
// name, hold, original (ch, vk), defaults {classic, Diablo, WASD} x {primary, secondary}, schemes
const ActionInfo kActions[kNumActions] = {
    {"Move up", true, 0, 0, {{0, 0}, {0, 0}, {'W', K_UP}}, W},
    {"Move left", true, 0, 0, {{0, 0}, {0, 0}, {'A', K_LEFT}}, W},
    {"Move down", true, 0, 0, {{0, 0}, {0, 0}, {'S', K_DOWN}}, W},
    {"Move right", true, 0, 0, {{0, 0}, {0, 0}, {'D', K_RIGHT}}, W},
    {"Walk forward", true, 0, 0, {{K_UP, 0}, {K_UP, 0}, {0, 0}}, CD},
    {"Walk back", true, 0, 0, {{K_DOWN, 0}, {K_DOWN, 0}, {0, 0}}, CD},
    {"Turn left", true, 0, 0, {{K_LEFT, 0}, {K_LEFT, 0}, {0, 0}}, CD},
    {"Turn right", true, 0, 0, {{K_RIGHT, 0}, {K_RIGHT, 0}, {0, 0}}, CD},
    {"Run (hold)", true, 0, 0, {{K_SHIFT, 0}, {0, 0}, {K_SHIFT, 0}}, ALL},
    {"Face cursor (hold)", true, 0, 0, {{K_ALT, 0}, {K_ALT, 0}, {0, 0}}, CD},
    {"Attack (hold)", true, 0, 0, {{K_CTRL, 0}, {K_CTRL, 0}, {K_CTRL, 0}}, ALL},
    {"Attack in place", true, 0, 0, {{0, 0}, {K_SHIFT, 0}, {0, 0}}, 2},
    {"Jump (hold)", true, 0, 0, {{0, 0}, {'J', 0}, {K_SPACE, 0}}, 6},
    {"Weapon mode", false, 'A', 0, {{'A', 0}, {'A', 0}, {'Z', 0}}, ALL},
    {"Spell mode", false, 'C', 0, {{'C', 0}, {'C', 0}, {'X', 0}}, ALL},
    {"Jump mode", false, 'J', 0, {{'J', 0}, {0, 0}, {0, 0}}, 1},
    {"Equipment", false, ' ', 0, {{K_SPACE, 0}, {K_SPACE, 0}, {'I', K_TAB}}, ALL},
    {"Map", false, '\t', 0, {{K_TAB, 0}, {K_TAB, 0}, {'M', 0}}, ALL},
    {"Memorise spells", false, 'M', 0, {{'M', 0}, {'M', 0}, {'B', 0}}, ALL},
    {"Spell hotkeys", false, 'K', 0, {{'K', 0}, {'K', 0}, {'K', 0}}, ALL},
    {"Show health", false, 'H', 0, {{'H', 0}, {'H', 0}, {'H', 0}}, ALL},
    {"Open doors", false, 'O', 0, {{'O', 0}, {'O', 0}, {'F', 0}}, ALL},
    {"Drink elixir", false, 'E', 0, {{'E', 0}, {'E', 0}, {'Q', 0}}, ALL},
    {"Drop item", false, 'D', 0, {{'D', 0}, {'D', 0}, {'G', 0}}, ALL},
    {"Use item", false, 'U', 0, {{'U', 0}, {'U', 0}, {'E', 0}}, ALL},
    {"Identify", false, 'I', 0, {{'I', 0}, {'I', 0}, {'N', 0}}, ALL},
    {"Search", false, 'S', 0, {{'S', 0}, {'S', 0}, {'R', 0}}, ALL},
    {"Centre view", false, '.', 0, {{K_PERIOD, 0}, {K_PERIOD, 0}, {'C', 0}}, ALL},
    {"Status bar", false, 0, K_BACK, {{K_BACK, 0}, {K_BACK, 0}, {K_BACK, 0}}, ALL},
    {"Mouse speed", false, 0, K_F9, {{K_F9, 0}, {K_F9, 0}, {K_F9, 0}}, ALL},
    {"Darker", false, 0, K_F11, {{K_F11, 0}, {K_F11, 0}, {K_F11, 0}}, ALL},
    {"Brighter", false, 0, K_F12, {{K_F12, 0}, {K_F12, 0}, {K_F12, 0}}, ALL},
    {"Quick save", false, 0, 0, {{K_F5, 0}, {K_F5, 0}, {K_F5, 0}}, ALL},
    {"Quick load", false, 0, 0, {{K_F8, 0}, {K_F8, 0}, {K_F8, 0}}, ALL},
};
#undef ALL
#undef CD
#undef W

// ---------------------------------------------------------------------------
// Key names

static const struct { uint8_t vk; const char *name; } kKeyNames[] = {
    {0x08, "Bksp"}, {0x09, "Tab"}, {0x0d, "Enter"}, {0x10, "Shift"}, {0x11, "Ctrl"}, {0x12, "Alt"},
    {0x13, "Pause"}, {0x14, "CapsLock"}, {0x1b, "Esc"}, {0x20, "Space"}, {0x21, "PgUp"}, {0x22, "PgDn"},
    {0x23, "End"}, {0x24, "Home"}, {0x25, "Left"}, {0x26, "Up"}, {0x27, "Right"}, {0x28, "Down"},
    {0x2d, "Ins"}, {0x2e, "Del"}, {0x60, "Num0"}, {0x61, "Num1"}, {0x62, "Num2"}, {0x63, "Num3"},
    {0x64, "Num4"}, {0x65, "Num5"}, {0x66, "Num6"}, {0x67, "Num7"}, {0x68, "Num8"}, {0x69, "Num9"},
    {0x6a, "Num*"}, {0x6b, "Num+"}, {0x6d, "Num-"}, {0x6e, "Num."}, {0x6f, "Num/"},
    {0xba, ";"}, {0xbb, "="}, {0xbc, ","}, {0xbd, "-"}, {0xbe, "."}, {0xbf, "/"}, {0xc0, "`"},
    {0xdb, "["}, {0xdc, "\\"}, {0xdd, "]"}, {0xde, "'"},
};

const char *KeyName(int vk)
{
    static char buf[16];
    if (vk <= 0) return "";
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        buf[0] = (char)vk;
        buf[1] = 0;
        return buf;
    }
    if (vk >= 0x70 && vk <= 0x7b) {
        snprintf(buf, sizeof buf, "F%d", vk - 0x6f);
        return buf;
    }
    for (auto &k : kKeyNames)
        if (k.vk == vk) return k.name;
    snprintf(buf, sizeof buf, "#%d", vk);
    return buf;
}

// The keys bound to an action in the current scheme, for hints: 'D',
// 'D' or 'F1', or Unbound.
const char *BindText(int action)
{
    static char buf[4][40];
    static int n;
    char *b = buf[n++ & 3];
    const uint8_t *k = gSettings.bind[gSettings.scheme][action];
    if (!ActionApplies(action) || (!k[0] && !k[1])) return "Unbound";
    if (k[0] && k[1]) {
        char a[16];
        snprintf(a, sizeof a, "%s", KeyName(k[0]));
        snprintf(b, 40, "'%s' or '%s'", a, KeyName(k[1]));
    } else {
        snprintf(b, 40, "'%s'", KeyName(k[0] ? k[0] : k[1]));
    }
    return b;
}

static int KeyFromName(const std::string &s)
{
    if (s.empty() || s == "-") return 0;
    if (s.size() == 1 && ((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= '0' && s[0] <= '9'))) return s[0];
    if (s.size() == 1 && s[0] >= 'a' && s[0] <= 'z') return s[0] - 32;
    if (s[0] == 'F' && s.size() > 1 && isdigit((unsigned char)s[1])) {
        int n = atoi(s.c_str() + 1);
        if (n >= 1 && n <= 12) return 0x6f + n;
    }
    for (auto &k : kKeyNames)
        if (s == k.name) return k.vk;
    if (s[0] == '#') return atoi(s.c_str() + 1) & 0xff;
    return 0;
}

// A key queue entry {character, virtual key} -> VK code. Letters, digits and
// punctuation arrive as characters; Tab, Enter and Backspace arrive both
// ways (only one is used).
int KeyEventToVk(uint8_t ch, uint8_t vk)
{
    if (ch) {
        if (ch == 0x08) return 0;            // (also sent as a virtual key)
        if (ch >= 'a' && ch <= 'z') return ch - 32;
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) return ch;
        switch (ch) {
        case '\t': return K_TAB;
        case '\r': return K_RET;
        case 0x1b: return K_ESC;
        case ' ': return K_SPACE;
        case ';': case ':': return 0xba;
        case '=': case '+': return 0xbb;
        case ',': case '<': return 0xbc;
        case '-': case '_': return 0xbd;
        case '.': case '>': return 0xbe;
        case '/': case '?': return 0xbf;
        case '`': case '~': return 0xc0;
        case '[': case '{': return 0xdb;
        case '\\': case '|': return 0xdc;
        case ']': case '}': return 0xdd;
        case '\'': case '"': return 0xde;
        case '!': return '1'; case '@': return '2'; case '#': return '3'; case '$': return '4';
        case '%': return '5'; case '^': return '6'; case '&': return '7'; case '*': return '8';
        case '(': return '9'; case ')': return '0';
        }
        return 0;
    }
    if (vk == K_TAB || vk == K_RET || (vk >= 0x30 && vk <= 0x5a)) return 0;   // (also sent as characters)
    return vk;
}

// ---------------------------------------------------------------------------
// Bindings

void ResetBindings(int scheme)
{
    for (int a = 0; a < kNumActions; a++)
        for (int k = 0; k < 2; k++) gSettings.bind[scheme][a][k] = kActions[a].def[scheme][k];
}

bool ActionApplies(int a) { return (kActions[a].schemes >> gSettings.scheme) & 1; }

bool ActionHeld(int a)
{
    if (!ActionApplies(a)) return false;
    for (int k = 0; k < 2; k++) {
        int vk = gSettings.bind[gSettings.scheme][a][k];
        if (vk && plat_GetAsyncKeyState(vk) < 0) return true;
    }
    return false;
}

// Does a key queue entry give a command (as TranslateGameKey would turn it
// into one)? Held keys, unbound keys and the halves of keys sent twice do not.
bool KeyIsCommand(uint8_t ch, uint8_t vkIn)
{
    int vk = KeyEventToVk(ch, vkIn);
    if (!vk) return false;
    if (vk == K_ESC) return true;
    const int s = gSettings.scheme;
    for (int a = 0; a < kNumActions; a++) {
        if (!ActionApplies(a)) continue;
        if (gSettings.bind[s][a][0] != vk && gSettings.bind[s][a][1] != vk) continue;
        return !kActions[a].hold;
    }
    return (vk >= '0' && vk <= '9') || vk == 0xbb || vk == 0xbd;
}

// Keys pressed since the last game frame. (The Windows "pressed since the
// last call" bit is cleared by whoever asks first, so a quick tap was seen
// by one check and missed by the others in the same frame.)
static uint32_t gPressSeen[256];
static uint8_t gPressedThisFrame[256];

void InputFrameBegin()
{
    for (int vk = 0; vk < 256; vk++) {
        unsigned n = plat_key_presses(vk);
        gPressedThisFrame[vk] = n != gPressSeen[vk];
        gPressSeen[vk] = n;
    }
}

bool ActionAny(int a)
{
    if (!ActionApplies(a)) return false;
    bool r = false;
    for (int k = 0; k < 2; k++) {
        int vk = gSettings.bind[gSettings.scheme][a][k];
        if (vk && (plat_GetAsyncKeyState(vk) < 0 || gPressedThisFrame[vk])) r = true;
    }
    return r;
}

static void QuickSave()
{
    if (gPlayer.gameMode) return;
    ReadSaveHeaders();
    snprintf(gSaveHeaders[9].desc, sizeof gSaveHeaders[9].desc, "%s", "Quicksave");
    SaveGame(9);
    RedrawGameScreen(-1);
    ShowMessage("Game saved (slot 10)", gColorWhite);
}

static void QuickLoad()
{
    if (!SaveExists(9)) {
        ShowMessage("No quicksave yet (F5 saves)", gColorRed);
        return;
    }
    ReadSaveHeaders();
    LoadGame(9);
    UpdateLightRadius();
    UpdateLighting();
    gDDW.FillRect(0, 0, 0x27f, 0x1df, 0);
    ResetMouseClicks();
    ShowMessage("Quicksave loaded", gColorWhite);
}

// A key from the queue in normal play: rewrite it into the original key of
// the action it is bound to. Returns 0 when the key is to be ignored.
int TranslateGameKey(uint8_t *buf)
{
    int vk = KeyEventToVk(buf[0], buf[1]);
    if (!vk) {
        // the duplicate half of Tab/Enter, and keys with no VK: leave the
        // original's own handling (Backspace, F9, F11, F12 come as VKs)
        if (buf[0] == 0 && buf[1] == 0) return 0;
        if (buf[0] == 0x08) return 0;
        buf[0] = buf[1] = 0;
        return 1;
    }
    if (vk == K_ESC) return 1;   // the options menu is not rebindable
    const int s = gSettings.scheme;
    for (int a = 0; a < kNumActions; a++) {
        if (!ActionApplies(a)) continue;
        if (gSettings.bind[s][a][0] != vk && gSettings.bind[s][a][1] != vk) continue;
        if (kActions[a].hold) {
            buf[0] = buf[1] = 0;   // held keys are read directly
            return 1;
        }
        if (a == ACT_QUICKSAVE) { QuickSave(); buf[0] = buf[1] = 0; return 1; }
        if (a == ACT_QUICKLOAD) { QuickLoad(); buf[0] = buf[1] = 0; return 1; }
        buf[0] = kActions[a].ch;
        buf[1] = kActions[a].vk;
        return 1;
    }
    // not bound: digits keep choosing spell hot keys; the original keys of
    // actions (now bound elsewhere) and anything else do nothing
    if (vk >= '0' && vk <= '9') {
        buf[0] = (uint8_t)vk;
        buf[1] = 0;
        return 1;
    }
    if (vk == 0xbb || vk == 0xbd) {   // '=' '-': the light level keys of the original
        buf[0] = vk == 0xbb ? '=' : '-';
        buf[1] = 0;
        return 1;
    }
    buf[0] = buf[1] = 0;
    return 1;
}

// ---------------------------------------------------------------------------
// ancientevil.cfg

static const char *kSchemeKey[kNumSchemes] = {"classic", "diablo", "wasd"};

const char *SchemeName(int s)
{
    static const char *n[kNumSchemes] = {"Classic", "Diablo style", "WASD + mouse aim"};
    return (s >= 0 && s < kNumSchemes) ? n[s] : "?";
}

double TickLengthMs(int t)
{
    switch (t) {
    case 1: return 10.0;
    case 2: return 15.625;
    case 3: return 1.0;
    }
    return 65536.0 * 1000.0 / 1193182.0;
}

const char *TickName(int t)
{
    switch (t) {
    case 1: return "Windows NT 4 (20 fps)";
    case 2: return "Windows 2000/XP (16 fps)";
    case 3: return "1 ms clock (about 18.7 fps)";
    }
    return "Windows 95/98 (18.2 fps)";
}

static std::string ActionKey(int a)
{
    std::string s;
    for (const char *p = kActions[a].name; *p && *p != '('; p++) {
        char c = (char)tolower((unsigned char)*p);
        if (isalnum((unsigned char)c)) s += c;
        else if (!s.empty() && s.back() != '_') s += '_';
    }
    while (!s.empty() && s.back() == '_') s.pop_back();
    return s;
}

static std::string CfgPath() { return fileio_resolve("ANCIENTEVIL.CFG", true); }

void SettingsSave()
{
    FILE *f = fopen(CfgPath().c_str(), "w");
    if (!f) return;
    PortSettings &s = gSettings;
    fprintf(f, "# Ancient Evil for Linux: settings of the port (the game's own are in PREFS.CFG)\n");
    fprintf(f, "enhanced=%d\nhires_models=%d\ntexture_filter=%d\ncolour_light=%d\n", s.enhanced, s.hiresModels,
            s.textureFilter, s.colorLight);
    fprintf(f, "smooth_light=%d\n", s.smoothLight);
    fprintf(f, "light_model=%d\n", s.lightModel);
    fprintf(f, "fullscreen=%d\nwindow_scale=%d\nscaling=%d\n", s.fullscreen, s.windowScale, s.scaling);
    fprintf(f, "tick=%d\npause_on_focus=%d\n", s.tick, s.pauseOnFocus);
    fprintf(f, "scheme=%s\nwasd_relative=%d\nfollow_camera=%d\n", kSchemeKey[s.scheme], s.wasdRelative, s.followCamera);
    for (int sc = 0; sc < kNumSchemes; sc++)
        for (int a = 0; a < kNumActions; a++) {
            if (!((kActions[a].schemes >> sc) & 1)) continue;
            std::string k0 = s.bind[sc][a][0] ? KeyName(s.bind[sc][a][0]) : "-";
            std::string k1 = s.bind[sc][a][1] ? KeyName(s.bind[sc][a][1]) : "-";
            fprintf(f, "bind.%s.%s=%s %s\n", kSchemeKey[sc], ActionKey(a).c_str(), k0.c_str(), k1.c_str());
        }
    fclose(f);
}

static void SettingsLoad()
{
    FILE *f = fopen(CfgPath().c_str(), "r");
    if (!f) return;
    char line[256];
    PortSettings &s = gSettings;
    while (fgets(line, sizeof line, f)) {
        std::string l(line);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        if (l.empty() || l[0] == '#') continue;
        size_t eq = l.find('=');
        if (eq == std::string::npos) continue;
        std::string k = l.substr(0, eq), v = l.substr(eq + 1);
        int n = atoi(v.c_str());
        if (k == "enhanced") s.enhanced = n != 0;
        else if (k == "hires_models") s.hiresModels = n != 0;
        else if (k == "texture_filter") s.textureFilter = n != 0;
        else if (k == "colour_light") s.colorLight = n != 0;
        else if (k == "smooth_light") s.smoothLight = n != 0;
        else if (k == "light_model") s.lightModel = n != 0;
        else if (k == "fullscreen") s.fullscreen = n != 0;
        else if (k == "window_scale") s.windowScale = n < 1 ? 1 : n > 4 ? 4 : n;
        else if (k == "scaling") s.scaling = n < 0 ? 0 : n > 2 ? 2 : n;
        else if (k == "tick") s.tick = n < 0 ? 0 : n > 3 ? 3 : n;
        else if (k == "pause_on_focus") s.pauseOnFocus = n != 0;
        else if (k == "wasd_relative") s.wasdRelative = n != 0;
        else if (k == "follow_camera") s.followCamera = n != 0;
        else if (k == "scheme") {
            for (int i = 0; i < kNumSchemes; i++)
                if (v == kSchemeKey[i]) s.scheme = i;
        } else if (k.compare(0, 5, "bind.") == 0) {
            size_t d = k.find('.', 5);
            if (d == std::string::npos) continue;
            std::string sc = k.substr(5, d - 5), an = k.substr(d + 1);
            int si = -1;
            for (int i = 0; i < kNumSchemes; i++)
                if (sc == kSchemeKey[i]) si = i;
            if (si < 0) continue;
            for (int a = 0; a < kNumActions; a++) {
                if (ActionKey(a) != an) continue;
                size_t sp = v.find(' ');
                std::string k0 = v.substr(0, sp), k1 = sp == std::string::npos ? "" : v.substr(sp + 1);
                s.bind[si][a][0] = (uint8_t)KeyFromName(k0);
                s.bind[si][a][1] = (uint8_t)KeyFromName(k1);
            }
        }
    }
    fclose(f);
}

void SettingsInit()
{
    for (int sc = 0; sc < kNumSchemes; sc++) ResetBindings(sc);
    SettingsLoad();
    // environment overrides (for this run)
    PortSettings &s = gSettings;
    if (getenv("AE_CLASSIC")) s.enhanced = 0;
    if (getenv("AE_NO_HIRES")) s.hiresModels = 0;
    if (getenv("AE_NO_FILTER")) s.textureFilter = 0;
    if (getenv("AE_NO_COLOR")) s.colorLight = 0;
    if (const char *e = getenv("AE_LIGHT_MODEL")) s.lightModel = atoi(e) != 0;
    if (getenv("AE_FULLSCREEN")) s.fullscreen = 1;
    if (const char *e = getenv("AE_SCALE")) s.windowScale = atoi(e) > 0 ? atoi(e) : 2;
    if (getenv("AE_SMOOTH")) s.scaling = 2;
    if (getenv("AE_NO_PAUSE_ON_FOCUS")) s.pauseOnFocus = 0;
    if (const char *e = getenv("AE_SCHEME")) {
        for (int i = 0; i < kNumSchemes; i++)
            if (!strcmp(e, kSchemeKey[i])) s.scheme = i;
    }
}

void SettingsApplyLive()
{
    PortSettings &s = gSettings;
    gColorLight = s.enhanced && s.colorLight;
    SetModelFilter(s.textureFilter != 0);
    if (!getenv("AE_TICK_MS")) plat_set_tick_ms(TickLengthMs(s.tick));
    plat_set_pause_on_focus(s.pauseOnFocus != 0);
    plat_set_fullscreen(s.fullscreen != 0);
    plat_set_scaling(s.scaling);
    plat_set_window_scale(s.windowScale);
    ClearFloorCache();
}
