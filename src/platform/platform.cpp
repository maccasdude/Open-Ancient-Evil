#include "platform.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <deque>
#include <mutex>
#include <map>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <string>
#include <ctype.h>
#include <strings.h>
#include <math.h>

static SDL_Window *gWin = nullptr;
static SDL_Renderer *gRen = nullptr;
static SDL_Texture *gTex = nullptr;
static int gW = 640, gH = 480;   // presented image size
static int gLogicalScale = 1;     // presented pixels per game pixel
static SDL_threadID gMainThread;
static std::mutex gFrameMutex;
static uint16_t *gStage = nullptr;
static bool gStageDirty = false;
static PlatWndProc gWndProc = nullptr;
static bool gQuit = false;
static bool gFullscreen = false;
static int gWindowScale = 2;
static int gScaling = -1;
static bool gHeadless = false;
static void maybe_screenshot();
static void write_shot(const char *name);
static void load_script();

// ---------------------------------------------------------------- init

void plat_init(int, char **) {
    gMainThread = SDL_ThreadID();
    const char *drv = getenv("SDL_VIDEODRIVER");
    gHeadless = drv && (!strcmp(drv, "dummy") || !strcmp(drv, "offscreen"));
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        exit(1);
    }
    if (getenv("AE_FULLSCREEN")) gFullscreen = true;
    load_script();
}

void plat_shutdown() {
    if (gTex) SDL_DestroyTexture(gTex);
    if (gRen) SDL_DestroyRenderer(gRen);
    if (gWin) SDL_DestroyWindow(gWin);
    SDL_Quit();
}

void *plat_create_window(const char *title) {
    if (gWin) return gWin;
    int scale = gWindowScale;
    (void)title;   // (the port's own name in the title bar)
    gWin = SDL_CreateWindow("Open AncientEvil", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            gW / gLogicalScale * scale, gH / gLogicalScale * scale,
                            SDL_WINDOW_RESIZABLE | (gFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!gWin) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        exit(1);
    }
    gRen = SDL_CreateRenderer(gWin, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!gRen) gRen = SDL_CreateRenderer(gWin, -1, 0);
    if (!gRen) gRen = SDL_CreateRenderer(gWin, -1, SDL_RENDERER_SOFTWARE);
    SDL_StopTextInput();   // (characters are made from the key presses, see key_char)
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, gScaling == 2 ? "linear" : "nearest");
    if (gScaling == 1) SDL_RenderSetIntegerScale(gRen, SDL_TRUE);
    plat_set_video_mode(gW, gH);
    return gWin;
}

void plat_configure_output(int w, int h, int logicalScale) {
    gW = w;
    gH = h;
    gLogicalScale = logicalScale > 0 ? logicalScale : 1;
}

void plat_set_video_mode(int w, int h) {
    gW = w; gH = h;
    if (!gRen) return;
    if (gTex) SDL_DestroyTexture(gTex);
    gTex = SDL_CreateTexture(gRen, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);
    SDL_RenderSetLogicalSize(gRen, w, h);
    std::lock_guard<std::mutex> lk(gFrameMutex);
    free(gStage);
    gStage = (uint16_t *)calloc((size_t)w * h, 2);
}

