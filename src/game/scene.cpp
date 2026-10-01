// The map view: visibility flood fill, per level object ranges, floor
// tiles, walls, doors, items, decals, animated tiles and objects
// (0x415640-0x417700).
#include "game.h"

#define TILE(x, y) gLevelMap[gMapRow[(y)] + (x)]
#define VIS(x, y) gMapVis[(x) * 65 + (y)]

// 0x415640
void FindFeatureRange()
{
    int lvl = gCurLevel;
    gFeatFirst = 0;
    gFeatLast = 0x36a;
    for (int i = 0; i < 875; i++)
        if (gFeatures[i].active && gFeatures[i].level == lvl) {
            gFeatFirst = i;
            break;
        }
    for (int i = 0x36a; i >= 0; i--)
        if (gFeatures[i].active && gFeatures[i].level == lvl) {
            gFeatLast = i;
            break;
        }
}

// 0x4156b0
void FindItemRange()
{
    int lvl = gCurLevel;
    gItemFirst = 0;
    gItemLast = 0x5dc;
    for (int i = 0; i < 1500; i++)
        if (gLevelItems[i].active && gLevelItems[i].level == lvl + 1) {
            gItemFirst = i;
            break;
        }
    for (int i = 0x5db; i >= 0; i--)
        if (gLevelItems[i].active && gLevelItems[i].level == lvl + 1) {
            gItemLast = i;
            break;
        }
}

// 0x415730
void FindObjRange()
{
    int lvl = gCurLevel;
    int first = 0, last = 0xc7;
    gObjFirst = 0;
    gObjLast = 0xc7;
    for (int i = 0; i < 200; i++)
        if (gLevelObjs[i].active && gLevelObjs[i].level == lvl) {
            gObjFirst = first = i;
            break;
        }
    for (int i = 0xc7; i >= 0; i--)
        if (gLevelObjs[i].active && gLevelObjs[i].level == lvl) {
            gObjLast = last = i;
            break;
        }
    if (first == 0 && last == 0xc7) {
        gObjLast = -1;
        gObjFirst = -1;
    }
}

// 0x4157c0
void FindLightRange()
{
    int lvl = gCurLevel;
    int first = 0, last = 0x2ed;
    gLightFirst = 0;
    gLightLast = 0x2ed;
    for (int i = 0; i < 200; i++)   // (sic: the forward scan stops after 200 entries)
        if (gLights[i].f0 == lvl) {
            gLightFirst = first = i;
            break;
        }
    for (int i = 0x2ed; i >= 0; i--)
        if (gLights[i].f0 == lvl) {
            gLightLast = last = i;
            break;
        }
    if (first == 0 && last == 0x2ed) {
        gLightLast = -1;
        gLightFirst = -1;
    }
}

// 0x415840
void FindDoorRange()
{
    int lvl = gCurLevel;
    gDoorFirst = 0;
    gDoorLast = 0x18f;
    for (int i = 0; i < 400; i++)
        if (gDoors[i].active && gDoors[i].level == lvl + 1) {
            gDoorFirst = i;
            break;
        }
    for (int i = 0x18f; i >= 0; i--)
        if (gDoors[i].active && gDoors[i].level == lvl + 1) {
            gDoorLast = i;
            break;
        }
}

// 0x4158c0
void ResetScene()
{
    for (int i = 0; i < kMaxVisItems; i++) gVisItems[i].idx = -1;
    for (int i = 0; i < kMaxVisFeatures; i++) gVisFeatures[i].active = 0;
    for (int i = 0; i < kMaxVisDoors; i++) gVisDoors[i].active = 0;
    gWallCount = 0;
    gTileCount = 0;
    gNumVisItems = 0;
}

// 0x415910: work out what can be seen from (x,y) and collect it.
void ComputeView(int x, int y, int secret)
{
    ResetScene();
    do {
        memset(gMapVis, 0xff, 4550);
        memset(gDecalMap, 0xff, sizeof(gDecalMap));
        gVisRestart = 0;
        gWallCount = 0;
        gTileCount = 0;
        gNumVisItems = 0;
        FloodView(x, y, secret);
        secret--;
    } while (gVisRestart);
    ComputeLighting();
    CollectItems();
    CollectDoors();
    CollectFeatures();
    g_5c5824 = 0;
}

// 0x4159a0: draw the map view.
void RenderView()
{
    gDDW.SetClippingWindowSize(0, 0, 0x27f, gViewHeight);
    gClip.SetViewport(ViewL(), ViewT(), ViewR() + (gLayout.wide() ? 1 : 0), gViewHeight);   // (port: the clipper's right edge is exclusive)
    uint8_t *p;
    unsigned long pitch;
    if (!gDDW.Lock(&p, &pitch)) return;
    DrawFloorTiles();
    UpdateAnimTiles();
    DrawDecals();
    DrawItems();
    gDDW.Unlock();
    DrawWalls();
    DrawDoors();
    DrawFeatures();
    gClip.SetViewport(0, 0, 0x27f, 0x1df);
}

// 0x415a40: recursive flood fill over the tiles visible on screen.
void FloodView(int x, int y, int secret)
{
    if (x < 0 || y < 0 || x >= gMapWidth || y >= gMapHeight) return;
    if (VIS(x, y) < 0xfe) return;
    int sx, sy;
    WorldToScreen((float)x, (float)y, &sx, &sy);
    if (sy < ViewT() - 0x20 || sy > gViewHeight + 0x64) return;
    if (sx < ViewL() - 0x80 || sx > ViewR() + 0x3f) return;
    uint8_t tile = TILE(x, y);
    if (tile == 0xff) return;
    AddTile(x, y, sx, sy, tile);
    if (tile == 0x1c || tile == 0x1d) {
        // secret door: found when searching
        if (!secret) return;
        SetMapCell(x, y, 3, 0);
        SetMapCell(x, y, 2, 0xff);
        RemoveWall(x, y);
        ShowMessage(gMsg[20] /* You have found a secret door!!! */, gColorAzure);
        GainExperience(100);
        gVisRestart = 1;
        return;
    }
    if (tile >= 0x1a) return;
    FloodView(x + 1, y, secret);
    FloodView(x - 1, y, secret);
    FloodView(x, y - 1, secret);
    FloodView(x, y + 1, secret);
    FloodView(x + 1, y - 1, secret);
    FloodView(x - 1, y + 1, secret);
    FloodView(x - 1, y - 1, secret);
    FloodView(x + 1, y + 1, secret);
}

