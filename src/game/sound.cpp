// Sound effects, speech, environment loops, sound/CD volume dialogs, the
// game clock, CD audio and the title menu (0x412790-0x413a10).
#include "game.h"

#define TILE(x, y) gLevelMap[gMapRow[(y)] + (x)]

// 0x412790
void SoundInit()
{
    gDSound = nullptr;
    gDSound = SndInit();
    if (!gDSound) {
        plat_message_box("Direct Sound is unable to initialize\nSound FX will not be availible",
                         "I just thought you'd like to know...");
    } else {
        PreloadSounds();
        if (!SndSetFormat(gDSound, 0x2b11, 8, 1))
            plat_message_box("Unable to set DirectSound Primary Buffer format",
                             "I just thought you'd like to know...");
    }
    gSavedWaveVolume = GetWaveVolume();
}

// 0x412810
void SoundShutdown()
{
    SetWaveVolume(gSavedWaveVolume);
    FreeAllSounds();
}

// 0x412830
void PreloadSounds()
{
    if (!gDSound) return;
    for (int i = 0; i < 106; i++) {
        gSoundBufs[i] = nullptr;
        if (!gSamples[i].onDemand) LoadSound(i);
    }
}

// 0x412870
int LoadSound(int i)
{
    char path[0x100];
    sprintf(path, "WAV\\%s.WAV", gSamples[i].name);
    gSoundBufs[i] = SndCreate(gDSound, path);
    if (!gSoundBufs[i]) {
        sprintf(path, "Could load %s. Sorry", gSamples[i].name);
        plat_message_box(path, "DOH!!");
        gSoundBufs[i] = nullptr;
        return 0;
    }
    gSoundBufs[i]->loop = 0;
    gSamples[i].lastUsed = gTime;
    return 1;
}

// 0x412950
void FreeAllSounds()
{
    for (int i = 0; i < 0x6a; i++) FreeSound(i);
}

// 0x412970
void FreeSound(int i)
{
    if (gSoundBufs[i] && SndRelease(gDSound, gSoundBufs[i])) gSoundBufs[i] = nullptr;
}

// 0x4129a0: play sample 'id' at tile (x,y), or with volume y when x == -1.
int PlaySound(int id, int x, int y)
{
    if (!gDSound || id < 0 || id > 0x69) return 0;
    if (!gSoundBufs[id]) {
        EvictOldSound();
        if (!LoadSound(id)) return 0;
    }
    if (SndIsPlaying(gDSound, gSoundBufs[id])) return 1;
    int vol, pan;
    if (x == -1) {
        vol = y;
        pan = 0;
    } else {
        int d = TileDistance(gPlayer.tileX, gPlayer.tileY, x, y);
        if (d >= 20) return 0;
        vol = -125 * d;
        int sx, sy;
        TileToScreen(x, y, &sx, &sy);
        pan = (sx - 320) * 10;
    }
    SndSetVolume(gDSound, gSoundBufs[id], vol);
    SndSetPan(gDSound, gSoundBufs[id], pan);
    SndPlay(gDSound, gSoundBufs[id]);
    gSamples[id].lastUsed = gTime;
    return 1;
}

// 0x412ae0: free the least recently used on-demand sample if it has not been
// used for 30 seconds.
void EvictOldSound()
{
    int best = -1;
    int oldest = 999999999;
    for (int i = 0; i < 106; i++) {
        if (gSamples[i].onDemand == 1 && gSoundBufs[i] && gSamples[i].lastUsed < oldest) {
            best = i;
            oldest = gSamples[i].lastUsed;
        }
    }
    if (best != -1 && gTime - oldest > 0x2ee) FreeSound(best);
}

// 0x412b40
void SetSoundLoop(int id, int loop)
{
    gSoundBufs[id]->loop = loop;
}

// 0x412b60
void StopSound(int id)
{
    SndStop(gDSound, gSoundBufs[id]);
    EvictOldSound();
}

// 0x412b80
void StopAllSounds()
{
    for (int i = 0; i < 106; i++)
        if (gSoundBufs[i] && SndIsPlaying(gDSound, gSoundBufs[i])) StopSound(i);
}

