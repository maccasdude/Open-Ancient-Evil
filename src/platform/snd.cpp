// Software mixer on SDL audio (replaces DirectSound and waveOut).
#include "snd.h"
#include "fileio.h"
#include <SDL.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <deque>
#include <mutex>

static const int kRate = 44100;
static SDL_AudioDeviceID gDev;
static SndDevice gDevice;
static std::vector<Sound *> gSounds;       // every created buffer
static unsigned gWaveVolume = 0xffff;      // waveOut volume (left channel used)

struct SndStream {
    int rate, channels;
    std::deque<int16_t> q;                 // interleaved at the source rate
    double frac = 0;
    float volume = 1.0f;
    long long played = 0;
};
static std::vector<SndStream *> gStreams;

static inline float db_to_gain(int hundredthsDb)
{
    if (hundredthsDb <= -10000) return 0.0f;
    return powf(10.0f, (float)hundredthsDb / 2000.0f);
}

static inline float sample_at(const Sound *s, long frame, int ch)
{
    int c = s->channels > 1 ? ch : 0;
    if (s->bits == 8) {
        const uint8_t *p = s->data + ((size_t)frame * s->channels + c);
        return ((int)*p - 128) / 128.0f;
    }
    const int16_t *p = (const int16_t *)s->data + ((size_t)frame * s->channels + c);
    return *p / 32768.0f;
}

static void mix_cb(void *, Uint8 *stream, int len)
{
    int frames = len / 4;
    std::vector<float> acc((size_t)frames * 2, 0.0f);
    float master = (gWaveVolume & 0xffff) / 65535.0f;
    for (Sound *s : gSounds) {
        if (!s->active || !s->data || !s->size) continue;
        long total = (long)(s->size / (s->channels * (s->bits / 8)));
        if (total <= 0) {
            s->active = false;
            continue;
        }
        float g = db_to_gain(s->volume) * master;
        float gl = g, gr = g;
        if (s->pan > 0) gl *= db_to_gain(-s->pan);
        if (s->pan < 0) gr *= db_to_gain(s->pan);
        double step = (double)s->rate / kRate;
        for (int i = 0; i < frames; i++) {
            long f = (long)s->pos;
            if (f >= total) {
                if (s->loop) {
                    s->pos -= total;
                    f = (long)s->pos;
                    if (f >= total) f = 0;
                } else {
                    s->active = false;
                    s->playing = 0;
                    break;
                }
            }
            acc[i * 2] += sample_at(s, f, 0) * gl;
            acc[i * 2 + 1] += sample_at(s, f, 1) * gr;
            s->pos += step;
        }
    }
    for (SndStream *st : gStreams) {
        double step = (double)st->rate / kRate;
        float g = st->volume;
        for (int i = 0; i < frames; i++) {
            if ((int)st->q.size() < st->channels) break;
            float l = st->q[0] / 32768.0f;
            float r = st->channels > 1 ? st->q[1] / 32768.0f : l;
            acc[i * 2] += l * g;
            acc[i * 2 + 1] += r * g;
            st->frac += step;
            while (st->frac >= 1.0 && (int)st->q.size() >= st->channels) {
                st->frac -= 1.0;
                for (int c = 0; c < st->channels; c++) st->q.pop_front();
                st->played++;
            }
        }
    }
    int16_t *out = (int16_t *)stream;
    for (int i = 0; i < frames * 2; i++) {
        float v = acc[i];
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        out[i] = (int16_t)(v * 32767.0f);
    }
}

static bool open_device()
{
    if (gDev) return true;
    if (getenv("AE_NOSOUND")) return false;
    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO)) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    }
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = kRate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = mix_cb;
    gDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!gDev) return false;
    SDL_PauseAudioDevice(gDev, 0);
    return true;
}

struct Lock {
    Lock() { if (gDev) SDL_LockAudioDevice(gDev); }
    ~Lock() { if (gDev) SDL_UnlockAudioDevice(gDev); }
};

// 0x40c8c0
SndDevice *SndInit()
{
    if (!open_device()) return nullptr;
    gDevice.opened = 1;
    return &gDevice;
}

// RIFF parser as in the original (0x42da30).
static int parse_wav(const uint8_t *buf, size_t len, const uint8_t **fmt, const uint8_t **data, uint32_t *dataLen)
{
    *fmt = nullptr;
    *data = nullptr;
    *dataLen = 0;
    if (len < 12 || memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) return 0;
    uint32_t riffLen;
    memcpy(&riffLen, buf + 4, 4);
    const uint8_t *p = buf + 12;
    const uint8_t *end = buf + 8 + riffLen;
    if (end > buf + len) end = buf + len;
    while (p + 8 <= end) {
        uint32_t id, n;
        memcpy(&id, p, 4);
        memcpy(&n, p + 4, 4);
        p += 8;
        if (!memcmp(&id, "fmt ", 4)) {
            if (n < 14) return 0;
            *fmt = p;
        } else if (!memcmp(&id, "data", 4)) {
            *data = p;
            *dataLen = n;
            if ((size_t)(end - p) < n) *dataLen = (uint32_t)(end - p);
        }
        if (*fmt && *data) return 1;
        p += (n + 1) & ~1u;
    }
    return 0;
}