// 0x415c20
int AddTile(int x, int y, int sx, int sy, int tile)
{
    uint8_t overlay = gOverlayLayer[gMapRow[y] + x];
    if (gTileCount >= kMaxTiles) {
        gDDW.Flip2GDI();
        char buf[64];
        sprintf(buf, "Reached MAX_TILES : %d\n", gTileCount);
        plat_message_box(buf, "Warning");
        return 0;
    }
    if (x >= 0 && y >= 0 && x < gMapWidth && y < gMapHeight) {
        VIS(x, y) = (uint8_t)tile;
        if (overlay != 0xff) {
            TileEntry *t = &gTiles[gTileCount];
            t->overlay = overlay;
            t->x = x;
            t->y = y;
            t->sx = sx;
            t->sy = sy;
            gTileCount++;
            AddWallSprite(x, y);
        }
        // Thieves spot traps.
        if (gStats.charClass == 3 && (unsigned)gFrameCounter % 5 == 0) {
            uint8_t t = TILE(x, y);
            if ((t == 9 || t == 0xb) && rand() % 20 < gStats.intel &&
                LineOfSight((float)x + 0.5f, (float)y + 0.5f, gPlayer.x, gPlayer.y, 0))
                DetectTrap(x, y, 0);
        }
    }
    return 1;
}

// 0x415db0
void AddWallSprite(int x, int y)
{
    if (gWallCount >= kMaxWalls) FatalError("too many walls", 0);
    unsigned w = gWallLayer[gMapRow[y] + x];
    if (w == 0xff) return;
    WallEntry *e = &gWalls[gWallCount];
    float fy = (float)y, fx = (float)x;
    e->sprite = w & 0x7f;
    WorldToScreen(fx, fy, &e->sx, &e->sy);
    int h = gWallSprites[w].Height();
    e->y = y;
    e->sy = e->sy + (0x20 - h);
    e->x = x;
    e->depth = ScreenDepth(fx + 0.1f, fy + 0.1f);
    e->f20 = 0;
    gWallCount++;
}

// Port (enhanced mode): the light that is drawn eases towards the game's
// light instead of jumping: the torch flicker (a new random level every
// other frame) and the flickering ranges of the wall lights made the edges
// of the light pool flash. The game logic keeps the unsmoothed levels.
template <int N> struct SmoothBank {
    uint16_t s[N][3];      // light per channel in 1/256 levels
    uint16_t p[N][3];
    uint32_t frame[N];
    int level = -1;
    uint32_t last = 0;
};
static SmoothBank<4550> gSmoothTiles;
static SmoothBank<kVertStride * 71> gSmoothVerts;

// t: target per channel in 1/256 levels (0..31*256); returns the eased value
template <int N> static void SmoothStep(SmoothBank<N> &b, int c, const int *t, int *out)
{
    const uint32_t frame = gFrameCounter + 1;   // (0 marks 'never')
    if (b.level != gCurLevel || frame < b.last) {   // (new level, or a loaded game)
        memset(b.frame, 0, sizeof b.frame);
        b.level = gCurLevel;
    }
    b.last = frame;
    uint16_t *s = b.s[c], *p = b.p[c];
    if (!gSettings.smoothLight) {
        for (int k = 0; k < 3; k++) out[k] = t[k];
        return;
    }
    if (b.frame[c] == frame) {
        // a second pass in the same frame: redo this frame's step
        for (int k = 0; k < 3; k++) s[k] = p[k];
    } else if (b.frame[c] + 3 < frame || !b.frame[c]) {
        // not seen lately: start at the light it has now
        for (int k = 0; k < 3; k++) s[k] = p[k] = (uint16_t)t[k];
        b.frame[c] = frame;
    } else {
        for (int k = 0; k < 3; k++) p[k] = s[k];
    }
    for (int k = 0; k < 3; k++) {
        int v = s[k] + ((t[k] - (int)s[k]) * 2) / 5;   // 40% of the way per step
        if (abs(t[k] - v) < 24) v = t[k];
        s[k] = (uint16_t)v;
        out[k] = v;
    }
    b.frame[c] = frame;
}

static uint16_t Pack555(const int *v)
{
    int r = (v[0] + 128) >> 8, g = (v[1] + 128) >> 8, b = (v[2] + 128) >> 8;
    return (uint16_t)((r > 31 ? 31 : r) << 10 | (g > 31 ? 31 : g) << 5 | (b > 31 ? 31 : b));
}

uint16_t SmoothLight(int c, uint16_t target)
{
    if (!gSettings.smoothLight) return target;
    const int t[3] = {((target >> 10) & 31) << 8, ((target >> 5) & 31) << 8, (target & 31) << 8};
    int o[3];
    SmoothStep(gSmoothTiles, c, t, o);
    return Pack555(o);
}

// ---------------------------------------------------------------------------
// Port (enhanced mode): the smooth light model. The 3dfx build lights whole
// tiles: every tile takes the light at its centre, the floor blends those
// between its corners (shifted half a tile), distances are cut to whole
// tiles and the light stops dead at the end of its range. That gives the
// pools of light stepped, diamond shaped rims. Here the light is worked out
// at the tile corners (and centres, for walls and objects) from the real
// distance to every light with a soft falloff, walls shade a corner only as
// far as they hide the tiles around it, and bright light rolls off instead
// of clipping. The game logic keeps the original light levels (gLightMap).
uint16_t gVertRGB[kVertStride * 71];   // light at tile corner (x, y): x * kVertStride + y
static bool gVertValid;                // gVertRGB holds this frame's light
static float gAccC[4550][3];           // light at tile centres
static float gAccV[kVertStride * 71][3];
static int gViewX0, gViewY0, gViewX1, gViewY1;   // tiles being drawn (+ margin)

