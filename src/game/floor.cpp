// Floor tiles: decoding, a cache of Gouraud shaded copies and drawing
// (0x41d5c0-0x41ddb0).  A floor tile is a 62x32 diamond stored as 32 rows of
// gTileRowWidth[r] pixels starting gTileRowOffset[r] pixels in.
#include "game.h"

static const int kTilePixels = 0x410;          // 0x820 bytes per tile

struct FloorCacheEntry {                       // 0x14, 500 at 0x4bfc18
    int32_t used;
    const uint16_t *tile;                      // source pixels
    uint64_t key;                              // packed corner lights (port: 64 bits for colours)
    uint32_t time;
    uint16_t *pixels;
};
static FloorCacheEntry gFloorCache[500];
static uint16_t gFloorPixels[240][kTilePixels]; // 0x4c2378

// 0x41d5c0
void InitFloorCache()
{
    for (int i = 0; i < 500; i++) {
        gFloorCache[i].used = 0;
        gFloorCache[i].key = 0;
        gFloorCache[i].pixels = new uint16_t[kTilePixels];
        gFloorCache[i].time = 0;
    }
}

// 0x41d5f0
int LoadFloorTiles(const char *name)
{
    char path[48];
    sprintf(path, "%s%s.CST", "LEVELS\\", name);
    gFloorCount = LoadCSpriteTable(path, gFloorSprites, 0xf0);
    if (!gFloorCount) return 0;
    for (uint32_t i = 0; i < gFloorCount; i++) {
        DecodeFloorTile(&gFloorSprites[i], gFloorPixels[i]);
        gFloorSprites[i].Release();
    }
    return 1;
}

// 0x41d680: unpack the diamond out of a run length encoded sprite.
void DecodeFloorTile(CSprite16 *spr, uint16_t *out)
{
    const uint16_t *src = spr->data;
    int remain = 0;      // pixels left in the current run
    int literal = 0;     // current run carries pixel data
    for (int r = 0; r < 32; r++) {
        // leading transparent margin (always starts a new run)
        int n = gTileRowOffset[r];
        while (n > 0) {
            uint16_t code = *src++;
            remain = code & 0x7fff;
            literal = code & 0x8000;
            if ((int16_t)remain > n) {
                if (literal) src += n;
                remain -= n;
                n = 0;
            } else {
                n -= remain;
                if (literal) src += remain;
                remain = 0;
            }
        }
        // the visible pixels
        n = gTileRowWidth[r];
        while (n > 0) {
            if ((uint16_t)remain == 0) {
                uint16_t code = *src++;
                remain = code & 0x7fff;
                literal = code & 0x8000;
            }
            int c = (int16_t)remain;
            if (c > n) c = n;
            remain -= c;
            n -= c;
            if (literal) {
                for (int k = 0; k < c; k++) *out++ = *src++;
            } else {
                memset(out, 0, (size_t)c * 2);
                out += c;
            }
        }
        // trailing margin
        n = gTileRowOffset[r];
        while (n > 0) {
            if ((uint16_t)remain == 0) {
                uint16_t code = *src++;
                remain = code & 0x7fff;
                literal = code & 0x8000;
            }
            int c = (int16_t)remain;
            if (c > n) {
                if (literal) src += n;
                remain -= n;
                n = 0;
            } else {
                n -= c;
                if (literal) src += c;
                remain = 0;
            }
        }
    }
}

// 0x41d7d0
void FreeFloorTiles()
{
    if (gFloorCount) gFloorCount = 0;
}