void plat_toggle_fullscreen() {
    gFullscreen = !gFullscreen;
    SDL_SetWindowFullscreen(gWin, gFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void plat_set_fullscreen(bool on) {
    if (on == gFullscreen) return;
    if (!gWin) { gFullscreen = on; return; }
    plat_toggle_fullscreen();
}

bool plat_is_fullscreen() { return gFullscreen; }

void plat_set_window_scale(int scale) {
    if (scale < 1) scale = 1;
    gWindowScale = scale;
    if (!gWin || gFullscreen || gHeadless) return;
    int w = gW / gLogicalScale * scale, h = gH / gLogicalScale * scale;
    int cw, ch;
    SDL_GetWindowSize(gWin, &cw, &ch);
    if (cw == w && ch == h) return;
    SDL_SetWindowSize(gWin, w, h);
    SDL_SetWindowPosition(gWin, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void plat_set_scaling(int mode) {
    if (mode == gScaling) return;
    gScaling = mode;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, mode == 2 ? "linear" : "nearest");
    if (!gRen) return;
    SDL_RenderSetIntegerScale(gRen, mode == 1 ? SDL_TRUE : SDL_FALSE);
    plat_set_video_mode(gW, gH);   // (the filter is chosen when the texture is made)
}

static bool gPauseOnFocus = true;
void plat_set_pause_on_focus(bool on) { gPauseOnFocus = on; }

static int gLastKeypress;
int plat_take_keypress() {
    int k = gLastKeypress;
    gLastKeypress = 0;
    return k;
}

// Cursor overlay / screenshot hooks (optional)
static void render_stage() {
    if (!gRen || !gTex) return;
    {
        std::lock_guard<std::mutex> lk(gFrameMutex);
        if (!gStage) return;
        SDL_UpdateTexture(gTex, nullptr, gStage, gW * 2);
        gStageDirty = false;
    }
    SDL_SetRenderDrawColor(gRen, 0, 0, 0, 255);
    SDL_RenderClear(gRen);
    SDL_RenderCopy(gRen, gTex, nullptr, nullptr);
    SDL_RenderPresent(gRen);
}

static int gShotCount = 0;

void plat_present(const uint16_t *px, int pitch, int w, int h) {
    {
        std::lock_guard<std::mutex> lk(gFrameMutex);
        if (!gStage || w != gW || h != gH) return;
        for (int y = 0; y < h; y++) memcpy(gStage + (size_t)y * w, (const uint8_t *)px + (size_t)y * pitch, (size_t)w * 2);
        gStageDirty = true;
    }
    if (SDL_ThreadID() == gMainThread) {
        maybe_screenshot();
        render_stage();
    }
}

void plat_message_box(const char *text, const char *caption) {
    fprintf(stderr, "[%s] %s\n", caption ? caption : "", text ? text : "");
    if (!gHeadless && !getenv("AE_NO_MSGBOX"))
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, caption ? caption : "Open AncientEvil", text ? text : "", gWin);
}

// ---------------------------------------------------------------- keyboard

static uint8_t gKeyDown[256];
static uint8_t gKeyPressedSince[256];
static uint32_t gKeyPresses[256];   // down transitions (for per-frame press checks)

unsigned plat_key_presses(int vk) { return (vk >= 0 && vk < 256) ? gKeyPresses[vk] : 0; }

// Port: the character a key types, made here from the key press rather
// than taken from SDL's text input, which drops letters while Ctrl or Alt
// is held (both are bindable hold keys) and can lose or delay keys behind
// an input method (IBus and the like). US symbols for shifted keys.
static int key_char(const SDL_Keysym &ks) {
    SDL_Keycode k = ks.sym;
    bool shift = (ks.mod & KMOD_SHIFT) != 0;
    if (k >= 'a' && k <= 'z') {
        bool upper = shift != ((ks.mod & KMOD_CAPS) != 0);
        return upper ? (int)(k - 'a' + 'A') : (int)k;
    }
    if (k >= SDLK_KP_1 && k <= SDLK_KP_9) return (ks.mod & KMOD_NUM) ? '1' + (int)(k - SDLK_KP_1) : 0;
    if (k == SDLK_KP_0) return (ks.mod & KMOD_NUM) ? '0' : 0;
    switch (k) {
    case SDLK_KP_MULTIPLY: return '*';
    case SDLK_KP_PLUS: return '+';
    case SDLK_KP_MINUS: return '-';
    case SDLK_KP_DIVIDE: return '/';
    case SDLK_KP_PERIOD: return (ks.mod & KMOD_NUM) ? '.' : 0;
    }
    if (k < 0x20 || k > 0x7e) return 0;
    if (!shift) return (int)k;
    static const char kFrom[] = "1234567890-=[]\\;',./`";
    static const char kTo[] = "!@#$%^&*()_+{}|:\"<>?~";
    for (int i = 0; kFrom[i]; i++)
        if (kFrom[i] == (char)k) return (unsigned char)kTo[i];
    return (int)k;
}

// Let go of every key and button (the window lost the focus: their
// releases go to another window, and they stayed held down).
static void release_all_input();

static int sdl_to_vk(SDL_Keycode k, SDL_Scancode sc) {
    if (k >= 'a' && k <= 'z') return k - 'a' + 'A';
    if (k >= '0' && k <= '9') return k;
    switch (k) {
    case SDLK_BACKSPACE: return 0x08;
    case SDLK_TAB: return 0x09;
    case SDLK_RETURN: case SDLK_KP_ENTER: return 0x0d;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return 0x10;
    case SDLK_LCTRL: case SDLK_RCTRL: return 0x11;
    case SDLK_LALT: case SDLK_RALT: return 0x12;
    case SDLK_PAUSE: return 0x13;
    case SDLK_CAPSLOCK: return 0x14;
    case SDLK_ESCAPE: return 0x1b;
    case SDLK_SPACE: return 0x20;
    case SDLK_PAGEUP: return 0x21;
    case SDLK_PAGEDOWN: return 0x22;
    case SDLK_END: return 0x23;
    case SDLK_HOME: return 0x24;
    case SDLK_LEFT: return 0x25;
    case SDLK_UP: return 0x26;
    case SDLK_RIGHT: return 0x27;
    case SDLK_DOWN: return 0x28;
    case SDLK_INSERT: return 0x2d;
    case SDLK_DELETE: return 0x2e;
    case SDLK_KP_0: return 0x60;
    case SDLK_KP_1: return 0x61;
    case SDLK_KP_2: return 0x62;
    case SDLK_KP_3: return 0x63;
    case SDLK_KP_4: return 0x64;
    case SDLK_KP_5: return 0x65;
    case SDLK_KP_6: return 0x66;
    case SDLK_KP_7: return 0x67;
    case SDLK_KP_8: return 0x68;
    case SDLK_KP_9: return 0x69;
    case SDLK_KP_MULTIPLY: return 0x6a;
    case SDLK_KP_PLUS: return 0x6b;
    case SDLK_KP_MINUS: return 0x6d;
    case SDLK_KP_PERIOD: return 0x6e;
    case SDLK_KP_DIVIDE: return 0x6f;
    case SDLK_SEMICOLON: return 0xba;
    case SDLK_EQUALS: return 0xbb;
    case SDLK_COMMA: return 0xbc;
    case SDLK_MINUS: return 0xbd;
    case SDLK_PERIOD: return 0xbe;
    case SDLK_SLASH: return 0xbf;
    case SDLK_BACKQUOTE: return 0xc0;
    case SDLK_LEFTBRACKET: return 0xdb;
    case SDLK_BACKSLASH: return 0xdc;
    case SDLK_RIGHTBRACKET: return 0xdd;
    case SDLK_QUOTE: return 0xde;
    }
    if (k >= SDLK_F1 && k <= SDLK_F12) return 0x70 + (k - SDLK_F1);
    (void)sc;
    return 0;
}

short plat_GetAsyncKeyState(int vk) {
    if (vk < 0 || vk > 255) return 0;
    // mouse buttons as VK_LBUTTON/VK_RBUTTON/VK_MBUTTON
    short r = 0;
    if (gKeyDown[vk]) r |= (short)0x8000;
    if (gKeyPressedSince[vk]) { r |= 1; gKeyPressedSince[vk] = 0; }
    return r;
}

// ---------------------------------------------------------------- mouse

static std::mutex gMouseMutex;
static int gMouseDX = 0, gMouseDY = 0;
static int gMouseBtn[3];
static int gMouseBtnLatch[3];   // pressed since the last poll (so short clicks are not lost)
static std::deque<PlatMouseEvent> gMouseEvents;
static bool gCaptured = false;

void plat_mouse_capture(bool on) {
    gCaptured = on;
    if (gHeadless) return;
    SDL_SetRelativeMouseMode(on ? SDL_TRUE : SDL_FALSE);
}

void plat_mouse_poll(PlatMouseState *s) {
    std::lock_guard<std::mutex> lk(gMouseMutex);
    s->dx = gMouseDX; s->dy = gMouseDY;
    gMouseDX = gMouseDY = 0;
    for (int i = 0; i < 3; i++) {
        s->buttons[i] = gMouseBtn[i] || gMouseBtnLatch[i];
        gMouseBtnLatch[i] = 0;
    }
}

int plat_mouse_get_events(PlatMouseEvent *ev, int max) {
    std::lock_guard<std::mutex> lk(gMouseMutex);
    int n = 0;
    while (n < max && !gMouseEvents.empty()) {
        ev[n++] = gMouseEvents.front();
        gMouseEvents.pop_front();
    }
    return n;
}

static void push_mouse_event(int type, int data) {
    std::lock_guard<std::mutex> lk(gMouseMutex);
    if (gMouseEvents.size() > 256) gMouseEvents.pop_front();
    gMouseEvents.push_back({type, data, SDL_GetTicks()});
}

// Mouse speed: the game was tuned for DirectInput mickeys; the window is
// scaled so scale motion down to logical pixels.
static float gMouseAccX = 0, gMouseAccY = 0;
static void mouse_motion(int xrel, int yrel) {
    int ww, wh;
    SDL_GetWindowSize(gWin, &ww, &wh);
    float sx = ww > 0 ? (float)(gW / gLogicalScale) / (float)ww : 1.0f;
    float sy = wh > 0 ? (float)(gH / gLogicalScale) / (float)wh : 1.0f;
    float s = sx < sy ? sy : sx;
    gMouseAccX += xrel * s;
    gMouseAccY += yrel * s;
    int dx = (int)gMouseAccX, dy = (int)gMouseAccY;
    gMouseAccX -= dx; gMouseAccY -= dy;
    if (dx) { { std::lock_guard<std::mutex> lk(gMouseMutex); gMouseDX += dx; } push_mouse_event(0, dx); }
    if (dy) { { std::lock_guard<std::mutex> lk(gMouseMutex); gMouseDY += dy; } push_mouse_event(1, dy); }
}

// ---------------------------------------------------------------- messages

struct Msg { unsigned msg; unsigned long w; long l; };
static std::deque<Msg> gMsgs;

void plat_set_wndproc(PlatWndProc p) { gWndProc = p; }
void plat_request_quit() { gQuit = true; }
bool plat_quit_requested() { return gQuit; }

static void translate_event(const SDL_Event &e) {
    switch (e.type) {
    case SDL_QUIT:
        gQuit = true;
        break;
    case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
            release_all_input();
            gMsgs.push_back({WM_ACTIVATEAPP_, 1, 0});
        } else if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            release_all_input();
            if (gPauseOnFocus) gMsgs.push_back({WM_ACTIVATEAPP_, 0, 0});
        }
        else if (e.window.event == SDL_WINDOWEVENT_EXPOSED) gStageDirty = true;
        break;
    case SDL_KEYDOWN: {
        if (e.key.keysym.sym == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT)) {
            if (!e.key.repeat) plat_toggle_fullscreen();
            break;
        }
        int vk = sdl_to_vk(e.key.keysym.sym, e.key.keysym.scancode);
        if (!vk) break;
        if (!gKeyDown[vk]) { gKeyPressedSince[vk] = 1; gKeyPresses[vk]++; }
        gKeyDown[vk] = 1;
        if (!e.key.repeat) gLastKeypress = vk;
        gMsgs.push_back({WM_KEYDOWN_, (unsigned long)vk, 0});
        // TranslateMessage generates WM_CHAR for these, and for the keys
        // that type a character
        if (vk == 0x08 || vk == 0x09 || vk == 0x0d || vk == 0x1b) {
            gMsgs.push_back({WM_CHAR_, (unsigned long)vk, 0});
        } else if (int c = key_char(e.key.keysym)) {
            gMsgs.push_back({WM_CHAR_, (unsigned long)c, 0});
        }
        break;
    }
    case SDL_KEYUP: {
        int vk = sdl_to_vk(e.key.keysym.sym, e.key.keysym.scancode);
        if (!vk) break;
        gKeyDown[vk] = 0;
        gMsgs.push_back({WM_KEYUP_, (unsigned long)vk, 0});
        break;
    }
    case SDL_TEXTINPUT:   // (characters come from the key presses, see key_char)
        break;
    case SDL_MOUSEMOTION:
        mouse_motion(e.motion.xrel, e.motion.yrel);
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
        int b = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_RIGHT ? 1 : e.button.button == SDL_BUTTON_MIDDLE ? 2 : -1;
        if (b < 0) break;
        int down = e.type == SDL_MOUSEBUTTONDOWN;
        { std::lock_guard<std::mutex> lk(gMouseMutex); gMouseBtn[b] = down; if (down) gMouseBtnLatch[b] = 1; }
        push_mouse_event(2 + b, down ? 0x80 : 0);
        int vk = b == 0 ? 1 : b == 1 ? 2 : 4;
        if (down && !gKeyDown[vk]) { gKeyPressedSince[vk] = 1; gKeyPresses[vk]++; }
        gKeyDown[vk] = (uint8_t)down;
        if (!gCaptured && down) plat_mouse_capture(true);
        break;
    }
    }
}