// 0x412bc0: footstep sound for the floor under the player.
void PlayFootstep()
{
    int tx = gPlayer.tileX, ty = gPlayer.tileY;
    int id = gFootstepSound[gCurLevel];
    unsigned tile = TILE(tx, ty);
    if (id < 0) return;
    switch (tile) {
    case 1:
        return;
    case 6:
    case 7:
    case 8:
    case 13:
    case 14:
        id = 0x20;
        break;
    }
    if (gCurLevel == 2 && tx >= 0x19 && tx <= 0x28 && ty >= 9 && ty <= 0x10) id = 1;
    int vol;
    if (id == 0 && gEquip.e0c == 0)
        vol = (gStats.charClass != 3 ? 1000 : 0) - 2500;
    else
        vol = (gStats.charClass != 3 ? 999 : 0) - 1000;
    if (!SndIsPlaying(gDSound, gSoundBufs[id])) PlaySound(id, -1, vol);
}

// 0x412cc0: wait for a sample to finish.
void WaitSound(int id)
{
    if (id < 0 || !gSoundBufs[id]) return;
    while (SndIsPlaying(gDSound, gSoundBufs[id])) PumpMessages();
}

// 0x412d10
int SoundFinished(int id)
{
    if (id < 0 || !gSoundBufs[id]) return 1;
    return SndIsPlaying(gDSound, gSoundBufs[id]) ? 0 : 1;
}

// 0x412d40: play a speech file and wait for it to finish.
// (port: gSpeechSkipCheck, when set, can cut the sample short: the
// conversations let the player skip a line)
bool (*gSpeechSkipCheck)() = nullptr;
bool gSpeechSkipped = false;

int PlaySpeechFile(const char *name)
{
    gSpeech = SndCreate(gDSound, name);
    if (!gSpeech) return 0;
    gSpeech->loop = 0;
    SndPlay(gDSound, gSpeech);
    while (SndIsPlaying(gDSound, gSpeech)) {
        PumpMessages();
        if (gSpeechSkipCheck && gSpeechSkipCheck()) {
            SndStop(gDSound, gSpeech);
            gSpeechSkipped = true;
            break;
        }
        plat_sleep(5);
    }
    SndRelease(gDSound, gSpeech);
    return 1;
}

static void EVTimerProc(unsigned, unsigned, unsigned long, unsigned long, unsigned long);

// 0x412dd0: start the looping environment sounds of this level.
void StartEVSound()
{
    if (gEVRunning) return;
    gEVTimer = plat_timer_start(0x32, EVTimerProc, 0);
    if (!gEVTimer) FatalError("couldn't get EVS timer");
    for (int i = 0; i < 6; i++) {
        EVSound *e = &gEVSounds[i];
        if (e->level != gCurLevel || e->dup) continue;
        if (!gSoundBufs[e->sound]) continue;   // (not loaded without a sound device)
        if (SndDuplicate(gDSound, gSoundBufs[e->sound], &e->dup))
            SndSetLoop(gDSound, e->dup, 1);
        else
            FatalError("Cannot dup. EV sound");
    }
    gEVRunning = 1;
}

// 0x412ea0
void KillEVSound()
{
    gEVRunning = 0;
    while (gEVBusy) plat_sleep(1);
    plat_timer_stop(gEVTimer);
    for (int i = 0; i < 6; i++) {
        EVSound *e = &gEVSounds[i];
        if (e->level != gCurLevel) continue;
        SndStop(gDSound, e->dup);
        SndRelease(gDSound, e->dup);
        e->dup = nullptr;
    }
}

// 0x412f20
static void EVTimerProc(unsigned, unsigned, unsigned long, unsigned long, unsigned long)
{
    if (!gEVRunning || gEVBusy) return;
    gEVBusy = 1;
    for (int i = 0; i < 6; i++)
        if (gCurLevel == gEVSounds[i].level) UpdateEVSound(i);
    gEVBusy = 0;
}

