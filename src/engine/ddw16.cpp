// Reimplementation of ddw16.dll (see ddw16.h).
#include "ddw16.h"
#include <mutex>
#include "../platform/fileio.h"
#include "../platform/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mutex>

static std::recursive_mutex gFrontLock;    // guards the front buffer against the cursor thread

DirectDrawWindow *gPresentWindow = nullptr;
ScreenLayout gLayout;

void ExtendFullWidth(int &x0, int &y0, int &x1, int &y1) {
    if (!gLayout.wide()) return;
    // (0x27e: FillRect(0, 0, 0x27f, ...) ends one pixel short)
    if (x0 <= 0 && x1 >= 0x27e) {
        x0 = gLayout.minX();
        x1 = gLayout.maxX() + 1;
        if (y0 <= 0) y0 = gLayout.minY();
        if (y1 >= 0x1de) y1 = gLayout.maxY() + 1;
    }
}

HiresLayer gHires;

static void HiresAlloc() {
    if (gLayout.hiresScale <= 1 || gHires.out) return;
    HiresLayer &h = gHires;
    h.S = gLayout.hiresScale;
    h.W = gLayout.physW * h.S;
    h.H = gLayout.physH * h.S;
    for (int i = 0; i < 2; i++) {
        h.img[i] = (uint16_t *)calloc((size_t)h.W * h.H, 2);
        h.own[i] = (uint32_t *)calloc((size_t)gLayout.physW * gLayout.physH, 4);
    }
    h.out = (uint16_t *)calloc((size_t)h.W * h.H, 2);
}

// Present the front buffer, merging in the hi-res model pixels. With a
// dirty rectangle (physical pixels) only that part is composited again (the
// cursor thread moves the cursor this way).
static void Present(Surface *front, const RECT *dirty = nullptr) {
    HiresLayer &h = gHires;
    if (!h.enabled()) {
        plat_present(front->px, front->pitch, front->w, front->h);
        return;
    }
    const int S = h.S;
    const uint16_t *hi = h.img[h.back ^ 1];
    const uint32_t *own = h.own[h.back ^ 1];
    const uint32_t tag = h.frontStamp << 16;
    // boxes whose model has been erased: forget their background copies
    {
        uint32_t *ownw = h.own[h.back ^ 1];
        std::vector<RECT> &bx = h.boxes[h.back ^ 1];
        for (size_t i = 0; i < bx.size();) {
            const RECT &b = bx[i];
            // (alive while a good part of its model pixels are still there:
            // a few can match the background by chance)
            int total = 0, live = 0;
            for (int y = b.top; y <= b.bottom; y++) {
                const uint16_t *src = front->row(y);
                const uint32_t *o = ownw + (size_t)y * front->w;
                for (int x = b.left; x <= b.right; x++) {
                    if ((o[x] & 0x80000000u) || (o[x] & 0x7fff0000u) != tag) continue;
                    total++;
                    if (src[x] && o[x] == (tag | src[x])) live++;
                }
            }
            if (total && live * 4 >= total) { i++; continue; }
            for (int y = b.top; y <= b.bottom; y++) {
                uint32_t *o = ownw + (size_t)y * front->w;
                for (int x = b.left; x <= b.right; x++)
                    if ((o[x] & 0x7fff0000u) == tag) o[x] = 0;
            }
            bx.erase(bx.begin() + (long)i);
        }
    }
    int x0 = 0, y0 = 0, x1 = front->w, y1 = front->h;
    if (dirty) {
        x0 = dirty->left < 0 ? 0 : dirty->left;
        y0 = dirty->top < 0 ? 0 : dirty->top;
        x1 = dirty->right > front->w ? front->w : dirty->right;
        y1 = dirty->bottom > front->h ? front->h : dirty->bottom;
    }
    for (int y = y0; y < y1; y++) {
        const uint16_t *src = front->row(y);
        const uint32_t *o = own + (size_t)y * front->w;
        uint16_t *d0 = h.out + (size_t)y * S * h.W;
        const uint16_t *hrow = hi + (size_t)y * S * h.W;
        for (int x = x0; x < x1; x++) {
            uint16_t v = src[x];
            // (black pixels never show the hi-res layer: when a screen fades
            // to black they would match the black a model once left there)
            bool useHi = h.frontStamp && v && (o[x] & 0x7fffffff) == (tag | v);
            if (S == 2) {
                uint32_t *d = (uint32_t *)(d0 + x * 2), *e = (uint32_t *)(d0 + h.W + x * 2);
                if (useHi) {
                    *d = *(const uint32_t *)(hrow + x * 2);
                    *e = *(const uint32_t *)(hrow + h.W + x * 2);
                } else {
                    *d = *e = (uint32_t)v | (uint32_t)v << 16;
                }
            } else if (useHi) {
                const uint16_t *hs = hrow + x * S;
                for (int j = 0; j < S; j++)
                    for (int i = 0; i < S; i++) d0[(size_t)j * h.W + x * S + i] = hs[(size_t)j * h.W + i];
            } else {
                for (int j = 0; j < S; j++)
                    for (int i = 0; i < S; i++) d0[(size_t)j * h.W + x * S + i] = v;
            }
        }
    }
    plat_present(h.out, h.W * 2, h.W, h.H);
}

// logical -> physical pixel pointer
static inline uint16_t *PhysRow(Surface *s, int y) { return s->row(y + gLayout.oy) + gLayout.ox; }

// ---------------------------------------------------------------------------
// Surfaces

Surface::Surface(int w_, int h_) : w(w_), h(h_) {
    pitch = w * 2;
    px = (uint16_t *)calloc((size_t)w * h + 16, 2);
}
Surface::~Surface() { free(px); }

void Surface::fill(const RECT *r, uint16_t c) {
    RECT rr = r ? *r : RECT{0, 0, w, h};
    if (rr.left < 0) rr.left = 0;
    if (rr.top < 0) rr.top = 0;
    if (rr.right > w) rr.right = w;
    if (rr.bottom > h) rr.bottom = h;
    for (int y = rr.top; y < rr.bottom; y++) {
        uint16_t *p = row(y);
        for (int x = rr.left; x < rr.right; x++) p[x] = c;
    }
}

void SurfaceBlt(Surface *dst, int dx, int dy, Surface *src, const RECT *sr, bool useKey) {
    RECT s = sr ? *sr : RECT{0, 0, src->w, src->h};
    // clip to source
    if (s.left < 0) { dx -= s.left; s.left = 0; }
    if (s.top < 0) { dy -= s.top; s.top = 0; }
    if (s.right > src->w) s.right = src->w;
    if (s.bottom > src->h) s.bottom = src->h;
    // clip to destination
    if (dx < 0) { s.left -= dx; dx = 0; }
    if (dy < 0) { s.top -= dy; dy = 0; }
    if (dx + (s.right - s.left) > dst->w) s.right = s.left + (dst->w - dx);
    if (dy + (s.bottom - s.top) > dst->h) s.bottom = s.top + (dst->h - dy);
    int w = s.right - s.left, h = s.bottom - s.top;
    if (w <= 0 || h <= 0) return;
    bool key = useKey && src->colorKeyed;
    uint16_t k = src->colorKey;
    for (int y = 0; y < h; y++) {
        uint16_t *sp = src->row(s.top + y) + s.left;
        uint16_t *dp = dst->row(dy + y) + dx;
        if (!key) {
            memmove(dp, sp, (size_t)w * 2);
        } else {
            for (int x = 0; x < w; x++)
                if (sp[x] != k) dp[x] = sp[x];
        }
    }
}

// ---------------------------------------------------------------------------
// DirectDrawWindow

void *DirectDrawWindow::Init(void *, char *className, int fs) {
    fullscreen = fs;
    lockCount = 0;
    pageFlipping = 0;
    backBufferType = 2;
    frameCount = 0;
    preUpdate = postUpdate = nullptr;
    modeNumber = 0;
    active = 1;
    initialised = 1;
    numBuffers = 2;
    gPresentWindow = this;
    hwnd = plat_create_window(className);
    return hwnd;
}

