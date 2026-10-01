// Status bar, screen/world coordinate conversion and screen save/restore
// (0x414e20-0x415640).
#include "game.h"

static inline bool IsPanelMode(int m) { return m == 1 || m == 3 || m == 5 || m == 2 || m == 6; }

// Port (enhanced mode): stone panels either side of the status bar, made
// from the status bar's own colours so the widescreen bottom row is closed.
static void DrawHudWings()
{
    if (!gLayout.wide() || gLayout.minX() >= 0) return;
    enum { kRows = 80 };
    static uint16_t base[kRows];
    static bool ready = false;
    uint8_t *p;
    unsigned long pitch;
    if (!gDDW.Lock(&p, &pitch)) return;
    auto px = [&](int x, int y) -> uint16_t & { return ((uint16_t *)(p + (ptrdiff_t)y * (ptrdiff_t)pitch))[x]; };
    if (!ready) {
        // mean colour of each row of the bar's brick band, a little darker
        for (int r = 0; r < kRows; r++) {
            unsigned R = 0, G = 0, B = 0, n = 0;
            for (int x = 0x60; x < 0x220; x++) {
                uint16_t c = px(x, 0x190 + r);
                R += c >> 11; G += (c >> 5) & 63; B += c & 31; n++;
            }
            R = R * 3 / (n * 4); G = G * 3 / (n * 4); B = B * 3 / (n * 4);
            base[r] = (uint16_t)((R << 11) | (G << 5) | B);
        }
        ready = true;
    }
    auto scale = [](uint16_t c, int f) -> uint16_t {   // f/256
        int r = ((c >> 11) * f) >> 8, g = (((c >> 5) & 63) * f) >> 8, b = ((c & 31) * f) >> 8;
        if (r > 31) r = 31;
        if (g > 63) g = 63;
        if (b > 31) b = 31;
        return (uint16_t)((r << 11) | (g << 5) | b);
    };
    const int w = -gLayout.minX();
    for (int side = 0; side < 2; side++) {
        int x0 = side ? 0x280 : gLayout.minX();
        for (int r = 0; r < kRows; r++) {
            for (int i = 0; i < w; i++) {
                int x = x0 + i;
                // bricks 32 x 13 with staggered joints, each a slightly different
                // tone; darker towards the screen edge and at the bar's edge
                int course = (r - 3) / 13;
                int bx = (i + (course & 1) * 16) >> 5;
                bool mortar = r < 3 || r >= kRows - 3 || (r - 3) % 13 == 0 ||
                              ((i + (course & 1) * 16) & 31) == 0;
                unsigned hb = (unsigned)((bx + side * 7) * 2654435761u ^ course * 40503u);
                unsigned hp = (unsigned)(x * 73856093u ^ r * 19349663u);
                int f = mortar ? 60 : 130 + (int)((hb >> 27) & 31) - 16 + (int)((hp >> 28) & 7) - 4;
                int edge = side ? w - 1 - i : i;          // distance from the outer edge
                if (edge < 32) f = f * (128 + edge * 4) / 256;
                int inner = side ? i : w - 1 - i;         // distance from the status bar
                if (inner < 3) f = 30;
                px(x, 0x190 + r) = scale(base[r], f);
            }
        }
    }
    gDDW.Unlock();
}

// 0x414e20
void DrawHud()
{
    if (gHudVisible) {
        gShade.SetShadeLevel(0x1f);
        gHudSprites.Blt(0, 0, 0x190, 1);
        DrawHudWings();
        DrawHudPanel(gHudFlags);
        UpdatePoisonDisplay(1);
        DrawStatusBar(gPlayer.jumpCharge);
        if (!IsPanelMode(gUIMode)) DrawActionIcon(1);
        DrawSpellIcon();
        return;
    }
    char buf[20];
    DrawActionIcon(0);
    sprintf(buf, "%d/%d", gStats.hp, gStats.maxHp);
    gText.SetColor(gColorRed, 0);
    gText.PrintRJ(0x27b, 0x1b8, buf);
    sprintf(buf, "%d/%d", gStats.mana, gStats.maxMana);
    gText.SetColor(gColorBlue, 0);
    gText.PrintRJ(0x27b, 0x1cc, buf);
}

// 0x414f50
void DrawHudPanel(int flags)
{
    int m = gUIMode;
    if (IsPanelMode(m)) {
        gHudSprites.Blt(0xb, 0x78, 0x190, 1);
        for (int i = 0, x = 0xa4; x < 0x20c; i++, x += 0x48) {
            int16_t item = gInventory[i + gInvScroll].item;
            if (item != -1) gItemIcons[gItemIconIdx[item]].Draw(x, 0x19c, gDDW);
        }
        m = gUIMode;
    }
    if (m == 4) gHudSprites.Blt(8, 0x23f, 0x190, 1);
    uint8_t f = (uint8_t)flags;
    if (f & 4) gHudSprites.Blt(7, 0x172, 0x190, 1);
    if (f & 2) gHudSprites.Blt(9, 0x200, 0x19c, 1);
    if (f & 1) gHudSprites.Blt(0xa, 0x8a, 0x19c, 1);
}