// 0x412f80
void UpdateEVSound(int i)
{
    EVSound *e = &gEVSounds[i];
    if (!e->dup) return;
    int d = TileDistance(gPlayer.tileX, gPlayer.tileY, e->x, e->y);
    if (d >= 20) {
        if (e->playing) SndStop(gDSound, e->dup);
        e->playing = 0;
        return;
    }
    int sx, sy;
    TileToScreen(e->x, e->y, &sx, &sy);
    int pan = (sx - 0x140) * 10;
    SndSetVolume(gDSound, e->dup, -125 * d);
    SndSetPan(gDSound, e->dup, pan);
    SndPlayAt(gDSound, e->dup, SndGetPosition(gDSound, e->dup));
    e->playing = 1;
}

// 0x413080: "Sound" options (CD volume / effects volume).
void SoundOptionsMenu()
{
    static MenuItem items[3];
    static bool init = false;
    if (!init) {
        init = true;
        MenuItem a = {nullptr, (uint16_t)gColorWhite, 'M', 0, (uint16_t)gColorAzure, 0, 0, -1, 0x96, 0xdc, 0x96, 0x1a4, 0xa9};
        MenuItem b = {nullptr, (uint16_t)gColorWhite, 'F', 0, (uint16_t)gColorAzure, 0, 1, -1, 0xaa, 0xdc, 0xaa, 0x1a4, 0xbd};
        MenuItem c = {nullptr, (uint16_t)gColorWhite, 0x1b, 0, (uint16_t)gColorAzure, 0, 2, -1, 0xbe, 0xdc, 0xbe, 0x1a4, 0xd1};
        items[0] = a;
        items[1] = b;
        items[2] = c;
    }
    Sprite *saved = GrabScreen(0x96, 0x64, 0x154, 0xfa);
    int sel;
    do {
        ShowMouse(0);
        saved->Blt(gDDW, 0x96, 0x64);
        gText.SetColor(gColorWhite, 0);
        gText.PrintC(0x64, (char *)gMsg[153] /* Select An Option : */);
        for (int i = 0; i < 3; i++) items[i].text = gMsg[166 + i];
        AddDirtyRect(0x96, 0x64, 0x154, 0xfa);
        sel = RunMenu(items, 3, gColorRed, nullptr, 1);
        saved->Blt(gDDW, 0x96, 0x64);
        AddDirtyRect(0x96, 0x64, 0x154, 0xfa);
        if (sel == 0)
            CDVolumeDialog();
        else if (sel == 1)
            WavVolumeDialog();
    } while (sel != 2 && sel != -1);
    RestoreScreen(0x96, 0x64, saved);
}

// 0x4132c0
void WavVolumeDialog()
{
    int cur = GetWaveVolume();
    ShowMouse(1);
    gPrefs.wavVolume = VolumeDialog(0, 10, (char *)"gamedat\\wav-vol.pcx", gMsg[180] /* WAV Audio Volume */, cur);
    SetWaveVolume(gPrefs.wavVolume);
}

// 0x413300: wave output volume 0..10.
int GetWaveVolume()
{
    if (!gDSound) return 0;
    unsigned v = snd_get_wave_volume() & 0xffff;
    return (int)(((unsigned long long)v * 0xa003c017ull >> 32) >> 12);
}

// 0x413340
void SetWaveVolume(int n)
{
    if (!gDSound) return;
    if (n < 0) n = 0;
    if (n > 10) n = 10;
    unsigned v = (unsigned)n * 6553;
    snd_set_wave_volume(v | (v << 16));
}

static void ClockTimerProc(unsigned, unsigned, unsigned long, unsigned long, unsigned long);

// 0x413380: start the 25 Hz game clock.
void StartClock()
{
    if (gClockRunning) return;
    gClockTimer = plat_timer_start(0x28, ClockTimerProc, 0);
    if (!gClockTimer) FatalError("couldn't get clock timer");
    gClockRunning = 1;
}

// 0x4133f0
void StopClock()
{
    if (!gClockRunning) return;
    plat_timer_stop(gClockTimer);
    gClockRunning = 0;
}

// 0x413440
static void ClockTimerProc(unsigned, unsigned, unsigned long, unsigned long, unsigned long)
{
    if (gAppActive && gClockRunning) gTime++;
}

// 0x413460: wait n clock ticks.
void WaitTicks(int n)
{
    int target = n + gTime + 1;
    while (gTime < target) {
        PumpMessages();
        plat_sleep(1);
    }
}