int DirectDrawWindow::GetFreeMem() { return 8192; } // KB, plenty

void DirectDrawWindow::End() {
    RestoreVideoMode();
    initialised = 0;
}

int DirectDrawWindow::SetModeNumber(int w, int h, int b) {
    // The original only accepts modes enumerated from the display driver; the
    // port supports the 16bpp modes the game asks for.
    if (b != 16) return 0;
    width = w; height = h; bpp = b;
    modeNumber = 0;
    return 1;
}

void DirectDrawWindow::AddVideoMode(int, int, int) {}

int DirectDrawWindow::SetVideoMode() {
    if (!initialised) return 0;
    SetRect(&clientRect, 0, 0, width, height);
    plat_set_video_mode(gLayout.physW * gLayout.hiresScale, gLayout.physH * gLayout.hiresScale);
    return 1;
}

void DirectDrawWindow::RestoreVideoMode() {
    delete back; back = nullptr;
    delete front; front = nullptr;
    surfacesCreated = 0;
}

int DirectDrawWindow::CreatePrimarySurface(int) {
    if (front) return 0;
    front = new Surface(gLayout.physW, gLayout.physH);
    back = new Surface(gLayout.physW, gLayout.physH);
    // As in DDW16.DLL: a full-screen mode with more than one buffer and video
    // memory back buffers gets a flipping chain. (Not in the widescreen mode:
    // the game only restores the original 640x480 area between frames.)
    pageFlipping = (fullscreen && numBuffers > 1 && backBufferType == 2 && !gLayout.wide()) ? 1 : 0;
    HiresAlloc();
    DetermineColorBits();
    Clear(0);
    surfacesCreated = 1;
    return 1;
}

void DirectDrawWindow::DetermineColorBits() {
    redBits = 5; redShift = 11; redMask = 0xf800;
    greenBits = 6; greenShift = 5; greenMask = 0x07e0;
    blueBits = 5; blueShift = 0; blueMask = 0x001f;
    bytesPerPixel = 2;
}

void DirectDrawWindow::RegisterUpdateCallback(void (*cb)(), unsigned long which) {
    if (which == 1) preUpdate = cb;
    else if (which == 2) postUpdate = cb;
}

void DirectDrawWindow::UpdateScreen() {
    if (!active) return;
    if (preUpdate) preUpdate();
    if (back && front) {
        std::lock_guard<std::recursive_mutex> lk(gFrontLock);
        if (fullscreen && pageFlipping && backBufferType != 1 && numBuffers > 1) {
            // Flip: the old front buffer becomes the new back buffer, so the
            // game sees the frame before last when it draws next, exactly as
            // with a real DirectDraw flipping chain.
            Surface *t = front;
            front = back;
            back = t;
        } else {
            memcpy(front->px, back->px, (size_t)front->pitch * front->h);
        }
        if (gHires.drewThisFrame) {   // this frame drew models: its hi-res layer goes to the front
            gHires.back ^= 1;
            gHires.boxes[gHires.back].clear();   // (the new back layer starts afresh)
            gHires.frontStamp = gHires.stamp;
            if (++gHires.stamp > 0x7fff) {
                // the stamps come round again: forget every old record so
                // none can match a frame drawn 32767 frames later
                gHires.stamp = 1;
                for (int i = 0; i < 2; i++)
                    memset(gHires.own[i], 0, (size_t)gLayout.physW * gLayout.physH * 4);
            }
            gHires.drewThisFrame = false;
        }
        Present(front);
    }
    frameCount++;
    if (postUpdate) postUpdate();
}

int DirectDrawWindow::UpdateScreenNoWait() {
    if (!active) return 0;
    UpdateScreen();
    return 1;
}

Surface *DirectDrawWindow::CreatePlainSurface(int w, int h, int, int colorKey, int) {
    Surface *s = new Surface(w, h);
    if (colorKey) { s->colorKeyed = true; s->colorKey = 0; }
    return s;
}

int DirectDrawWindow::Clear(unsigned long color) {
    std::lock_guard<std::recursive_mutex> lk(gFrontLock);   // (port: the cursor thread)
    if (back) back->fill(nullptr, (uint16_t)color);
    if (front) {
        front->fill(nullptr, (uint16_t)color);
        Present(front);
    }
    return 1;
}

// (The rectangle is filled up to, not including, x+w-1 / y+h-1, as in the
// DLL, which built a RECT from the inclusive coordinates.)
void DirectDrawWindow::FillRect(int x, int y, int w, int h, int color) {
    if (!back) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    RECT r = {x, y, x + w - 1, y + h - 1};
    if (r.right >= clientRect.right) r.right = clientRect.right;
    if (r.bottom >= clientRect.bottom) r.bottom = clientRect.bottom;
    FillRect(&r, color);
}

void DirectDrawWindow::FillRect(RECT *r, int color) {
    if (!back) return;
    if (!r) {
        back->fill(nullptr, (uint16_t)color);
        return;
    }
    if (r->top < 0) r->top = 0;
    if (r->left < 0) r->left = 0;
    if (r->right >= clientRect.right) r->right = clientRect.right;
    if (r->bottom >= clientRect.bottom) r->bottom = clientRect.bottom;
    int x0 = r->left, y0 = r->top, x1 = r->right, y1 = r->bottom;
    if (x1 >= 0x27e) ExtendFullWidth(x0, y0, x1, y1);
    RECT p = {x0 + gLayout.ox, y0 + gLayout.oy, x1 + gLayout.ox, y1 + gLayout.oy};
    back->fill(&p, (uint16_t)color);
}

void DirectDrawWindow::TintRect(int x, int y, int w, int h, unsigned short color) {
    unsigned c = color;
    unsigned r = (c & redMask) >> redShift;
    unsigned g = (c & greenMask) >> greenShift;
    unsigned b = (c & blueMask) >> blueShift;
    float rf = (float)(int)r / (float)(1 << redBits);
    float bf = (float)(int)b / (float)(1 << blueBits);
    uint16_t redT[32], blueT[32], greenT[64];
    for (int i = 0; i < 32; i++) {
        redT[i] = (uint16_t)(int)((float)i * rf);
        blueT[i] = (uint16_t)(int)((float)i * bf);
    }
    float gf = (float)(int)g / (float)(1 << greenBits);
    for (int i = 0; i < (1 << greenBits); i++) greenT[i] = (uint16_t)(int)((float)i * gf);

    uint8_t *p = nullptr; unsigned long pt = 0;
    Lock(&p, &pt);
    int x1 = x + w, y1 = y + h;
    ExtendFullWidth(x, y, x1, y1);
    w = x1 - x; h = y1 - y;
    for (int yy = 0; yy < h; yy++) {
        int py = y + yy;
        if (py < gLayout.minY() || py > gLayout.maxY()) continue;
        uint16_t *d = (uint16_t *)(p + (long)py * (long)pt);
        for (int xx = 0; xx < w; xx++) {
            int px = x + xx;
            if (px < gLayout.minX() || px > gLayout.maxX()) continue;
            unsigned v = d[px];
            unsigned rr = redT[(v & redMask) >> redShift];
            unsigned gg = greenT[(v & greenMask) >> greenShift];
            unsigned bb = blueT[(v & blueMask) >> blueShift];
            d[px] = (uint16_t)((rr << redShift) | (gg << greenShift) | bb);
        }
    }
    Unlock();
}

void DirectDrawWindow::GreyRect(int x, int y, int w, int h) {
    uint8_t *p = nullptr; unsigned long pt = 0;
    Lock(&p, &pt);
    int x1 = x + w, y1 = y + h;
    ExtendFullWidth(x, y, x1, y1);
    w = x1 - x; h = y1 - y;
    for (int yy = 0; yy < h; yy++) {
        uint16_t *d = (uint16_t *)(p + (long)(y + yy) * (long)pt) + x;
        for (int xx = 0; xx < w; xx++) {
            unsigned v = (d[xx] & redMask) >> redShift;
            unsigned gv = (unsigned)(int)((float)(int)v * 0.33f);
            d[xx] = (uint16_t)((gv << greenShift) | (gv << redShift) | gv);
        }
    }
    Unlock();
}