bool SmoothLightModel()
{
    return gColorLight && gSettings.lightModel && gCurLevel != 0x16 && gVertValid;
}

// Light at distance t (0..1 of the reach): bright near the flame, a long
// soft tail and no hard rim. With the reach 1.05 times the game's range it
// gives about the same amount of light as the game's straight falloff.
static inline float Falloff(float t)
{
    if (t >= 1.0f) return 0.0f;
    float u = 1.0f - t * t;
    return u * u / (1.0f + t * t);
}

static inline bool FloorTile(int x, int y)
{
    return x >= 0 && y >= 0 && x < gMapWidth && y < gMapHeight && gLevelMap[gMapRow[y] + x] < 0x1a;
}

// Add a light at (lx, ly) of the given level and range, colour (r, g, b).
static void AddSmoothLight(float lx, float ly, float level, float range, float r, float g, float b, bool walls = false)
{
    if (level <= 0.0f || range <= 0.0f) return;
    range *= 1.0f;
    int x0 = (int)floorf(lx - range) - 1, x1 = (int)floorf(lx + range) + 1;
    int y0 = (int)floorf(ly - range) - 1, y1 = (int)floorf(ly + range) + 1;
    if (x0 < gViewX0) x0 = gViewX0;
    if (y0 < gViewY0) y0 = gViewY0;
    if (x1 > gViewX1) x1 = gViewX1;
    if (y1 > gViewY1) y1 = gViewY1;
    if (x0 > x1 || y0 > y1) return;
    // how much of every tile in reach the light sees (the centre and four
    // points around it, so light fades in and out across a tile as the
    // light moves instead of switching)
    static float vis[72][67];
    static const float kOff[5][2] = {{0.5f, 0.5f}, {0.2f, 0.2f}, {0.8f, 0.2f}, {0.2f, 0.8f}, {0.8f, 0.8f}};
    const float reach = range + 1.5f;
    for (int x = x0; x <= x1; x++)
        for (int y = y0; y <= y1; y++) {
            float cx = (float)x + 0.5f, cy = (float)y + 0.5f;
            float d = Distance(lx, ly, cx, cy);
            vis[x][y] = 0.0f;
            if (!(d < reach)) continue;
            int seen = 0;
            for (int k = 0; k < 5; k++)
                seen += LineOfSight(lx, ly, (float)x + kOff[k][0], (float)y + kOff[k][1], 0) ? 1 : 0;
            if (!seen) continue;
            vis[x][y] = (float)seen / 5.0f;
            if (!walls && gLevelMap[gMapRow[y] + x] > 0x1c) continue;   // (as the game's light lists)
            float f = level * Falloff(d / range) * vis[x][y];
            if (f <= 0.0f) continue;
            float *a = gAccC[x * 65 + y];
            a[0] += f * r;
            a[1] += f * g;
            a[2] += f * b;
        }
    // tile corners: lit as far as the floor around them is in sight
    for (int x = x0 + 1; x <= x1; x++)
        for (int y = y0 + 1; y <= y1; y++) {
            float d = Distance(lx, ly, (float)x, (float)y);
            float f = level * Falloff(d / range);
            if (f <= 0.0f) continue;
            int n = 0;
            float k = 0.0f;
            for (int i = x - 1; i <= x; i++)
                for (int j = y - 1; j <= y; j++) {
                    if (!FloorTile(i, j)) continue;
                    n++;
                    k += vis[i][j];
                }
            if (!n) {   // (inside a wall block: take any tile in sight)
                for (int i = x - 1; i <= x; i++)
                    for (int j = y - 1; j <= y; j++) {
                        n++;
                        k += vis[i][j];
                    }
            }
            if (k <= 0.0f) continue;
            f *= k / (float)n;
            float *a = gAccV[x * kVertStride + y];
            a[0] += f * r;
            a[1] += f * g;
            a[2] += f * b;
        }
}

// Light levels to 1/256 steps; bright light rolls off towards full instead
// of clipping into flat discs.
static inline int Knee(float v)
{
    const float k = 25.0f, w = 31.0f - 25.0f;
    if (v > k) v = k + w * (1.0f - expf(-(v - k) / w));
    int n = (int)(v * 256.0f + 0.5f);
    return n < 0 ? 0 : n > 31 * 256 ? 31 * 256 : n;
}

// Smooth noise in -1..1 for flickering flames (ms clock, per light seed).
static float FlameNoise(unsigned ms, int seed)
{
    float t = (float)(ms % 600000u) * 0.001f + (float)seed * 1.7f;
    return 0.5f * sinf(t * 7.3f) + 0.3f * sinf(t * 13.1f + 1.3f) + 0.2f * sinf(t * 23.7f + 2.1f);
}