// 0x41d7f0: shaded copy of a floor tile for the given corner lights.
uint16_t *GetFloorSprite(int tile, uint16_t *lights)
{
    uint32_t key = ((((uint32_t)lights[0] << 8) + lights[1]) << 8) + lights[2];   // (0 for all-dark)
    key = (key << 8) + lights[3];
    const uint16_t *src = gFloorPixels[tile];
    uint32_t oldest = (uint32_t)gTime;
    uint32_t best = 0;
    uint32_t n = gFloorCacheMax + 2;
    if (n >= 500) n = 500;
    uint32_t i = 0;
    for (; i < n; i++) {
        FloorCacheEntry &e = gFloorCache[i];
        if (e.key == key) {
            if (!e.used) goto freeSlot;
            if (e.tile == src) {
                e.time = (uint32_t)gTime;
                gFloorCacheHits++;
                gFloorCacheProbes += i;
                return e.pixels;
            }
        }
        if (!e.used) goto freeSlot;
        if (e.time < oldest) {
            oldest = e.time;
            best = i;
        }
    }
    gFloorCacheProbes += 500;
    goto fill;
freeSlot:
    if (oldest) best = i;
fill: {
    gFloorCacheMisses++;
    FloorCacheEntry &e = gFloorCache[best];
    e.key = key;
    e.tile = src;
    e.time = (uint32_t)gTime;
    e.used = 1;
    if (lights[0] == lights[1] && lights[1] == lights[2] && lights[2] == lights[3]) {
        gShade.SetShadeLevel(lights[0]);
        ShadeTileFlat(e.pixels, e.tile);
    } else {
        ShadeTileGouraud(e.pixels, src, lights);
    }
    if (best > gFloorCacheMax) gFloorCacheMax = best;
    return e.pixels;
}
}

// Port (enhanced mode): a floor tile lit by four 555 light colours,
// interpolated smoothly across the diamond (the 3dfx build draws the tiles
// as Gouraud shaded polygons).
static void ShadeTileRGB(uint16_t *out, const uint16_t *src, const uint16_t *C)
{
    int T[3], R[3], B[3], L[3];
    const uint16_t *cs[4] = {&C[0], &C[1], &C[2], &C[3]};
    int *dst[4] = {T, R, B, L};
    for (int k = 0; k < 4; k++) {
        uint16_t c = *cs[k];
        dst[k][0] = ((c >> 10) & 31) << 8;
        dst[k][1] = ((c >> 5) & 31) << 8;
        dst[k][2] = (c & 31) << 8;
    }
    for (int r = 0; r < 32; r++) {
        int w = gTileRowWidth[r];
        int x = gTileRowOffset[r];
        int vv = ((2 * r + 1) << 7) / 32;
        for (int k = 0; k < w; k++, x++) {
            int u = ((2 * x + 1) << 7) / 62;
            int s = u + vv - 128, t = vv - u + 128;
            if (s < 0) s = 0;
            if (s > 256) s = 256;
            if (t < 0) t = 0;
            if (t > 256) t = 256;
            int w00 = (256 - s) * (256 - t), w10 = s * (256 - t), w01 = (256 - s) * t, w11 = s * t;
            int l[3];
            for (int ch = 0; ch < 3; ch++) l[ch] = (T[ch] * w00 + R[ch] * w10 + L[ch] * w01 + B[ch] * w11) >> 16;
            *out++ = gShade.ShadeRGB(*src++ & 0x7fff, l[0], l[1], l[2], Dither4(x, r));
        }
    }
}

uint16_t *GetFloorSpriteRGB(int tile, const uint16_t *C)
{
    uint64_t key = (uint64_t)C[0] | (uint64_t)C[1] << 16 | (uint64_t)C[2] << 32 | (uint64_t)C[3] << 48;
    key |= 0x8000800080008000ull;   // never 0 (0 marks a free entry)
    const uint16_t *src = gFloorPixels[tile];
    uint32_t oldest = 0xffffffffu;
    int best = 0;
    uint32_t n = gFloorCacheMax + 2;   // entries past the highest used one are free
    if (n >= 500) n = 500;
    for (uint32_t i = 0; i < n; i++) {
        FloorCacheEntry &e = gFloorCache[i];
        if (!e.used) {
            best = (int)i;
            break;
        }
        if (e.key == key && e.tile == src) {
            e.time = (uint32_t)gTime;
            gFloorCacheHits++;
            return e.pixels;
        }
        if (e.time < oldest) {
            oldest = e.time;
            best = (int)i;
        }
    }
    gFloorCacheMisses++;
    FloorCacheEntry &e = gFloorCache[best];
    e.key = key;
    e.tile = src;
    e.time = (uint32_t)gTime;
    e.used = 1;
    if (C[0] == C[1] && C[1] == C[2] && C[2] == C[3]) {
        const uint16_t *t = gShade.ColorTable(C[0]);
        for (int i = 0; i < kTilePixels; i++) e.pixels[i] = t[src[i] & 0x7fff];
    } else {
        ShadeTileRGB(e.pixels, src, C);
    }
    if ((uint32_t)best > gFloorCacheMax) gFloorCacheMax = best;
    return e.pixels;
}