void DirectDrawWindow::OnMove() {}

void DirectDrawWindow::OnActivate(int a) {
    if (!initialised || !surfacesCreated) return;
    active = a;
}

int DirectDrawWindow::Lock(uint8_t **ptr, unsigned long *pt) {
    if (!back) return 0;
    surfacePtr = (uint8_t *)PhysRow(back, 0);
    pitch = back->pitch;
    *ptr = surfacePtr;
    *pt = (unsigned long)pitch;
    lockCount++;
    return 1;
}

void DirectDrawWindow::Unlock() {
    lockCount--;
    if (lockCount < 0) lockCount = 0;
}

// The mouse cursor is drawn into the front buffer from a timer thread; this
// lock keeps it from interleaving with a flip/present on the main thread.

int DirectDrawWindow::LockFront(uint8_t **ptr, unsigned long *pt) {
    if (!front) return 0;
    gFrontLock.lock();
    // (port: the cursor thread calls this while the game draws through
    // surfacePtr, so that must keep pointing at the back buffer; setting it
    // here sent whole runs of floor tiles to the front buffer)
    *ptr = (uint8_t *)PhysRow(front, 0);
    *pt = (unsigned long)front->pitch;
    return 1;
}

void DirectDrawWindow::UnlockFront() {
    if (front) Present(front);
    gFrontLock.unlock();
}

void DirectDrawWindow::UnlockFront(int x, int y, int w, int h) {
    if (front) {
        RECT r = {x + gLayout.ox, y + gLayout.oy, x + gLayout.ox + w, y + gLayout.oy + h};
        Present(front, &r);
    }
    gFrontLock.unlock();
}

unsigned short DirectDrawWindow::MakePixel16(unsigned char r, unsigned char g, unsigned char b) {
    return (unsigned short)(((unsigned)(g >> (8 - greenBits)) << greenShift) | ((unsigned)(r >> 3) << redShift) | (b >> 3));
}

void DirectDrawWindow::GetRGBPixel(int x, int y, unsigned char *r, unsigned char *g, unsigned char *b) {
    uint8_t *p = nullptr; unsigned long pt = 0;
    Lock(&p, &pt);
    unsigned v = *(uint16_t *)(p + (size_t)y * pt + x * 2);
    *r = (unsigned char)(((v & redMask) & 0xff) >> (redShift - 3)); // (original truncates to 8 bits)
    *r = (unsigned char)((v & redMask) >> redShift << 3);
    *g = (unsigned char)((v & greenMask) >> greenShift << (8 - greenBits));
    *b = (unsigned char)((v & blueMask) << 3);
    Unlock();
}

void DirectDrawWindow::GetColorBits(unsigned char *r, unsigned char *g, unsigned char *b) {
    *r = (unsigned char)redBits; *g = (unsigned char)greenBits; *b = (unsigned char)blueBits;
}

void DirectDrawWindow::BreakPixel(unsigned short p, unsigned char *r, unsigned char *g, unsigned char *b) {
    *r = (unsigned char)((p & redMask) >> redShift);
    *g = (unsigned char)((p & greenMask) >> greenShift);
    *b = (unsigned char)((p & blueMask) >> blueShift);
}

void DirectDrawWindow::PageFlipping(int on) {
    // (not in the widescreen mode, where the game only restores the original
    // 640x480 area between flips)
    if (fullscreen && numBuffers > 1 && !gLayout.wide()) pageFlipping = on & 1;
}

void DirectDrawWindow::Fade() {
    uint16_t mask = MakePixel16(0x7f, 0x7f, 0x7f);
    uint8_t *p = nullptr; unsigned long pt = 0;
    Lock(&p, &pt);
    for (int i = 0; i < 6; i++) {
        for (int y = 0; y < back->h; y++) {
            uint16_t *d = back->row(y);
            for (int x = 0; x < back->w; x++) d[x] = (uint16_t)((d[x] >> 1) & mask);
        }
        UpdateScreen();
    }
    Unlock();
    Clear(0);
}

void DirectDrawWindow::DDError(const char *msg) {
    char buf[512];
    snprintf(buf, sizeof buf, "Direct Draw Error : %s", msg);
    plat_message_box(buf, msg);
}

// ---------------------------------------------------------------------------
// Clipper

void Clipper::SetViewport(int l, int t, int r, int b) {
    if (l < gLayout.minX() || t < gLayout.minY()) return;
    if (r - l > gLayout.physW) return;   // (port: the right edge is exclusive, so the full width is physW)
    if (b - t > gLayout.physH - 1) return;
    left = l; right = r; top = t; bottom = b;
}

int Clipper::Clip(int *r) {
    int x = r[0], y = r[1], w = r[2], h = r[3];
    r[8] = 0; r[6] = 0; r[9] = h; r[7] = w;
    int x2 = x + w, y2 = y + h;
    if (x2 <= left || x >= right || y >= bottom || y2 <= top) return 2;
    int n = 0;
    if (x <= left) { r[6] = left - x; r[7] = w - r[6]; n = 1; }
    if (x2 > right) { r[7] += right - x2; n++; }
    if (y <= top) { r[8] = top - y; r[9] = h - r[8]; n++; }
    if (y2 > bottom) { r[9] += bottom - y2; n++; }
    r[4] = r[7] + r[6];
    r[5] = r[9] + r[8];
    return n != 0;
}

// ---------------------------------------------------------------------------
// Shade16

struct Shade16::ColorSlot {
    uint16_t key;
    uint32_t used;
    uint16_t tab[0x8000];
};
enum { kColorSlots = 256 };

int Shade16::Init(unsigned long levels, DirectDrawWindow &w) {
    if (levels > 32) return 0;
    numLevels = levels;
    base = (uint16_t *)malloc((size_t)levels << 16);
    if (!base) return 0;
    ddw = &w;
    GenerateShadeTables();
    SetShadeLevel(numLevels - 1);
    return 1;
}

int Shade16::Init(unsigned long levels, DirectDrawWindow &w, unsigned long g) {
    if (levels > 32) return 0;
    numLevels = levels;
    base = (uint16_t *)malloc((size_t)levels << 16);
    if (!base) return 0;
    ddw = &w;
    SetGammaLevel((float)(long long)g);
    SetShadeLevel(numLevels - 1);
    return 1;
}

void Shade16::UnInit() {
    free(base);
    base = nullptr;
    memset(tables, 0, sizeof tables);
    delete[] colorSlots;
    delete[] colorIndex;
    colorSlots = nullptr;
    colorIndex = nullptr;
}

void Shade16::SetShadeLevel(unsigned long l) {
    if (l == 999999) {
        if (!numLevels || numLevels > 32) return;
        level = numLevels - 1;
        current = tables[level];
        return;
    }
    if (l < numLevels) {
        level = l;
        current = tables[l];
    }
}

void Shade16::SetGammaLevel(float g) {
    if (g <= 10.0f) {
        gamma = (float)((double)(g * 0.1f) * (double)(long long)(numLevels >> 2));
        GenerateShadeTables();
    }
}

void Shade16::GenerateShadeTables() {
    if (!base) return;
    ResetColorTables();
    for (unsigned long l = 0; l < numLevels; l++) {
        tables[l] = base + ((size_t)l << 15);
        float f = (float)(((double)(long long)l + gamma - -1.0) / (double)(long)numLevels);
        if (f > 1.0f) f = 1.0f;
        uint16_t *t = tables[l];
        for (int i = 0; i < 0x8000; i++) {
            int b = (int)((float)(i & 0x1f) * f);
            int g = (int)((float)((i >> 5) & 0x1f) * f);
            int r = (int)((float)((i >> 10) & 0x1f) * f);
            t[i] = ddw->MakePixel16((unsigned char)(r << 3), (unsigned char)(g << 3), (unsigned char)(b << 3));
        }
    }
}

// ---------------------------------------------------------------------------
// Port: coloured light tables (enhanced mode, after the 3dfx build).


