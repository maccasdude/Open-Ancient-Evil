// Character sheet, the depth sorted foreground object list, shadows, lines,
// panels and dynamic lighting (0x417700-0x419010).
#include "game.h"
#include <algorithm>

static inline int32_t &EquipSlot(int i) { return ((int32_t *)&gEquip)[i]; }

// 0x417700
void DrawCharacterSheet()
{
    char buf[32];
    RecalcStats();
    gText.SetColor(gColorAzure, 0);
    gText.PrintS(0x140, 0x3c, gStats.name, gColorNearBlack);
    gText.PrintS(0x140, 0x50, (char *)gMsg[208 + gStats.charClass], gColorNearBlack);

    sprintf(buf, "%d", gStats.baseStr);
    gText.Print(0x178, 0x64, buf);
    sprintf(buf, "%d", gStats.str);
    gText.Print(0x1b5, 0x64, buf);
    int w = gEquip.weaponType;
    if (w != 0 && w != 4) {
        sprintf(buf, "+%d", gWeaponDamage[w]);
        gText.Print(0x1d2, 0x64, buf);
    }

    sprintf(buf, "%d", gStats.baseTou);
    gText.Print(0x178, 0x7a, buf);
    sprintf(buf, "%d", gStats.tou);
    gText.Print(0x1b5, 0x7a, buf);
    if (gEquip.armourType || gEquip.shield || gPlayer.stoneSkin) {
        int v = gArmourDefence[gEquip.armourType];
        if (gPlayer.stoneSkin) v += 2;
        if (gEquip.shield == 1) v++;
        if (v) {
            sprintf(buf, "+%d", v);
            gText.Print(0x1d2, 0x7a, buf);
        }
    }

    sprintf(buf, "%d", gStats.baseDex);
    gText.Print(0x178, 0x91, buf);
    sprintf(buf, "%d", gStats.dex);
    gText.Print(0x1b5, 0x91, buf);
    sprintf(buf, "%d", gStats.baseInt);
    gText.Print(0x178, 0xaa, buf);
    sprintf(buf, "%d", gStats.intel);
    gText.Print(0x1b5, 0xaa, buf);

    sprintf(buf, "%d", gStats.xp);
    gText.PrintRJ(0x1ac, 0xe0, buf);
    sprintf(buf, "%d", gStats.level);
    gText.PrintRJ(0x1ac, 0xf7, buf);
    sprintf(buf, "%d", gStats.magicXp);
    gText.PrintRJ(0x1e8, 0xe0, buf);
    sprintf(buf, "%d", gStats.maxSpellLevel);
    gText.PrintRJ(0x1e8, 0xf7, buf);

    float carried = CarriedWeight();
    sprintf(buf, "%2.1f %s %2.1f", (double)carried, gMsg[230] /*  */, (double)gStats.maxWeight);
    gText.PrintRJ(0x1d0, 0x110, buf);
}

// 0x417aa0
int RectsOverlap(const RECT *a, const RECT *b)
{
    if (!a || !b) return 0;
    if (a->left > b->right || a->right < b->left) return 0;
    if (a->top > b->bottom) return 0;
    return a->bottom >= b->top;
}

// 0x417af0
void ResetFGList()
{
    gFGHead.next = &gFGTail;
    gFGTail.next = nullptr;
    gFGHead.prev = nullptr;
    gFGTail.depth = 2000.0f;
    gFGHead.depth = -2000.0f;
    gFGCount = 0;
}

// 0x417b20
void ClearFGList()
{
    gFGHead.next = &gFGTail;
    gFGTail.next = nullptr;
    gFGHead.prev = nullptr;
    gFGTail.prev = &gFGHead;
    gFGMinDepth = gFGTail.depth;
    gFGMaxDepth = gFGHead.depth;
    gFGCount = 0;
}

