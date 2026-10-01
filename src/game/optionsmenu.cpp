// Port: the options screen (title screen and Esc menu): display, game,
// audio and control settings, and the key bindings. Drawn in the game's own
// style: its fonts, white labels, azure values, red under the mouse.
#include "game.h"
#include "settings.h"
#include "../platform/platform.h"
#include <functional>
#include <string>
#include <vector>

namespace {

enum RowKind { kChoice, kSlider, kButton };

struct Row {
    const char *label;
    const char *help;
    RowKind kind;
    int min, max;
    std::function<int()> get;
    std::function<void(int)> set;
    std::function<std::string(int)> text;   // value text (choices)
    std::function<void()> press;            // buttons
    bool wraps = true;
};

// Screen layout (logical 640x480 coordinates)
const int kPanelL = 40, kPanelT = 30, kPanelR = 600, kPanelB = 452;
const int kRowsT = 112, kRowH = 24;
const int kLabelX = 72, kValueR = 568;
const int kSliderW = 150;
const char *kTabs[4] = {"Display", "Game", "Audio", "Controls"};

// ---- drawing helpers ------------------------------------------------------

uint16_t Rgb(int r, int g, int b) { return (uint16_t)gDDW.MakePixel16((unsigned char)r, (unsigned char)g, (unsigned char)b); }

void BlendRect(int x, int y, int w, int h, uint16_t c, int a)   // a 0..256
{
    uint8_t *p;
    unsigned long pitch;
    if (!gDDW.Lock(&p, &pitch)) return;
    int x0 = x < gLayout.minX() ? gLayout.minX() : x, y0 = y < gLayout.minY() ? gLayout.minY() : y;
    int x1 = x + w - 1 > gLayout.maxX() ? gLayout.maxX() : x + w - 1;
    int y1 = y + h - 1 > gLayout.maxY() ? gLayout.maxY() : y + h - 1;
    const unsigned cr = c >> 11, cg = (c >> 5) & 63, cb = c & 31;
    for (int yy = y0; yy <= y1; yy++) {
        uint16_t *d = (uint16_t *)(p + (long)yy * (long)pitch);
        for (int xx = x0; xx <= x1; xx++) {
            unsigned o = d[xx];
            unsigned r = ((o >> 11) * (256 - a) + cr * a) >> 8;
            unsigned g = (((o >> 5) & 63) * (256 - a) + cg * a) >> 8;
            unsigned b = ((o & 31) * (256 - a) + cb * a) >> 8;
            d[xx] = (uint16_t)(r << 11 | g << 5 | b);
        }
    }
    gDDW.Unlock();
}

void Frame(int x, int y, int w, int h, uint16_t c)
{
    BlendRect(x, y, w, 1, c, 256);
    BlendRect(x, y + h - 1, w, 1, c, 256);
    BlendRect(x, y, 1, h, c, 256);
    BlendRect(x + w - 1, y, 1, h, c, 256);
}

// (the game's small font only has digits; everything uses the large one)
void Say(int x, int y, const std::string &s, uint16_t c, bool small = false)
{
    (void)small;
    gText.SetFont(&gFontLarge);
    gText.AntiAlias(1);
    gText.SetColor(c, 0);
    gText.Print(x, y, (char *)s.c_str());
}

int TextW(const std::string &s, bool small = false)
{
    (void)small;
    gText.SetFont(&gFontLarge);
    return gText.StringSize((char *)s.c_str());
}

void TextRJ(int r, int y, const std::string &s, uint16_t c, bool small = false) { Say(r - TextW(s, small), y, s, c, small); }
void TextC(int y, const std::string &s, uint16_t c, bool small = false) { Say(320 - TextW(s, small) / 2, y, s, c, small); }

bool MouseIn(int l, int t, int r, int b) { return gMouseX >= l && gMouseX <= r && gMouseY >= t && gMouseY <= b; }

// ---- the rows of each page ------------------------------------------------

std::string OnOff(int v) { return v ? "On" : "Off"; }

Row Toggle(const char *label, const char *help, int *v, std::function<void()> after = nullptr)
{
    Row r{label, help, kChoice, 0, 1, [v] { return *v ? 1 : 0; },
          [v, after](int n) { *v = n; if (after) after(); }, OnOff, nullptr};
    return r;
}

Row Slider(const char *label, const char *help, int min, int max, std::function<int()> get, std::function<void(int)> set)
{
    Row r{label, help, kSlider, min, max, get, set, nullptr, nullptr};
    r.wraps = false;
    return r;
}

Row Button(const char *label, const char *help, std::function<void()> press)
{
    Row r{label, help, kButton, 0, 0, nullptr, nullptr, nullptr, press};
    return r;
}

void ApplyGamma()
{
    gShade.SetGammaLevel((float)gPrefs.gamma);
    ClearFloorCache();
}

bool gInGame;
int gPage;          // 0-3 tabs, 4 key bindings
bool gRestartNote;

std::vector<Row> PageRows(int page)
{
    std::vector<Row> rows;
    PortSettings &s = gSettings;
    switch (page) {
    case 0:   // Display
        rows.push_back(Row{"Mode", "Enhanced: 960x540 widescreen shown at 1920x1080. Classic: the original 640x480. Takes effect on restart.",
                           kChoice, 0, 1, [&s] { return s.enhanced; }, [&s](int n) { s.enhanced = n; gRestartNote = true; },
                           [](int n) { return std::string(n ? "Enhanced 1080p" : "Classic 640x480"); }, nullptr});
        rows.push_back(Toggle("Hi-res 3D models", "Draws the 3D characters at full 1920x1080 resolution. Takes effect on restart.",
                              &s.hiresModels, [] { gRestartNote = true; }));
        rows.push_back(Toggle("Texture smoothing", "Smooth (bilinear) textures on the hi-res 3D models.", &s.textureFilter,
                              [&s] { SetModelFilter(s.textureFilter != 0); }));
        rows.push_back(Toggle("3dfx coloured lighting", "Coloured torch, spell and missile light as in the 3dfx version of the game (enhanced mode).",
                              &s.colorLight, [] { SettingsApplyLive(); }));
        rows.push_back(Row{"Light model", "Smooth: soft round pools of light, shadows that fade with distance. Per tile: the 3dfx look.",
                           kChoice, 0, 1, [&s] { return s.lightModel; }, [&s](int n) { s.lightModel = n; },
                           [](int n) { return std::string(n ? "Smooth" : "Per tile (3dfx)"); }, nullptr});
        rows.push_back(Toggle("Smooth light flicker", "Torch light eases between its flicker steps instead of flashing at the edges of its pool (coloured lighting).",
                              &s.smoothLight));
        rows.push_back(Row{"Shadows", "Shadows cast by the 3D models (a setting of the original game).", kChoice, 0, 2,
                           [] { return (int)gPrefs.shadows; }, [](int n) { gPrefs.shadows = n; },
                           [](int n) { static const char *t[3] = {"None", "No monster shadows", "Full"}; return std::string(t[n]); }, nullptr});
        rows.push_back(Slider("Gamma", "Brightness of the picture (also F11 / F12 in the game).", 1, 8,
                              [] { return (int)gPrefs.gamma; }, [](int n) { gPrefs.gamma = n; ApplyGamma(); }));
        rows.push_back(Toggle("Fullscreen", "Fill the screen (also Alt+Enter).", &s.fullscreen, [&s] { plat_set_fullscreen(s.fullscreen != 0); }));
        rows.push_back(Row{"Window size", "Size of the window as a multiple of the game picture.", kChoice, 1, 4,
                           [&s] { return s.windowScale; }, [&s](int n) { s.windowScale = n; plat_set_window_scale(n); },
                           [](int n) { char b[16]; snprintf(b, sizeof b, "%dx", n); return std::string(b); }, nullptr});
        rows.push_back(Row{"Scaling", "Sharp keeps the pixels square, integer steps keep them all the same size, smooth blurs them.", kChoice, 0, 2,
                           [&s] { return s.scaling; }, [&s](int n) { s.scaling = n; plat_set_scaling(n); },
                           [](int n) { static const char *t[3] = {"Sharp", "Sharp, integer steps", "Smooth"}; return std::string(t[n]); }, nullptr});
        break;
    case 1:   // Game
        rows.push_back(Row{"Game speed", "The original's frame rate depended on the Windows it ran on. The game was made for Windows 95/98.",
                           kChoice, 0, 3, [&s] { return s.tick; }, [&s](int n) { s.tick = n; SettingsApplyLive(); },
                           [](int n) { return std::string(TickName(n)); }, nullptr});
        rows.push_back(Row{"Always run", "Run everywhere instead of walking (Shift runs otherwise).", kChoice, 0, 1,
                           [] { return (int)gPrefs.alwaysRun; }, [](int n) { gPrefs.alwaysRun = n; }, OnOff, nullptr});
        rows.push_back(Row{"Hints", "The hints the game gives about the interface.", kChoice, 0, 1,
                           [] { return (int)gPrefs.hints; }, [](int n) { gPrefs.hints = n; }, OnOff, nullptr});
        rows.push_back(Toggle("Pause in the background", "Pause the game while its window is not in focus.", &s.pauseOnFocus,
                              [&s] { plat_set_pause_on_focus(s.pauseOnFocus != 0); }));
        break;
    case 2:   // Audio
        rows.push_back(Slider("Music volume", "Volume of the CD music.", 0, 10, [] { return (int)gPrefs.cdVolume; },
                              [](int n) {
                                  gPrefs.cdVolume = n;
                                  if (n <= 0) CDAudio_Stop(&gCD);
                                  CDAudio_SetVolume(&gCD, n);
                              }));
        rows.push_back(Slider("Sound volume", "Volume of the sound effects and speech.", 0, 10, [] { return (int)gPrefs.wavVolume; },
                              [](int n) { gPrefs.wavVolume = n; SetWaveVolume(n); PlaySound(5, -1, -1); }));
        break;
    case 3:   // Controls
        rows.push_back(Row{"Control scheme",
                           "Classic: as the original. Diablo style: hold to walk, click monsters to fight. WASD: move with the keys, aim with the mouse.",
                           kChoice, 0, kNumSchemes - 1, [&s] { return s.scheme; }, [&s](int n) { s.scheme = n; ControlsReset(); },
                           [](int n) { return std::string(SchemeName(n)); }, nullptr});
        if (s.scheme != kSchemeClassic)
            rows.push_back(Toggle("Camera follows", "Keep the view centred on your character (Diablo and WASD schemes).", &s.followCamera));
        if (s.scheme == kSchemeWASD)
            rows.push_back(Row{"WASD moves", "Towards the mouse: W walks to the cursor, S backs away, A and D circle it. Screen: W is up the screen.",
                               kChoice, 0, 1, [&s] { return s.wasdRelative; }, [&s](int n) { s.wasdRelative = n; },
                               [](int n) { return std::string(n ? "Towards the mouse" : "Screen directions"); }, nullptr});
        rows.push_back(Slider("Mouse speed", "Speed of the mouse cursor (also F9 in the game).", 1, 5, [] { return (int)gPrefs.mouseSpeed; },
                              [](int n) { gPrefs.mouseSpeed = (int16_t)n; SetMouseSpeed(n); }));
        rows.push_back(Button("Key bindings...", "Change the keys of the current control scheme.", [] { gPage = 4; }));
        rows.push_back(Button("Reset key bindings", "Put back the default keys of the current control scheme.",
                              [&s] { ResetBindings(s.scheme); }));
        break;
    }
    return rows;
}

std::string SchemeHelp(int scheme)
{
    switch (scheme) {
    case kSchemeDiablo:
        return "Mouse: hold left to walk, click a monster to fight it, click things to use them, right click casts, Shift+click attacks in place.";
    case kSchemeWASD:
        return "Mouse: you face the cursor, left click attacks (or uses what is under it), right click casts the current spell.";
    }
    return "Mouse: left click walks and uses things, right click does the current action (weapon, spell or jump).";
}

}   // namespace