void Shade16::ResetColorTables() {
    for (int i = 0; i < 256; i++) {
        packR[i] = ddw->MakePixel16((unsigned char)i, 0, 0);
        packG[i] = ddw->MakePixel16(0, (unsigned char)i, 0);
        packB[i] = ddw->MakePixel16(0, 0, (unsigned char)i);
    }
    gOff = (int)((gamma + 1.0f) * 256.0f);
    if (colorIndex) memset(colorIndex, 0, 0x8000 * sizeof(uint16_t));
    if (colorSlots)
        for (int i = 0; i < kColorSlots; i++) colorSlots[i].used = 0;
}

const uint16_t *Shade16::ColorTable(uint16_t c) {
    c &= 0x7fff;
    int r = c >> 10, g = (c >> 5) & 31, b = c & 31;
    if (r == g && g == b && (unsigned long)r < numLevels) return tables[r];
    if (!colorSlots) {
        colorSlots = new ColorSlot[kColorSlots];
        colorIndex = new uint16_t[0x8000];
        ResetColorTables();
    }
    colorClock++;
    if (int k = colorIndex[c]) {
        colorSlots[k - 1].used = colorClock;
        return colorSlots[k - 1].tab;
    }
    int best = 0;
    for (int i = 1; i < kColorSlots; i++)
        if (colorSlots[i].used < colorSlots[best].used) best = i;
    ColorSlot &s = colorSlots[best];
    if (s.used) colorIndex[s.key] = 0;
    s.key = c;
    s.used = colorClock;
    colorIndex[c] = (uint16_t)(best + 1);
    // the same arithmetic as GenerateShadeTables, per channel
    float f[3];
    const int lv[3] = {r, g, b};
    for (int k = 0; k < 3; k++) {
        f[k] = (float)(((double)lv[k] + gamma + 1.0) / (double)(long)numLevels);
        if (f[k] > 1.0f) f[k] = 1.0f;
    }
    uint16_t pr[32], pg[32], pb[32];
    for (int v = 0; v < 32; v++) {
        pr[v] = packR[(int)((float)v * f[0]) << 3];
        pg[v] = packG[(int)((float)v * f[1]) << 3];
        pb[v] = packB[(int)((float)v * f[2]) << 3];
    }
    for (int i = 0; i < 0x8000; i++) s.tab[i] = (uint16_t)(pr[i >> 10] | pg[(i >> 5) & 31] | pb[i & 31]);
    return s.tab;
}

void Shade16::SetShadeColor(uint16_t c) {
    int r = (c >> 10) & 31, g = (c >> 5) & 31, b = c & 31;
    int m = r > g ? r : g;
    if (b > m) m = b;
    level = (unsigned long)m;
    current = (uint16_t *)ColorTable(c);
}

// ---------------------------------------------------------------------------
// CSprite16

static Shade16 *gCSpriteShade = nullptr;
static int gCSpriteFlag = 0;

void SetCSpriteShadeTable(Shade16 *s) { gCSpriteShade = s; }
void SetCSpriteFlag(int which, int value) {
    if (which == 0) gCSpriteFlag = value;
}

static uint16_t *plainTable() {
    return gCSpriteFlag ? gCSpriteShade->tables[gCSpriteShade->numLevels - 1] : gCSpriteShade->current;
}

int CSprite16::Load(int fd) {
    uint8_t hdr[12];
    w_read(fd, hdr, 12);
    memcpy(&w, hdr + 0, 2); memcpy(&h, hdr + 2, 2);
    memcpy(&hotX, hdr + 4, 2); memcpy(&hotY, hdr + 6, 2);
    memcpy(&size, hdr + 8, 4);
    data = (uint16_t *)malloc(size + 4);
    if (!data) return 0;
    w_read(fd, data, size);
    return 1;
}

int CSprite16::Save(int fd) {
    uint8_t hdr[12];
    memcpy(hdr + 0, &w, 2); memcpy(hdr + 2, &h, 2);
    memcpy(hdr + 4, &hotX, 2); memcpy(hdr + 6, &hotY, 2);
    memcpy(hdr + 8, &size, 4);
    w_write(fd, hdr, 12);
    w_write(fd, data, size);
    return 1;
}

void CSprite16::Release() {
    free(data);
    data = nullptr;
}

void CSprite16::BoundingRect(RECT &r, int x, int y) {
    r.left = x - hotX;
    r.top = y - hotY;
    r.right = r.left + w;
    r.bottom = r.top + h;
}

// Iterate RLE rows. For each row: runs of [count|0x8000 + pixels] (literal) or
// [count] (transparent skip) until the row width is covered.
// Generic clipped renderer: calls fn(dstPixel*, srcValue, spriteX, spriteY)
// for visible literal pixels with sprite-local x in [sx0,sx1), y in [sy0,sy1).
template <typename F>
static void rleWalk(const CSprite16 *s, int sx0, int sx1, int sy0, int sy1, uint8_t *dstRow0, unsigned long pitch, F fn) {
    const uint16_t *p = s->data;
    int W = s->w;
    for (int y = 0; y < sy1; y++) {
        uint16_t *d = (uint16_t *)(dstRow0 + (long)y * (long)pitch);
        int x = 0;
        while (x < W) {
            uint16_t c = *p++;
            int n = c & 0x7fff;
            if (c & 0x8000) {
                if (y >= sy0) {
                    for (int i = 0; i < n; i++) {
                        int xx = x + i;
                        if (xx >= sx0 && xx < sx1) fn(&d[xx], p[i], xx, y);
                    }
                }
                p += n;
            }
            x += n;
        }
    }
}

int CSprite16::Draw(int x, int y, unsigned char *dst, unsigned long pitch) {
    if (!data) return 0;
    uint16_t *t = plainTable();
    x -= hotX; y -= hotY;
    uint8_t *row0 = dst + (long)y * (long)pitch + x * 2;
    rleWalk(this, 0, w, 0, h, row0, pitch, [&](uint16_t *d, uint16_t v, int, int) { *d = t[v]; });
    return 1;
}

// Returns 0 if nothing visible, 2 if the sprite is not clipped at all (full
// ranges filled in), 1 if partially clipped.
static int clipSprite(const CSprite16 *s, int x, int y, Clipper &c, int *out) {
    out[0] = x; out[1] = y; out[2] = s->w; out[3] = s->h;
    int r = c.Clip(out);
    if (r == 2) return 0;
    if (r == 0) { out[6] = 0; out[4] = s->w; out[8] = 0; out[5] = s->h; return 2; }
    return 1;
}

int CSprite16::Draw(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c) {
    if (!data) return 0;
    int x0 = x - hotX, y0 = y - hotY;
    int cr[10];
    if (!clipSprite(this, x0, y0, c, cr)) return 0;
    uint16_t *t = plainTable();
    uint8_t *row0 = dst + (long)y0 * (long)pitch + x0 * 2;
    rleWalk(this, cr[6], cr[4], cr[8], cr[5], row0, pitch, [&](uint16_t *d, uint16_t v, int, int) { *d = t[v]; });
    return 1;
}

int CSprite16::Draw(int x, int y, DirectDrawWindow &w) {
    if (!data) return 0;
    uint8_t *p = nullptr; unsigned long pt = 0;
    w.Lock(&p, &pt);
    Draw(x, y, p, pt);
    w.Unlock();
    return 1;
}

int CSprite16::Draw(int x, int y, DirectDrawWindow &w, Clipper &c) {
    if (!data) return 0;
    uint8_t *p = nullptr; unsigned long pt = 0;
    w.Lock(&p, &pt);
    int r = Draw(x, y, p, pt, c);
    w.Unlock();
    return r;
}