static void ComputeSmoothLight()
{
    // the tiles being drawn, with a margin for lights just outside the view
    gViewX0 = 1000, gViewY0 = 1000, gViewX1 = -1, gViewY1 = -1;
    for (int i = 0; i < gTileCount; i++) {
        int x = gTiles[i].x, y = gTiles[i].y;
        if (x < gViewX0) gViewX0 = x;
        if (y < gViewY0) gViewY0 = y;
        if (x > gViewX1) gViewX1 = x;
        if (y > gViewY1) gViewY1 = y;
    }
    gVertValid = false;
    if (gViewX1 < 0) return;
    gViewX0 = gViewX0 > 1 ? gViewX0 - 1 : 0;
    gViewY0 = gViewY0 > 1 ? gViewY0 - 1 : 0;
    gViewX1 = gViewX1 + 1 < gMapWidth ? gViewX1 + 1 : gMapWidth - 1;
    gViewY1 = gViewY1 + 1 < gMapHeight ? gViewY1 + 1 : gMapHeight - 1;
    if (gViewX1 > 69) gViewX1 = 69;
    if (gViewY1 > 64) gViewY1 = 64;
    memset(gAccC, 0, sizeof gAccC);
    memset(gAccV, 0, sizeof gAccV);
    const unsigned ms = plat_ticks();

    // wall torches (map tiles 2 and 10): magenta and red glows
    for (int x = gViewX0 - 4 < 0 ? 0 : gViewX0 - 4; x <= gViewX1 + 4 && x < gMapWidth; x++)
        for (int y = gViewY0 - 4 < 0 ? 0 : gViewY0 - 4; y <= gViewY1 + 4 && y < gMapHeight; y++) {
            uint8_t t = gLevelMap[gMapRow[y] + x];
            if (t != 2 && t != 0xa) continue;
            AddSmoothLight((float)x + 0.5f, (float)y + 0.5f, 20.0f, 3.0f, 1.0f, 0.0f, t == 2 ? 1.0f : 0.0f);
        }

    // placed lights: warm, with a gentle flicker
    if (gLightFirst != -1 && gLightLast != -1) {
        for (int i = gLightFirst < 0 ? 0 : gLightFirst; i <= gLightLast; i++) {
            Light &l = gLights[i];
            if (l.f0 != gCurLevel) continue;
            float level = (float)LightLevelOf((int16_t)(l.fc & 0xffff));
            float range = (float)(int16_t)(l.fc >> 16);
            float lx = (float)l.f4 + 0.5f, ly = (float)l.f8 + 0.5f;
            if (lx + range < gViewX0 || lx - range > gViewX1 + 1 || ly + range < gViewY0 || ly - range > gViewY1 + 1) continue;
            float n = FlameNoise(ms, i);
            AddSmoothLight(lx, ly, level * (0.94f + 0.06f * n), range + 0.25f * n, 1.0f, 0.87f, 0.75f);
        }
    }

    // the player's torch or light spell
    {
        float level = (float)gLightBright;
        bool torch = gEquip.shield == 2 && !gLightSpell;
        if (torch) level += -1.0f + 1.2f * FlameNoise(ms, 999);
        bool warm = gEquip.shield == 2 && gActionMode == 0 && gLightSpell <= 0;
        float range = (float)gLightRadius;
        if (warm)
            AddSmoothLight(gPlayer.x, gPlayer.y, level, range, 1.0f, 0.875f, 0.75f, true);
        else
            AddSmoothLight(gPlayer.x, gPlayer.y, level, range, 1.0f, 1.0f, 1.0f, true);
    }

    // flying fireballs (red) and other missiles (blue)
    for (int i = 0; i < 10; i++) {
        Projectile &p = gProjectiles[i];
        if (p.active != 1 && p.active != 2) continue;
        switch (p.kind) {
        case 0: case 1: case 3: case 5: case 6: {
            float r = p.active == 2 ? 7.0f : 3.5f;
            if (p.kind == 0 || p.kind == 5)
                AddSmoothLight(p.x, p.y, 24.0f, r, 1.5f, 0.5f, 0.5f, true);
            else
                AddSmoothLight(p.x, p.y, 24.0f, r, 0.5f, 0.5f, 1.5f, true);
            break;
        }
        }
    }

    for (int i = 0; i < gTileCount; i++) {
        int x = gTiles[i].x, y = gTiles[i].y, c = x * 65 + y;
        const int t[3] = {Knee(gAccC[c][0]), Knee(gAccC[c][1]), Knee(gAccC[c][2])};
        int o[3];
        SmoothStep(gSmoothTiles, c, t, o);
        gLightRGB[c] = Pack555(o);
    }
    for (int x = gViewX0; x <= gViewX1 + 1; x++)
        for (int y = gViewY0; y <= gViewY1 + 1; y++) {
            int v = x * kVertStride + y;
            const int t[3] = {Knee(gAccV[v][0]), Knee(gAccV[v][1]), Knee(gAccV[v][2])};
            int o[3];
            SmoothStep(gSmoothVerts, v, t, o);
            gVertRGB[v] = Pack555(o);
        }
    gVertValid = true;
}

// Drawn light at a world position: between the tile corners in the smooth
// model, else the light of the tile.
uint16_t LightRGBAt(float x, float y)
{
    int tx = (int)x, ty = (int)y;
    if (tx < 0) tx = 0;
    if (ty < 0) ty = 0;
    if (tx > 69) tx = 69;
    if (ty > 64) ty = 64;
    if (!SmoothLightModel() || tx < gViewX0 || ty < gViewY0 || tx > gViewX1 || ty > gViewY1)
        return gLightRGB[tx * 65 + ty];
    float fx = x - (float)tx, fy = y - (float)ty;
    if (fx < 0.0f) fx = 0.0f;
    if (fx > 1.0f) fx = 1.0f;
    if (fy < 0.0f) fy = 0.0f;
    if (fy > 1.0f) fy = 1.0f;
    const uint16_t *v = &gVertRGB[tx * kVertStride + ty];
    const uint16_t c00 = v[0], c10 = v[kVertStride], c01 = v[1], c11 = v[kVertStride + 1];
    int out = 0;
    for (int sh = 0; sh <= 10; sh += 5) {
        float a = (float)((c00 >> sh) & 31) * (1 - fx) + (float)((c10 >> sh) & 31) * fx;
        float b = (float)((c01 >> sh) & 31) * (1 - fx) + (float)((c11 >> sh) & 31) * fx;
        int n = (int)(a * (1 - fy) + b * fy + 0.5f);
        out |= (n > 31 ? 31 : n) << sh;
    }
    return (uint16_t)out;
}

// 0x415ea0
void ComputeLighting()
{
    gVertValid = false;
    if (gCurLevel == 0x16) {
        memcpy(gLightMap, gDynLight, 0x8e3 * 4);
        if (gColorLight) memcpy(gLightRGB, gDynRGB, sizeof gLightRGB);
        return;
    }
    memset(gLightMap, 0, 0x8e3 * 4);
    if (gColorLight) memset(gLightRGB, 0, sizeof gLightRGB);
    for (int i = 0; i < gTileCount; i++) {
        int x = gTiles[i].x, y = gTiles[i].y;
        gLightMap[x * 65 + y] = (uint16_t)TileLightLevel(x, y);
    }
    if (!gColorLight) return;
    if (gSettings.lightModel) {
        ComputeSmoothLight();
        return;
    }
    for (int i = 0; i < gTileCount; i++) {
        int x = gTiles[i].x, y = gTiles[i].y;
        gLightRGB[x * 65 + y] = SmoothLight(x * 65 + y, TileLightRGB(x, y));
    }
}