// 0x417b60: insert a copy of o into the depth sorted list (ascending depth).
void AddFGObject(FGObject *o)
{
    float d = o->depth;
    if (gFGCount >= kMaxFGObjects) {
        fprintf(stderr, "INFO!! Too many FG Objects : %d\n", gFGCount);
        gFGCount = 0;
        gFGHead.next = &gFGTail;  // original pointed the head at itself
        gFGTail.prev = &gFGHead;
        return;
    }
    if (d > gFGMaxDepth) gFGMaxDepth = d;
    if (d < gFGMinDepth) gFGMinDepth = d;

    FGObject *after;
    if (d > (gFGMinDepth + gFGMaxDepth) * 0.5f) {
        // walk back from the tail
        after = &gFGTail;
        if (gFGTail.prev) {
            while (after->depth > d) {
                after = after->prev;
                if (!after->prev) break;
            }
        }
    } else {
        after = &gFGHead;
        for (FGObject *n = gFGHead.next; n && n->depth < d; n = n->next) after = n;
    }
    FGObject *node = &gFGPool[gFGCount];
    memcpy(node, o, offsetof(FGObject, next));
    FGObject *next = after->next;
    after->next = node;
    next->prev = node;
    node->next = next;
    node->prev = after;
    gFGCount++;
}

// A wall or sprite in front of the player: dithered, or in the enhanced
// mode blended at the 3dfx build's opacity (vertex alpha 85, 0x40b640).
static void SeeThrough(CSprite16 *spr, FGObject *o, uint8_t *dst, unsigned long pitch)
{
    if (gColorLight)
        spr->DrawBlend(o->x0, o->y0, dst, pitch, gClip, 85 * 256 / 255);
    else
        spr->DrawDithered(o->x0, o->y0, dst, pitch, gClip);
}

// 0x417cc0: draw the foreground list back to front and reset it.
void DrawFGObjects()
{
    gFGRendering = 1;
    gClip.SetViewport(ViewL(), ViewT(), ViewR() + (gLayout.wide() ? 1 : 0), gViewHeight);   // (port: the clipper's right edge is exclusive)
    uint8_t *dst;
    unsigned long pitch;
    if (!gDDW.Lock(&dst, &pitch)) {
        fprintf(stderr, "FGL lock failed\n");
        return;
    }
    FGObject *o = gFGTail.prev;
    for (int i = 0; i < gFGCount; i++, o = o->prev) {
        int shade = o->light & 0x1f;
        const bool rgb = gColorLight && (o->lrgb || o->crgb);
        gShade.SetShadeLevel(shade);
        // (the colour table only where a sprite is drawn through it: making
        // one costs a 32K entry table when it is not cached)
        auto colour = [&] { if (rgb) gShade.SetShadeColor(o->lrgb); };
        uint32_t t = (uint32_t)o->type;
        CSprite16 *spr = (CSprite16 *)o->obj;
        if (t & 0x10) {
            SetModelCutY((float)o->color);
            SetModelVertexScale(o->scale);
            if (t & 0x200) SetModelOpaque(0);
            o->model->SetLightRange(shade);
            SetModelLightRGB(rgb ? o->lrgb : 0);
            o->model->Draw(o->x0, o->y0, o->frame, o->angle);
            GetModelRect(o->rect);
            SetModelOpaque(1);
            SetModelLightRGB(0);
        } else if (t & 0x80) {
            if (o->color == 0) DrawFGShadow(o);
        } else if (t & 0x20) {
            o->particle->Draw(&gShade, &gDDW);
        } else if (t & 0x100) {
            if (gColorLight)
                DrawLineAA(o->x0, o->y0, o->x1, o->y1, (uint16_t)o->color, 1);
            else
                DrawLine(o->x0, o->y0, o->x1, o->y1, (uint16_t)o->color, 1);
        } else if (t & 0x40) {
            if (o->depth < gPlayer.depth && RectsOverlap((RECT *)gPlayer.rect, o->rect)) {
                colour();
                SeeThrough(spr, o, dst, pitch);
            } else {
                if (rgb) {
                    const uint16_t c[2] = {o->lrgb ? o->lrgb : GreyLight(o->light), o->crgb ? o->crgb : GreyLight(o->color)};
                    spr->DrawLitLeftToRightRGB(o->x0, o->y0, dst, pitch, gClip, c);
                } else {
                    uint16_t lights[2] = {(uint16_t)o->light, (uint16_t)o->color};
                    spr->DrawLitLeftToRight(o->x0, o->y0, dst, pitch, gClip, lights);
                }
            }
        } else if (t & 4) {
            colour();
            SeeThrough(spr, o, dst, pitch);
        } else if (t & 8) {
            spr->DrawWhite(o->x0, o->y0, dst, pitch, gClip);
        } else if (o->depth < gPlayer.depth && RectsOverlap((RECT *)gPlayer.rect, o->rect)) {
            colour();
            SeeThrough(spr, o, dst, pitch);
        } else {
            colour();
            spr->Draw(o->x0, o->y0, dst, pitch, gClip);
        }
    }
    ClearFGList();
    gClip.SetViewport(0, 0, 0x27f, 0x1df);
    gDDW.Unlock();
    gFGRendering = 0;
}

