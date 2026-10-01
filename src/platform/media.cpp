// Movie playback and CD music from files, decoded with FFmpeg (optional).
#include "media.h"
#include "snd.h"
#include "fileio.h"
#include "platform.h"
#include "../engine/ddw16.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <atomic>
#include <mutex>

#ifdef HAVE_LIBAV
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

// Audio decoder shared by the movie and music players: decodes to 44.1 kHz
// stereo signed 16-bit.
struct AudioDec {
    AVCodecContext *ctx = nullptr;
    SwrContext *swr = nullptr;
    int open(AVStream *st)
    {
        const AVCodec *c = avcodec_find_decoder(st->codecpar->codec_id);
        if (!c) return 0;
        ctx = avcodec_alloc_context3(c);
        avcodec_parameters_to_context(ctx, st->codecpar);
        if (avcodec_open2(ctx, c, nullptr) < 0) return 0;
        AVChannelLayout out = AV_CHANNEL_LAYOUT_STEREO;
        AVChannelLayout in;
        if (ctx->ch_layout.nb_channels > 0)
            av_channel_layout_copy(&in, &ctx->ch_layout);
        else
            av_channel_layout_default(&in, 1);
        if (swr_alloc_set_opts2(&swr, &out, AV_SAMPLE_FMT_S16, 44100, &in, ctx->sample_fmt,
                                ctx->sample_rate, 0, nullptr) < 0)
            return 0;
        av_channel_layout_uninit(&in);
        return swr_init(swr) >= 0;
    }
    void close()
    {
        if (swr) swr_free(&swr);
        if (ctx) avcodec_free_context(&ctx);
    }
    // Decode one packet into interleaved stereo samples.
    void decode(AVPacket *pkt, std::vector<int16_t> &out)
    {
        if (avcodec_send_packet(ctx, pkt) < 0) return;
        AVFrame *f = av_frame_alloc();
        while (avcodec_receive_frame(ctx, f) >= 0) {
            int n = swr_get_out_samples(swr, f->nb_samples);
            size_t base = out.size();
            out.resize(base + (size_t)n * 2);
            uint8_t *dst = (uint8_t *)(out.data() + base);
            int got = swr_convert(swr, &dst, n, (const uint8_t **)f->extended_data, f->nb_samples);
            out.resize(base + (size_t)(got > 0 ? got : 0) * 2);
        }
        av_frame_free(&f);
    }
};

// ---------------------------------------------------------------------------
// Movies

struct VideoFrame {
    double t;
    std::vector<uint16_t> px;
};

struct AviImpl {
    AVFormatContext *fmt = nullptr;
    AVCodecContext *vdec = nullptr;
    AudioDec adec;
    bool haveAudio = false;
    int vidx = -1, aidx = -1;
    SwsContext *sws = nullptr;
    SndStream *audio = nullptr;
    DirectDrawWindow *ddw = nullptr;
    int x = 0, y = 0, w = 0, h = 0;
    int (*intr)() = nullptr;
    int intrFrames = 0;
    int lastIntrBlock = 0;
    bool playing = false;
    bool eof = false;
    unsigned startTicks = 0;
    int frameNo = 0;
    std::deque<VideoFrame> frames;

    ~AviImpl()
    {
        if (audio) snd_stream_close(audio);
        if (sws) sws_freeContext(sws);
        if (vdec) avcodec_free_context(&vdec);
        adec.close();
        if (fmt) avformat_close_input(&fmt);
    }

    void decode_some()
    {
        AVPacket *pkt = av_packet_alloc();
        int r = av_read_frame(fmt, pkt);
        if (r < 0) {
            eof = true;
            av_packet_free(&pkt);
            return;
        }
        if (pkt->stream_index == vidx) {
            if (avcodec_send_packet(vdec, pkt) >= 0) {
                AVFrame *f = av_frame_alloc();
                while (avcodec_receive_frame(vdec, f) >= 0) {
                    VideoFrame vf;
                    AVRational tb = fmt->streams[vidx]->time_base;
                    int64_t pts = f->best_effort_timestamp;
                    vf.t = pts == AV_NOPTS_VALUE ? 0.0 : pts * av_q2d(tb);
                    vf.px.resize((size_t)w * h);
                    uint8_t *dst[1] = {(uint8_t *)vf.px.data()};
                    int ls[1] = {w * 2};
                    sws_scale(sws, f->data, f->linesize, 0, h, dst, ls);
                    frames.push_back(std::move(vf));
                }
                av_frame_free(&f);
            }
        } else if (pkt->stream_index == aidx && haveAudio && audio) {
            std::vector<int16_t> pcm;
            adec.decode(pkt, pcm);
            if (!pcm.empty()) snd_stream_write(audio, pcm.data(), (int)(pcm.size() / 2));
        }
        av_packet_free(&pkt);
    }

