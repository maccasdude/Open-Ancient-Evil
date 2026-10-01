// Reimplementation of Silver Lightning's "ddw16.dll" 2D engine (DirectDraw
// window, 16bpp sprites, PCX loader, bitmap fonts, shade tables) on top of a
// plain software framebuffer. Method names and semantics follow the original
// DLL exports; the DirectDraw specifics are replaced by in-memory surfaces
// that the platform layer presents with SDL2.
#pragma once
#include <vector>
#include "../platform/wincompat.h"

// Screen layout. The game draws in the original 640x480 coordinates; the
// physical framebuffer can be larger (enhanced widescreen mode), with the
// original screen placed at (ox, oy). Coordinates outside 0..639 / 0..479
// then address the extra border, down to -ox / -oy.
struct ScreenLayout {
    int physW = 640, physH = 480;   // framebuffer size
    int ox = 0, oy = 0;             // where logical (0,0) sits
    int hiresScale = 1;             // 3D models are also drawn at this scale
    bool wide() const { return physW != 640 || physH != 480; }
    int minX() const { return -ox; }
    int minY() const { return -oy; }
    int maxX() const { return physW - ox - 1; }   // inclusive
    int maxY() const { return physH - oy - 1; }
};
extern ScreenLayout gLayout;
// A rectangle (x0,y0)-(x1,y1) (exclusive) that spans the whole original width
// is widened to the whole framebuffer; one that also starts at the top is
// extended up to the framebuffer's top edge.
void ExtendFullWidth(int &x0, int &y0, int &x1, int &y1);

// Hi-resolution layer for 3D models (enhanced mode). Models are rendered a
// second time at S times the resolution; the low-resolution pass records
// which framebuffer pixels it wrote (owner = stamp << 16 | colour). When a
// frame is presented, every pixel still showing the colour its model wrote is
// replaced by the S x S hi-res samples; everything else is pixel-multiplied.
// Double-buffered like the framebuffer: the back pair is drawn into, the front
// pair is what the presented frame uses.
struct HiresLayer {
    int S = 1;                 // scale, 1 = off
    int W = 0, H = 0;          // hi-res size (framebuffer size * S)
    uint16_t *img[2] = {};     // hi-res model pixels (W x H)
    uint32_t *own[2] = {};     // per framebuffer pixel: stamp << 16 | colour (bit 31: background copied)
    uint16_t *out = nullptr;   // composited image (W x H)
    int back = 0;
    uint32_t stamp = 1;        // stamp of the frame being drawn
    uint32_t frontStamp = 0;   // stamp of the presented frame
    bool drewThisFrame = false;
    // physical boxes the models of each layer drew in: a box whose model
    // pixels are all gone (drawn over by the game's background) drops its
    // background-copied hi-res pixels, else thin details the low-res pass
    // missed stayed on screen until the next frame with a model
    std::vector<RECT> boxes[2];
    bool enabled() const { return S > 1 && out; }
    // pointers at logical (0,0) of the back buffers
    uint16_t *BackImgOrigin() const { return img[back] + (size_t)(gLayout.oy * S) * W + gLayout.ox * S; }
    uint32_t *BackOwnOrigin() const { return own[back] + (size_t)gLayout.oy * gLayout.physW + gLayout.ox; }
};
extern HiresLayer gHires;

// A plain 16-bit (RGB565) offscreen surface.
struct Surface {
    int w = 0, h = 0;
    int pitch = 0;          // bytes
    uint16_t *px = nullptr;
    bool colorKeyed = false;
    uint16_t colorKey = 0;

    Surface(int w_, int h_);
    ~Surface();
    uint16_t *row(int y) { return (uint16_t *)((uint8_t *)px + (size_t)y * pitch); }
    void fill(const RECT *r, uint16_t c);
};

// Blit src (optionally a sub-rect) to dst at (x,y); honours src color key if useKey.
void SurfaceBlt(Surface *dst, int dx, int dy, Surface *src, const RECT *srcRect, bool useKey);