// 0x418040
void DrawFGShadow(FGObject *o)
{
    if (!gPrefs.shadows) return;
    gShadowAlpha = o->crgb;   // (0: the original halving)
    o->model->DrawShadow(o->x0, o->y0, o->fx, o->fy, 0.0f, o->frame, o->angle,
                         o->lightX, o->lightY, o->lightH);
    gShadowAlpha = 0;
}

// 0x4180b0: queue shadows of a model for every light that reaches it.
void DrawModelShadow(float x, float y, Model *m, int frame, int angle, int flag)
{
    if (!gPrefs.shadows) return;
    FGObject o;
    memset(&o, 0, sizeof o);
    o.type = 0x80;
    o.fx = x;
    o.fy = y;
    WorldToScreen(x, y, &o.x0, &o.y0);
    o.depth = ScreenDepth(x, y);
    o.model = m;
    o.angle = (float)angle;
    o.frame = frame;
    o.color = TileHeight((int)x, (int)y);
    for (int i = gLightFirst < 0 ? 0 : gLightFirst; i <= gLightLast; i++) {   // (-1: none)
        Light &l = gLights[i];
        if (gCurLevel != l.f0) continue;
        CastShadow(&o, (float)l.f4 + 0.5f, (float)l.f8 + 0.5f, 0x50, (int16_t)(l.fc >> 16));
    }
    if (flag) {
        if (gLightSpell) {
            CastShadow(&o, gPlayer.x, gPlayer.y, 100, gLightRadius);
        } else if (gEquip.shield == 2 && gActionMode == 0) {
            int a = gPlayer.angle + 270;
            float ly = gPlayer.y - CosDeg(a) * 0.2f;
            float lx = SinDeg(a) * 0.2f + gPlayer.x;
            CastShadow(&o, lx, ly, 100, gLightRadius);
        }
    }
    for (int i = 0; i < 10; i++) {
        Projectile &p = gProjectiles[i];
        if (!p.active) continue;
        switch (p.kind) {
        case 0: case 1: case 3: case 5: case 6:
            CastShadow(&o, p.x, p.y, 0x50, 3);
            break;
        }
    }
}

// 0x4182b0
void CastShadow(FGObject *o, float lx, float ly, int height, int range)
{
    float d = DistanceB(lx, ly, o->fx, o->fy);
    if (SmoothLightModel()) {
        // Port (smooth light model): a shadow is strongest near its light and
        // fades out with the distance instead of vanishing at a set range;
        // right next to a light (where it would spread out all round) it
        // fades too. The player's own torch gives a constant soft shadow.
        float dist = sqrtf(d), reach = (float)range * 1.4f;
        if (!(dist < reach) || !LineOfSight(lx, ly, o->fx, o->fy, 0)) return;
        float u = dist / reach, s = (1.0f - u * u) * (1.0f - u * u);
        bool own = dist < 0.3f && fabsf(o->fx - gPlayer.x) < 0.001f && fabsf(o->fy - gPlayer.y) < 0.001f;
        float a;
        if (own) {
            a = 0.42f;
        } else {
            float n = (dist - 0.2f) / 0.7f;
            n = n < 0.0f ? 0.0f : n > 1.0f ? 1.0f : n;
            a = 0.62f * s * n * n * (3.0f - 2.0f * n);
        }
        int alpha = (int)(a * 256.0f);
        if (alpha < 6) return;
        o->crgb = (uint16_t)alpha;
        o->lightX = lx;
        o->lightY = ly;
        o->lightH = (float)((double)height + (double)d * 10.0f);
        AddFGObject(o);
        o->crgb = 0;
        return;
    }
    if (!(d < (float)(range * range * 2))) return;
    int tx = (int)o->fx, ty = (int)o->fy;
    if (gLightMap[tx * 65 + ty] <= 8) return;
    if (!LineOfSight(lx, ly, o->fx, o->fy, 0)) return;
    o->lightX = lx;
    o->lightY = ly;
    o->lightH = (float)((double)height + (double)d * 10.0f);
    AddFGObject(o);
}

