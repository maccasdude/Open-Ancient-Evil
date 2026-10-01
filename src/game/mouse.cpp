// Debug log file (0x40bc80-0x40bdc0) and the DirectInput mouse with its
// software cursor (0x40bdf0-0x40c8c0).
#include "game.h"
#include <atomic>
#include <stdarg.h>

// ---------------------------------------------------------------------------
// Log
// ---------------------------------------------------------------------------

// 0x40bc80
void Log::Construct() { file = nullptr; }

// 0x40bc90
int Log::Open(const char *fname)
{
    file = w_fopen(fname, "w");
    if (!file) return 0;
    strcpy(name, fname);
    return 1;
}

// 0x40bce0: append a line; the file is closed and reopened after each write.
void LogPrintf(Log *log, const char *fmt, ...)
{
    char buf[0x100];
    if (!log->file) return;
    buf[0] = 0;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf + strlen(buf), sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(buf, log->file);
    fflush(log->file);
    fclose(log->file);
    log->file = w_fopen(log->name, "a");
}

// 0x40bd80
void Log::Close()
{
    if (file) {
        fflush(file);
        fclose(file);
        file = nullptr;
    }
}

// 0x40bdc0
void Log::Destroy()
{
    if (file) fclose(file);
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

static DirectDrawWindow *gMouseDDW;      // 0x4afb78
static void *gMouseHwnd;                 // 0x4b14c8
static std::recursive_mutex gMouseCS;    // 0x4afb60
static Clipper gMouseClip;               // 0x4afb88
static int gMouseBackPage;               // 0x4afb80
static int gMouseFrontPage;              // 0x4b1628
static int gSavedX[2], gSavedY[2];       // 0x4afba0 / 0x4afba8
static int gSaved[2];                    // 0x4afbb8
static uint8_t gSaveBuf[2][0x3200];      // 0x4afbc8
static CSprite16 gCursors[20];           // 0x4b14d0
static int gNumCursors;                  // 0x4b1638
static unsigned gMouseTimer;             // 0x4b163c
static volatile int gCursorDrawing;      // 0x4b1640
static volatile int gTimerBusy;          // 0x4b1644
static volatile int gFlipBusy;           // 0x4b1648
static int gLeftDown, gRightDown;        // 0x4b164c / 0x4b1650
static std::atomic<int> gLeftClicks, gRightClicks;    // 0x4b1654 / 0x4b1658 (port: the cursor thread counts too)
static int gMousePosX = 320;             // 0x448784
static int gMousePosY = 240;             // 0x448788
static int gMouseMaxX = 639;             // 0x44878c
static int gMouseMaxY = 479;             // 0x448790
static int gMouseMinX, gMouseMinY;       // 0x4b1630 / 0x4b1634
static int gCursorIndex = -1;            // 0x448794
static int gMouseSpeed = 2;              // 0x448798
static int gCursorVisible = 1;           // 0x44879c
static int gLastDrawX = -1;              // 0x4487a0
static int gLastDrawY = 99999999;        // 0x4487a4
static int gCursorFrame;                 // 0x4487a8

static void MouseTimerProc(unsigned, unsigned, unsigned long, unsigned long, unsigned long);

// 0x40be00 (static initialiser): the cursor sprites
void StaticInit_Cursors() {}

// 0x40be20: set up the mouse. DirectInput is replaced by the platform layer.
int MouseInit(DirectDrawWindow *ddw, void *hwnd)
{
    gMouseDDW = ddw;
    gMouseHwnd = hwnd;
    plat_mouse_capture(true);
    if (!MouseStartTimer()) return 0;
    gMouseClip.SetScreenSize(gMouseDDW->width, gMouseDDW->height);
    gMouseClip.SetViewport(0, 0, gMouseDDW->width - 1, gMouseDDW->height - 1);
    MouseUpdateRange();   // enhanced mode: the cursor reaches the wider screen
    gMouseFrontPage = 0;
    gMouseBackPage = 1;
    return 1;
}

// 0x40bfa0
int MouseStartTimer()
{
    if (!gMouseDDW) return 1;
    if (!gMouseTimer) {
        gMouseTimer = plat_timer_start(0x42, MouseTimerProc, 0);
        if (!gMouseTimer) plat_message_box("unable to make timer", "DirectInput Error");
    }
    return gMouseTimer != 0;
}

// 0x40bff0: DirectDrawWindow pre-update callback: put the cursor into the
// back buffer before it becomes visible.
void MousePreFlip()
{
    int x, y;
    gFlipBusy = 1;
    MouseRead(&x, &y);
    uint8_t *p;
    unsigned long pitch;
    gMouseDDW->Lock(&p, &pitch);
    MouseSaveUnder(gMouseBackPage, gMouseDDW->surfacePtr, gMouseDDW->pitch);
    MouseDrawCursor(gMouseBackPage, gMouseDDW->surfacePtr, gMouseDDW->pitch);
    gMouseDDW->Unlock();
}

// 0x40c070: post-update callback: the pages have swapped, remove the cursor
// from the new back buffer.
void MousePostFlip()
{
    if (gLayout.wide() && !gMouseDDW->IsPageFlipping()) {
        // port: the widescreen mode copies the back buffer instead of
        // flipping, so the save under the cursor just made belongs to both
        // buffers (with the pages swapped, cursors were left behind in the
        // side borders, which no screen redraws)
        gMouseFrontPage = gMouseBackPage;
    } else {
        gMouseBackPage ^= 1;
        gMouseFrontPage ^= 1;
    }
    uint8_t *p;
    unsigned long pitch;
    gMouseDDW->Lock(&p, &pitch);
    MouseRestoreUnder(gMouseBackPage, gMouseDDW->surfacePtr, gMouseDDW->pitch);
    gMouseDDW->Unlock();
    // (shared page: the front buffer still shows the cursor over this save)
    if (gLayout.wide() && !gMouseDDW->IsPageFlipping()) gSaved[gMouseFrontPage] = 1;
    gFlipBusy = 0;
}

// 0x40c0e0
void MouseStopTimer()
{
    if (gMouseTimer) plat_timer_stop(gMouseTimer);
    gMouseTimer = 0;
}

// 0x40c100
void MouseShutdown()
{
    if (gMouseTimer) plat_timer_stop(gMouseTimer);
    gMouseTimer = 0;
    if (gNumCursors) ReleaseCSpriteTable(gCursors, gNumCursors);
    plat_mouse_capture(false);
}

// 0x40c1a0: timer callback: move the cursor on the visible screen.
static void MouseTimerProc(unsigned, unsigned, unsigned long, unsigned long, unsigned long)
{
    if (gCursorDrawing || gTimerBusy || gFlipBusy) return;
    gTimerBusy = 1;
    int x, y;
    MouseRead(&x, &y);
    if (x != gLastDrawX || y != gLastDrawY) {
        uint8_t *p;
        unsigned long pitch;
        if (gMouseDDW->LockFront(&p, &pitch)) {
            const int page = gMouseFrontPage;
            int ox = gSavedX[page], oy = gSavedY[page], had = gSaved[page];
            MouseRestoreUnder(page, p, pitch);
            MouseSaveUnder(page, p, pitch);
            MouseDrawCursor(page, p, pitch);
            // port: only the old and new cursor areas are presented again
            int nx = gSavedX[page], ny = gSavedY[page];
            if (!had) {
                ox = nx;
                oy = ny;
            }
            const int m = 64;   // (larger than any cursor)
            int l = (ox < nx ? ox : nx), t = (oy < ny ? oy : ny);
            int r = (ox > nx ? ox : nx) + m, b = (oy > ny ? oy : ny) + m;
            gMouseDDW->UnlockFront(l, t, r - l, b - t);
        }
        gLastDrawX = x;
        gLastDrawY = y;
    }
    gTimerBusy = 0;
}

// 0x40c2d0: acquire / unacquire the device.
void MouseAcquire(int on)
{
    plat_mouse_capture(on != 0);
}

// 0x40c300: read the device and update the position and click counters.
int MouseRead(int *px, int *py)
{
    static std::mutex readLock;   // (the timer thread reads too)
    std::lock_guard<std::mutex> lk(readLock);
    PlatMouseState st;
    plat_mouse_poll(&st);
    int x = gMousePosX + gMouseSpeed * st.dx;
    int y = gMousePosY + gMouseSpeed * st.dy;
    gMousePosX = x;
    gMousePosY = y;
    if (st.buttons[0]) {
        gLeftDown = 1;
    } else if (gLeftDown) {
        gLeftDown = 0;
        gLeftClicks++;
    }
    if (st.buttons[1]) {
        gRightDown = 1;
    } else if (gRightDown) {
        gRightDown = 0;
        gRightClicks++;
    }
    if (x < gMouseMinX || x > gMouseMaxX) {
        x = x < gMouseMinX ? gMouseMinX : gMouseMaxX;
        gMousePosX = x;
    }
    if (y < gMouseMinY)
        gMousePosY = gMouseMinY;
    else if (y > gMouseMaxY)
        gMousePosY = gMouseMaxY;
    *px = x;
    *py = gMousePosY;
    return 1;
}

// 0x40c430
void MouseDrawCursor(int page, uint8_t *dst, unsigned long pitch)
{
    (void)page;
    if (!gMouseDDW->active || gCursorDrawing) return;
    gCursorDrawing = 1;
    {
        std::lock_guard<std::recursive_mutex> lk(gMouseCS);
        if (gNumCursors && gCursorIndex >= 0) {
            if (gCursorVisible) {
                // (port: the full-bright table given directly; the original
                // switched a global sprite flag, racing the main thread)
                gCursors[gCursorIndex].DrawTable(gMouseDDW->clientRect.left + gMousePosX,
                                                 gMouseDDW->clientRect.top + gMousePosY, dst, pitch, gMouseClip,
                                                 gShade.GetBrightShadePtr());
            }
            gCursorFrame = gMouseDDW->frameCount;
        }
    }
    gCursorDrawing = 0;
}

// 0x40c500: save the screen under the cursor for 'page'.
void MouseSaveUnder(int page, uint8_t *src, unsigned long pitch)
{
    if (!gNumCursors) return;
    CSprite16 *c = &gCursors[gCursorIndex];
    int w = c->Width();
    int h = c->Height();
    short hx, hy;
    c->GetHotspot(&hx, &hy);
    gSavedX[page] = gMouseDDW->clientRect.left - hx + gMousePosX;
    gSavedY[page] = gMouseDDW->clientRect.top - hy + gMousePosY;
    // (the screen edges of the widescreen mode are the layout's)
    int minX = gLayout.minX(), minY = gLayout.minY();
    int maxX = gLayout.maxX() + 1 - w - 1, maxY = gLayout.maxY() + 1 - h - 1;
    if (!gLayout.wide()) {
        maxX = gMouseDDW->width - w - 1;
        maxY = gMouseDDW->height - h - 1;
    }
    if (gSavedX[page] < minX)
        gSavedX[page] = minX;
    else if (gSavedX[page] > maxX)
        gSavedX[page] = maxX;
    if (gSavedY[page] < minY)
        gSavedY[page] = minY;
    else if (gSavedY[page] > maxY)
        gSavedY[page] = maxY;
    const uint8_t *s = src + (ptrdiff_t)pitch * gSavedY[page] + gSavedX[page] * 2;
    uint8_t *d = gSaveBuf[page];
    for (int r = 0; r < h; r++) {
        memcpy(d, s, w * 2);
        d += w * 2;
        s += pitch;
    }
    gSaved[page] = 1;
}

// 0x40c660
void MouseRestoreUnder(int page, uint8_t *dst, unsigned long pitch)
{
    if (gSaved[page]) {
        CSprite16 *c = &gCursors[gCursorIndex];
        int w = c->Width();
        int h = c->Height();
        uint8_t *d = dst + (ptrdiff_t)pitch * gSavedY[page] + gSavedX[page] * 2;
        const uint8_t *s = gSaveBuf[page];
        for (int r = 0; r < h; r++) {
            memcpy(d, s, w * 2);
            s += w * 2;
            d += pitch;
        }
    }
    gSaved[page] = 0;
}

// 0x40c720
void SetMouseSpeed(int n)
{
    if (n > 0) gMouseSpeed = n;
}

// 0x40c730
int LoadCursors(char *file)
{
    if (gNumCursors) ReleaseCSpriteTable(gCursors, gNumCursors);
    gNumCursors = LoadCSpriteTable(file, gCursors, 20);
    return gNumCursors != 0;
}

// 0x40c770
void SetCursor(int i)
{
    if (i < gNumCursors) {
        gCursorIndex = i;
        gLastDrawY = -1000;
        gLastDrawX = -1000;
    }
}

// 0x40c7a0
int GetCursor() { return gCursorIndex; }

// 0x40c7b0
void ShowMouse(int show)
{
    switch (show & 0xff) {
    case 1:
        gCursorVisible = 1;
        break;
    case 0:
        if (gSaved[gMouseFrontPage]) {
            uint8_t *p;
            unsigned long pitch;
            if (!gMouseDDW->LockFront(&p, &pitch)) return;
            int sx = gSavedX[gMouseFrontPage], sy = gSavedY[gMouseFrontPage];
            MouseRestoreUnder(gMouseFrontPage, p, pitch);
            gMouseDDW->UnlockFront(sx, sy, 64, 64);
        }
        gSaved[1] = 0;
        gSaved[0] = 0;
        gCursorVisible = 0;
        break;
    }
}

// 0x40c840
void ResetMouseClicks()
{
    gRightDown = 0;
    gLeftDown = 0;
    gRightClicks = 0;
    gLeftClicks = 0;
}

// 0x40c860
int MouseRightClicked()
{
    if (gRightClicks.exchange(0)) return 1;   // (port: take and clear at once)
    return 0;
}

// 0x40c880
int MouseLeftClicked()
{
    if (gLeftClicks.exchange(0)) return 1;
    return 0;
}

// 0x40c8a0
int MouseButtonDown(int button)
{
    switch (button & 0xff) {
    case 0: return gLeftDown;
    case 1: return gRightDown;
    }
    return 0;
}

// Enhanced mode: the cursor range follows the screen layout.
void MouseUpdateRange()
{
    if (!gLayout.wide()) return;
    gMouseClip.SetViewport(gLayout.minX(), gLayout.minY(), gLayout.maxX(), gLayout.maxY());
    gMouseMinX = gLayout.minX();
    gMouseMinY = gLayout.minY();
    gMouseMaxX = gLayout.maxX();
    gMouseMaxY = gLayout.maxY();
    if (gMousePosY < gMouseMinY) gMousePosY = gMouseMinY;
    if (gMousePosY > gMouseMaxY) gMousePosY = gMouseMaxY;
}

// Test scripts (platform AE_INPUT_SCRIPT) place the cursor directly.
void MouseSetPosition(int x, int y)
{
    gMousePosX = x;
    gMousePosY = y;
}