// 0x415f10
void CollectItems()
{
    for (int i = gItemFirst; i <= gItemLast; i++) {
        LevelItem *it = &gLevelItems[i];
        if (it->active != 1 || it->level != gCurLevel + 1 || it->fa < 0) continue;
        if (VIS(it->x, it->y) >= 0xfe) continue;
        if (gNumVisItems >= kMaxVisItems) break;   // (unchecked in the original)
        VisItem *v = &gVisItems[gNumVisItems];
        v->idx = i;
        WorldToScreen((float)it->x, (float)it->y, &v->sx, &v->sy);
        v->sx += 0x1f;
        v->sy += 0xf;
        gNumVisItems++;
    }
}

// 0x415ff0
void DrawItems()
{
    gItemHover = 0;
    for (int i = 0; i < gNumVisItems; i++) {
        VisItem *v = &gVisItems[i];
        LevelItem *it = &gLevelItems[v->idx];
        CSprite16 *spr = &gItemSprites[gItemSpriteIdx[it->type]];
        spr->BoundingRect(*(RECT *)v->rect, v->sx, v->sy);
        uint8_t light = (uint8_t)gLightMap[it->x * 65 + it->y];
        if (gUIMode == 0 && MouseInRect((RECT *)v->rect) && light > 4) {
            spr->DrawWhite(v->sx, v->sy, gDDW.surfacePtr, gDDW.pitch, gClip);
            if (IsPickable(v->idx)) gItemHover = 1;
        } else {
            SetTileShade(it->x * 65 + it->y);
            spr->Draw(v->sx, v->sy, gDDW.surfacePtr, gDDW.pitch, gClip);
        }
    }
}

// 0x416120
void DrawDecals()
{
    for (int i = 0; i < 600; i++) {
        Decal *d = &gDecals[i];
        if (!d->active || d->level != gCurLevel + 1) continue;
        int x = d->x, y = d->y;
        int idx = x * 65 + y;
        if (gMapVis[idx] >= 0xfe) continue;
        gDecalMap[idx] = i;
        int spr = d->sprite;
        int sx, sy;
        WorldToScreen((float)x, (float)y, &sx, &sy);
        SetTileShade(idx);
        gDecalSprites[spr].Draw(sx, sy, gDDW.surfacePtr, gDDW.pitch, gClip);
    }
}

// 0x416200
void CollectFeatures()
{
    for (int i = gFeatFirst; i <= gFeatLast; i++) {
        Feature *f = &gFeatures[i];
        if (!f->active || f->level != gCurLevel) continue;
        if (VIS(f->x, f->y) >= 0xfe) continue;
        AddFeature(i);
    }
}

// 0x416270
void AddFeature(int i)
{
    Feature *f = &gFeatures[i];
    int sprite = f->sprite;
    int n = 0;
    while (gVisFeatures[n].active == 1) {
        if (++n == kMaxVisFeatures) return;
    }
    VisFeature *v = &gVisFeatures[n];
    int y = f->y, x = f->x;
    float fy = (float)y, fx = (float)x;
    int sx, sy;
    WorldToScreen(fx, fy, &sx, &sy);
    CSprite16 *spr = &gFeatureSprites[sprite];
    sy += 0x20 - spr->Height();
    spr->BoundingRect(*(RECT *)v->rect, sx, sy);
    v->fg.fx = fx;
    v->fg.fy = fy;
    v->fg.x0 = sx;
    v->fg.y0 = sy;
    v->fg.depth = ScreenDepth(fx + 0.5f, fy + 0.5f);
    v->fg.type = 0;
    if (gUIMode == 1) v->fg.type = 4;
    v->fg.obj = spr;
    v->fg.rect = nullptr;
    v->sprite = sprite;
    v->feature = i;
    v->active = 1;
    int c = x * 65 + y;
    int sum = gLightMap[c] + gLightMap[c + 65] + gLightMap[c + 66] + gLightMap[c + 1];
    v->fg.light = sum / 4;
    if (gColorLight)
        v->fg.lrgb = SmoothLightModel() ? LightRGBAt((float)x + 1.0f, (float)y + 1.0f)
                                        : AverageLightColor(gLightRGB[c], gLightRGB[c + 65], gLightRGB[c + 66], gLightRGB[c + 1]);
}

// 0x416420
void CollectDoors()
{
    for (int i = gDoorFirst; i <= gDoorLast; i++) {
        Door *d = &gDoors[i];
        if (d->active != 1 || d->level != gCurLevel + 1) continue;
        if (VIS(d->x, d->y) >= 0xff) continue;
        AddDoor(i);
    }
}

// 0x416490
void AddDoor(int i)
{
    int n = 0;
    while (gVisDoors[n].active == 1) {
        if (++n == kMaxVisDoors) return;
    }
    VisDoor *v = &gVisDoors[n];
    Door *d = &gDoors[i];
    v->door = i;
    float fx = (float)d->x, fy = (float)d->y;
    int kind = d->kind;
    if (d->orient == 1) {
        float a = fy + 0.5f;
        v->depth1 = ScreenDepth(fx + 0.25f, a);
        v->depth2 = ScreenDepth(fx + 0.75f, a);
        v->sprA = &gDoorSprites[kind * 24 + 0];
        v->sprB = &gDoorSprites[kind * 24 + 1];
    } else {
        float b = fx + 0.5f;
        v->depth1 = ScreenDepth(b, fy + 0.75f);
        v->depth2 = ScreenDepth(b, fy + 0.25f);
        v->sprA = &gDoorSprites[kind * 24 + 2];
        v->sprB = &gDoorSprites[kind * 24 + 3];
    }
    WorldToScreen(fx, fy, &v->sx, &v->sy);
    v->sy += 0x20 - v->sprA->Height();
    v->active = 1;
}