class DirectDrawWindow {
public:
    HWND hwnd = nullptr;
    RECT clientRect = {0, 0, 0, 0};   // original +0x04
    int lastError = 0;               // +0x18
    int width = 0, height = 0, bpp = 0; // +0x1c/+0x20/+0x24
    uint8_t *surfacePtr = nullptr;   // +0x28 (valid while locked)
    int pitch = 0;                   // +0x2c
    Surface *back = nullptr;         // +0x30
    int frameCount = 0;              // +0x34
    int active = 0;                  // +0x38
    int initialised = 0;             // +0x3c
    int fullscreen = 0;              // +0x44
    int pageFlipping = 0;            // +0x48
    uint8_t backBufferType = 2;      // +0x4c
    int surfacesCreated = 0;         // +0x50
    int lockCount = 0;               // +0x54
    Surface *front = nullptr;        // +0x5c
    void (*preUpdate)() = nullptr;   // +0x64
    void (*postUpdate)() = nullptr;  // +0x68
    int modeNumber = 0;              // +0x6c
    int numBuffers = 2;              // +0x70
    // pixel format (+0x78..+0x9c)
    int redBits = 5, redShift = 11, redMask = 0xf800;
    int greenBits = 6, greenShift = 5, greenMask = 0x07e0;
    int blueBits = 5, blueShift = 0, blueMask = 0x001f;
    int bytesPerPixel = 2;

    void *Init(void *hInstance, char *className, int fullscreen);
    int GetFreeMem();
    void End();
    int SetModeNumber(int w, int h, int bpp);
    void AddVideoMode(int w, int h, int bpp);
    int SetVideoMode();
    void RestoreVideoMode();
    int CreatePrimarySurface(int);
    void DetermineColorBits();
    void RegisterUpdateCallback(void (*cb)(), unsigned long which);
    void UpdateScreen();
    int UpdateScreenNoWait();
    Surface *CreatePlainSurface(int w, int h, int sysmem, int colorKey, int flags);
    int Clear(unsigned long color);
    void FillRect(int x, int y, int w, int h, int color);
    void FillRect(RECT *r, int color);
    void TintRect(int x, int y, int w, int h, unsigned short color);
    void GreyRect(int x, int y, int w, int h);
    void SetClippingWindowSize(int, int, int, int) {}
    void OnMove();
    void OnActivate(int active);
    int Lock(uint8_t **ptr, unsigned long *pitch);
    void Unlock();
    int LockFront(uint8_t **ptr, unsigned long *pitch);
    void UnlockFront();
    void UnlockFront(int x, int y, int w, int h);   // port: only this (logical) area changed
    unsigned short MakePixel16(unsigned char r, unsigned char g, unsigned char b);
    void GetRGBPixel(int x, int y, unsigned char *r, unsigned char *g, unsigned char *b);
    void GetColorBits(unsigned char *r, unsigned char *g, unsigned char *b);
    void BreakPixel(unsigned short p, unsigned char *r, unsigned char *g, unsigned char *b);
    void PageFlipping(int on);
    int IsPageFlipping() { return pageFlipping; }
    void SetNumBuffers(int n) { if (!front) numBuffers = n; }
    void SetBackBufferType(unsigned char t) { if (!front) backBufferType = t; }
    void Fade();
    void Flip2GDI() {}
    void DDError(const char *msg);
};

class Clipper {
public:
    int screenW = 0, screenH = 0;
    int left = 0, top = 0, right = 0, bottom = 0;
    void SetScreenSize(int w, int h) { screenW = w; screenH = h; }
    void SetViewport(int l, int t, int r, int b);
    // rect: in [0..3] = x,y,w,h. out [4]=xEnd,[5]=yEnd,[6]=skipX,[7]=visW,[8]=skipY,[9]=visH
    int Clip(int *rect);
};

class Shade16 {
public:
    uint16_t *base = nullptr;        // +0x00 all tables
    uint16_t *tables[32] = {};       // +0x04
    uint16_t *current = nullptr;     // +0x84
    unsigned long level = 0;         // +0x88
    unsigned long numLevels = 0;     // +0x8c
    float gamma = 0;                 // +0x90
    DirectDrawWindow *ddw = nullptr; // +0x94

    Shade16() {}
    int Init(unsigned long levels, DirectDrawWindow &w);
    int Init(unsigned long levels, DirectDrawWindow &w, unsigned long gamma);
    void UnInit();
    void SetShadeLevel(unsigned long lvl);
    void SetGammaLevel(float g);
    void GenerateShadeTables();
    uint16_t *GetShadeTablePtr() { return current; }
    uint16_t **GetShadeArrayPtr() { return tables; }
    uint16_t *GetShadeBasePtr() { return base; }
    uint16_t GetColor(unsigned long c) { return current[c]; }
    unsigned long GetShadeLevel() { return level; }
    uint16_t *GetBrightShadePtr() { return tables[numLevels - 1]; }