    void show(const VideoFrame &vf)
    {
        uint8_t *p;
        unsigned long pitch;
        if (!ddw->LockFront(&p, &pitch)) return;
        for (int row = 0; row < h; row++) {
            int dy = y + row;
            if (dy < 0 || dy >= ddw->height) continue;
            int cw = w;
            if (x + cw > ddw->width) cw = ddw->width - x;
            if (cw <= 0 || x < 0) continue;
            memcpy(p + (size_t)dy * pitch + x * 2, vf.px.data() + (size_t)row * w, cw * 2);
        }
        ddw->UnlockFront();
    }
};

AviPlayer::AviPlayer() {}
AviPlayer::~AviPlayer() { delete (AviImpl *)impl; }

int AviPlayer::Open(char *file, DirectDrawWindow *ddw, void *dsound, int)
{
    std::string path = fileio_resolve(file, false);
    AviImpl *a = new AviImpl;
    a->ddw = ddw;
    if (avformat_open_input(&a->fmt, path.c_str(), nullptr, nullptr) < 0 ||
        avformat_find_stream_info(a->fmt, nullptr) < 0) {
        delete a;
        return 0;
    }
    a->vidx = av_find_best_stream(a->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    a->aidx = av_find_best_stream(a->fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (a->vidx < 0) {
        delete a;
        return 0;
    }
    AVStream *vs = a->fmt->streams[a->vidx];
    const AVCodec *vc = avcodec_find_decoder(vs->codecpar->codec_id);
    if (!vc) {
        delete a;
        return 0;
    }
    a->vdec = avcodec_alloc_context3(vc);
    avcodec_parameters_to_context(a->vdec, vs->codecpar);
    if (avcodec_open2(a->vdec, vc, nullptr) < 0) {
        delete a;
        return 0;
    }
    a->w = a->vdec->width;
    a->h = a->vdec->height;
    a->sws = sws_getContext(a->w, a->h, a->vdec->pix_fmt, a->w, a->h, AV_PIX_FMT_RGB565, SWS_BILINEAR,
                            nullptr, nullptr, nullptr);
    if (!a->sws) {
        delete a;
        return 0;
    }
    if (a->aidx >= 0 && dsound) a->haveAudio = a->adec.open(a->fmt->streams[a->aidx]) != 0;
    impl = a;
    return 1;
}

void AviPlayer::SetPosition(int x, int y)
{
    AviImpl *a = (AviImpl *)impl;
    if (!a) return;
    a->x = x;
    a->y = y;
}

void AviPlayer::SetInterrupt(int (*fn)(), int frames)
{
    AviImpl *a = (AviImpl *)impl;
    if (!a) return;
    if (frames) {
        a->intr = fn;
        a->intrFrames = frames;
        a->lastIntrBlock = 0;
    } else {
        a->intr = nullptr;
        a->intrFrames = 0;
    }
}

void AviPlayer::Start()
{
    AviImpl *a = (AviImpl *)impl;
    if (!a) return;
    if (a->haveAudio) {
        a->audio = snd_stream_open(44100, 2);
        unsigned wv = snd_get_wave_volume() & 0xffff;
        snd_stream_set_volume(a->audio, wv / 65535.0f);
    }
    a->playing = true;
    a->startTicks = plat_ticks();
}

int AviPlayer::IsPlaying()
{
    AviImpl *a = (AviImpl *)impl;
    if (!a || !a->playing) return 0;
    double now = (plat_ticks() - a->startTicks) / 1000.0;
    while (!a->eof && (a->frames.empty() || a->frames.back().t < now + 0.25)) a->decode_some();
    bool shown = false;
    while (!a->frames.empty() && a->frames.front().t <= now) {
        if (a->frames.size() == 1 || a->frames[1].t > now) {
            a->show(a->frames.front());
            a->frameNo++;
            shown = true;
        }
        a->frames.pop_front();
    }
    if (shown && a->intr && a->intrFrames) {
        int block = a->frameNo / a->intrFrames;
        if (block != a->lastIntrBlock) {
            a->lastIntrBlock = block;
            if (a->intr()) a->playing = false;
        }
    }
    if (a->eof && a->frames.empty()) a->playing = false;
    if (!shown) plat_sleep(2);
    return a->playing ? 1 : 0;
}

void AviPlayer::Stop()
{
    AviImpl *a = (AviImpl *)impl;
    if (!a) return;
    a->playing = false;
    if (a->audio) {
        snd_stream_close(a->audio);
        a->audio = nullptr;
    }
}

#else  // no FFmpeg: movies are skipped

AviPlayer::AviPlayer() {}
AviPlayer::~AviPlayer() {}
int AviPlayer::Open(char *, DirectDrawWindow *, void *, int) { return 0; }
void AviPlayer::SetPosition(int, int) {}
void AviPlayer::SetInterrupt(int (*)(), int) {}
void AviPlayer::Start() {}
int AviPlayer::IsPlaying() { return 0; }
void AviPlayer::Stop() {}

#endif

// ---------------------------------------------------------------------------
// CD music: the audio tracks are expected as files (any format FFmpeg reads,
// or WAV without it) in the CD directory, its "music" subdirectory or a
// "music" directory next to the game data. The track number is taken from
// the first number in the file name ("Track02.ogg", "02 - Title.flac").

static SndStream *gMusic;
static std::thread gMusicThread;
static std::atomic<bool> gMusicStop;
static std::atomic<bool> gMusicRunning;
static int gMusicVolume = 10;

static bool is_audio_ext(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    std::string e = dot + 1;
    for (auto &c : e) c = (char)tolower((unsigned char)c);
    return e == "wav" || e == "ogg" || e == "mp3" || e == "flac" || e == "opus" || e == "m4a";
}

static std::string find_track_uncached(int track);
static std::string find_track(int track)
{
    static std::string cache[100];
    static bool known[100];
    if (track < 0 || track >= 100) return "";
    if (!known[track]) {
        cache[track] = find_track_uncached(track);
        known[track] = true;
    }
    return cache[track];
}

static std::string find_track_uncached(int track)
{
    std::vector<std::string> dirs = {std::string(fileio_cd_dir()), std::string(fileio_cd_dir()) + "/music",
                                     std::string(fileio_cd_dir()) + "/MUSIC",
                                     std::string(fileio_game_dir()) + "/music"};
    for (auto &d : dirs) {
        DIR *dir = opendir(d.c_str());
        if (!dir) continue;
        struct dirent *e;
        std::string found;
        while ((e = readdir(dir))) {
            if (!is_audio_ext(e->d_name)) continue;
            const char *p = e->d_name;
            while (*p && !isdigit((unsigned char)*p)) p++;
            if (!*p) continue;
            if (atoi(p) == track) {
                found = d + "/" + e->d_name;
                break;
            }
        }
        closedir(dir);
        if (!found.empty()) return found;
    }
    return "";
}

static void music_thread(std::string path)
{
#ifdef HAVE_LIBAV
    AVFormatContext *fmt = nullptr;
    if (avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) >= 0 &&
        avformat_find_stream_info(fmt, nullptr) >= 0) {
        int aidx = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        AudioDec dec;
        if (aidx >= 0 && dec.open(fmt->streams[aidx])) {
            AVPacket *pkt = av_packet_alloc();
            std::vector<int16_t> pcm;
            while (!gMusicStop) {
                if (snd_stream_queued(gMusic) > 44100 * 2) {
                    SDL_Delay(20);
                    continue;
                }
                if (av_read_frame(fmt, pkt) < 0) break;
                if (pkt->stream_index == aidx) {
                    pcm.clear();
                    dec.decode(pkt, pcm);
                    if (!pcm.empty()) snd_stream_write(gMusic, pcm.data(), (int)(pcm.size() / 2));
                }
                av_packet_unref(pkt);
            }
            av_packet_free(&pkt);
        }
        dec.close();
    }
    if (fmt) avformat_close_input(&fmt);
#else
    SDL_AudioSpec spec;
    Uint8 *buf = nullptr;
    Uint32 len = 0;
    if (SDL_LoadWAV(path.c_str(), &spec, &buf, &len)) {
        SDL_AudioCVT cvt;
        if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, 2, 44100) >= 0) {
            std::vector<Uint8> tmp((size_t)len * (cvt.len_mult > 0 ? cvt.len_mult : 1));
            memcpy(tmp.data(), buf, len);
            cvt.buf = tmp.data();
            cvt.len = (int)len;
            if (cvt.needed) SDL_ConvertAudio(&cvt);
            int frames = (cvt.needed ? cvt.len_cvt : (int)len) / 4;
            const int16_t *p = (const int16_t *)tmp.data();
            for (int i = 0; i < frames && !gMusicStop; i += 4096) {
                while (!gMusicStop && snd_stream_queued(gMusic) > 44100 * 2) SDL_Delay(20);
                int n = frames - i < 4096 ? frames - i : 4096;
                snd_stream_write(gMusic, p + (size_t)i * 2, n);
            }
        }
        SDL_FreeWAV(buf);
    }
#endif
    gMusicRunning = false;
}

void cdmusic_stop()
{
    gMusicStop = true;
    if (gMusicThread.joinable()) gMusicThread.join();
    gMusicStop = false;
    if (gMusic) {
        snd_stream_close(gMusic);
        gMusic = nullptr;
    }
}

int cdmusic_play(int track)
{
    cdmusic_stop();
    std::string path = find_track(track);
    if (path.empty()) return 0;
    gMusic = snd_stream_open(44100, 2);
    if (!gMusic) return 0;
    snd_stream_set_volume(gMusic, gMusicVolume / 10.0f);
    gMusicRunning = true;
    gMusicThread = std::thread(music_thread, path);
    return 1;
}

int cdmusic_playing()
{
    if (!gMusic) return 0;
    return gMusicRunning || snd_stream_queued(gMusic) > 0;
}

void cdmusic_set_volume(int v)
{
    if (v < 0) v = 0;
    if (v > 10) v = 10;
    gMusicVolume = v;
    if (gMusic) snd_stream_set_volume(gMusic, v / 10.0f);
}

int cdmusic_get_volume() { return gMusicVolume; }