static const int kObjFirst[24] = {0, 74, 6, 9, 12, 18, 24, 30, 35, 40, 45, 50, 56, 59, 62, 65, 68, 71, 79, 84, 87, 90, 93, 98};   // 0x44a068
static const int kObjLast[24] = {4, 78, 8, 11, 17, 23, 29, 34, 39, 44, 49, 55, 58, 61, 64, 67, 70, 73, 83, 86, 89, 92, 97, 100}; // 0x44a0c8
static const unsigned kObjSpeed[24] = {1, 1, 1, 1, 3, 3, 1, 1, 1, 1, 1, 2, 3, 3, 3, 3, 3, 3, 1, 4, 4, 1, 1, 2};                   // 0x44a128

// 0x416630: animated level objects.
void DrawLevelObjs()
{
    if (gObjFirst == -1) return;
    FGObject o;
    memset(&o, 0, sizeof(o));
    for (int i = gObjFirst < 0 ? 0 : gObjFirst; i <= gObjLast; i++) {   // (-1: none)
        LevelObj *ob = &gLevelObjs[i];
        int x = ob->x, y = ob->y;
        int idx = x * 65 + y;
        if (gMapVis[idx] >= 0xff) continue;
        if ((unsigned)gTicks % kObjSpeed[ob->kind] == 0 && !gTimeStop) {
            if (++ob->frame > kObjLast[ob->kind]) ob->frame = kObjFirst[ob->kind];
        }
        CSprite16 *spr = &gObjSprites[ob->frame];
        int w = spr->Width();
        ob->w = w;
        int h = spr->Height();
        ob->h = h;
        if (gUIMode == 1) o.type = 4;
        float fx = (float)x, fy = (float)y;
        o.fx = fx;
        o.fy = fy;
        o.obj = spr;
        WorldToScreen(fx, fy, &ob->sx, &ob->sy);
        ob->sy += 0x20 - h;
        o.x0 = ob->sx;
        o.y0 = ob->sy;
        o.depth = ScreenDepth(fx + 0.5f, fy + 0.5f);
        o.light = gLightMap[idx];
        o.lrgb = gColorLight ? gLightRGB[idx] : 0;
        ob->x0 = ob->sx;
        ob->y0 = ob->sy;
        ob->x1 = ob->sx + w;
        ob->y1 = ob->sy + h;
        AddFGObject(&o);
    }
}

static const int kAnimFirst[12] = {0, 6, 9, 12, 15, 18, 21, 24, 27, 30, 36, 0};  // 0x449fd8
static const int kAnimLast[12] = {5, 7, 11, 14, 17, 20, 23, 26, 29, 35, 41, 0};  // 0x44a008
static const int kAnimDelay[12] = {1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 0};          // 0x44a038

// 0x416800
void InitAnimTiles()
{
    gNumAux = 0;
    for (int i = 0; i < 400; i++) {
        LevelAux *a = &gLevelAux[i];
        if (gCurLevel != a->level) continue;
        AnimTile *t = &gAnimTiles[gNumAux];
        if (++gNumAux > 0xa0) FatalError("to many anim tiles this level\n", 0);
        t->kind = a->kind;
        t->x = a->x;
        t->y = a->y;
        t->counter = 0;
        int range = kAnimLast[t->kind] - kAnimFirst[t->kind];
        t->frame = rand() % range + kAnimFirst[t->kind];
    }
}

// 0x4168a0
void UpdateAnimTiles()
{
    gDDW.SetClippingWindowSize(0, 0, 0x27f, gHudVisible ? 0x18f : 0x1df);
    for (int i = 0; i < gNumAux; i++) {
        AnimTile *t = &gAnimTiles[i];
        if (++t->counter > kAnimDelay[t->kind] && !gTimeStop) {
            t->counter = 0;
            if (++t->frame > kAnimLast[t->kind]) t->frame = kAnimFirst[t->kind];
        }
        DrawAnimTile(t);
    }
}

// Four corner lights of a tile (missing neighbours use the tile's own).
static void CornerLights(int c, uint16_t *l, const uint16_t *map = gLightMap)
{
    if (map == gLightRGB && SmoothLightModel()) {
        // smooth model: the light at the tile's own corners
        int x = c / 65, y = c % 65, v = x * kVertStride + y;
        l[0] = gVertRGB[v];
        l[1] = gVertRGB[v + kVertStride];
        l[2] = gVertRGB[v + kVertStride + 1];
        l[3] = gVertRGB[v + 1];
        return;
    }
    uint16_t own = map[c];
    l[0] = own;
    l[1] = gMapVis[c + 65] != 0xff ? map[c + 65] : own;
    l[2] = gMapVis[c + 66] != 0xff ? map[c + 66] : own;
    l[3] = gMapVis[c + 1] != 0xff ? map[c + 1] : own;
}

// 0x416af0
void DrawAnimTile(AnimTile *t)
{
    int x = t->x, y = t->y;
    if (VIS(x, y) >= 0xfe) return;
    float fx = (float)x, fy = (float)y;
    int sx, sy;
    WorldToScreen(fx, fy, &sx, &sy);
    switch (t->kind) {
    case 0:
    case 9:
        gShade.SetShadeLevel(0x1f);
        gAnimSprites[t->frame].Draw(sx, sy, gDDW.surfacePtr, gDDW.pitch, gClip);
        if (rand() % 100 < 10) GroundGlow(x, y);
        return;
    case 10:
        if (rand() % 100 == 0x42) {
            int a = rand() % 360;
            Sparks(fx + 0.5f, fy + 0.5f, a);
        }
        break;
    }
    uint16_t l[4];
    if (gColorLight) {
        CornerLights(x * 65 + y, l, gLightRGB);
        if (l[0] == l[1] && l[1] == l[2] && l[2] == l[3]) {
            gShade.SetShadeColor(l[0]);
            gAnimSprites[t->frame].Draw(sx, sy, gDDW.surfacePtr, gDDW.pitch, gClip);
        } else {
            gAnimSprites[t->frame].DrawLitRGB(sx, sy, gDDW.surfacePtr, gDDW.pitch, gClip, l);
        }
        return;
    }
    CornerLights(x * 65 + y, l);
    gShade.SetShadeLevel(l[0]);
    gAnimSprites[t->frame].DrawLit(sx, sy, gDDW.surfacePtr, gDDW.pitch, gClip, l);
}

