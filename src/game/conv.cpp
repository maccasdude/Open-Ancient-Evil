// Conversation system, cut-scene AVIs and CD music helpers (0x401200-0x402140).
#include "game.h"

// 0x401200: "falling" cut-scene at the start of a new game
void PlayFallingMovie() {
    AviPlayer avi;
    StopCDMusic();
    ShowMouse(0);
    gDDW.Clear(0);
    gDDW.UpdateScreen();
    if (avi.Open((char *)"gamedat\\FALLING.AVI", &gDDW, gDSound, 0)) {
        avi.SetPosition(0, 0x40);
        avi.Start();
        ResetMouseClicks();
        do {
            if (MovieInterrupt()) break;   // (port: Esc or a click skips it)
        } while (avi.IsPlaying());
        avi.Stop();
    }
    ShowMouse(1);
}

// 0x4012f0: "smash" cut-scene
void PlaySmashMovie() {
    AviPlayer avi;
    ShowMouse(0);
    RedrawGameScreen(3);
    gDDW.UpdateScreen();
    RedrawGameScreen(3);
    StopCDMusic();
    KillEVSound();
    if (avi.Open((char *)"gamedat\\SMASH.AVI", &gDDW, gDSound, 0)) {
        avi.SetPosition(0x40, 0x10);
        avi.Start();
        ResetMouseClicks();
        do {
            if (MovieInterrupt()) break;   // (port: Esc or a click skips it)
        } while (avi.IsPlaying());
        avi.Stop();
    }
    StartEVSound();
    ShowMouse(1);
}

// 0x4013f0: pick a random CD track when the current one has finished
void UpdateCDMusic() {
    if (gPrefs.cdVolume <= 0) return;
    if (gPlayer.gameMode == 7) return;
    if (CDAudio_Poll(&gCD)) return;
    if (gMusicDelay) {
        gMusicDelay--;
        return;
    }
    int track = rand() % 8 + 2;
    if (track == gLastTrack) return;
    CDAudio_Play(&gCD, track);
    gLastTrack = track;
    gMusicDelay = rand() % 50;
}

// 0x401470
void PlayCDTrack(int track) { CDAudio_Play(&gCD, track); }

// 0x401480
void StopCDMusic() { CDAudio_Stop(&gCD); }

// 0x401490: CD volume dialog
void CDVolumeDialog() {
    int cur = CDAudio_GetVolume(&gCD);
    if (cur == -1) cur = 9;
    ShowMouse(1);
    gPrefs.cdVolume = VolumeDialog(0, 10, (char *)"gamedat\\cd-vol.pcx", gMsg[179] /* CD Audio Volume */, cur);
    if (gPrefs.cdVolume <= 0) CDAudio_Stop(&gCD);
    CDAudio_SetVolume(&gCD, gPrefs.cdVolume);
}

// 0x4014f0: reset all conversations to their initial state
void ResetConversations() {
    memcpy(gConvNodes, gConvNodesInit, sizeof(gConvNodes));
    LinkConversations();
}

// 0x401510
void LinkConversations() {
    for (int i = 0; i < 50; i++) gConvNodes[i].next = nullptr;
    for (int i = 0; i < 12; i++) gConvHeaders[i].first = nullptr;
    ConvAddNode(&gConvHeaders[0], &gConvNodes[1]);
    ConvAddNode(&gConvHeaders[0], &gConvNodes[2]);
    ConvAddNode(&gConvHeaders[0], &gConvNodes[3]);
    ConvAddNode(&gConvHeaders[0], &gConvNodes[0]);
    ConvAddNode(&gConvHeaders[1], &gConvNodes[4]);
    ConvAddNode(&gConvHeaders[1], &gConvNodes[5]);
    ConvAddNode(&gConvHeaders[1], &gConvNodes[6]);
    ConvAddNode(&gConvHeaders[1], &gConvNodes[47]);
    ConvAddNode(&gConvHeaders[3], &gConvNodes[46]);
    for (int i = 7; i <= 30; i++) ConvAddNode(&gConvHeaders[3], &gConvNodes[i]);
    ConvAddNode(&gConvHeaders[4], &gConvNodes[33]);
    ConvAddNode(&gConvHeaders[4], &gConvNodes[34]);
    ConvAddNode(&gConvHeaders[4], &gConvNodes[35]);
    ConvAddNode(&gConvHeaders[4], &gConvNodes[48]);
    ConvAddNode(&gConvHeaders[5], &gConvNodes[36]);
    ConvAddNode(&gConvHeaders[5], &gConvNodes[48]);
    ConvAddNode(&gConvHeaders[6], &gConvNodes[31]);
    ConvAddNode(&gConvHeaders[6], &gConvNodes[32]);
    ConvAddNode(&gConvHeaders[6], &gConvNodes[49]);
    for (int i = 37; i <= 41; i++) ConvAddNode(&gConvHeaders[8], &gConvNodes[i]);
    ConvAddNode(&gConvHeaders[9], &gConvNodes[42]);
    ConvAddNode(&gConvHeaders[9], &gConvNodes[43]);
    ConvAddNode(&gConvHeaders[10], &gConvNodes[44]);
    ConvAddNode(&gConvHeaders[10], &gConvNodes[45]);
}