// Four-corner (diamond) lighting: L[0]=top, L[1]=right, L[2]=bottom, L[3]=left.
static void litRows(const CSprite16 *s, const unsigned short *L, int sx0, int sx1, int sy0, int sy1, uint8_t *row0, unsigned long pitch) {
    uint16_t **tabs = gCSpriteShade->tables;
    int W = s->w, H = s->h;
    int half = H / 2;
    if (half == 0) half = 1;
    int left = L[0] << 8, right = L[0] << 8;
    int dL = (((int)L[3] - (int)L[0]) << 8) / half;
    int dR = (((int)L[1] - (int)L[0]) << 8) / half;
    const uint16_t *p = s->data;
    for (int y = 0; y < sy1; y++) {
        uint16_t *d = (uint16_t *)(row0 + (long)y * (long)pitch);
        int step = (right - left) / W;
        int cur = left + 0x80;
        int x = 0;
        while (x < W) {
            uint16_t c = *p++;
            int n = c & 0x7fff;
            if (c & 0x8000) {
                for (int i = 0; i < n; i++) {
                    int xx = x + i;
                    if (y >= sy0 && xx >= sx0 && xx < sx1) d[xx] = tabs[(cur >> 8) & 31][p[i]];
                    cur += step;
                }
                p += n;
            } else {
                cur += step * n;
            }
            x += n;
        }
        if (y == half) {
            left = L[3] << 8;
            dL = (((int)L[2] - (int)L[3]) << 8) / half;
            right = L[1] << 8;
            dR = (((int)L[2] - (int)L[1]) << 8) / half;
        }
        left += dL;
        right += dR;
    }
}

int CSprite16::DrawLit(int x, int y, unsigned char *dst, unsigned long pitch, unsigned short *L) {
    if (!data) return 0;
    if (L[0] == L[1] && L[2] == L[3] && L[0] == L[3]) return Draw(x, y, dst, pitch);
    x -= hotX; y -= hotY;
    litRows(this, L, 0, w, 0, h, dst + (long)y * (long)pitch + x * 2, pitch);
    return 1;
}

int CSprite16::DrawLit(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c, unsigned short *L) {
    if (!data) return 0;
    if (L[0] == L[1] && L[2] == L[3] && L[0] == L[3]) return Draw(x, y, dst, pitch, c);
    int x0 = x - hotX, y0 = y - hotY;
    int cr[10];
    if (!clipSprite(this, x0, y0, c, cr)) return 0;
    litRows(this, L, cr[6], cr[4], cr[8], cr[5], dst + (long)y0 * (long)pitch + x0 * 2, pitch);
    return 1;
}

int CSprite16::DrawLitLeftToRight(int x, int y, unsigned char *dst, unsigned long pitch, unsigned short *L) {
    if (!data) return 0;
    if (L[0] == L[1]) return Draw(x, y, dst, pitch);
    x -= hotX; y -= hotY;
    uint8_t *row0 = dst + (long)y * (long)pitch + x * 2;
    uint16_t *base = gCSpriteShade->base;
    int left = L[0] << 8, right = L[1] << 8;
    const uint16_t *p = data;
    for (int yy = 0; yy < h; yy++) {
        uint16_t *d = (uint16_t *)(row0 + (long)yy * (long)pitch);
        int cur = left + 0x80;
        int step = (right - left) / w;
        int xx = 0;
        while (xx < w) {
            uint16_t c = *p++;
            int n = c & 0x7fff;
            if (c & 0x8000) {
                // the unclipped version restarts the gradient for every run
                cur = left + 0x80;
                step = (right - left) / n;
                for (int i = 0; i < n; i++) {
                    d[xx + i] = base[((cur & 0x1f00) << 7) + p[i]];
                    cur += step;
                }
                p += n;
            } else {
                cur += step * n;
            }
            xx += n;
        }
    }
    return 1;
}

int CSprite16::DrawLitLeftToRight(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c, unsigned short *L) {
    if (!data) return 0;
    if (L[0] == L[1]) return Draw(x, y, dst, pitch, c);
    int x0 = x - hotX, y0 = y - hotY;
    int cr[10];
    int cs = clipSprite(this, x0, y0, c, cr);
    if (!cs) return 0;
    if (cs == 2) {
        // not clipped: the original falls back to the unclipped routine
        return DrawLitLeftToRight(x, y, dst, pitch, L);
    }
    uint16_t **tabs = gCSpriteShade->tables;
    int left = L[0] << 8, right = L[1] << 8;
    int step = (right - left) / w;
    uint8_t *row0 = dst + (long)y0 * (long)pitch + x0 * 2;
    const uint16_t *p = data;
    for (int yy = 0; yy < cr[5]; yy++) {
        uint16_t *d = (uint16_t *)(row0 + (long)yy * (long)pitch);
        int cur = left + 0x80;
        int xx = 0;
        while (xx < w) {
            uint16_t cc = *p++;
            int n = cc & 0x7fff;
            if (cc & 0x8000) {
                for (int i = 0; i < n; i++) {
                    int px = xx + i;
                    if (yy >= cr[8] && px >= cr[6] && px < cr[4]) d[px] = tabs[(cur >> 8) & 31][p[i]];
                    cur += step;
                }
                p += n;
            } else {
                // skipped pixels only advance the gradient once inside the visible span
                for (int i = 0; i < n; i++)
                    if (xx + i >= cr[6]) cur += step;
            }
            xx += n;
        }
    }
    return 1;
}

int CSprite16::DrawTable(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c, const uint16_t *t) {
    if (!data) return 0;
    int x0 = x - hotX, y0 = y - hotY;
    int cr[10];
    if (!clipSprite(this, x0, y0, c, cr)) return 0;
    uint8_t *row0 = dst + (long)y0 * (long)pitch + x0 * 2;
    rleWalk(this, cr[6], cr[4], cr[8], cr[5], row0, pitch, [&](uint16_t *d, uint16_t v, int, int) { *d = t[v]; });
    return 1;
}

static inline uint16_t Mix565(unsigned o, unsigned s, int a) {
    unsigned r = ((o >> 11) * (256 - a) + (s >> 11) * a) >> 8;
    unsigned g = (((o >> 5) & 63) * (256 - a) + ((s >> 5) & 63) * a) >> 8;
    unsigned b = ((o & 31) * (256 - a) + (s & 31) * a) >> 8;
    return (uint16_t)(r << 11 | g << 5 | b);
}

int CSprite16::DrawBlend(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c, int a) {
    if (!data) return 0;
    int x0 = x - hotX, y0 = y - hotY;
    int cr[10];
    if (!clipSprite(this, x0, y0, c, cr)) return 0;
    const uint16_t *t = gCSpriteShade->current;
    uint8_t *row0 = dst + (long)y0 * (long)pitch + x0 * 2;
    // Over a hi-res model (enhanced mode) the hi-res samples are blended
    // too, so the model stays sharp behind see-through walls.
    HiresLayer &h = gHires;
    const bool hires = h.enabled() && gPresentWindow && gPresentWindow->back &&
                       dst == (unsigned char *)PhysRow(gPresentWindow->back, 0);
    uint32_t *own = hires ? h.BackOwnOrigin() : nullptr;
    uint16_t *img = hires ? h.BackImgOrigin() : nullptr;
    const uint32_t tag = h.stamp << 16;
    const int S = h.S;
    rleWalk(this, cr[6], cr[4], cr[8], cr[5], row0, pitch, [&](uint16_t *d, uint16_t v, int sx, int sy) {
        unsigned s = t[v], o = *d;
        uint16_t n = Mix565(o, s, a);
        if (own) {
            int px = x0 + sx, py = y0 + sy;
            uint32_t &w = own[(long)py * gLayout.physW + px];
            if ((w & 0x7fffffff) == (tag | o) && n) {
                uint16_t *hs = img + (long)py * S * h.W + (long)px * S;
                for (int j = 0; j < S; j++)
                    for (int i = 0; i < S; i++) hs[(long)j * h.W + i] = Mix565(hs[(long)j * h.W + i], s, a);
                w = (w & 0x80000000u) | tag | n;
            }
        }
        *d = n;
    });
    return 1;
}

// Port: light colours as 1/256 levels per channel.
struct LightC { int r, g, b; };
static inline LightC unpackLight(uint16_t c) { return {((c >> 10) & 31) << 8, ((c >> 5) & 31) << 8, (c & 31) << 8}; }