// ---------------------------------------------------------------- input scripts
// AE_INPUT_SCRIPT=file replays input for automated tests. Each line is
// "<ms> <op> [args]" with ms counted from start-up:
//   key <vk>          key press (vk in hex 0x.., decimal, or ESC RET SPACE
//                     LEFT RIGHT UP DOWN TAB F1..F12 or a single character)
//   text <string>     typed characters
//   move <x> <y>      put the mouse cursor at game coordinates
//   click <x> <y> [r] move and click (left, or right with 'r')
//   press <x> <y> <ms> [r]  move and hold the button down for ms
//   keydown <vk> / keyup <vk>  hold a key down / let it go
//   shot <file>       write the presented frame as a PPM
//   quit              close the window
struct ScriptEv { unsigned t; std::string op, a1, a2, a3, a4; };
static std::vector<ScriptEv> gScript;
static size_t gScriptPos = 0;
static unsigned gScriptStart = 0;
static void (*gAbsMouseHook)(int, int) = nullptr;
struct PendingUp { unsigned t; int b; };
static std::vector<PendingUp> gPendingUps;

void plat_set_abs_mouse_hook(void (*hook)(int, int)) { gAbsMouseHook = hook; }

static void load_script() {
    const char *f = getenv("AE_INPUT_SCRIPT");
    if (!f) return;
    FILE *fp = fopen(f, "r");
    if (!fp) { fprintf(stderr, "cannot open input script %s\n", f); return; }
    char line[512];
    while (fgets(line, sizeof line, fp)) {
        char op[64] = "", a1[256] = "", a2[64] = "", a3[64] = "", a4[64] = "";
        unsigned t;
        if (line[0] == '#' || sscanf(line, "%u %63s", &t, op) < 2) continue;
        if (!strcmp(op, "text")) {
            char *p = strstr(line, "text") + 4;
            while (*p == ' ') p++;
            size_t n = strcspn(p, "\r\n");
            p[n] = 0;
            snprintf(a1, sizeof a1, "%s", p);
        } else {
            sscanf(line, "%u %63s %255s %63s %63s %63s", &t, op, a1, a2, a3, a4);
        }
        gScript.push_back({t, op, a1, a2, a3, a4});
    }
    fclose(fp);
    gScriptStart = SDL_GetTicks();
}