// 0x401710: append node to a conversation's topic list (no duplicate topics)
void ConvAddNode(ConvHeader *h, ConvNode *n) {
    ConvNode *p = h->first;
    if (!p) {
        h->first = n;
        n->next = nullptr;
        return;
    }
    while (p->next) {
        if (p->topic == n->topic) return;
        p = p->next;
    }
    if (p->topic == n->topic) {
        n->next = nullptr;
        return;
    }
    p->next = n;
    n->next = nullptr;
}

// 0x401760: run a conversation, returns gConvResult
int RunConversation(ConvHeader *h) {
    bool done = false;
    gConvResult = 0;
    ShowMouse(0);
    StopCDMusic();
    RedrawGameScreen(3);
    AddDirtyRect(0, 0, 640, 480);
    gConvBackground = GrabScreen(30, 100, 520, 300);
    gShade.SetShadeLevel(31);
    if (h->greeting >= 0) {
        ConvPrintText(h, gConvGreetings, h->greeting);
        PlayConvSpeech(h, 1, 0);
        gConvBackground->Blt(gDDW, 30, 100);
        AddDirtyRect(30, 100, 520, 300);
    }
    ResetMouseClicks();
    if (h->first) {
        do {
            PumpMessages();
            ConvNode *sel = ConvChooseTopic(h->first);
            gConvBackground->Blt(gDDW, 30, 100);
            AddDirtyRect(30, 100, 520, 300);
            if (sel) {
                if (ConvSelectTopic(h, sel) == 4) done = true;
                gConvBackground->Blt(gDDW, 30, 100);
                AddDirtyRect(30, 100, 520, 300);
            } else {
                done = true;
            }
        } while (!done);
    }
    if (h->farewell >= 0) {
        gConvBackground->Blt(gDDW, 30, 100);
        ConvPrintText(h, gConvFarewells, h->farewell);
        PlayConvSpeech(h, 2, 0);
    }
    RestoreScreen(30, 100, gConvBackground);
    FreeDirtyRects();
    ShowMouse(1);
    return gConvResult;
}

// 0x401920: print paragraph number 'index' of a NULL-separated text table
void ConvPrintText(ConvHeader *h, const char **table, int index) {
    int y = 100;
    if (index != -1) {
        gDDW.FillRect(30, 100, 80, 80, 1);
        gFaces[h->portrait].Draw(30, 100, gDDW);
        int n = 0, i = 0;
        if (index) {
            do {
                if (!table[i]) n++;
                i++;
            } while (n != index);
        }
        const char **p = &table[i];
        while (*p) {
            ConvPrintLine(h, y, *p);
            p++;
            y += 20;
        }
    }
    AddDirtyRect(30, 100, 520, 300);
    UpdateAndRestore(&gDDW);
}

