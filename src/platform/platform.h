// SDL2 platform layer: window, presentation, input, timers, audio.
#pragma once
#include "wincompat.h"

// ---- window / video
void *plat_create_window(const char *title);
void plat_set_video_mode(int w, int h);
// Before the window is made: size of the presented image and how many
// presented pixels make one game pixel (mouse motion is in game pixels).
void plat_configure_output(int w, int h, int logicalScale);
void plat_present(const uint16_t *px, int pitch, int w, int h);
void plat_message_box(const char *text, const char *caption);
void plat_toggle_fullscreen();

// ---- messages
// Windows-style window procedure used by the game.
typedef long (*PlatWndProc)(void *hwnd, unsigned msg, unsigned long wParam, long lParam);
void plat_set_wndproc(PlatWndProc p);
// Retrieve one pending message and dispatch it. Returns 0 if none was pending,
// -1 if the application should quit, 1 otherwise.
int plat_dispatch_one();
void plat_wait_message();
void plat_request_quit();
bool plat_quit_requested();

enum {
    WM_DESTROY_ = 0x0002, WM_MOVE_ = 0x0003, WM_SIZE_ = 0x0005, WM_CLOSE_ = 0x0010,
    WM_ACTIVATEAPP_ = 0x001c, WM_KEYDOWN_ = 0x0100, WM_KEYUP_ = 0x0101, WM_CHAR_ = 0x0102,
};

// ---- keyboard
short plat_GetAsyncKeyState(int vk);
unsigned plat_key_presses(int vk);   // count of presses of a key so far

// ---- mouse (DirectInput style relative mouse)
struct PlatMouseState {
    int dx, dy;          // accumulated relative motion since last poll
    int buttons[3];      // current button state
};
void plat_mouse_poll(PlatMouseState *s);
// buffered DirectInput-style events: type 0=x,1=y,2..4 = button 0..2
struct PlatMouseEvent { int type; int data; unsigned time; };
int plat_mouse_get_events(PlatMouseEvent *ev, int max);
void plat_mouse_capture(bool on);
// test scripts: how to put the cursor at game coordinates
void plat_set_abs_mouse_hook(void (*hook)(int x, int y));

// ---- time
unsigned plat_ticks();              // GetTickCount / timeGetTime (ms)
unsigned long long plat_perf_counter();
unsigned long long plat_perf_freq();
void plat_sleep(unsigned ms);
// The game's GetTickCount frame limiters, on an emulated system tick: the
// clock only advances in steps of the tick length of the Windows it ran on
// (AE_TICK_MS; default Windows 95/98, 54.925 ms).
unsigned plat_game_ticks();
void plat_set_tick_ms(double ms);
// port settings applied live
void plat_set_fullscreen(bool on);
bool plat_is_fullscreen();
void plat_set_window_scale(int scale);
void plat_set_scaling(int mode);      // 0 sharp, 1 sharp integer steps, 2 smooth
void plat_set_pause_on_focus(bool on);
int plat_take_keypress();             // last key pressed (VK code), 0 none; clears it
void plat_wait_game_ticks(unsigned t0, unsigned ms);   // until plat_game_ticks() - t0 >= ms

// ---- periodic timers (timeSetEvent), run on a separate thread
typedef void (*PlatTimerProc)(unsigned id, unsigned msg, unsigned long user, unsigned long, unsigned long);
unsigned plat_timer_start(unsigned periodMs, PlatTimerProc cb, unsigned long user);
void plat_timer_stop(unsigned id);

// ---- init / shutdown
void plat_init(int argc, char **argv);
void plat_shutdown();