static int script_vk(const std::string &s) {
    static const struct { const char *n; int vk; } names[] = {
        {"ESC", 0x1b}, {"RET", 0x0d}, {"ENTER", 0x0d}, {"SPACE", 0x20}, {"LEFT", 0x25}, {"UP", 0x26},
        {"RIGHT", 0x27}, {"DOWN", 0x28}, {"TAB", 0x09}, {"BACK", 0x08},
    };
    for (auto &n : names) if (!strcasecmp(s.c_str(), n.n)) return n.vk;
    if ((s[0] == 'F' || s[0] == 'f') && s.size() > 1 && isdigit((unsigned char)s[1])) return 0x6f + atoi(s.c_str() + 1);
    if (s.size() == 1) return toupper((unsigned char)s[0]);
    return (int)strtol(s.c_str(), nullptr, 0);
}

static void release_all_input() {
    for (int vk = 0; vk < 256; vk++) {
        if (!gKeyDown[vk]) continue;
        gKeyDown[vk] = 0;
        if (vk > 4) gMsgs.push_back({WM_KEYUP_, (unsigned long)vk, 0});
    }
    std::lock_guard<std::mutex> lk(gMouseMutex);
    for (int b = 0; b < 3; b++) {
        if (gMouseBtn[b]) gMouseEvents.push_back({2 + b, 0, SDL_GetTicks()});
        gMouseBtn[b] = 0;
    }
}