// 0x41d980: expire unused entries and pack the live ones to the front.
void UpdateFloorCache()
{
    uint32_t limit = (uint32_t)gTime - 100;
    for (int i = 0; i < 500; i++) {
        if (gFloorCache[i].time < limit) {
            gFloorCache[i].used = 0;
            gFloorCache[i].key = 0;
        }
    }
    int lo = 0, hi = 499;
    do {
        while (lo < 500 && gFloorCache[lo].used == 1) lo++;
        while (gFloorCache[hi].used == 0 && hi > 0) hi--;
        if (lo >= hi) break;
        uint16_t *keep = gFloorCache[lo].pixels;
        gFloorCache[lo] = gFloorCache[hi];
        gFloorCache[hi].used = 0;
        gFloorCache[hi].key = 0;
        gFloorCache[hi].pixels = keep;
    } while (lo < hi);
    uint32_t m = 0;
    for (uint32_t i = 0; i < 500; i++)
        if (gFloorCache[i].used && i > m) m = i;
    gFloorCacheMax = m;
}

// 0x41da90
void ClearFloorCache()
{
    for (int i = 0; i < 500; i++) {
        gFloorCache[i].used = 0;
        gFloorCache[i].key = 0;
        gFloorCache[i].time = 0;
    }
}

// 0x41dab0: light interpolated between the corners (top, right, bottom, left).
void ShadeTileGouraud(uint16_t *out, const uint16_t *src, const uint16_t *l)
{
    memcpy(out, src, kTilePixels * 2);
    const uint16_t *table = gShade.base;
    int a = l[0] << 8, b = l[0] << 8;           // right and left edges
    int da = ((l[1] - l[0]) << 8) / 16;
    int db = ((l[3] - l[0]) << 8) / 16;
    for (int r = 0; r < 32; r++) {
        int w = gTileRowWidth[r];
        int cur = b;
        int step = (a - b) / w;
        for (int k = 0; k < w; k++) {
            *out++ = table[((cur & 0x1f00) << 7) + *src++];
            cur += step;
        }
        if (r == 16) {
            b = l[3] << 8;
            a = l[1] << 8;
            db = ((l[2] - l[3]) << 8) / 16;
            da = ((l[2] - l[1]) << 8) / 16;
        } else {
            b += db;
            a += da;
        }
    }
}

// 0x41dc00
void ShadeTileFlat(uint16_t *out, const uint16_t *src)
{
    const uint16_t *table = gShade.current;
    for (int i = 0; i < kTilePixels; i++) out[i] = table[src[i]];
}

// 0x41dc50
void FreeFloorCache()
{
    for (int i = 0; i < 500; i++) {
        delete[] gFloorCache[i].pixels;
        gFloorCache[i].pixels = nullptr;
        gFloorCache[i].used = 0;
    }
}

// 0x41dc80: tile fully inside the view (surface locked).
void DrawFloorFast(uint16_t *pix, int x, int y)
{
    uint8_t *row = gDDW.surfacePtr + (ptrdiff_t)y * gDDW.pitch;
    for (int r = 0; r < 32; r++) {
        int w = gTileRowWidth[r];
        memcpy((uint16_t *)row + x + gTileRowOffset[r], pix, (size_t)w * 2);
        pix += w;
        row += gDDW.pitch;
    }
}

// 0x41dce0
void DrawFloorClipped(uint16_t *pix, int x, int y)
{
    uint8_t *row = gDDW.surfacePtr + (ptrdiff_t)y * gDDW.pitch;
    for (int r = 0; r < 32; r++, y++) {
        if (y > gViewHeight) return;
        int xs = gTileRowOffset[r] + x;
        int w = gTileRowWidth[r];
        const uint16_t *s = pix;
        if (y >= ViewT()) {
            bool draw = true;
            if (xs < ViewL()) {
                int cut = ViewL() - xs;
                w -= cut;
                if (w < 0) draw = false;
                s += cut;
                xs = ViewL();
            }
            if (draw && w + xs > gViewRight) {
                w = gViewRight - xs + 1;
                if (w <= 0) draw = false;
            }
            if (draw) memcpy((uint16_t *)row + xs, s, (size_t)w * 2);
        }
        row += gDDW.pitch;
        pix += gTileRowWidth[r];
    }
}