// Diamond: colours[0] top, [1] right, [2] bottom, [3] left, bilinear across
// the diamond's own axes.
int CSprite16::DrawLitRGB(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c, const uint16_t *C) {
    if (!data) return 0;
    int x0 = x - hotX, y0 = y - hotY;
    int cr[10];
    if (!clipSprite(this, x0, y0, c, cr)) return 0;
    const LightC T = unpackLight(C[0]), R = unpackLight(C[1]), B = unpackLight(C[2]), L = unpackLight(C[3]);
    const Shade16 *sh = gCSpriteShade;
    const int W = w, H = h;
    uint8_t *row0 = dst + (long)y0 * (long)pitch + x0 * 2;
    rleWalk(this, cr[6], cr[4], cr[8], cr[5], row0, pitch, [&](uint16_t *d, uint16_t v, int sx, int sy) {
        // u, v in 1/256 of the sprite; s runs top->right, t top->left
        int u = ((2 * sx + 1) << 7) / W, vv = ((2 * sy + 1) << 7) / H;
        int s = u + vv - 128, t = vv - u + 128;
        if (s < 0) s = 0;
        if (s > 256) s = 256;
        if (t < 0) t = 0;
        if (t > 256) t = 256;
        int w00 = (256 - s) * (256 - t), w10 = s * (256 - t), w01 = (256 - s) * t, w11 = s * t;
        int lr = (T.r * w00 + R.r * w10 + L.r * w01 + B.r * w11) >> 16;
        int lg = (T.g * w00 + R.g * w10 + L.g * w01 + B.g * w11) >> 16;
        int lb = (T.b * w00 + R.b * w10 + L.b * w01 + B.b * w11) >> 16;
        *d = sh->ShadeRGB(v, lr, lg, lb, Dither4(sx, sy));
    });
    return 1;
}

int CSprite16::DrawLitLeftToRightRGB(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c, const uint16_t *C) {
    if (!data) return 0;
    int x0 = x - hotX, y0 = y - hotY;
    int cr[10];
    if (!clipSprite(this, x0, y0, c, cr)) return 0;
    const LightC A = unpackLight(C[0]), Z = unpackLight(C[1]);
    const Shade16 *sh = gCSpriteShade;
    const int W = w;
    uint8_t *row0 = dst + (long)y0 * (long)pitch + x0 * 2;
    rleWalk(this, cr[6], cr[4], cr[8], cr[5], row0, pitch, [&](uint16_t *d, uint16_t v, int sx, int sy) {
        int t = ((2 * sx + 1) << 7) / W;
        *d = sh->ShadeRGB(v, A.r + (((Z.r - A.r) * t) >> 8), A.g + (((Z.g - A.g) * t) >> 8), A.b + (((Z.b - A.b) * t) >> 8),
                          Dither4(sx, sy));
    });
    return 1;
}

int CSprite16::DrawDithered(int x, int y, unsigned char *dst, unsigned long pitch) {
    if (!data) return 0;
    uint16_t *t = gCSpriteShade->current;
    x -= hotX; y -= hotY;
    uint8_t *row0 = dst + (long)y * (long)pitch + x * 2;
    rleWalk(this, 0, w, 0, h, row0, pitch, [&](uint16_t *d, uint16_t v, int sx, int sy) {
        if (((x + sx + y + sy) & 1) == 0) *d = t[v];
    });
    return 1;
}

int CSprite16::DrawDithered(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c) {
    if (!data) return 0;
    int x0 = x - hotX, y0 = y - hotY;
    int cr[10];
    if (!clipSprite(this, x0, y0, c, cr)) return 0;
    uint16_t *t = gCSpriteShade->current;
    uint8_t *row0 = dst + (long)y0 * (long)pitch + x0 * 2;
    rleWalk(this, cr[6], cr[4], cr[8], cr[5], row0, pitch, [&](uint16_t *d, uint16_t v, int sx, int sy) {
        if (((x0 + sx + y0 + sy) & 1) == 0) *d = t[v];
    });
    return 1;
}

int CSprite16::DrawWhite(int x, int y, unsigned char *dst, unsigned long pitch) {
    if (!data) return 0;
    x -= hotX; y -= hotY;
    uint8_t *row0 = dst + (long)y * (long)pitch + x * 2;
    rleWalk(this, 0, w, 0, h, row0, pitch, [&](uint16_t *d, uint16_t, int, int) { *d = 0xffff; });
    return 1;
}

int CSprite16::DrawWhite(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c) {
    if (!data) return 0;
    int x0 = x - hotX, y0 = y - hotY;
    int cr[10];
    if (!clipSprite(this, x0, y0, c, cr)) return 0;
    uint8_t *row0 = dst + (long)y0 * (long)pitch + x0 * 2;
    rleWalk(this, cr[6], cr[4], cr[8], cr[5], row0, pitch, [&](uint16_t *d, uint16_t, int, int) { *d = 0xffff; });
    return 1;
}

int LoadCSpriteTable(char *file, CSprite16 *table, unsigned long max) {
    int fd = w_open(file, W_O_BINARY);
    if (fd < 0) return 0;
    uint32_t n = 0;
    w_read(fd, &n, 4);
    if (n > max) {
        plat_message_box(file, "Range Error Warning");
        w_close(fd);
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) table[i].Load(fd);
    w_close(fd);
    return (int)n;
}

int LoadCSpriteTableA(char *file, CSprite16 *table, unsigned long max, char *which) {
    int fd = w_open(file, W_O_BINARY);
    if (fd < 0) return 0;
    uint32_t n = 0;
    w_read(fd, &n, 4);
    if (n > max) {
        plat_message_box(file, "Range Error Warning");
        w_close(fd);
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        table[i].Load(fd);
        if (!which[i]) table[i].Release();
    }
    w_close(fd);
    return (int)n;
}

void ReleaseCSpriteTable(CSprite16 *table, unsigned long n) {
    for (unsigned long i = 0; i < n; i++) table[i].Release();
}

unsigned short Convert565to555(unsigned short p) {
    return (unsigned short)(((((p >> 11) & 0x1f) << 5 | ((p >> 6) & 0x1f)) << 5) | (p & 0x1f));
}
unsigned short Convert555to565(unsigned short p) {
    return (unsigned short)(((p & 0xffe0) << 1) | (p & 0x1f));
}
unsigned short Convert555ToGrey(unsigned short p) {
    int s = ((p >> 4) & 0x3e) + ((p >> 10) & 0x1f) + (p & 0x1f);
    int v = s / 3;
    return (unsigned short)((((v << 5) | v) << 5) | v);
}

// ---------------------------------------------------------------------------
// Font / Text

Font::Font() {
    memset(widths, 0, sizeof widths);
    memset(heights, 0, sizeof heights);
    bits = (uint32_t *)calloc(0x8000, 1);
}

int Font::Load(char *file) {
    int fd = w_open(file, W_O_BINARY);
    if (fd < 0) return 0;
    if (w_read(fd, widths, 256) != 256) { w_close(fd); return 0; }
    if (w_read(fd, heights, 256) != 256) { w_close(fd); return 0; }
    if (!bits) bits = (uint32_t *)calloc(0x8000, 1);
    if (w_read(fd, bits, 0x8000) != 0x8000) { w_close(fd); return 0; }
    w_close(fd);
    DetermineFontHeight();
    return 1;
}

void Font::Release() { free(bits); bits = nullptr; }

void Font::DetermineFontHeight() {
    maxHeight = 0;
    for (int i = 0; i < 256; i++)
        if (heights[i] > maxHeight) maxHeight = heights[i];
}

void Text::Init(DirectDrawWindow &w) {
    ddw = &w;
    surfW = w.width;
    surfH = w.height;
    delete surf;
    // (port: as wide as the widescreen framebuffer; surfW stays the
    // 640 pixel width that centred text is measured against)
    surf = w.CreatePlainSurface(surfW > gLayout.physW ? surfW : gLayout.physW, 32, 1, 0, 0);
    surf->fill(nullptr, 0);
    surf->colorKeyed = true;
    surf->colorKey = 0;
    initialised = 1;
}

