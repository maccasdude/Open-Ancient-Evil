// Movie playback (replaces the Video for Windows / Indeo player of the
// original, 0x40ae60-0x40bc90) using FFmpeg when available.
#pragma once

class DirectDrawWindow;

struct AviPlayer {
    AviPlayer();                                              // 0x40b6e0
    ~AviPlayer();                                             // 0x40b740
    int Open(char *file, DirectDrawWindow *ddw, void *dsound, int flags); // 0x40ba50
    void SetPosition(int x, int y);                           // 0x40af00
    void SetInterrupt(int (*fn)(), int frames);               // 0x40b300
    void Start();                                             // 0x40bbc0
    int IsPlaying();                                          // 0x40b6d0
    void Stop();                                              // 0x40bc50
    void *impl = nullptr;
};