// 0x4019d0: show the list of available topics and let the player pick one
ConvNode *ConvChooseTopic(ConvNode *list) {
    uint16_t white = gColorWhite;
    ConvNode *nodes[13];
    if (!(gConvMenuInit & 1)) {
        gConvMenuInit |= 1;
        gConvMenu[0].color = white;
        gConvMenu[0].hotkey = 0;
        gConvMenu[0].hiColor = 0xffff;
        gConvMenu[0].id = 0;
        gConvMenu[0].x = -1;
        gConvMenu[0].y = 150;
        gConvMenu[0].left = 220;
        gConvMenu[0].top = 150;
        gConvMenu[0].right = 420;
        gConvMenu[0].bottom = 169;
        memset(&gConvMenu[1], 0, sizeof(MenuItem) * 12);
    }
    int count = 0;
    ConvNode *n = list;
    while (n) {
        if (n->state == 4) {
            MenuItem *m = &gConvMenu[count];
            nodes[count] = n;
            m->hotkey = (char)0xff;
            m->id = count;
            m->color = white;
            m->text = gConvTopics[n->topic];
            m->left = 120;
            m->x = 120;
            m->right = 420;
            int y = count * 18 + 100;
            m->top = y;
            m->y = y;
            m->bottom = y + 17;
            count++;
        }
        n = n->next;
        if (count == 13) n = nullptr;
    }
    gDDW.FillRect(30, 100, 80, 80, 1);
    gFaces[gStats.portrait].Draw(30, 100, gDDW);
    AddDirtyRect(30, 100, 580, 260);
    ShowMouse(1);
    int sel;
    do {
        sel = RunMenu(gConvMenu, count, gColorRed, 0, 1);
    } while (sel == -1);
    ShowMouse(0);
    ConvNode *r = nodes[sel];
    if (gStats.charClass == 3) PlayConvSpeech(nullptr, 5, r->topic);
    else PlayConvSpeech(nullptr, 4, r->topic);
    return r;
}

// 0x401b80: act on the chosen topic; returns flags & 4 (end conversation)
int ConvSelectTopic(ConvHeader *h, ConvNode *n) {
    if (!n) return 4;
    if (n->flags & 0x10) {
        if (!ConvPayForAnswer(h, n->costText, n->cost)) return 0;
    }
    if (n->answer != -1) {
        ConvPrintText(h, gConvAnswers, n->answer);
        PlayConvSpeech(h, 3, n->answer);
    }
    UpdateAndRestore(&gDDW);
    if (n->flags & 2) ConvHideTopic(h->first, n->topic);
    if (n->flags & 8) gConvResult = n->costText;
    return (uint8_t)n->flags & 4;
}

// 0x401c20: print one line of conversation text with inline markup:
//   %...%   toggle highlight colour
//   #n#     unlock topic n
//   |n|     mark topic n as asked
//   @       insert the current price
void ConvPrintLine(ConvHeader *h, int y, const char *s) {
    char num1[12], num2[12], price[12], line[64];
    int n = 0;
    int highlight = 0;
    gText.SetColor(gColorGrey128, 0);
    gText.SetPosition(120, y);
    if (*s) {
        do {
            char c = *s;
            if (c == '%') {
                line[n] = 0;
                gText.Print(line);
                highlight ^= 1;
                if (highlight) gText.SetColor(gColorWhite, 0);
                else gText.SetColor(gColorGrey128, 0);
                n = 0;
            } else if (c == '#') {
                int i = 0;
                c = s[1];
                s++;
                do {
                    num1[i] = c;
                    c = s[1];
                    i++;
                    s++;
                } while (c != '#');
                num1[i] = 0;
                ConvShowTopic(h->first, atoi(num1));
            } else if (c == '|') {
                int i = 0;
                c = s[1];
                s++;
                do {
                    num2[i] = c;
                    c = s[1];
                    i++;
                    s++;
                } while (c != '|');
                num2[i] = 0;
                ConvHideTopic(h->first, atoi(num2));
            } else if (c == '@') {
                line[n] = 0;
                win_itoa(gConvCost, price, 10);
                strcat(line, price);
                n += (int)strlen(price);
            } else {
                line[n++] = c;
            }
            c = s[1];
            s++;
        } while (*s);
    }
    line[n] = 0;
    gText.Print(line);
}

// 0x401de0: make a hidden topic (state 2) visible (state 4)
void ConvShowTopic(ConvNode *n, int topic) {
    for (; n; n = n->next) {
        if (n->topic == topic && n->state == 2) {
            n->state = 4;
            return;
        }
    }
}

// 0x401e10: mark a visible topic as used up (state 1)
void ConvHideTopic(ConvNode *n, int topic) {
    for (; n; n = n->next) {
        if (n->topic == topic && n->state == 4) {
            n->state = 1;
            return;
        }
    }
}