// 0x418380
float ScreenDepth(float x, float y)
{
    return (float)(320.0f - ((double)y + x));
}

// 0x418390
void DrawLine(int x0, int y0, int x1, int y1, int color, int blend)
{
    if (abs(x0 - x1) >= abs(y0 - y1))
        DrawLineH(x0, y0, x1, y1, color, blend);
    else
        DrawLineV(x0, y0, x1, y1, color, blend);
}

static inline uint16_t *PixelAt(int x, int y)
{
    return (uint16_t *)(gDDW.surfacePtr + (ptrdiff_t)y * gDDW.pitch) + x;
}

// 0x4183f0: x-major Bresenham (surface must be locked).
void DrawLineH(int x0, int y0, int x1, int y1, int color, int blend)
{
    int xs, ys, xe, ye;
    if (x0 > x1) { xs = x1; ys = y1; xe = x0; ye = y0; }
    else { xs = x0; ys = y0; xe = x1; ye = y1; }
    if (xs < ViewL() || xe > ViewR() || ys < ViewT() || ye < ViewT() || ys > ViewCap() || ye > ViewCap()) return;
    int step = ye <= ys ? -1 : 1;
    int dx = xe - xs;
    int ady = abs(ye - ys);
    int incE = ady * 2;
    int incNE = (ady - dx) * 2;
    int d = incE - dx;
    int y = ys;
    uint16_t *p = PixelAt(xs, ys);
    if (blend) {
        unsigned mask = gDDW.MakePixel16(0x7f, 0x7f, 0x7f);
        unsigned c = ((uint16_t)color >> 1) & mask;
        *p = (uint16_t)(((*p >> 1) & mask) + c);
        for (int x = xs + 1; x <= xe; x++) {
            if (d >= 0) { y += step; d += incNE; }
            else d += incE;
            p = PixelAt(x, y);
            *p = (uint16_t)(((*p >> 1) & mask) + c);
        }
    } else {
        *p = (uint16_t)color;
        for (int x = xs + 1; x <= xe; x++) {
            if (d >= 0) { y += step; d += incNE; }
            else d += incE;
            *PixelAt(x, y) = (uint16_t)color;
        }
    }
}

// 0x418580: y-major Bresenham.
void DrawLineV(int x0, int y0, int x1, int y1, int color, int blend)
{
    int xs, ys, xe, ye;
    if (y0 > y1) { xs = x1; ys = y1; xe = x0; ye = y0; }
    else { xs = x0; ys = y0; xe = x1; ye = y1; }
    if (xs < ViewL() || xe > ViewR() || ys < ViewT() || ye < ViewT() || ys > ViewCap() || ye > ViewCap()) return;
    int step = xe <= xs ? -1 : 1;
    int adx = abs(xe - xs);
    int dy = ye - ys;
    int incE = adx * 2;
    int incNE = (adx - dy) * 2;
    int d = incE - dy;
    int x = xs;
    uint16_t *p = PixelAt(xs, ys);
    if (blend) {
        unsigned mask = gDDW.MakePixel16(0x7f, 0x7f, 0x7f);
        unsigned c = ((uint16_t)color >> 1) & mask;
        *p = (uint16_t)(((*p >> 1) & mask) + c);
        for (int y = ys + 1; y <= ye; y++) {
            if (d >= 0) { x += step; d += incNE; }
            else d += incE;
            p = PixelAt(x, y);
            *p = (uint16_t)(((*p >> 1) & mask) + c);
        }
    } else {
        *p = (uint16_t)color;
        for (int y = ys + 1; y <= ye; y++) {
            if (d >= 0) { x += step; d += incNE; }
            else d += incE;
            *PixelAt(x, y) = (uint16_t)color;
        }
    }
}