static void set_button(int b, int down) {
    std::lock_guard<std::mutex> lk(gMouseMutex);
    gMouseBtn[b] = down;
    if (down) gMouseBtnLatch[b] = 1;
}

static void run_script() {
    if (gScript.empty() && gPendingUps.empty()) return;
    unsigned now = SDL_GetTicks() - gScriptStart;
    for (size_t i = 0; i < gPendingUps.size();) {
        if (gPendingUps[i].t <= now) { set_button(gPendingUps[i].b, 0); gPendingUps.erase(gPendingUps.begin() + i); }
        else i++;
    }
    while (gScriptPos < gScript.size() && gScript[gScriptPos].t <= now) {
        ScriptEv &e = gScript[gScriptPos++];
        if (e.op == "keydown" || e.op == "keyup") {
            int vk = script_vk(e.a1) & 0xff;
            if (e.op == "keydown") {
                if (!gKeyDown[vk]) { gKeyPressedSince[vk] = 1; gKeyPresses[vk]++; }
                gKeyDown[vk] = 1;
                gLastKeypress = vk;
                gMsgs.push_back({WM_KEYDOWN_, (unsigned long)vk, 0});
                if (e.a1.size() == 1) gMsgs.push_back({WM_CHAR_, (unsigned long)(unsigned char)e.a1[0], 0});
            } else {
                gKeyDown[vk] = 0;
                gMsgs.push_back({WM_KEYUP_, (unsigned long)vk, 0});
            }
        } else if (e.op == "key") {
            int vk = script_vk(e.a1);
            gKeyPressedSince[vk & 0xff] = 1;
            gKeyPresses[vk & 0xff]++;
            gLastKeypress = vk & 0xff;
            gMsgs.push_back({WM_KEYDOWN_, (unsigned long)vk, 0});
            if (vk == 0x08 || vk == 0x09 || vk == 0x0d || vk == 0x1b || vk == 0x20)
                gMsgs.push_back({WM_CHAR_, (unsigned long)vk, 0});
            else if (e.a1.size() == 1)
                gMsgs.push_back({WM_CHAR_, (unsigned long)(unsigned char)e.a1[0], 0});
            gMsgs.push_back({WM_KEYUP_, (unsigned long)vk, 0});
        } else if (e.op == "rawkey") {
            // a key press through SDL's own events, as a real keyboard
            // gives it (a1: key name as SDL_GetKeyFromName knows it, a2:
            // "shift" / "ctrl" held)
            SDL_Keycode k = SDL_GetKeyFromName(e.a1.c_str());
            SDL_Event ev = {};
            ev.type = SDL_KEYDOWN;
            ev.key.state = SDL_PRESSED;
            ev.key.keysym.sym = k;
            ev.key.keysym.scancode = SDL_GetScancodeFromKey(k);
            ev.key.keysym.mod = e.a2 == "shift" ? KMOD_LSHIFT : e.a2 == "ctrl" ? KMOD_LCTRL : KMOD_NONE;
            translate_event(ev);
            ev.type = SDL_KEYUP;
            ev.key.state = SDL_RELEASED;
            translate_event(ev);
        } else if (e.op == "text") {
            for (char c : e.a1) gMsgs.push_back({WM_CHAR_, (unsigned long)(unsigned char)c, 0});
        } else if (e.op == "press") {
            if (gAbsMouseHook) gAbsMouseHook(atoi(e.a1.c_str()), atoi(e.a2.c_str()));
            int b = e.a4 == "r" ? 1 : 0;
            set_button(b, 1);
            gPendingUps.push_back({now + (unsigned)atoi(e.a3.c_str()), b});
        } else if (e.op == "move" || e.op == "click") {
            if (gAbsMouseHook) gAbsMouseHook(atoi(e.a1.c_str()), atoi(e.a2.c_str()));
            if (e.op == "click") {
                int b = e.a3 == "r" ? 1 : 0;
                set_button(b, 1);
                gPendingUps.push_back({now + 60, b});
            }
        } else if (e.op == "shot") {
            write_shot(e.a1.c_str());
            fprintf(stderr, "[script] %u ms: wrote %s\n", now, e.a1.c_str());
        } else if (e.op == "quit") {
            gQuit = true;
        }
    }
}