// 0x401e40: ask the player to pay 'cost' gold for an answer
int ConvPayForAnswer(ConvHeader *h, int text, int cost) {
    gConvCost = cost;
    ConvPrintText(h, gConvCostTexts, text);
    if (text != 3) PlayConvSpeech(h, 6, text);
    int sample;
    switch (cost) {
    case 2: sample = 5; break;
    case 10: sample = 6; break;
    case 12: sample = 7; break;
    case 15: sample = 8; break;
    case 17: sample = 9; break;
    case 20: sample = 10; break;
    default: sample = text; break;
    }
    PlayConvSpeech(h, 6, sample);
    if (text == 3) PlayConvSpeech(h, 6, text);
    gText.PrintC(200, (char *)gMsg[225] /* Will you pay ? */, gColorWhite);
    AddDirtyRect(0, 200, 640, 20);
    int no = YesNoMenu();
    gConvBackground->Blt(gDDW, 30, 100);
    ShowMouse(0);
    if (!no) return 0;
    if (gPlayer.gold < (unsigned)cost) {
        gText.PrintC(200, (char *)gMsg[226] /* You do not have enough gold */, gColorWhite);
        gDDW.UpdateScreen();
        do {
            PumpMessages();
        } while (!MouseLeftClicked());
        gConvBackground->Blt(gDDW, 30, 100);
        return 0;
    }
    gPlayer.gold -= cost;
    return 1;
}

// Port: skip a spoken line with a click, Space, Enter or Esc (other keys
// are ignored while a line plays).
static bool ConvSkipInput()
{
    bool skip = MouseLeftClicked() | MouseRightClicked();
    uint8_t k[2];
    while (KeyPop(k))
        if (k[0] == ' ' || k[0] == 0x0d || k[0] == 0x1b || k[1] == 0x20) skip = true;
    return skip;
}

// 0x401fe0: play a speech sample from the (optional) CD speech directory.
// Samples are split into parts A, B, C... which are played until one is missing.
void PlayConvSpeech(ConvHeader *h, int kind, int index) {
    char base[80];
    char name[96];
    switch (kind) {
    case 1: sprintf(base, "%c:\\SPEECH\\ENGLISH\\%sOP%d", gCDDrive, h->voice, h->voiceVariant); break;
    case 2: sprintf(base, "%c:\\SPEECH\\ENGLISH\\%sCL%d", gCDDrive, h->voice, index); break;
    case 3: sprintf(base, "%c:\\SPEECH\\ENGLISH\\%s%d", gCDDrive, h->voice, index); break;
    case 4: sprintf(base, "%c:\\SPEECH\\ENGLISH\\MM%d", gCDDrive, index); break;
    case 5: sprintf(base, "%c:\\SPEECH\\ENGLISH\\MF%d", gCDDrive, index); break;
    case 6: sprintf(base, "%c:\\SPEECH\\ENGLISH\\TGET%d", gCDDrive, index); break;
    default: break;
    }
    int stop = 0;
    char part = 'A';
    // (port: clicks and keys from before the line do not skip it)
    ResetMouseClicks();
    {
        uint8_t k[2];
        while (KeyPop(k)) {}
    }
    gSpeechSkipped = false;
    gSpeechSkipCheck = ConvSkipInput;
    do {
        sprintf(name, "%s%c.WAV", base, part);
        if (!PlaySpeechFile(name)) stop = 1;
        part++;
    } while (!stop && !gSpeechSkipped);
    gSpeechSkipCheck = nullptr;
    if (gSpeechSkipped) {
        gSpeechSkipped = false;
        ResetMouseClicks();
        return;
    }
    // Port: the speech is what keeps the character's words on screen (the
    // original would not start without its CD). Without the speech files,
    // or with no sound, wait for a click or key instead of letting the text
    // vanish at once. (Not for the player's own lines and the price
    // samples, whose text stays up anyway.)
    if (part == 'B' && (kind == 1 || kind == 2 || kind == 3)) {
        UpdateAndRestore(&gDDW);
        ResetMouseClicks();
        uint8_t k[2];
        while (KeyPop(k)) {}
        ShowMouse(1);
        for (;;) {
            PumpMessages();
            if (MouseLeftClicked() || MouseRightClicked() || KeyPop(k)) break;
            plat_sleep(10);
        }
        ShowMouse(0);
        ResetMouseClicks();
    }
}