// Port (enhanced mode): the 3dfx build draws these lines with grAADrawLine,
// half transparent when blending (0x417600). Wu's antialiased line.
void DrawLineAA(int x0, int y0, int x1, int y1, int color, int blend)
{
    // the same visibility test as the original
    int ys = y0, ye = y1;
    int xs = x0, xe = x1;
    if (xs > xe) std::swap(xs, xe);
    if (xs < ViewL() || xe > ViewR() || ys < ViewT() || ye < ViewT() || ys > gViewHeight || ye > gViewHeight) return;
    const unsigned cr = ((unsigned)color >> 11) & 31, cg = ((unsigned)color >> 5) & 63, cb = (unsigned)color & 31;
    const int alpha = blend ? 128 : 255;
    auto plot = [&](int x, int y, int cov) {   // cov 0..255
        if (x < ViewL() || x > ViewR() || y < ViewT() || y > gViewHeight) return;
        uint16_t *p = PixelAt(x, y);
        unsigned a = (unsigned)(cov * alpha) >> 8;
        unsigned d = *p;
        unsigned r = ((d >> 11) * (256 - a) + cr * a) >> 8;
        unsigned g = (((d >> 5) & 63) * (256 - a) + cg * a) >> 8;
        unsigned b = ((d & 31) * (256 - a) + cb * a) >> 8;
        *p = (uint16_t)(r << 11 | g << 5 | b);
    };
    bool steep = abs(y1 - y0) > abs(x1 - x0);
    float ax = (float)x0, ay = (float)y0, bx = (float)x1, by = (float)y1;
    if (steep) { std::swap(ax, ay); std::swap(bx, by); }
    if (ax > bx) { std::swap(ax, bx); std::swap(ay, by); }
    float dx = bx - ax, dy = by - ay;
    float grad = dx == 0.0f ? 1.0f : dy / dx;
    float y = ay;
    for (int x = (int)ax; x <= (int)bx; x++, y += grad) {
        int iy = (int)floorf(y);
        int f = (int)((y - (float)iy) * 255.0f);
        // ends fade by half so joined segments do not double up
        int end = (x == (int)ax || x == (int)bx) ? 1 : 0;
        int c0 = (255 - f) >> end, c1 = f >> end;
        if (steep) {
            plot(iy, x, c0);
            plot(iy + 1, x, c1);
        } else {
            plot(x, iy, c0);
            plot(x, iy + 1, c1);
        }
    }
}

// 0x418710: status icons of timed effects along the top of the screen.
void DrawEffectIcons()
{
    static int32_t *const kFlags[6] = {&gPlayer.stoneSkin, &gPlayer.magicShield,
                                       (int32_t *)&gPlayer.invisibility, &gPlayer.identify,
                                       &gPlayer.forceField, &gPlayer.resistUndead};
    static const int kIcons[6] = {31, 30, 29, 26, 28, 27};
    for (int i = 0, x = 0x1b8; x < 0x278; i++, x += 0x20) {
        if (*kFlags[i]) {
            if (!gEffectShown[i]) gEffectShown[i] = 1;
            gActionSprites.Blt(kIcons[i], x, 0, 2);
        } else if (gEffectShown[i]) {
            g_5c5824 = 1;
            gEffectShown[i] = 0;
        }
    }
}

// 0x418780: open container view.
void DrawContainer(int idx)
{
    gShade.SetShadeLevel(0x1f);
    Container &c = gContainers[idx];
    if (c.closed) {
        g_5a6448[c.type * 2].Draw(0xb6, 0xa9, gDDW);
        return;
    }
    g_5a6448[c.type * 2 + 1].Draw(0xb6, 0xa9, gDDW);
    for (int i = 0; i < 8; i++) {
        int16_t item = c.items[i].item;
        if (item == -1) continue;
        gItemIcons[gItemIconIdx[item]].Draw(gChestSlotX[i], gChestSlotY[i], gDDW);
    }
}

// 0x418840
void DrawMessages()
{
    for (int i = 0, y = ViewT() + 10; i < gNumMessages; i++, y += 0x10) {   // (10, 10)
        gText.SetColor(gMessages[i].color, 0);
        gText.Print(ViewL() + 10, y, gMessages[i].text);
    }
    g_5b5268 = 0;
}

// 0x4188a0: equipment screen.
void DrawEquipment()
{
    char buf[12];
    for (int slot = 0; slot < 9; slot++) {
        int v = EquipSlot(slot);
        if (!v) continue;
        gShade.SetShadeLevel(0x1f);
        if ((slot == 0 || slot == 4) && gActionMode) gShade.SetShadeLevel(0xf);
        gEquipSprites[(int8_t)gEquipSpriteIdx[slot * 20 + v]].Draw(gEquipX[slot], gEquipY[slot], gDDW);
    }
    gText.SetColor(gColorWhite, 0);
    sprintf(buf, "%d", gPlayer.arrows);
    gText.Print(0x9c, 0x26, buf);
    sprintf(buf, "%d", gPlayer.gold);
    gText.Print(0x91, 0x117, buf);
    gShade.SetShadeLevel(0x1f);
}