void Text::Activate(int on) {
    if (!on) return; // surfaces are never lost in the port
    if (initialised && !surf) Init(*ddw);
}

int Text::StringSize(char *s) {
    int n = 0;
    if (!s) return 0;
    for (; *s; s++)
        if (*s != '|') n += font->widths[(unsigned char)*s];
    return n;
}

void Text::PrintChar(int px, unsigned char c) {
    uint32_t *g = font->PtrTo(c);
    int hgt = font->heights[c], wid = font->widths[c];
    for (int r = 0; r < hgt && r < 32; r++) {
        uint32_t b = g[r];
        uint16_t *d = surf->row(r) + px;
        for (int i = 0; i < wid; i++) {
            if (px + i < surf->w) d[i] = (uint16_t)((b & 0x80000000u) ? color : bgColor);
            b <<= 1;
        }
    }
}

void Text::Blt(int dx, int dy, int w) {
    if (!ddw->back) return;
    int mh = font->maxHeight;
    RECT src = {0, 0, w, mh - 1};
    RECT dst = {dx, dy, dx + w, dy + mh - 1};
    int L = gLayout.minX(), T = gLayout.minY(), R = gLayout.maxX() + 1, B = gLayout.maxY() + 1;
    if (dst.left < L) { src.left += L - dst.left; dst.left = L; }
    if (dst.right > R - 1) { src.right += (R - dst.right) - 1; dst.right = R - 1; }
    if (dst.bottom > B - 1) {
        src.bottom += (B - dst.bottom) - 1;
        dst.bottom = B - 1;
        if (src.bottom < src.top) return;
    }
    if (dst.top < T) { src.top += T - dst.top; dst.top = T; }
    SurfaceBlt(ddw->back, dst.left + gLayout.ox, dst.top + gLayout.oy, surf, &src, true);
    surf->fill(nullptr, 0);
}

void Text::BltAA(int dx, int dy, int w) {
    if (!ddw->back) return;
    uint16_t mask = ddw->MakePixel16(0x7f, 0x7f, 0x7f);
    int lh = font->maxHeight;
    // clip against the whole framebuffer (the original: 0..639, 0..479)
    const int T = gLayout.minY(), B = gLayout.maxY() + 1;
    const int L = gLayout.minX(), R = gLayout.maxX() + 1;
    int skip = dy < T ? T - dy : 0;
    if (dy + lh > B - 1) lh = B - dy - 1;
    if (skip > lh) return;
    uint8_t *p = nullptr; unsigned long pt = 0;
    ddw->Lock(&p, &pt);
    for (int r = skip; r < lh && r < 32; r++) {
        uint16_t *s = surf->row(r);
        uint16_t *d = (uint16_t *)(p + (size_t)(dy + r) * pt);
        uint16_t prev = 0;
        int n = w - 1;
        if (n < 1) n = 1;
        for (int i = 0; i < n; i++) {
            int x = dx + i;
            uint16_t v = s[i];
            if (x >= L && x < R) {
                if (v) {
                    if (!prev) d[x] = (uint16_t)(((d[x] >> 1) & mask) + ((v >> 1) & mask));
                    else d[x] = v;
                } else if (prev) {
                    d[x] = (uint16_t)(((d[x] >> 1) & mask) + ((prev >> 1) & mask));
                }
            }
            prev = v;
        }
    }
    ddw->Unlock();
    surf->fill(nullptr, 0);
}

void Text::Print(char *s) {
    if (!font || !s) return;
    int room = gLayout.maxX() + 1 - x;   // (port: to the framebuffer's right edge)
    int pos = 0, acc = 0;
    int restore = 0, saved = 0;
    int len = (int)strlen(s);
    for (int i = 0; i < len; i++) {
        if (restore) { restore = 0; color = saved; }
        unsigned char c = (unsigned char)s[i];
        if (c == '|') {
            saved = color;
            color = highlight;
            restore = 1;
            i++;
            c = (unsigned char)s[i];
        }
        if (room < font->widths[c]) {
            if (acc) {
                if (antiAlias) BltAA(x, y, acc);
                else Blt(x, y, acc);
            }
            pos = 0; acc = 0;
            x = gLayout.minX();
            y += font->heights[c];
            room = gLayout.physW;
        }
        PrintChar(pos, c);
        pos += font->widths[c];
        acc += font->widths[c];
        room -= font->widths[c];
    }
    if (acc) {
        if (x + acc > gLayout.maxX() + 1) acc += gLayout.maxX() + 1 - (x + acc);
        if (acc > 0) {
            if (antiAlias) BltAA(x, y, acc);
            else Blt(x, y, acc);
        }
        x += acc;
    }
}

void Text::Print(int x_, int y_, char *s) {
    SetPosition(x_, y_);
    Print(s);
}
void Text::Print(int x_, int y_, char *s, int c) {
    SetPosition(x_, y_);
    SetColor(c, bgColor);
    Print(s);
}
void Text::Print(int x_, int y_, char *s, int c, int bg) {
    SetPosition(x_, y_);
    SetColor(c, bg);
    Print(s);
}
void Text::PrintC(int y_, char *s) {
    int sz = StringSize(s);
    SetPosition((surfW - sz) / 2, y_);
    Print(s);
}
void Text::PrintC(int y_, char *s, int c) {
    int sz = StringSize(s);
    int xx = (surfW - sz) / 2;
    SetColor(c, bgColor);
    SetPosition(xx, y_);
    Print(s);
}
void Text::PrintCS(int y_, char *s, int shadow) {
    int sz = StringSize(s);
    PrintS((surfW - sz) / 2, y_, s, shadow);
}
void Text::PrintS(int x_, int y_, char *s, int shadow) {
    int c = color;
    SetPosition(x_ + 1, y_ + 1);
    SetColor(shadow, bgColor);
    Print(s);
    color = c;
    SetPosition(x_, y_);
    Print(s);
}
void Text::PrintS(int x_, int y_, char *s, int shadow, int depth) {
    int c = color;
    SetColor(shadow, bgColor);
    for (int i = 1; i <= depth; i++) {
        SetPosition(x_ + i, y_ + i);
        Print(s);
    }
    color = c;
    SetPosition(x_, y_);
    Print(s);
}
void Text::PrintRJ(int x_, int y_, char *s) {
    int sz = StringSize(s);
    SetPosition(x_ - sz, y_);
    Print(s);
}

// ---------------------------------------------------------------------------
// PCX (24-bit, 3 planes, RLE)

int PCX::Init(DirectDrawWindow &w, char *file, int fl) {
    if (raw) ReleaseMem();
    flags = fl;
    ddw = &w;
    if (!Load(file)) return 0;
    delete surf;
    surf = w.CreatePlainSurface(this->w, h, flags, 0, 0);
    SetRect(&rect, 0, 0, this->w - 1, h - 1);
    Decode();
    free(raw);
    raw = nullptr;
    return 1;
}

int PCX::Load(char *file) {
    int fd = w_open(file, W_O_BINARY);
    if (fd < 0) return 0;
    long len = w_filelength(fd);
    raw = (uint8_t *)malloc(len > 0 ? len : 1);
    if (w_read(fd, header, 128) != 128) { w_close(fd); return 0; }
    rawSize = (int)len - 128;
    int16_t x0, y0, x1, y1;
    memcpy(&x0, header + 4, 2); memcpy(&y0, header + 6, 2);
    memcpy(&x1, header + 8, 2); memcpy(&y1, header + 10, 2);
    w = x1 - x0 + 1;
    h = y1 - y0 + 1;
    bitsPerPixel = (int8_t)header[65] * (int8_t)header[3];
    if (w_read(fd, raw, rawSize) != rawSize) ddw->DDError("PCX file read error");
    w_close(fd);
    loaded = 1;
    return 1;
}

void PCX::Decode() {
    if (!loaded) return;
    if (ddw->bpp == 16 && bitsPerPixel == 24) DecodeImage24to16();
    decoded = 1;
}