// 0x416940
void DrawFloorTiles()
{
    gFloorCacheProbes = 0;
    gFloorCacheMisses = 0;
    gFloorCacheHits = 0;
    for (int i = 0; i < gTileCount; i++) {
        TileEntry *t = &gTiles[i];
        int x = t->x, y = t->y;
        if (t->sx <= ViewL() - 0x3e || t->sy <= ViewT() - 0x20 || t->sx >= ViewR() || t->sy >= gViewHeight) continue;
        int c = x * 65 + y;
        gShade.SetShadeLevel(gLightMap[c] & 0x1f);
        uint16_t l[4];
        CornerLights(c, l);
        uint16_t *spr;
        if (gColorLight) {
            uint16_t cl[4];
            CornerLights(c, cl, gLightRGB);
            spr = GetFloorSpriteRGB(t->overlay, cl);
        } else {
            spr = GetFloorSprite(t->overlay, l);
        }
        if (t->sx >= ViewL() && t->sy >= ViewT() && t->sx <= ViewR() - 0x3e && t->sy <= gViewHeight - 0x20)
            DrawFloorFast(spr, t->sx, t->sy);
        else
            DrawFloorClipped(spr, t->sx, t->sy);
        if (gHaveMap && l[0] >= 2) MarkAutomap(x, y);
    }
    if ((unsigned)gFrameCounter % 10 == 0) UpdateFloorCache();
}

// Automap bitmap: 10 bytes of 8 columns per row, 65 rows per level.
void MarkAutomap(int x, int y)
{
    static const uint8_t kBits[8] = {1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80};   // 0x44bf68
    int col = (x >> 3) + gCurLevel * 10;
    gAutomap[col * 65 + y] |= kBits[x & 7];
}

// 0x416cc0: walls, lit from the side facing the player.
void DrawWalls()
{
    int px = gPlayer.tileX, py = gPlayer.tileY;
    for (int i = 0; i < gWallCount; i++) {
        FGObject o;
        memset(&o, 0, sizeof(o));
        o.type = 1;
        WallEntry *w = &gWalls[i];
        RECT *rect = (RECT *)gWallRects[i];
        o.obj = &gWallSprites[w->sprite & 0x7f];
        o.depth = w->depth;
        o.x0 = w->sx;
        o.y0 = w->sy;
        gWallSprites[w->sprite].BoundingRect(*rect, w->sx, w->sy);
        o.rect = rect;
        int x = w->x, y = w->y;
        int c = x * 65 + y;
        uint8_t *mp = &TILE(x, y);
        auto V = [&](int k) { return gMapVis[c + k]; };
        // (port: L yields a tagged offset, resolved after the switch so the
        // light colour can be looked up too)
        auto L = [&](int k) { return 0x10000 + k; };
        int light;
        switch (*mp) {
        case 0x20:
            if (V(0x40) == 0xff) {
                if (px > x) { light = L(1); break; }
                o.type = 4;
                light = L(-65);
                break;
            }
            if (py >= y && V(1) != 0xff) light = L(1);
            else if (px <= x && V(-0x41) != 0xff) light = L(-65);
            else light = L(64);
            if (!(py > y && px > x) && V(-0x41) != 0xff) o.type = 4;
            break;
        case 0x23:
            if (V(0x40) == 0xff) {
                if (py > y) { light = L(-64); break; }
                o.type |= 4;
                light = L(-64);
                break;
            }
            if (px <= x && V(-0x40) != 0xff) { o.type |= 4; light = L(-64); break; }
            if (py <= y && V(-1) != 0xff) { o.type |= 4; light = L(-1); break; }
            light = L(65);
            break;
        case 0x21:
            if (V(-0x42) == 0xff) {
                light = L(1);
                o.color = L(65);
                o.type = 0x40;
                break;
            }
            if (px > x || py > y) {
                if (V(0x42) != 0xff) {
                    o.type |= 0x40;
                    light = L(1);
                    o.color = L(65);
                    break;
                }
            }
            o.type |= 4;
            light = L(-66);
            break;
        case 0x22:
            if (V(-0x42) == 0xff || (px > x && py > y)) { light = L(66); break; }
            o.type |= 4;
            light = L(-66);
            break;
        case 0x24:
            if (px <= x) goto case1d_left;
            if (V(0x41) == 0xff) { light = L(-65); o.type |= 4; break; }
            if (py > y) { light = V(0x42) == 0xff ? L(64) : L(66); break; }
            light = V(0x40) == 0xff ? L(66) : L(64);
            break;
        case 0x25:
            if (px <= x) {
                if (V(0x41) != 0xff) {
                    if (px < x) o.type = 4;
                    light = L(65);
                    break;
                }
                if (py >= y) goto l417347;
                o.type = 4;
                light = V(-0x42) == 0xff ? L(64) : L(-66);
                break;
            }
            goto l417067;
        case 0x1d:
        case 0x1f:
            if (px > x) goto l417067;
        case1d_left:
            if (V(-0x41) == 0xff) goto l417067;
            o.type = 4;
            if (px < x || V(0x41) == 0xff) light = L(-65);
            else light = L(65);
            break;
        l417067: {
            uint8_t a = V(0x42);
            if (a >= 0x1a && a != 0xff) {
                light = mp[1] == 0xff ? L(-65) : L(65);
                if (px >= x && mp[-1] != 0xff && V(0x41) == 0xff) o.type = 4;
                break;
            }
            if (V(0x41) == 0xff) {
                light = L(-65);
                if (px >= x) o.type = 4;
                break;
            }
            light = L(66);
            o.color = L(65);
            o.type |= 0x40;
            break;
        }
        case 0x26:
            if (py >= y) goto l417223;
            if (V(-1) == 0xff) goto l41726b;
            if (V(1) != 0xff) {
                if (px >= x) { light = L(64); break; }
                o.type = 4;
                light = L(-66);
                break;
            }
            if (px >= x) {
                light = V(0x40) == 0xff ? L(-66) : L(64);
                break;
            }
            o.type = 4;
            light = V(-0x42) == 0xff ? L(64) : L(-66);
            break;
        case 0x1c:
        case 0x1e:
        l417223:
            if (py > y || V(-1) == 0xff) goto l41726b;
            o.type |= 4;
            if (py < y || V(1) == 0xff) light = L(-1);
            else light = L(1);
            break;
        l41726b: {
            uint8_t *below = &gLevelMap[gMapRow[y + 1] + x];
            uint8_t a = below[1];
            if (a >= 0x1a && a != 0xff) {
                light = below[0] == 0xff ? L(-1) : L(1);
                break;
            }
            if (V(1) == 0xff) { light = L(-1); break; }
            light = L(1);
            o.color = L(66);
            o.type |= 0x40;
            break;
        }
        case 0x27:
            if (py >= y) {
                light = px < x ? L(-64) : L(66);
                break;
            }
            if (V(-1) == 0xff) {
                if (px < x) goto l417347;
                light = V(0x42) == 0xff ? L(-64) : L(66);
                break;
            }
            o.type |= 4;
            light = L(-1);
            break;
        l417347:
            light = V(-0x40) == 0xff ? L(66) : L(-64);
            break;
        default:
            light = 0x1f;
            break;
        }
        int lightK = 9999;
        // (smooth light model: the light over the tile from the corner map,
        // which fades softly where walls hide the light instead of
        // switching the whole wall on and off)
        const bool smooth = SmoothLightModel();
        auto TileRGB = [&](int t) { return smooth ? LightRGBAt((float)(t / 65) + 0.5f, (float)(t % 65) + 0.5f) : gLightRGB[t]; };
        if (light >= 0x8000) {
            int k = light - 0x10000;
            lightK = k;
            light = gLightMap[c + k];
            if (gColorLight) o.lrgb = TileRGB(c + k);
        }
        if (o.color >= 0x8000) {
            int k = o.color - 0x10000;
            o.color = gLightMap[c + k];
            if (gColorLight) o.crgb = TileRGB(c + k);
        }
        if (gColorLight && (o.type & 4) && !(o.type & 0x40)) {
            // Port (enhanced mode): a see-through wall is lit from behind (the
            // player's side), which is often dark away from the player; blended
            // over a lit floor such walls showed as dark boxes. Take the light
            // of the floor in front of it (camera side) where that is brighter.
            uint16_t lc = o.lrgb ? o.lrgb : GreyLight(light);
            int r = (lc >> 10) & 31, g = (lc >> 5) & 31, b = lc & 31;
            static const int kDx[3] = {0, 1, 1}, kDy[3] = {1, 0, 1};
            for (int k = 0; k < 3; k++) {
                int nx = x + kDx[k], ny = y + kDy[k];
                if (nx >= gMapWidth || ny >= gMapHeight) continue;
                int ni = nx * 65 + ny;
                if (gMapVis[ni] == 0xff || TILE(nx, ny) >= 0x1a) continue;
                uint16_t n = TileRGB(ni);
                if (((n >> 10) & 31) > r) r = (n >> 10) & 31;
                if (((n >> 5) & 31) > g) g = (n >> 5) & 31;
                if ((n & 31) > b) b = n & 31;
            }
            o.lrgb = (uint16_t)(r << 10 | g << 5 | b);
        }
        if (smooth && gSettings.smoothLight) {
            // ease a wall's light too: which side of a wall is lit follows
            // the player, and switching sides flashed the whole wall
            static SmoothBank<4550> bankL, bankC;
            auto Ease = [](SmoothBank<4550> &b, int t, uint16_t v) {
                const int tv[3] = {((v >> 10) & 31) << 8, ((v >> 5) & 31) << 8, (v & 31) << 8};
                int out[3];
                SmoothStep(b, t, tv, out);
                return Pack555(out);
            };
            if (lightK != 9999) o.lrgb = Ease(bankL, c, o.lrgb);
            if (o.type & 0x40) o.crgb = Ease(bankC, c, o.crgb ? o.crgb : GreyLight(o.color));
        }
        o.light = light;
        if (gHaveMap && light >= 2) MarkAutomap(x, y);
        AddFGObject(&o);
    }
}