// 0x4189a0: light cast by the player's torch / light spell.
void AddPlayerLight()
{
    int r = gLightRadius;
    int x0 = (int)(gPlayer.x - (float)r);
    if (x0 < 0) x0 = 0;
    int x1 = (int)(gPlayer.x + (float)r);
    if (gMapWidth < x1) x1 = gMapWidth;
    int y0 = (int)(gPlayer.y - (float)r);
    if (y0 < 0) y0 = 0;
    int y1 = (int)(gPlayer.y + (float)r);
    if (gMapHeight < y1) y1 = gMapHeight;
    int level;
    if (gEquip.shield != 2 && !gLightSpell)
        level = gLightBright;
    else
        level = rand() % 3 + gLightBright - 2;
    for (int x = x0; x < x1; x++) {
        float fx = (float)x + 0.5f;
        for (int y = y0; y < y1; y++) {
            float fy = (float)y + 0.5f;
            if (gMapVis[x * 65 + y] == 0xff) continue;
            int d = (int)Distance(gPlayer.x, gPlayer.y, fx, fy);
            if (d >= gLightRadius) continue;
            if (!LineOfSight(fx, fy, gPlayer.x, gPlayer.y, 0)) continue;
            int fl = LightFalloff(level, (float)gLightRadius, (float)d);
            uint16_t v = (uint16_t)(gDynLight[x * 65 + y] + fl);
            if (v > 0x1f) v = 0x1f;
            gDynLight[x * 65 + y] = v;
            if (gColorLight) {
                // 3dfx (0x417990): a torch gives warm light, the light spell white
                uint16_t &c = gDynRGB[x * 65 + y];
                float lv = (float)(fl & 0xff);
                if (gEquip.shield == 2 && gActionMode == 0 && gLightSpell <= 0)
                    c = AddLightColor(c, lv, 1.0f, 0.875f, 0.75f);
                else
                    c = AddLightColor(c, lv, 1.0f, 1.0f, 1.0f);
            }
        }
    }
}

// 0x418b90: light level of a tile including flying projectiles.
int TileLightLevel(int x, int y)
{
    float fx = (float)x + 0.5f, fy = (float)y + 0.5f;
    uint16_t v = gDynLight[x * 65 + y];
    for (int i = 0; i < 10; i++) {
        Projectile &p = gProjectiles[i];
        if (p.active != 1 && p.active != 2) continue;
        switch (p.kind) {
        case 0: case 1: case 3: case 5: case 6: {
            float d = Distance(fx, fy, p.x, p.y);
            float r = p.active == 2 ? 7.0f : 3.0f;
            if (d < r && LineOfSight(fx, fy, p.x, p.y, 1)) v += (uint16_t)LightFalloff(0x18, r, d);
            break;
        }
        }
    }
    if (v > 0x1f) v = 0x1f;
    return v;
}

// 0x417c00 (3dfx): light colour of a tile including flying projectiles;
// fireballs (kinds 0 and 5) light red, the others blue.
uint16_t TileLightRGB(int x, int y)
{
    float fx = (float)x + 0.5f, fy = (float)y + 0.5f;
    uint16_t c = gDynRGB[x * 65 + y];
    for (int i = 0; i < 10; i++) {
        Projectile &p = gProjectiles[i];
        if (p.active != 1 && p.active != 2) continue;
        switch (p.kind) {
        case 0: case 1: case 3: case 5: case 6: {
            float d = Distance(fx, fy, p.x, p.y);
            float r = p.active == 2 ? 7.0f : 3.0f;
            if (!(d < r) || !LineOfSight(fx, fy, p.x, p.y, 1)) break;
            float lv = (float)LightFalloff(0x18, r, d);
            if (p.kind == 0 || p.kind == 5)
                c = AddLightColor(c, lv, 1.5f, 0.5f, 0.5f);
            else
                c = AddLightColor(c, lv, 0.5f, 0.5f, 1.5f);
            break;
        }
        }
    }
    return c;
}

// 0x418ca0
int TileDistance(int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    return (int)(sqrt((double)(dx * dx + dy * dy)) + 0.5);
}