static void pump_sdl() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) translate_event(e);
    run_script();
    if (gStageDirty) render_stage();
}

int plat_dispatch_one() {
    pump_sdl();
    if (gQuit) return -1;
    if (gMsgs.empty()) return 0;
    Msg m = gMsgs.front();
    gMsgs.pop_front();
    if (gWndProc) gWndProc(gWin, m.msg, m.w, m.l);
    return 1;
}

void plat_wait_message() {
    SDL_Event e;
    if (SDL_WaitEventTimeout(&e, 50)) translate_event(e);
    pump_sdl();
}

// ---------------------------------------------------------------- time

unsigned plat_ticks() { return SDL_GetTicks(); }
unsigned long long plat_perf_counter() { return SDL_GetPerformanceCounter(); }
unsigned long long plat_perf_freq() { return SDL_GetPerformanceFrequency(); }
void plat_sleep(unsigned ms) { SDL_Delay(ms); }

// Emulated GetTickCount. The original limits frames with busy loops on
// GetTickCount (0x41f130: at least 49 ms per frame; menus 50 ms). That
// clock advanced in steps of the system tick: 54.925 ms (the 18.2 Hz PC
// timer) on Windows 95/98, 10 ms on NT 4, 15.625 ms on later Windows, so
// the real frame rate was set by the tick. Frames land on a fixed grid, so
// the pace does not drift with the time spent drawing.
static double gTickMs = -1.0;
static std::chrono::steady_clock::time_point gTickEpoch;