// 0x415060
void DrawActionIcon(int big)
{
    if (big)
        gHudSprites.Blt(gActionMode + 1, 0x1b8, 0x190, 1);
    else
        gHudSprites.Blt(gActionMode + 0xc, 0x212, 0x1a4, 2);
}

// 0x4150b0
void DrawSpellIcon()
{
    static const int kWeaponIcon[21] = {0x15, 0x18, 0x18, 0x18, 0x19, 0x16, 0x16, 0x16, 0x16, 0x17, 0x17,
                                        0x17, 0x17, 0x15, 0x15, 0x18, 0x18, 0x16, 0x16, 0x17, 0x17};   // (port: 0x16 was 0xb, a skull)
    static const int kSpellIcon[30] = {0, 1, 2, 3, 4, 4, 5, 0x13, 1, 7, 8, 9, 2, 0xa, 3,
                                       0xb, 1, 1, 0xc, 0xd, 0xf, 0x10, 1, 0x11, 3, 0x12, 0x13, 0x14, 0xe, 0xb};
    if (gActionMode == 0) {
        gActionSprites.Blt(kWeaponIcon[gEquip.weaponType], 0x242, 0x1b3, 1);
        return;
    }
    if (gActionMode != 1 || gPlayer.currentSpell == -1) return;
    char buf[12];
    gActionSprites.Blt(kSpellIcon[gPlayer.currentSpell], 0x242, 0x1b3, 1);
    sprintf(buf, "%d", (int)gSpellsMemorized[gPlayer.currentSpell]);
    gText.SetColor(gColorNearBlack, 0);
    gText.PrintRJ(0x27e, 0x1cd, buf);
    gText.SetColor(gColorWhite, 0);
    gText.PrintRJ(0x27d, 0x1cc, buf);
}

// 0x415320: auto-repeat while the right button is held.
uint8_t MouseRepeat(int *counter)
{
    if (*counter == -1) return 3;
    if (!MouseButtonDown(1) && !ControlsJumpHeld()) return 2;   // (port: or the jump key)
    if (*counter >= 100) {
        *counter = 0;
        return 1;
    }
    *counter += 8;
    return 1;
}

// 0x415360
void DrawStatusBar(int v)
{
    if (v <= 0) return;
    int h = (int)((double)v * 0.01f * 60.0f);
    unsigned c = v > 30 ? gDDW.MakePixel16(0, 0xff, 0) : gDDW.MakePixel16(0xff, 0, 0);
    gDDW.TintRect(0x22a, 0x1d5 - h, 0xd, h, (unsigned short)(c & 0xffff));
}

// 0x4153d0
void TileToScreen(int tx, int ty, int *sx, int *sy)
{
    WorldToScreen((float)tx, (float)ty, sx, sy);
}

// 0x415400: isometric projection of a map position.
void WorldToScreen(float x, float y, int *sx, int *sy)
{
    double dx = (double)x - gCamX;
    double dy = (double)y - gCamY;
    double d = dy - dx;
    if (gAltView) {
        int offX = gViewOffX + 0x11e;
        *sx = (int)(640.0 - (d * 32.0 + (offX + 62.0)));
        int offY = gViewOffY + 0xc8;
        *sy = (int)((dy + dx) * 16.0 + offY);
    } else {
        *sx = (int)(640.0 - (d * 32.0 + 348.0));
        *sy = (int)((dy + dx) * 16.0 + 200.0);
    }
}

// 0x4154d0: inverse of WorldToScreen (default view).
void ScreenToWorld(int sx, int sy, float *wx, float *wy)
{
    double a = (double)(sy - 200) * 0.03125f;
    double b = (double)(sx - 0x11e) * 0.016129f;
    double c = (double)(0x11e - sx) * 0.016129f;
    *wx = (float)(b + a - 0.5f);
    *wy = (float)(c + a + 0.5f);
    *wx = *wx + gCamX;
    *wy = *wy + gCamY;
}

// 0x415560: save a screen area of the back buffer.
Sprite *GrabScreen(int x, int y, int w, int h)
{
    Sprite *s = new Sprite;
    s->Grab(gDDW, x, y, w, h);
    return s;
}

// 0x415600
void RestoreScreen(int x, int y, Sprite *s)
{
    if (!s) return;
    s->Blt(gDDW, x, y);
    delete s;
}
