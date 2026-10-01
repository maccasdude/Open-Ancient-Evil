// Static and animated lighting, line of sight, wall shape fixing, trap
// sparkles and a few menu initialisers (0x419010-0x41a8e0).
#include "game.h"

// Precomputed per-light tile lists, one per animation phase (0x57ecd4 and
// 0x542340 in the original).
struct LightNode {
    uint8_t x, y, level;
    LightNode *next;
};
static LightNode *gLightLists[750][16];
static int32_t gLightRects[750][16][4];

// Port: the 3dfx build keeps its light maps as 555 colours. The enhanced
// mode keeps the software intensity maps for the game logic and builds
// these alongside them for drawing.
bool gColorLight = false;
uint16_t gBaseRGB[4550], gDynRGB[4550], gLightRGB[4550];

// 0x419150 (3dfx): add level * (r,g,b) to a colour, each channel clamped at 31.
uint16_t AddLightColor(uint16_t c, float level, float r, float g, float b)
{
    float cr = (float)((c >> 10) & 0x1f) + r * level;
    float cg = (float)((c >> 5) & 0x1f) + g * level;
    float cb = (float)(c & 0x1f) + b * level;
    if (cr > 31.0f) cr = 31.0f;
    if (cg > 31.0f) cg = 31.0f;
    if (cb > 31.0f) cb = 31.0f;
    return (uint16_t)(((int)cr & 0x1f) << 10 | ((int)cg & 0x1f) << 5 | ((int)cb & 0x1f));
}

// 0x419220 (3dfx): average of four colours.
uint16_t AverageLightColor(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
    int r = (((a >> 10) & 31) + ((b >> 10) & 31) + ((c >> 10) & 31) + ((d >> 10) & 31)) >> 2;
    int g = (((a >> 5) & 31) + ((b >> 5) & 31) + ((c >> 5) & 31) + ((d >> 5) & 31)) >> 2;
    int bl = ((a & 31) + (b & 31) + (c & 31) + (d & 31)) >> 2;
    return (uint16_t)(r << 10 | g << 5 | bl);
}

// Shade table for drawing on a tile: its light colour, or its level.
void SetTileShade(int idx)
{
    if (gColorLight)
        gShade.SetShadeColor(gLightRGB[idx]);
    else
        gShade.SetShadeLevel(gLightMap[idx]);
}

// 0x4180d0 (3dfx) / 0x419010: light from wall torches (map tiles 2 and 10).
void BuildBaseLight()
{
    memset(gBaseLight, 0, sizeof(uint16_t) * 4550);
    memset(gBaseRGB, 0, sizeof gBaseRGB);
    for (int x = 0; x < gMapWidth; x++) {
        for (int y = 0; y < gMapHeight; y++) {
            uint8_t t = gLevelMap[gMapRow[y] + x];
            if (t != 2 && t != 0xa) continue;
            for (int i = x - 3; i < x + 3; i++) {
                for (int j = y - 3; j < y + 3; j++) {
                    if (i * 65 <= 0 || j <= 0 || x >= 0x46 || y >= 65) continue;
                    float fi = (float)i, fj = (float)j, fx = (float)x, fy = (float)y;
                    // (the original passes the target with x and y swapped)
                    if (!LineOfSight(fx + 0.5f, fy + 0.5f, fj + 0.5f, fi + 0.5f, 0)) continue;
                    int idx = i * 65 + j;
                    int cur = gBaseLight[idx];
                    float d = Distance(fi, fj, fx, fy);
                    int v = LightFalloff(0x14, 3.0f, d);
                    if (v <= 0) continue;
                    // 3dfx: warps (2) glow magenta, tile 10 red
                    if (t == 2)
                        gBaseRGB[idx] = AddLightColor(gBaseRGB[idx], (float)v, 1.0f, 0.0f, 1.0f);
                    else
                        gBaseRGB[idx] = AddLightColor(gBaseRGB[idx], (float)v, 1.0f, 0.0f, 0.0f);
                    cur += v;
                    if (cur < 0) cur = 0;
                    if (cur > 0x1f) cur = 0x1f;
                    gBaseLight[idx] = (uint8_t)cur;
                }
            }
        }
    }
}

static const int kLevel[16] = {9, 10, 13, 15, 17, 19, 21, 23, 25, 26, 27, 28, 29, 30, 31, 32};
int LightLevelOf(int i) { return kLevel[i & 15]; }