    // Port (enhanced mode): coloured light as in the 3dfx build. A light
    // colour is a 555 value holding one level (0-31) per channel.
    void SetShadeColor(uint16_t c555);
    const uint16_t *ColorTable(uint16_t c555);
    // Shade one texel by levels given in 1/256 steps (for smooth gradients).
    // 'dith' (0-7) is an ordered dither offset: it spreads the rounding to
    // 5/6 bits so smooth light gradients do not band.
    inline uint16_t ShadeRGB(uint16_t v, int lr, int lg, int lb, int dith = 0) const {
        int fr = lr + gOff, fg = lg + gOff, fb = lb + gOff;
        if (fr > kFull) fr = kFull;
        if (fg > kFull) fg = kFull;
        if (fb > kFull) fb = kFull;
        if (fr < 0) fr = 0;
        if (fg < 0) fg = 0;
        if (fb < 0) fb = 0;
        int r8 = ((((v >> 10) & 31) * fr) >> 10) + dith, g8 = ((((v >> 5) & 31) * fg) >> 10) + (dith >> 1);
        int b8 = (((v & 31) * fb) >> 10) + dith;
        return (uint16_t)(packR[r8 > 255 ? 255 : r8] | packG[g8 > 255 ? 255 : g8] | packB[b8 > 255 ? 255 : b8]);
    }
    enum { kFull = 32 << 8 };
    int gOff = 256;                  // (gamma + 1) in 1/256 levels
    uint16_t packR[256] = {}, packG[256] = {}, packB[256] = {};
    struct ColorSlot;
    ColorSlot *colorSlots = nullptr;
    uint16_t *colorIndex = nullptr;  // colour -> slot + 1
    uint32_t colorClock = 0;
    void ResetColorTables();
};

// 4x4 ordered dither offsets (0-7) for ShadeRGB.
inline int Dither4(int x, int y) {
    static const uint8_t k[4][4] = {{0, 4, 1, 5}, {6, 2, 7, 3}, {1, 5, 0, 4}, {7, 3, 6, 2}};
    return k[y & 3][x & 3];
}

// Grey 555 light colour of a level.
inline uint16_t GreyLight(int l) { if (l < 0) l = 0; if (l > 31) l = 31; return (uint16_t)(l | l << 5 | l << 10); }

// Run-length encoded 15-bit sprite (".CST" entries).
class CSprite16 {
public:
    uint16_t *data = nullptr;
    int16_t w = 0, h = 0, hotX = 0, hotY = 0;
    uint32_t size = 0;

    CSprite16() {}
    uint16_t *Ptr() { return data; }
    int Load(int fd);
    int Save(int fd);
    void Release();
    int Draw(int x, int y, unsigned char *dst, unsigned long pitch);
    int Draw(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c);
    int Draw(int x, int y, DirectDrawWindow &w);
    int Draw(int x, int y, DirectDrawWindow &w, Clipper &c);
    int DrawLit(int x, int y, unsigned char *dst, unsigned long pitch, unsigned short *lights);
    int DrawLit(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c, unsigned short *lights);
    int DrawLitLeftToRight(int x, int y, unsigned char *dst, unsigned long pitch, unsigned short *lights);
    int DrawLitLeftToRight(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c, unsigned short *lights);
    // Port: drawn through the given shade table (the cursor thread must not
    // touch the shared sprite state).
    int DrawTable(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c, const uint16_t *table);
    // Port: drawn over the screen with the given opacity (0-256).
    int DrawBlend(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c, int alpha);
    // Port: the same with 555 light colours, smoothly interpolated.
    int DrawLitRGB(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c, const uint16_t *colors);
    int DrawLitLeftToRightRGB(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c, const uint16_t *colors);
    int DrawDithered(int x, int y, unsigned char *dst, unsigned long pitch);
    int DrawDithered(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c);
    int DrawWhite(int x, int y, unsigned char *dst, unsigned long pitch);
    int DrawWhite(int x, int y, unsigned char *dst, unsigned long pitch, Clipper &c);
    long Width() { return w; }
    long Height() { return h; }
    void SetHotspot(short x, short y) { hotX = x; hotY = y; }
    void GetHotspot(short *x, short *y) { *x = hotX; *y = hotY; }
    void BoundingRect(RECT &r, int x, int y);
};

int LoadCSpriteTable(char *file, CSprite16 *table, unsigned long max);
int LoadCSpriteTableA(char *file, CSprite16 *table, unsigned long max, char *which);
void ReleaseCSpriteTable(CSprite16 *table, unsigned long n);
void SetCSpriteShadeTable(Shade16 *s);
void SetCSpriteFlag(int which, int value);

unsigned short Convert565to555(unsigned short p);
unsigned short Convert555to565(unsigned short p);
unsigned short Convert555ToGrey(unsigned short p);