// 0x40c960
Sound *SndCreate(SndDevice *d, const char *file)
{
    if (!d) return nullptr;
    int fd = w_open(file, W_O_RDONLY | W_O_BINARY);
    if (fd < 0) return nullptr;
    long len = w_filelength(fd);
    std::vector<uint8_t> buf(len > 0 ? len : 0);
    if (len > 0) w_read(fd, buf.data(), (unsigned)len);
    w_close(fd);
    const uint8_t *fmt, *data;
    uint32_t dataLen;
    if (!parse_wav(buf.data(), buf.size(), &fmt, &data, &dataLen)) return nullptr;
    uint16_t tag, ch, bits;
    uint32_t rate;
    memcpy(&tag, fmt, 2);
    memcpy(&ch, fmt + 2, 2);
    memcpy(&rate, fmt + 4, 4);
    memcpy(&bits, fmt + 14, 2);
    if (tag != 1 || (bits != 8 && bits != 16) || ch < 1 || ch > 2) return nullptr;
    Sound *s = new Sound;
    memset(s, 0, sizeof(*s));
    s->size = dataLen;
    s->rate = rate;
    s->channels = ch;
    s->bits = bits;
    s->data = new uint8_t[dataLen ? dataLen : 1];
    memcpy(s->data, data, dataLen);
    s->ownsData = true;
    Lock lk;
    gSounds.push_back(s);
    return s;
}

// 0x40cae0
int SndRelease(SndDevice *, Sound *s)
{
    if (!s) return 1;
    Lock lk;
    for (size_t i = 0; i < gSounds.size(); i++)
        if (gSounds[i] == s) {
            gSounds.erase(gSounds.begin() + i);
            break;
        }
    if (!s->next && !s->prev) {
        if (s->ownsData) delete[] s->data;
        delete s;
        return 1;
    }
    // Part of a duplicate chain: unlink; the data stays with the others.
    if (s->prev) s->prev->next = s->next;
    if (s->next) s->next->prev = s->prev;
    if (s->ownsData) {
        Sound *heir = s->prev ? s->prev : s->next;
        if (heir) heir->ownsData = true;
    }
    delete s;
    return 1;
}

// 0x40cb50
int SndPlay(SndDevice *d, Sound *s)
{
    return SndPlayAt(d, s, 0);
}

// 0x40cbe0
int SndPlayAt(SndDevice *d, Sound *s, int pos)
{
    if (!d || !s) return 0;
    Lock lk;
    int bpf = s->channels * (s->bits / 8);
    s->pos = bpf ? (double)(pos / bpf) : 0;
    s->active = true;
    s->playing = 1;
    return 1;
}

// 0x40cc70
int SndStop(SndDevice *, Sound *s)
{
    if (!s) return 1;
    Lock lk;
    s->playing = 0;
    s->active = false;
    return 1;
}

// 0x40cca0
int SndIsPlaying(SndDevice *, Sound *s)
{
    if (!s) return 0;
    Lock lk;
    s->playing = s->active ? 1 : 0;
    return s->playing;
}

// 0x40ccf0
void SndSetPan(SndDevice *, Sound *s, int pan)
{
    if (pan < -10000) pan = -10000;
    else if (pan > 10000) pan = 10000;
    Lock lk;
    s->pan = pan;
}

// 0x40cd30
void SndSetVolume(SndDevice *, Sound *s, int vol)
{
    if (vol < -10000) vol = -10000;
    if (vol > 0) vol = 0;
    Lock lk;
    s->volume = vol;
}

// 0x40cd70
void SndSetLoop(SndDevice *, Sound *s, int loop)
{
    s->loop = loop;
}

// 0x40cd80: play cursor in bytes.
int SndGetPosition(SndDevice *, Sound *s)
{
    Lock lk;
    int bpf = s->channels * (s->bits / 8);
    return (int)s->pos * bpf;
}

// 0x40cdb0
int SndDuplicate(SndDevice *, Sound *src, Sound **out)
{
    Sound *s = new Sound;
    *out = s;
    *s = *src;
    s->f10 = s->volume = s->pan = s->loop = s->playing = 0;
    s->next = nullptr;
    s->pos = 0;
    s->active = false;
    s->ownsData = false;
    Sound *last = src;
    while (last->next) last = last->next;
    s->prev = last;
    last->next = s;
    Lock lk;
    gSounds.push_back(s);
    return 1;
}

// 0x40ce50: primary buffer format; the mixer always runs at 44.1 kHz.
int SndSetFormat(SndDevice *, int, int bits, int)
{
    return bits == 8 || bits == 16;
}

unsigned snd_get_wave_volume() { return gWaveVolume | (gWaveVolume << 16); }
void snd_set_wave_volume(unsigned v) { gWaveVolume = v & 0xffff; }

// ---------------------------------------------------------------------------
// streams

SndStream *snd_stream_open(int rate, int channels)
{
    if (!open_device()) return nullptr;
    SndStream *s = new SndStream;
    s->rate = rate;
    s->channels = channels;
    Lock lk;
    gStreams.push_back(s);
    return s;
}

void snd_stream_close(SndStream *s)
{
    if (!s) return;
    {
        Lock lk;
        for (size_t i = 0; i < gStreams.size(); i++)
            if (gStreams[i] == s) {
                gStreams.erase(gStreams.begin() + i);
                break;
            }
    }
    delete s;
}

void snd_stream_write(SndStream *s, const int16_t *pcm, int frames)
{
    if (!s) return;
    Lock lk;
    s->q.insert(s->q.end(), pcm, pcm + (size_t)frames * s->channels);
}

int snd_stream_queued(SndStream *s)
{
    if (!s) return 0;
    Lock lk;
    return (int)(s->q.size() / s->channels);
}

void snd_stream_set_volume(SndStream *s, float v)
{
    if (!s) return;
    Lock lk;
    s->volume = v;
}

void snd_stream_clear(SndStream *s)
{
    if (!s) return;
    Lock lk;
    s->q.clear();
}

long long snd_stream_played(SndStream *s)
{
    if (!s) return 0;
    Lock lk;
    return s->played;
}