void OptionsScreen(bool inGame)
{
    gInGame = inGame;
    gPage = 0;
    gRestartNote = false;
    // the screen as it was, and a dimmed copy to draw over
    Surface *back = gDDW.back;
    std::vector<uint16_t> orig((size_t)back->w * back->h), dim;
    for (int y = 0; y < back->h; y++) memcpy(&orig[(size_t)y * back->w], back->row(y), (size_t)back->w * 2);
    dim = orig;
    for (auto &v : dim) v = (uint16_t)((v >> 2) & 0x39e7);   // a quarter brightness
    SetCursor(0);
    ShowMouse(1);
    ResetMouseClicks();
    plat_take_keypress();
    int sel = 0, lastPage = -1;
    int capture = -1, captureSlot = 0;   // key binding being set
    bool done = false;
    const uint16_t gold = Rgb(170, 130, 60), goldDim = Rgb(90, 70, 35), grey = gColorGrey128;
    while (!done) {
        PumpMessages();
        MouseRead(&gMouseX, &gMouseY);
        if (plat_quit_requested()) break;
        if (gPage != lastPage) {
            sel = 0;
            lastPage = gPage;
        }
        std::vector<Row> rows = gPage < 4 ? PageRows(gPage) : std::vector<Row>();
        // key bindings page: the actions of the scheme
        std::vector<int> acts;
        if (gPage == 4)
            for (int a = 0; a < kNumActions; a++)
                if (ActionApplies(a)) acts.push_back(a);
        const int half = ((int)acts.size() + 1) / 2;
        const int bindRowH = 18, bindT = 94;
        auto bindCell = [&](int i, int slot, int *l, int *t, int *r, int *b) {
            int col = i / half, row = i % half;
            int x0 = 52 + col * 270;
            *t = bindT + row * bindRowH;
            *b = *t + bindRowH - 1;
            *l = x0 + 174 + slot * 48;
            *r = *l + 45;
        };

        // ---- mouse hover
        int hoverTab = -1, hoverRow = -1, hoverBind = -1, hoverSlot = 0;
        bool hoverBack = MouseIn(250, 420, 390, 444);
        if (gPage < 4) {
            for (int i = 0; i < 4; i++)
                if (MouseIn(60 + i * 132, 72, 60 + i * 132 + 124, 94)) hoverTab = i;
            for (int i = 0; i < (int)rows.size(); i++)
                if (MouseIn(kPanelL + 12, kRowsT + i * kRowH - 2, kPanelR - 12, kRowsT + i * kRowH + kRowH - 3)) hoverRow = i;
        } else {
            for (int i = 0; i < (int)acts.size(); i++)
                for (int k = 0; k < 2; k++) {
                    int l, t, r, b;
                    bindCell(i, k, &l, &t, &r, &b);
                    if (MouseIn(l, t, r, b)) {
                        hoverBind = i;
                        hoverSlot = k;
                    }
                }
        }
        if (hoverRow >= 0) sel = hoverRow;

        // ---- input
        auto change = [&](int i, int dir) {
            if (i < 0 || i >= (int)rows.size()) return;
            Row &r = rows[i];
            if (r.kind == kButton) {
                if (dir > 0) r.press();
                return;
            }
            int v = r.get() + dir;
            if (v > r.max) v = r.wraps ? r.min : r.max;
            if (v < r.min) v = r.wraps ? r.max : r.min;
            r.set(v);
        };
        if (capture >= 0) {
            int k = plat_take_keypress();
            uint8_t key[2];
            while (KeyPop(key)) {}
            if (MouseRightClicked()) k = 0x1b;
            MouseLeftClicked();
            if (k) {
                uint8_t (&b)[kNumActions][2] = gSettings.bind[gSettings.scheme];
                if (k == 0x08 || k == 0x2e) {
                    b[capture][captureSlot] = 0;   // Backspace / Delete: clear
                } else if (k != 0x1b) {
                    // a key belongs to one action: swap with its old holder
                    uint8_t old = b[capture][captureSlot];
                    for (int a = 0; a < kNumActions; a++) {
                        if (!ActionApplies(a)) continue;
                        for (int j = 0; j < 2; j++)
                            if (b[a][j] == k && !(a == capture && j == captureSlot)) b[a][j] = (a == capture) ? 0 : old;
                    }
                    b[capture][captureSlot] = (uint8_t)k;
                }
                capture = -1;
            }
        } else {
            uint8_t key[2];
            while (KeyPop(key)) {
                int vk = KeyEventToVk(key[0], key[1]);
                if (vk == 0x1b) {
                    if (gPage == 4) gPage = 3;
                    else done = true;
                } else if (vk == 0x26 && gPage < 4) {
                    sel = sel > 0 ? sel - 1 : (int)rows.size() - 1;
                } else if (vk == 0x28 && gPage < 4) {
                    sel = sel + 1 < (int)rows.size() ? sel + 1 : 0;
                } else if (vk == 0x25 && gPage < 4) {
                    change(sel, -1);
                } else if ((vk == 0x27 || vk == 0x0d || vk == 0x20) && gPage < 4) {
                    change(sel, 1);
                } else if (vk == 0x09 && gPage < 4) {
                    gPage = (gPage + 1) % 4;
                } else if (vk == 0x21 && gPage < 4) {
                    gPage = (gPage + 3) % 4;
                } else if (vk == 0x22 && gPage < 4) {
                    gPage = (gPage + 1) % 4;
                }
            }
            plat_take_keypress();
            bool left = MouseLeftClicked(), right = MouseRightClicked();
            if (left && hoverBack) {
                if (gPage == 4) gPage = 3;
                else done = true;
            } else if (left && hoverTab >= 0) {
                gPage = hoverTab;
            } else if ((left || right) && hoverRow >= 0) {
                Row &r = rows[hoverRow];
                if (r.kind == kSlider && left && gMouseX >= kValueR - kSliderW) {
                    int span = r.max - r.min;
                    int v = r.min + ((gMouseX - (kValueR - kSliderW)) * (span + 1)) / kSliderW;
                    if (v > r.max) v = r.max;
                    r.set(v);
                } else {
                    change(hoverRow, left ? 1 : -1);
                }
            } else if (left && hoverBind >= 0) {
                capture = acts[hoverBind];
                captureSlot = hoverSlot;
                plat_take_keypress();
            } else if (right && hoverBind >= 0) {
                gSettings.bind[gSettings.scheme][acts[hoverBind]][hoverSlot] = 0;
            } else if (right && gPage == 4) {
                gPage = 3;
            }
        }
        // save as we go (closing the window from here quits at once)
        {
            static std::string last;
            PortSettings &ps = gSettings;
            std::string now((const char *)&ps, sizeof ps);
            now.append((const char *)&gPrefs, sizeof gPrefs);
            if (now != last) {
                if (!last.empty()) {
                    SettingsSave();
                    SavePrefs();
                }
                last = now;
            }
        }
        if (done) break;
        if (gPage != lastPage) continue;

        // ---- draw
        for (int y = 0; y < back->h; y++) memcpy(back->row(y), &dim[(size_t)y * back->w], (size_t)back->w * 2);
        BlendRect(kPanelL, kPanelT, kPanelR - kPanelL, kPanelB - kPanelT, Rgb(8, 6, 4), 200);
        Frame(kPanelL, kPanelT, kPanelR - kPanelL, kPanelB - kPanelT, gold);
        Frame(kPanelL + 3, kPanelT + 3, kPanelR - kPanelL - 6, kPanelB - kPanelT - 6, goldDim);
        std::string help;
        if (gPage < 4) {
            TextC(kPanelT + 12, "Options", gColorWhite);
            for (int i = 0; i < 4; i++) {
                int x = 60 + i * 132;
                uint16_t c = i == gPage ? gColorAzure : (i == hoverTab ? gColorRed : grey);
                Say(x + 62 - TextW(kTabs[i]) / 2, 74, kTabs[i], c);
                if (i == gPage) BlendRect(x + 12, 94, 100, 1, gColorAzure, 256);
            }
            BlendRect(kPanelL + 12, 100, kPanelR - kPanelL - 24, 1, goldDim, 256);
            for (int i = 0; i < (int)rows.size(); i++) {
                Row &r = rows[i];
                int y = kRowsT + i * kRowH;
                bool hot = i == sel;
                if (hot) BlendRect(kPanelL + 12, y - 2, kPanelR - kPanelL - 24, kRowH - 1, Rgb(60, 40, 20), 120);
                Say(kLabelX, y, r.label, hot ? gColorRed : gColorWhite);
                if (r.kind == kChoice) {
                    std::string v = r.text(r.get());
                    TextRJ(kValueR, y, v, gColorAzure);
                } else if (r.kind == kSlider) {
                    int n = r.max - r.min + 1, v = r.get() - r.min;
                    int bw = kSliderW / n;
                    for (int k = 0; k < n; k++) {
                        int x = kValueR - kSliderW + k * bw;
                        if (k <= v) BlendRect(x + 1, y + 4, bw - 3, 11, gColorAzure, 256);
                        else Frame(x + 1, y + 4, bw - 3, 11, grey);
                    }
                    char b[8];
                    snprintf(b, sizeof b, "%d", r.get());
                    TextRJ(kValueR - kSliderW - 10, y, b, gColorAzure);
                }
                if (hot) help = r.help;
            }
            if (gRestartNote && help.empty()) help = "Some changes take effect when the game is restarted.";
            if (gPage == 3 && sel == 0) help = SchemeHelp(gSettings.scheme);
        } else {
            std::string title = std::string("Key bindings: ") + SchemeName(gSettings.scheme);
            TextC(kPanelT + 12, title, gColorWhite);
            TextC(kPanelT + 38, "Click a key to change it, right click clears it.", grey);
            for (int i = 0; i < (int)acts.size(); i++) {
                int a = acts[i];
                int l, t, r, b;
                bindCell(i, 0, &l, &t, &r, &b);
                int col = i / half;
                Say(52 + col * 270, t + 1, kActions[a].name, gColorWhite);
                for (int k = 0; k < 2; k++) {
                    bindCell(i, k, &l, &t, &r, &b);
                    bool hot = hoverBind == i && hoverSlot == k;
                    bool cap = capture == a && captureSlot == k;
                    if (cap || hot) BlendRect(l, t, r - l + 1, b - t + 1, Rgb(60, 40, 20), 160);
                    Frame(l, t, r - l + 1, b - t + 1, cap ? gColorRed : goldDim);
                    std::string kn = cap ? "?" : KeyName(gSettings.bind[gSettings.scheme][a][k]);
                    Say(l + (r - l + 1) / 2 - TextW(kn) / 2, t + 1, kn, cap ? gColorRed : (hot ? gColorRed : gColorAzure));
                }
            }
            if (capture >= 0) help = std::string("Press a key for ") + kActions[capture].name + " - Esc cancels, Backspace clears";
            else help = "Keys 0-9 (spell hot keys) and Esc (the menu) cannot be changed.";
        }
        // help line (wrapped) and Back
        if (!help.empty()) {
            std::vector<std::string> lines;
            std::string line;
            size_t p = 0;
            while (p < help.size()) {
                size_t q = help.find(' ', p);
                std::string w = help.substr(p, q == std::string::npos ? std::string::npos : q - p);
                std::string t = line.empty() ? w : line + " " + w;
                if (TextW(t) > kPanelR - kPanelL - 40 && !line.empty()) {
                    lines.push_back(line);
                    line = w;
                } else {
                    line = t;
                }
                p = q == std::string::npos ? help.size() : q + 1;
            }
            if (!line.empty()) lines.push_back(line);
            int y0 = 412 - 18 * (int)lines.size();
            for (size_t i = 0; i < lines.size(); i++) TextC(y0 + 18 * (int)i, lines[i], grey);
        }
        TextC(422, gPage == 4 ? "Back (Esc)" : (inGame ? "Return to game (Esc)" : "Back (Esc)"), hoverBack ? gColorRed : gColorWhite);
        gDDW.UpdateScreen();
        plat_sleep(15);
    }
    // restore the screen, keep the settings
    for (int y = 0; y < back->h; y++) memcpy(back->row(y), &orig[(size_t)y * back->w], (size_t)back->w * 2);
    gText.SetFont(&gFontLarge);
    gText.AntiAlias(1);
    gText.SetColor(gColorWhite, 0);
    ResetMouseClicks();
    SettingsSave();
    SavePrefs();
    gDDW.UpdateScreen();
}