static double tick_len()
{
    if (gTickMs < 0.0) {
        const char *e = getenv("AE_TICK_MS");
        gTickMs = e ? atof(e) : 65536.0 * 1000.0 / 1193182.0;
        if (gTickMs < 0.001) gTickMs = 0.001;
        gTickEpoch = std::chrono::steady_clock::now();
    }
    return gTickMs;
}

static double ms_since_epoch()
{
    tick_len();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - gTickEpoch).count();
}

void plat_set_tick_ms(double ms)
{
    tick_len();
    if (ms >= 0.001) gTickMs = ms;
}

unsigned plat_game_ticks()
{
    double t = tick_len();
    return (unsigned)(long long)(floor(ms_since_epoch() / t) * t);
}

void plat_wait_game_ticks(unsigned t0, unsigned ms)
{
    double t = tick_len();
    // the first tick edge at which the emulated clock reads t0 + ms or more
    long long k = (long long)floor(ms_since_epoch() / t);
    while ((unsigned)(long long)((double)k * t) - t0 < ms) k++;
    double target = (double)k * t;
    for (;;) {
        double left = target - ms_since_epoch();
        if (left <= 0.0) break;
        if (left > 2.0)
            std::this_thread::sleep_for(std::chrono::microseconds((long long)((left - 1.0) * 1000.0)));
        else
            std::this_thread::yield();
    }
}