// ---------------------------------------------------------------------------
// CD audio (MCI in the original; music files in the port).
// ---------------------------------------------------------------------------

// 0x413480
int CDAudio_Open(CDAudio *cd)
{
    if (cd->opened) return 1;
    cd->deviceId = 1;
    cd->mode = 0x7d1;
    cd->opened = 1;
    return 1;
}

// 0x4134f0
void CDAudio_SetTimeFormat(CDAudio *cd, int fmt)
{
    (void)cd;
    (void)fmt;
}

// 0x413520
int CDAudio_Play(CDAudio *cd, int track)
{
    if (!cd->opened) return -4;
    CDAudio_SetTimeFormat(cd, 10);
    if (!cdmusic_play(track)) return -2;
    cd->track = track;
    cd->mode = 1;
    return 1;
}

// 0x4135a0
void CDAudio_Stop(CDAudio *cd)
{
    if (cd->opened) cdmusic_stop();
}

// 0x4135d0
int CDAudio_Close(CDAudio *cd)
{
    if (cd->opened) {
        CDAudio_Stop(cd);
        cd->opened = 0;
    }
    return 1;
}

// 0x413610
int CDAudio_GetPosition(CDAudio *cd)
{
    if (!cd->opened) return 0;
    return cdmusic_playing();
}

// 0x413660: nonzero while the track is still playing.
int CDAudio_Poll(CDAudio *cd)
{
    CDAudio_SetTimeFormat(cd, 0);
    return cdmusic_playing();
}

// 0x413690
void CDAudio_SetVolume(CDAudio *cd, int vol)
{
    if (!cd->opened) return;
    cdmusic_set_volume(vol);
}

// 0x413710
int CDAudio_GetVolume(CDAudio *cd)
{
    if (!cd->opened) return -1;
    return cdmusic_get_volume();
}

// ---------------------------------------------------------------------------
// Title screen
// ---------------------------------------------------------------------------

// 0x4137a0: returns 0 start, 1 continue, 2 exit.
int TitleMenu()
{
    ShowMouse(0);
    DrawTitleScreen();
    StopCDMusic();
    plat_sleep(5);
    PlayCDTrack(2);
    gUIMode = 4;
    int r;
    do {
        r = TitleMenuChoice();
        PumpMessages();
    } while (r == -1);
    gUIMode = 0;
    return r;
}

// 0x4137f0
int TitleMenuChoice()
{
    // (port: an Options entry between Continue and Exit)
    static MenuItem items[4];
    static bool init = false;
    if (!init) {
        init = true;
        MenuItem a = {nullptr, (uint16_t)gColorWhite, 'S', 0, (uint16_t)gColorAzure, 0, 1, -1, 0xf0, 0x5a, 0xf0, 0x226, 0x104};
        MenuItem b = {nullptr, (uint16_t)gColorWhite, 'C', 0, (uint16_t)gColorAzure, 0, 2, -1, 0x10e, 0x5a, 0x10e, 0x226, 0x122};
        MenuItem o = {nullptr, (uint16_t)gColorWhite, 'O', 0, (uint16_t)gColorAzure, 0, 4, -1, 0x12c, 0x5a, 0x12c, 0x226, 0x140};
        MenuItem c = {nullptr, (uint16_t)gColorWhite, 'E', 0, (uint16_t)gColorAzure, 0, 3, -1, 0x14a, 0x5a, 0x14a, 0x226, 0x15e};
        items[0] = a;
        items[1] = b;
        items[2] = o;
        items[3] = c;
    }
    items[0].text = gMsg[163];
    items[1].text = gMsg[164];
    items[2].text = "|Options";
    items[3].text = gMsg[165];
    Sprite *saved = GrabScreen(0x5a, 0xdc, 0x1cc, 0x8c);
    ShowMouse(0);
    int sel = RunMenu(items, 4, gColorGrey128, nullptr, 1);
    ShowMouse(1);
    RestoreScreen(0x5a, 0xdc, saved);
    switch (sel) {
    case 1: return 0;
    case 2: return 1;
    case 3: return 2;
    case 4:
        OptionsScreen(false);
        return -1;
    }
    return -1;
}