// 0x417450
void DrawDoors()
{
    gDoorHover = -1;
    for (int i = 0; i < kMaxVisDoors; i++) {
        VisDoor *v = &gVisDoors[i];
        if (v->active != 1) continue;
        Door *d = &gDoors[v->door];
        int base = ((d->timer / 2 + d->kind * 6) * 2 + d->orient) * 2;
        FGObject o;
        memset(&o, 0, sizeof(o));
        o.type = 0;
        o.x0 = v->sx;
        o.y0 = v->sy;
        o.depth = v->depth1;
        int c = d->x * 65 + d->y;
        int k;
        if (d->orient == 1) {
            if ((int)gPlayer.y <= d->y && gMapVis[c - 1] != 0xff) {
                o.type = 4;
                k = -1;
            } else {
                k = 1;
            }
        } else {
            if ((int)gPlayer.x <= d->x && gMapVis[c - 0x41] != 0xff) {
                o.type = 4;
                k = -65;
            } else {
                k = 65;
            }
        }
        int light = gLightMap[c + k];
        o.light = light;
        if (gColorLight) o.lrgb = gLightRGB[c + k];
        CSprite16 *a = &gDoorSprites[base];
        o.obj = a;
        AddFGObject(&o);
        a->BoundingRect(*(RECT *)v->rect, o.x0, o.y0);
        o.x0 += a->Width();
        o.depth = v->depth2;
        CSprite16 *b = &gDoorSprites[base + 1];
        o.obj = b;
        AddFGObject(&o);
        RECT r2;
        b->BoundingRect(r2, o.x0, o.y0);
        v->rect[2] = r2.right;
        v->rect[3] = r2.bottom;
        if (MouseInRect((RECT *)v->rect) && DoorReachable(v->door)) gDoorHover = i;
        if (gHaveMap && light >= 2) MarkAutomap(d->x, d->y);
    }
}

// 0x4176a0
void DrawFeatures()
{
    gFeatureHover = -1;
    for (int i = 0; i < kMaxVisFeatures; i++) {
        VisFeature *v = &gVisFeatures[i];
        if (v->active != 1) continue;
        AddFGObject(&v->fg);
        if (MouseInRect((RECT *)v->rect) && FeatureClickable(i)) gFeatureHover = i;
    }
}
