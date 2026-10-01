// Sound output: a small software mixer on SDL audio that replaces the
// DirectSound wrapper of the original (0x40c8c0-0x40d000) and the MCI CD
// audio / waveOut volume calls.
#pragma once
#include <stdint.h>

struct SndDevice {
    int opened;
};

// One sound buffer (a DirectSound secondary buffer in the original, 0x2c
// bytes there). The game only touches 'loop' directly.
struct Sound {
    uint32_t size;          // +0x00 data size in bytes
    int rate;               // wave format
    int channels;
    int bits;
    uint8_t *data;          // +0x08 PCM data (shared by duplicates)
    int f10;
    int volume;             // +0x14 hundredths of dB (-10000..0)
    int pan;                // +0x18 -10000..10000
    int loop;               // +0x1c
    int playing;            // +0x20
    Sound *prev;            // +0x24 duplicate chain
    Sound *next;            // +0x28
    // mixer state
    double pos;             // play position in sample frames
    bool active;
    bool ownsData;
};

SndDevice *SndInit();                                         // 0x40c8c0
Sound *SndCreate(SndDevice *d, const char *file);             // 0x40c960
int SndRelease(SndDevice *d, Sound *s);                       // 0x40cae0
int SndPlay(SndDevice *d, Sound *s);                          // 0x40cb50
int SndPlayAt(SndDevice *d, Sound *s, int pos);               // 0x40cbe0
int SndStop(SndDevice *d, Sound *s);                          // 0x40cc70
int SndIsPlaying(SndDevice *d, Sound *s);                     // 0x40cca0
void SndSetPan(SndDevice *d, Sound *s, int pan);              // 0x40ccf0
void SndSetVolume(SndDevice *d, Sound *s, int vol);           // 0x40cd30
void SndSetLoop(SndDevice *d, Sound *s, int loop);            // 0x40cd70
int SndGetPosition(SndDevice *d, Sound *s);                   // 0x40cd80
int SndDuplicate(SndDevice *d, Sound *src, Sound **out);      // 0x40cdb0
int SndSetFormat(SndDevice *d, int rate, int bits, int channels); // 0x40ce50

// waveOutGetVolume / waveOutSetVolume replacement: 0..65535 per channel.
unsigned snd_get_wave_volume();
void snd_set_wave_volume(unsigned v);

// Streaming voices (movie sound track, CD music).
struct SndStream;
SndStream *snd_stream_open(int rate, int channels);
void snd_stream_close(SndStream *s);
// Queue interleaved signed 16-bit samples.
void snd_stream_write(SndStream *s, const int16_t *pcm, int frames);
int snd_stream_queued(SndStream *s);            // frames not yet played
void snd_stream_set_volume(SndStream *s, float v);
void snd_stream_clear(SndStream *s);
long long snd_stream_played(SndStream *s);      // frames played since open

// CD audio replacement: music tracks read from files.
int cdmusic_play(int track);        // returns 1 if a file for the track exists
void cdmusic_stop();
int cdmusic_playing();
void cdmusic_set_volume(int v0to10);
int cdmusic_get_volume();           // 0..10, -1 if unknown