// 0x419200: build the lit tile lists of every light for its 16 flicker phases.
void BuildLightLists()
{
    static const float kFlicker[2][16] = {
        {-0.5f, -0.8f, -1.0f, -0.6f, -0.8f, -1.0f, -0.7f, -0.5f, -0.3f, -0.7f, -1.0f, -0.7f, -0.2f, -0.6f, -0.2f, 0.0f},
        {-0.5f, -0.3f, -0.2f, -0.5f, -1.0f, -0.5f, -0.1f, -0.5f, -1.0f, -0.4f, -0.5f, -1.0f, -0.4f, -0.8f, -0.2f, 0.0f}};
    for (int i = gLightFirst < 0 ? 0 : gLightFirst; i <= gLightLast; i++) {   // (-1: none)
        Light &l = gLights[i];
        if (l.f0 != gCurLevel) continue;
        int level = kLevel[(int16_t)(l.fc & 0xffff)];
        int range = (int16_t)(l.fc >> 16);
        float lx = (float)l.f4 + 0.5f, ly = (float)l.f8 + 0.5f;
        for (int phase = 0; phase < 16; phase++) {
            int minX = 0x46, minY = 0x41, maxX = 0, maxY = 0;
            LightNode **tail = &gLightLists[i][phase];
            while (*tail) tail = &(*tail)->next;
            for (int y = 0; y < gMapHeight; y++) {
                for (int x = 0; x < gMapWidth; x++) {
                    if (gLevelMap[gMapRow[y] + x] > 0x1c) continue;
                    float rr = (float)range + kFlicker[i % 2][phase];
                    float fx = (float)x, fy = (float)y;
                    float d = Distance(fx, fy, lx, ly);
                    if (!(d < rr)) continue;
                    if (!LineOfSight(lx, ly, fx + 0.5f, fy + 0.5f, 0)) continue;
                    int v = LightFalloff((int16_t)level, rr, d);
                    if (v <= 0) continue;
                    LightNode *n = new LightNode;
                    if (!n) FatalError("no mem for Light level list");
                    n->x = (uint8_t)x;
                    n->y = (uint8_t)y;
                    n->level = (uint8_t)v;
                    n->next = nullptr;
                    *tail = n;
                    tail = &n->next;
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
            int32_t *r = gLightRects[i][phase];
            r[0] = minX;
            r[1] = minY;
            r[2] = maxX;
            r[3] = maxY;
        }
    }
}

// 0x4196e0
void FreeLightLists()
{
    for (int i = gLightFirst < 0 ? 0 : gLightFirst; i < gLightLast; i++) {
        for (int p = 0; p < 16; p++) {
            LightNode *n = gLightLists[i][p];
            while (n) {
                LightNode *next = n->next;
                delete n;
                n = next;
            }
            gLightLists[i][p] = nullptr;
        }
    }
}

// 0x419740: rebuild the dynamic light map for this frame.
void UpdateLighting()
{
    if (gCurLevel == 22) {
        // the pulsating level
        int v = gPulseLight + gPulseDir;
        gPulseLight = v;
        if (v >= 0x14) {
            gPulseDir = -1;
            v = 0x14;
            gPulseLight = v;
        } else if (v <= 4) {
            gPulseDir = 1;
            v = 4;
            gPulseLight = v;
        }
        for (int y = 0; y < gMapHeight; y++)
            for (int x = 0; x < gMapWidth; x++) {
                gDynLight[x * 65 + y] = (uint16_t)v;
                gDynRGB[x * 65 + y] = GreyLight(v);
            }
        AddPlayerLight();
        return;
    }
    memcpy(gDynLight, gBaseLight, 0x8e3 * 4);
    if (gColorLight) memcpy(gDynRGB, gBaseRGB, sizeof gDynRGB);
    if (gLightFirst != -1 && gLightLast != -1) {
        int phase = gTicks & 0xf;
        for (int i = gLightFirst < 0 ? 0 : gLightFirst; i <= gLightLast; i++) {   // (-1: none)
            if (!LightInView(gLightRects[i][phase])) continue;
            for (LightNode *n = gLightLists[i][phase]; n; n = n->next) {
                uint16_t &d = gDynLight[n->x * 65 + n->y];
                d += n->level;
                if (d > 0x1f) d = 0x1f;
                if (gColorLight) {   // 3dfx: warm torch light
                    uint16_t &c = gDynRGB[n->x * 65 + n->y];
                    c = AddLightColor(c, (float)n->level, 1.0f, 0.87f, 0.75f);
                }
            }
        }
    }
    AddPlayerLight();
}

// 0x4198a0: range of zone table entries for the current level (unused result).
void FindZoneRange()
{
    int cur = gCurLevel;
    int i = 0;
    gZoneFirst = 0;
    gZoneEnd = 0;
    if (gZoneTable[0] != cur) {
        for (;;) {
            i++;
            int lv = gZoneTable[i * 5];
            if (lv == -1) {
                gZoneFirst = i;
                return;
            }
            if (lv == cur) break;
        }
        gZoneFirst = i;
    }
    gZoneEnd = i;
    if (gZoneTable[i * 5] != cur) return;
    while (gZoneTable[(i + 1) * 5] == cur) i++;
    gZoneEnd = i + 1;
}

// 0x419920 (the flag is ignored)
int LineOfSight(float x0, float y0, float x1, float y1, int flag)
{
    (void)flag;
    if (x0 == x1 && y0 == y1) return 1;
    float adx = fabsf(x1 - x0), ady = fabsf(y1 - y0);
    if (ady < adx) return LineOfSightX(x0, y0, x1, y1);
    return LineOfSightY(x0, y0, x1, y1);
}

// 0x4199b0: x-major DDA in 16.16 fixed point.
int LineOfSightX(float x0, float y0, float x1, float y1)
{
    float adx = fabsf(x1 - x0);
    float dy = y1 - y0;
    float slope = adx == 0.0f ? 0.0f : dy / adx;
    int ix = (int)x0;
    float fx = (float)ix;
    float frac = x0 - fx;
    float ys = (float)((double)slope * frac + y0);
    int step = fx < x1 ? 1 : -1;
    int count = abs((int)x1 - (int)fx);
    int yfix = (int)lrint((double)ys * 65336.0f);   // sic, not 65536
    int slopefix = (int)lrint((double)slope * 65336.0f);
    int x = ix;
    uint8_t w = (uint8_t)gMapWidth;
    for (; count > 0; count--) {
        unsigned row = (unsigned)yfix >> 16;
        unsigned idx = (uint16_t)((row & 0xff) * w) + x;
        if (gLevelMap[idx] >= 0x1a) return 0;
        x += step;
        yfix += slopefix;
    }
    return 1;
}

// 0x419ad0: y-major DDA in floating point.
int LineOfSightY(float x0, float y0, float x1, float y1)
{
    double dx = (double)x1 - x0;
    double ady = fabs((double)y1 - y0);
    double slope = ady == 0.0 ? 0.0 : dx / ady;
    int iy = (int)y0;
    double fy = (double)iy;
    double xs = ((double)y0 - fy) * slope + x0;
    double step = fy < y1 ? 1.0 : -1.0;
    int count = abs((int)y1 - (int)fy);
    for (int n = 0; n < count; n++) {
        int y = (int)fy;
        int x = (int)xs;
        if (gLevelMap[gMapRow[y] + x] >= 0x1a) return 0;
        xs += slope;
        fy += step;
    }
    return 1;
}

static inline bool Solid(uint8_t c) { return c >= 0x1a && c != 0xff; }

// 0x419bd0: pick wall tile shapes from their neighbours.
void FixWallTiles()
{
    for (int y = 0; y < gMapHeight; y++) {
        for (int x = 0; x < gMapWidth; x++) {
            uint8_t *m = gLevelMap;
            int off = gMapRow[y] + x;
            uint8_t c = m[off];
            if (c < 0x1e || c == 0xff) continue;
            int down = gMapRow[y + 1] + x, up = gMapRow[y - 1] + x;
            bool L = Solid(m[off - 1]), R = Solid(m[off + 1]);
            bool D = Solid(m[down]), U = Solid(m[up]);
            bool UR = Solid(m[up + 1]), UL = Solid(m[up - 1]);
            bool DR = Solid(m[down + 1]), DL = Solid(m[down - 1]);
            unsigned h = L + R, v = D + U;
            int t = -1;
            if (U && D && R && !L && !UR && !DR) t = 0x24;
            else if (U && D && L && !R && !DL && !UL) t = 0x25;
            else if (R && L && U && !D && !UR && !UL) t = 0x26;
            else if (R && L && D && !U && !DR && !DL) t = 0x27;
            else if (D && R && !DR) t = 0x22;
            else if (D && L && !DL) t = 0x23;
            else if (U && R && !UR) t = 0x20;
            else if (U && L && !UL) t = 0x21;
            else if (v < h) t = 0x1e;
            else if (v > h) t = 0x1f;
            if (t >= 0) SetMapCell(x, y, 3, (uint8_t)t);
        }
    }
}

// 0x419ee0
uint8_t TileLight(int x, int y)
{
    return (uint8_t)gLightMap[x * 65 + y];
}

// 0x419f00: is a light's bounding box near the camera?
int LightInView(const int32_t *r)
{
    // 13 tiles covers the 640x480 view; the widescreen view needs more, or
    // lights near its edges switch on and off as the camera moves
    const float m = gLayout.wide() ? 22.0f : 13.0f;
    int a = (int)(gCamY - m);
    int b = (int)(gCamX - m);
    int c = (int)(gCamY + m);
    int d = (int)(gCamX + m);
    return r[0] <= d && r[2] >= b && r[1] <= c && r[3] >= a;
}

// 0x419f80: start a sparkle marking a detected trap / secret.
void DetectTrap(int x, int y, int longer)
{
    int free = -1;
    for (int i = 0; i < 20; i++) {
        Sparkle &s = gSparkles[i];
        if (!s.active) free = i;
        else if (s.x == x && s.y == y) return;
    }
    if (free == -1) return;
    Sparkle &s = gSparkles[free];
    s.x = x;
    s.y = y;
    s.timer = longer ? 50 : 10;
    s.active = 1;
}

// 0x419ff0
void DrawSparkles()
{
    for (int i = 0; i < 20; i++) {
        Sparkle &s = gSparkles[i];
        if (!s.active) continue;
        TileToScreen(s.x, s.y, &s.sx, &s.sy);
        FGObject o;
        memset(&o, 0, sizeof(o));
        o.x0 = s.sx + 0x18;
        o.y0 = s.sy - gSparkleBounce[s.timer % 10];
        o.obj = &gSparkleSprites[0];
        o.depth = ScreenDepth((float)s.x + 0.5f, (float)s.y + 0.5f);
        o.rect = nullptr;
        o.light = 0x1f;
        o.type = 0;
        AddFGObject(&o);
        if (--s.timer <= 0) s.active = 0;
    }
}

static void SetMenuItem(MenuItem *m, uint16_t color, int hotkey, uint16_t hi, int id, int x, int y,
                        int l, int t, int r, int b)
{
    m->color = color;
    m->hotkey = (int8_t)hotkey;
    m->hiColor = hi;
    m->id = id;
    m->x = x;
    m->y = y;
    m->left = l;
    m->top = t;
    m->right = r;
    m->bottom = b;
}

// 0x41a0c0 (static initialiser)
void StaticInit_CastMenu()
{
    MenuItem *m = gCastMenu;
    SetMenuItem(&m[0], gColorGrey128, 0x1b, gColorAzure, -1, 0x17c, 0xbe, 0x168, 0xbe, 0x26c, 0xd1);
    SetMenuItem(&m[1], gColorBlue, 'C', gColorAzure, -10, 0x17c, 0xd2, 0x168, 0xd2, 0x26c, 0xe5);
    for (int i = 2; i < 8; i++) {
        int y = 0xe6 + (i - 2) * 0x14;
        SetMenuItem(&m[i], gColorWhite, '1' + i - 2, gColorAzure, -i, 0x17c, y, 0x168, y, 0x26c, y + 0x13);
    }
    for (int i = 1; i < 8; i++) m[i].text = nullptr;
}

// 0x41a390 (static initialiser)
void StaticInit_SpellLevelMenu()
{
    MenuItem *m = gSpellLevelMenu;
    SetMenuItem(&m[0], gColorWhite, 0x1b, gColorAzure, -1, 0x159, 0xb9, 0x159, 0xb9, 0x26c, 0xd1);
    for (int i = 1; i < 7; i++) {
        int y = 0xe6 + (i - 1) * 0x14;
        SetMenuItem(&m[i], gColorWhite, '1' + i - 1, gColorAzure, -(i + 1), 0x17c, y, 0x168, y, 0x26c, y + 0x13);
        m[i].text = nullptr;
    }
}

// 0x41a600 (static initialiser)
void StaticInit_ActionMenu()
{
    static const char kKeys[8] = {0x1b, 'I', 'L', 'T', 'S', 'M', 'N', 'R'};
    MenuItem *m = gActionMenu;
    SetMenuItem(&m[0], gColorWhite, 0x1b, gColorAzure, -1, 0x168, 0xa0, 0x168, 0xa0, 0x26c, 0xb3);
    SetMenuItem(&m[1], gColorWhite, 'I', gColorAzure, 1, 0x168, 0xb4, 0x168, 0xb4, 0x26c, 0xc7);
    for (int i = 2; i < 8; i++)
        SetMenuItem(&m[i], gColorWhite, kKeys[i], gColorAzure, i, 0x168, 0xc8, 0x168, 0xc8, 0x26c, 0xdb);
    for (int i = 1; i < 8; i++) m[i].text = nullptr;
}

// 0x41a880: rune inventory (two rows of 13).
void DrawRunes(int y0, int y1)
{
    for (int i = 0; i < 26; i++) {
        if (!gRuneCounts[i]) continue;
        int x = (i % 13) * 28 + 0x8e;
        gRuneSprites[i].Draw(x, i < 13 ? y0 : y1, gDDW);
    }
}