// 0x418cf0: squared distance.
float DistanceB(float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0;
    double dy = (double)y1 - y0;
    return (float)fabs((double)dx * dx + dy * dy);
}

// 0x418d20
float Distance(float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0, dy = y1 - y0;
    return (float)sqrt((double)dx * dx + (double)dy * dy);
}

// 0x418d60: linear light falloff.
int LightFalloff(int16_t level, float range, float dist)
{
    float r = dist / range;
    if (r < 0.0f || dist > range) return 0;
    return (int)((1.0f - r) * (float)level);
}

// 0x418db0: a wall was destroyed; open up the regions around it.
void RemoveWall(int x, int y)
{
    RevealRegion(x + 1, y);
    RevealRegion(x, y + 1);
    RevealRegion(x - 1, y);
    RevealRegion(x, y - 1);
    FixWallTiles();
}

// 0x418e00: flood fill a hidden region into the wall and base layers.
void RevealRegion(int x, int y)
{
    for (;;) {
        int idx = y * gMapWidth + x;
        uint8_t v = gRegionLayer[idx];
        if (v == 0xff) return;
        SetMapCell(x, y, 2, v);
        SetMapCell(x, y, 3, 0x1e);
        gRegionLayer[idx] = 0xff;
        RevealRegion(x + 1, y);
        RevealRegion(x - 1, y);
        RevealRegion(x, y + 1);
        y--;
    }
}

// 0x418e90: shop item grid.
void DrawShopGrid(int sel)
{
    char buf[12];
    for (int i = 0; i < 12; i++) {
        int x = gShopSlotX[i], y = gShopSlotY[i];
        if (i == sel) {
            gDDW.FillRect(x, y, 0x38, 2, gColorRed);
            gDDW.FillRect(x, y, 2, 0x38, gColorRed);
            gDDW.FillRect(x + 0x36, y, 2, 0x38, gColorRed);
            gDDW.FillRect(x, y + 0x36, 0x38, 2, gColorRed);
        }
        int item = gShopItems[gShopScroll + i];
        if (item > 0) gItemIcons[gItemIconIdx[item]].Draw(x, y, gDDW);
    }
    gText.SetColor(gColorWhite, 0);
    sprintf(buf, "%d", gPlayer.gold);
    gText.Print(0x79, 0x7b, buf);
}

// 0x418fa0
void ClearWallFlags()
{
    for (int i = 0; i < kMaxWalls; i++) gWalls[i].f20 = 0;
}

// 0x418fc0: 1 overlay, 2 wall, 3 base layer.
void SetMapCell(int x, int y, int layer, uint8_t v)
{
    int off = gMapRow[y] + x;
    switch (layer) {
    case 1: gOverlayLayer[off] = v; break;
    case 2: gWallLayer[off] = v; break;
    case 3: gLevelMap[off] = v; break;
    }
}

// 0x420a00: total weight of the inventory and equipped items.
float CarriedWeight()
{
    float w = 0.0f;
    for (int i = 0; i < 30; i++)
        if (gInventory[i].item != -1) w += gItemWeight[gInventory[i].item];
    for (int s = 0; s < 9; s++) {
        int v = EquipSlot(s);
        if (v) w += gItemWeight[gEquipItemId[s * 20 + v]];
    }
    return w;
}

// 0x4282d0: apply equipment modifiers to the base attributes.
void RecalcStats()
{
    int e = gEquip.e14;
    gPlayer.speed = 0.2f;
    gStats.dex = gStats.baseDex;
    gStats.str = gStats.baseStr;
    gStats.intel = gStats.baseInt;
    gStats.tou = gStats.baseTou;
    if (e == 1) gStats.dex += 2;
    if (gEquip.difficulty == 1) gStats.str += 2;
    if (e == 3) gStats.str -= 2;
    if (e == 2) gStats.intel += 2;
    if (e == 4) gStats.intel -= 2;
    if (gEquip.ring1 == 1 || gEquip.ring2 == 1) gStats.tou += 2;
    if (gEquip.e0c == 2) gPlayer.speed = 0.4f;
    gStats.maxWeight = (float)gStats.str * 2.5f;
    switch (gEquip.weaponType) {
    case 2: case 3: case 6: case 7: case 8: case 10: case 11: case 12:
        gWeaponFlag = 1;
        break;
    default:
        gWeaponFlag = 0;
    }
}