class Font {
public:
    uint8_t widths[256];
    uint8_t heights[256];
    uint8_t maxHeight = 0;
    uint32_t *bits = nullptr;  // 256 glyphs * 32 rows of 32 bits

    Font();
    ~Font() {}
    int Load(char *file);
    void Release();
    void DetermineFontHeight();
    uint32_t *PtrTo(unsigned char c) { return bits + (size_t)c * 32; }
};

class Text {
public:
    int initialised = 0;       // +0x00
    int antiAlias = 0;         // +0x04
    int x = 0, y = 0;          // +0x08
    int color = 0, bgColor = 0; // +0x10
    int highlight = 0;         // +0x18
    int surfW = 0, surfH = 0;  // +0x1c
    DirectDrawWindow *ddw = nullptr; // +0x24
    Surface *surf = nullptr;   // +0x28
    Font *font = nullptr;      // +0x2c

    Text() {}
    void Init(DirectDrawWindow &w);
    void AntiAlias(int on) { antiAlias = on; }
    void Activate(int on);
    void SetFont(Font *f) { font = f; }
    void SetColor(int fg, int bg) { color = fg; bgColor = bg; }
    void SetPosition(int x_, int y_) { x = x_; y = y_; }
    void SetHighlightColor(int c) { highlight = c; }
    void Print(char *s);
    void Print(int x, int y, char *s);
    void Print(int x, int y, char *s, int color);
    void Print(int x, int y, char *s, int color, int bg);
    void PrintC(int y, char *s);
    void PrintC(int y, char *s, int color);
    void PrintCS(int y, char *s, int shadow);
    void PrintS(int x, int y, char *s, int shadow);
    void PrintS(int x, int y, char *s, int shadow, int depth);
    void PrintRJ(int x, int y, char *s);
    int StringSize(char *s);
    void BltAA(int x, int y, int w);
    void Blt(int x, int y, int w);
    void PrintChar(int x, unsigned char c);
};

class PCX {
public:
    int loaded = 0;            // +0x00
    int decoded = 0;           // +0x04
    int flags = 0;             // +0x08
    uint8_t header[128];       // +0x0c
    DirectDrawWindow *ddw = nullptr; // +0x8c
    int w = 0, h = 0;          // +0x90
    int bitsPerPixel = 0;      // +0x98
    RECT rect;                 // +0x9c
    Surface *surf = nullptr;   // +0xac
    uint8_t *raw = nullptr;    // +0xb0
    int rawSize = 0;

    PCX() {}
    ~PCX() { ReleaseMem(); }
    int Init(DirectDrawWindow &w, char *file, int flags);
    int Load(char *file);
    void Decode();
    void Display(int x, int y);
    void Display(int x, int y, unsigned long colorKey);
    void DecodeImage24to16();
    // port: keep the key colour see-through only where it reaches the
    // picture's edge or fills a large enclosed hole; small enclosed patches
    // of it (dark parts of the art, under maxPatch pixels) become opaque
    void KeyOutsideOnly(unsigned long colorKey, int maxPatch = 400);
    uint8_t *ReadLine(uint8_t *src, uint8_t *dst, int n);
    void Release() { ReleaseMem(); }
    void ReleaseMem();
    Surface *GetSurfacePtr() { return surf; }
    uint8_t *GetHeaderPtr() { return header; }
};

class Sprite {
public:
    int w = 0, h = 0;          // +0x00
    Surface *surf = nullptr;   // +0x08
    RECT src = {0, 0, 0, 0};   // +0x0c

    Sprite() {}
    ~Sprite();
    int Init(int w, int h, DirectDrawWindow &d, uint8_t *pixels);
    int Blt(DirectDrawWindow &d, int x, int y) { return ClipBlt(d, x, y, 0); }
    int TransBlt(DirectDrawWindow &d, int x, int y) { return ClipBlt(d, x, y, 1); }
    int ClipBlt(DirectDrawWindow &d, int x, int y, int keyed);
    void Grab(DirectDrawWindow &d, int x, int y, int w, int h);
};

class SpriteTable {
public:
    DirectDrawWindow *ddw = nullptr;
    unsigned long count = 0;
    int widths[200] = {};
    int heights[200] = {};
    Sprite *sprites[200] = {};

    SpriteTable() {}
    ~SpriteTable();
    unsigned long Load(char *file, DirectDrawWindow &d);
    void Unload();
    int Blt(unsigned long n, int x, int y, unsigned long flags);
    int CentreBlt(unsigned long n, int y);
};

// The game's global DirectDrawWindow (used by the platform layer for presentation).
extern DirectDrawWindow *gPresentWindow;