uint8_t *PCX::ReadLine(uint8_t *src, uint8_t *dst, int n) {
    int i = 0;
    while (i < n) {
        uint8_t c = *src++;
        if (c >= 0xc0) {
            uint8_t v = *src++;
            int cnt = c & 0x3f;
            if (cnt) {
                for (int k = 0; k < cnt && i + k < n + 64; k++) dst[i + k] = v;
                i += cnt;
            }
        } else {
            dst[i++] = c;
        }
    }
    return src;
}

void PCX::DecodeImage24to16() {
    int planes = (int8_t)header[65];
    uint8_t *line = (uint8_t *)malloc((size_t)w * planes + 128);
    uint8_t *src = raw;
    uint16_t bpl;
    memcpy(&bpl, header + 66, 2);
    for (int y = 0; y < h; y++) {
        // the original decodes w*planes bytes per line (bytesPerLine == w for these files)
        src = ReadLine(src, line, w * planes);
        uint16_t *d = surf->row(y);
        for (int x = 0; x < w; x++)
            d[x] = ddw->MakePixel16(line[x], line[w + x], line[2 * w + x]);
    }
    free(line);
    (void)bpl;
}

void PCX::Display(int x, int y) {
    if (!decoded) {
        if (!raw) return;
        Decode();
    }
    if (!ddw->back || !surf) return;
    SurfaceBlt(ddw->back, x + gLayout.ox, y + gLayout.oy, surf, &rect, false);
}

void PCX::Display(int x, int y, unsigned long key) {
    if (!decoded) {
        if (!raw) return;
        Decode();
    }
    if (!ddw->back || !surf) return;
    surf->colorKeyed = true;
    surf->colorKey = (uint16_t)key;
    SurfaceBlt(ddw->back, x + gLayout.ox, y + gLayout.oy, surf, &rect, true);
}

void PCX::KeyOutsideOnly(unsigned long colorKey, int maxPatch) {
    if (!decoded) {
        if (!raw) return;
        Decode();
    }
    if (!surf) return;
    const uint16_t key = (uint16_t)colorKey;
    const uint16_t alt = (uint16_t)(key ^ 0x0020);   // the nearest colour that is not the key
    const int W = surf->w, H = surf->h;
    std::vector<uint8_t> seen((size_t)W * H, 0);
    std::vector<int> stack, patch;
    for (int sy = 0; sy < H; sy++)
        for (int sx = 0; sx < W; sx++) {
            size_t s0 = (size_t)sy * W + sx;
            if (seen[s0] || surf->row(sy)[sx] != key) continue;
            // one connected patch of the key colour
            bool edge = false;
            patch.clear();
            stack.push_back((int)s0);
            seen[s0] = 1;
            while (!stack.empty()) {
                int i = stack.back();
                stack.pop_back();
                patch.push_back(i);
                int x = i % W, y = i / W;
                if (x == 0 || y == 0 || x == W - 1 || y == H - 1) edge = true;
                const int nx[4] = {x - 1, x + 1, x, x}, ny[4] = {y, y, y - 1, y + 1};
                for (int k = 0; k < 4; k++) {
                    if (nx[k] < 0 || ny[k] < 0 || nx[k] >= W || ny[k] >= H) continue;
                    size_t j = (size_t)ny[k] * W + nx[k];
                    if (seen[j] || surf->row(ny[k])[nx[k]] != key) continue;
                    seen[j] = 1;
                    stack.push_back((int)j);
                }
            }
            if (edge || (int)patch.size() >= maxPatch) continue;
            for (int i : patch) surf->row(i / W)[i % W] = alt;
        }
}

void PCX::ReleaseMem() {
    free(raw);
    raw = nullptr;
    delete surf;
    surf = nullptr;
    loaded = 0;
}

// ---------------------------------------------------------------------------
// Sprite / SpriteTable

Sprite::~Sprite() {
    delete surf;
    surf = nullptr;
}

int Sprite::Init(int w_, int h_, DirectDrawWindow &d, uint8_t *pixels) {
    w = w_; h = h_;
    surf = d.CreatePlainSurface(w, h, 1, 1, 0);
    for (int y = 0; y < h; y++) memcpy(surf->row(y), pixels + (size_t)y * w * 2, (size_t)w * 2);
    SetRect(&src, 0, 0, w, h);
    return 1;
}

int Sprite::ClipBlt(DirectDrawWindow &d, int x, int y, int keyed) {
    if (!surf || !d.back) return 0;
    RECT s = src;
    RECT dr = {x, y, x + w, y + h};
    int L = gLayout.minX(), T = gLayout.minY(), R = gLayout.maxX() + 1, B = gLayout.maxY() + 1;
    if (dr.right < L || dr.bottom < T || dr.left >= R || dr.top >= B) return 0;
    if (x < L) { s.left += L - x; dr.left = L; }
    if (y < T) { s.top += T - y; dr.top = T; }
    if (dr.right >= R) { s.right += (R - dr.right) - 1; dr.right = R - 1; }
    if (dr.bottom >= B) { s.bottom += (B - dr.bottom) - 1; dr.bottom = B - 1; }
    SurfaceBlt(d.back, dr.left + gLayout.ox, dr.top + gLayout.oy, surf, &s, keyed != 0);
    return 1;
}

void Sprite::Grab(DirectDrawWindow &d, int x, int y, int w_, int h_) {
    SetRect(&src, 0, 0, w_, h_);
    w = w_; h = h_;
    delete surf;
    surf = d.CreatePlainSurface(w, h, 1, 0, 0);
    uint8_t *p = nullptr; unsigned long pt = 0;
    d.Lock(&p, &pt);
    for (int yy = 0; yy < h; yy++) {
        int sy = y + yy;
        if (sy < gLayout.minY() || sy > gLayout.maxY()) continue;
        for (int xx = 0; xx < w; xx++) {
            int sx = x + xx;
            if (sx < gLayout.minX() || sx > gLayout.maxX()) continue;
            surf->row(yy)[xx] = ((uint16_t *)(p + (long)sy * (long)pt))[sx];
        }
    }
    d.Unlock();
}

SpriteTable::~SpriteTable() { Unload(); }

unsigned long SpriteTable::Load(char *file, DirectDrawWindow &d) {
    ddw = &d;
    if (count) Unload();
    int fd = w_open(file, W_O_BINARY);
    if (fd < 0) { d.DDError("cannot open sprite file"); return 0; }
    uint32_t hdr[16];
    w_read(fd, hdr, 64);
    if (memcmp(hdr, "SPR ", 4) != 0 || hdr[1] < 0x100) d.DDError("not a sprite file or wrong version");
    count = hdr[2];
    if (count > 200) count = 200;
    int32_t tmp[200];
    w_read(fd, tmp, count * 4);
    for (unsigned i = 0; i < count; i++) widths[i] = tmp[i];
    w_read(fd, tmp, count * 4);
    for (unsigned i = 0; i < count; i++) heights[i] = tmp[i];
    // files are RGB565, the same format as the port's framebuffer
    for (unsigned i = 0; i < count; i++) {
        size_t n = (size_t)widths[i] * heights[i] * 2;
        uint8_t *buf = (uint8_t *)malloc(n + 2);
        if ((size_t)w_read(fd, buf, (unsigned)n) != n) d.DDError("error reading sprite data");
        sprites[i] = new Sprite();
        sprites[i]->Init(widths[i], heights[i], d, buf);
        free(buf);
    }
    w_close(fd);
    return count;
}

void SpriteTable::Unload() {
    for (unsigned i = 0; i < 200; i++) {
        delete sprites[i];
        sprites[i] = nullptr;
    }
    count = 0;
}

int SpriteTable::Blt(unsigned long n, int x, int y, unsigned long flags) {
    if (n >= count || !sprites[n]) return 0;
    return sprites[n]->ClipBlt(*ddw, x, y, (flags & 1) ? 0 : 1);
}

int SpriteTable::CentreBlt(unsigned long n, int y) {
    return Blt(n, (int)((unsigned)(ddw->width - widths[n]) >> 1), (int)((unsigned)(ddw->height - heights[n]) >> 1), (unsigned long)y);
}