// Periodic timers (timeSetEvent with TIME_PERIODIC): each runs on its own
// thread against a fixed schedule, so the rate does not drift (SDL timers
// reschedule from the time a callback ran).
struct TimerRec {
    PlatTimerProc cb;
    unsigned long user;
    unsigned period;
    std::thread th;
    std::mutex m;
    std::condition_variable cv;
    bool stop = false;
};
static std::mutex gTimerMutex;
static std::map<unsigned, TimerRec *> gTimers;
static unsigned gNextTimer = 1;

static void timer_thread(unsigned id, TimerRec *r)
{
    using clk = std::chrono::steady_clock;
    const auto period = std::chrono::milliseconds(r->period ? r->period : 1);
    auto next = clk::now() + period;
    std::unique_lock<std::mutex> lk(r->m);
    for (;;) {
        if (r->cv.wait_until(lk, next, [r] { return r->stop; })) return;
        lk.unlock();
        r->cb(id, 0, r->user, 0, 0);
        lk.lock();
        next += period;
        auto now = clk::now();
        if (now - next > period * 8) next = now;   // (after a stall: no burst of catch-up calls)
    }
}

unsigned plat_timer_start(unsigned period, PlatTimerProc cb, unsigned long user) {
    std::lock_guard<std::mutex> lk(gTimerMutex);
    unsigned id = gNextTimer++;
    TimerRec *r = new TimerRec;
    r->cb = cb; r->user = user; r->period = period;
    gTimers[id] = r;
    r->th = std::thread(timer_thread, id, r);
    return id;
}

// Like timeKillEvent: once this returns the callback is not running and
// will not run again, so the caller may free what it uses.
void plat_timer_stop(unsigned id) {
    TimerRec *r;
    {
        std::lock_guard<std::mutex> lk(gTimerMutex);
        auto it = gTimers.find(id);
        if (it == gTimers.end()) return;
        r = it->second;
        gTimers.erase(it);
    }
    {
        std::lock_guard<std::mutex> lk(r->m);
        r->stop = true;
    }
    r->cv.notify_all();
    if (r->th.get_id() == std::this_thread::get_id()) {
        r->th.detach();   // stopped from its own callback: the thread exits after it
        return;           // (the record is leaked rather than freed under it)
    }
    r->th.join();
    delete r;
}

// ---------------------------------------------------------------- debug screenshots
// AE_SHOT_EVERY=N writes every Nth presented frame to AE_SHOT_DIR as PPM.
static void write_shot(const char *name) {
    FILE *f = fopen(name, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", gW, gH);
    std::lock_guard<std::mutex> lk(gFrameMutex);
    for (int i = 0; i < gW * gH; i++) {
        uint16_t p = gStage[i];
        unsigned char rgb[3] = {(unsigned char)((p >> 11) << 3), (unsigned char)(((p >> 5) & 63) << 2), (unsigned char)((p & 31) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void maybe_screenshot() {
    static int every = -1;
    static const char *dir = nullptr;
    if (every < 0) {
        const char *e = getenv("AE_SHOT_EVERY");
        every = e ? atoi(e) : 0;
        dir = getenv("AE_SHOT_DIR");
        if (!dir) dir = ".";
    }
    static int frame = 0;
    frame++;
    if (every <= 0 || frame % every) return;
    char name[512];
    snprintf(name, sizeof name, "%s/shot_%05d.ppm", dir, gShotCount++);
    write_shot(name);
}
