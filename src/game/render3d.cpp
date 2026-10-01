// Software 3D renderer for OMT models: transformation, painter's sorting,
// flat and textured triangle rasterisers and projected shadows
// (0x40ee90-0x411870).
#include "game.h"

// Rasteriser state (globals of the original).
static ModelFrame gCurFrame;          // 0x4b9228
static float gCutY;                   // 0x4b9a68 (0 = no cut)
static Shade16 *gRenderShade;         // 0x4b9278
static int gOpaque = 1;               // 0x448b74
static uint16_t gModelRGB;            // port: light colour of the model (0: by level)
static RECT gModelRect;               // 0x4b9288 screen bounds of the last model
static float gVertexScale;            // 0x4b9a64 (0 = none)
static int *gVertDone;                // 0x4b92d0 vertex transformed
static int *gVertProjected;           // 0x4b9220 vertex projected
static Vec3 *gXf;                     // 0x4b9254 transformed vertices
static ScreenPt *gScr;                // 0x4b9260 projected vertices
static float gAngle;                  // 0x4b92bc
static int gOriginX, gOriginY;        // 0x4b9240 / 0x4b9244
static uint8_t gTexShift;             // 0x4b9258 log2(texture width)
static uint8_t gTexShift1;            // 0x4b9298
static uint16_t gFlatColor;           // 0x4b925c

// Painter's list of visible faces, sorted far to near.
struct FaceEntry {
    int face;
    float depth;
    int shade;
    float nz;             // port: facing, for coloured light
    FaceEntry *next;
};
static FaceEntry *gFaceEntries;       // 0x4b924c
static unsigned gNumFaceEntries;      // 0x4b9280
static FaceEntry gListHead;           // 0x4b92c0 (its 'next' is the list head 0x4b92cc)
static FaceEntry gListTail;           // 0x4b9268

// Current polygon.
static int gPolyType;                 // 0x4b927c
static unsigned gPolyCount;           // 0x4b92d4
static int gPolyIdx[3];               // 0x4b9230
static int gPolyU[3], gPolyV[3];      // 0x4b92a0 / 0x4b92b0

// Flat polygon span tables and extents (480 rows in the original; indexed
// from kSpanMin so the widescreen and hi-res targets fit).
enum { kSpanMin = -256, kSpanRows = 1536 };
static int gSpanLeftTab[kSpanRows];   // 0x4b8aa0
static int gSpanRightTab[kSpanRows];  // 0x4b92e0
static int *const gSpanLeft = gSpanLeftTab - kSpanMin;
static int *const gSpanRight = gSpanRightTab - kSpanMin;

// Render target of the rasterisers. The original drew into the locked
// 640x480 back buffer; the port also draws models a second time into the
// hi-res layer (enhanced mode), and records the pixels the normal pass owns.
struct RTarget {
    uint8_t *base;        // pixel (0,0)
    long pitch;           // bytes
    int L, T, R;          // clip edges (inclusive)
    int flatMaxY;         // flat polygons must end above this
    int viewH;            // textured polygons reaching this row are clipped
    int cap;              // last row the clipped rasteriser draws
    int S;                // scale against game coordinates
    uint32_t *owner;      // ownership record at (0,0), or null
    uint32_t tag;         // stamp << 16
};
static RTarget gRT;
static bool gInHiresPass;

static inline uint16_t *RTRow(int y) { return (uint16_t *)(gRT.base + (long)y * gRT.pitch); }
static inline uint32_t *RTOwner(uint16_t *d)
{
    return gRT.owner ? gRT.owner + (d - (uint16_t *)gRT.base) : nullptr;
}

// Hi-res pass: bilinear texture filtering (texel centres at +0.5).
static bool gBilinear = true;
static inline uint16_t Blend565(uint16_t a, uint16_t b, unsigned f)   // f 0..256
{
    unsigned ra = a >> 11, ga = (a >> 5) & 63, ba = a & 31;
    unsigned rb = b >> 11, gb = (b >> 5) & 63, bb = b & 31;
    unsigned r = (ra * (256 - f) + rb * f) >> 8;
    unsigned g = (ga * (256 - f) + gb * f) >> 8;
    unsigned bl = (ba * (256 - f) + bb * f) >> 8;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}
// The texels the current face covers: filtering stays inside them, so
// neighbouring parts of the model's texture sheet do not bleed into seams.
static int gFaceU0, gFaceU1, gFaceV0, gFaceV1;
static inline uint16_t SampleBilinear(const Texture *t, const uint16_t *shadeTab, float u, float v)
{
    u -= 0.5f;
    v -= 0.5f;
    int x0 = (int)floorf(u), y0 = (int)floorf(v);
    unsigned fx = (unsigned)((u - (float)x0) * 256.0f), fy = (unsigned)((v - (float)y0) * 256.0f);
    int x1 = x0 + 1, y1 = y0 + 1;
    const int w = t->w;
    x0 = x0 < gFaceU0 ? gFaceU0 : (x0 > gFaceU1 ? gFaceU1 : x0);
    x1 = x1 < gFaceU0 ? gFaceU0 : (x1 > gFaceU1 ? gFaceU1 : x1);
    y0 = y0 < gFaceV0 ? gFaceV0 : (y0 > gFaceV1 ? gFaceV1 : y0);
    y1 = y1 < gFaceV0 ? gFaceV0 : (y1 > gFaceV1 ? gFaceV1 : y1);
    const uint16_t *d = t->data;
    uint16_t c00 = shadeTab[d[y0 * w + x0] & 0x7fff], c10 = shadeTab[d[y0 * w + x1] & 0x7fff];
    uint16_t c01 = shadeTab[d[y1 * w + x0] & 0x7fff], c11 = shadeTab[d[y1 * w + x1] & 0x7fff];
    return Blend565(Blend565(c00, c10, fx), Blend565(c01, c11, fx), fy);
}

static void TargetLowRes()
{
    gRT.base = gDDW.surfacePtr;
    gRT.pitch = gDDW.pitch;
    gRT.L = ViewL();
    gRT.T = ViewT();
    gRT.R = ViewR();
    gRT.flatMaxY = 479;
    gRT.viewH = gViewHeight;
    gRT.cap = ViewCap();
    gRT.S = 1;
    gRT.owner = gHires.enabled() ? gHires.BackOwnOrigin() : nullptr;
    gRT.tag = gHires.stamp << 16;
}

static void TargetHiRes()
{
    const int S = gHires.S;
    gRT.base = (uint8_t *)gHires.BackImgOrigin();
    gRT.pitch = (long)gHires.W * 2;
    gRT.L = ViewL() * S;
    gRT.T = ViewT() * S;
    gRT.R = ViewR() * S + S - 1;
    gRT.flatMaxY = 479 * S + S - 1;
    gRT.viewH = gViewHeight * S;
    gRT.cap = ViewCap() * S;
    gRT.S = S;
    gRT.owner = nullptr;
    gRT.tag = 0;
}
static int gPolyMinY, gPolyMaxY;      // 0x4b9248 / 0x4b923c
static int gPolyMinX, gPolyMaxX;      // 0x4b92d8 / 0x4b9250

// 0x40f5e0 (static initialiser)
void StaticInit_CurFrame()
{
    gCurFrame.count = 0;
    gCurFrame.verts = nullptr;
}

// 0x40f5f0
void SetModelCutY(float y) { gCutY = y; }
// 0x40f600
void SetRenderShade(Shade16 *s) { gRenderShade = s; }
// 0x40f610
void SetModelOpaque(int on) { gOpaque = on; }
void SetModelLightRGB(uint16_t c) { gModelRGB = c; }
void SetModelFilter(bool on) { gBilinear = on; }

// 0x40f620
void GetModelRect(RECT *r)
{
    if (r) *r = gModelRect;
}

// 0x40f650
void SetModelVertexScale(float s) { gVertexScale = s; }

// 0x40f670
void Model::Draw(int sx, int sy, int frame, float angle)
{
    // (port: a model that failed to load, or a frame past its end, draws
    // nothing instead of reading through a null frame table)
    if (!frames || !numFrames || frame < 0 || (uint32_t)frame >= numFrames) {
        SetRect(&gModelRect, 10000, 10000, -10000, -10000);
        return;
    }
    SetOrigin(sx, sy);
    if (gCutY != 0.0f)
        RenderCut(frame, angle, gCutY);
    else
        Render(frame, angle);
}

static int AllocWork(Model *m, bool faceList)
{
    gVertDone = new int[m->numVerts];
    gVertProjected = new int[m->numVerts];
    gXf = new Vec3[m->numVerts];
    gScr = new ScreenPt[m->numVerts];
    gFaceEntries = faceList ? new FaceEntry[m->numFaces] : nullptr;
    memset(gVertDone, 0, m->numVerts * sizeof(int));
    memset(gVertProjected, 0, m->numVerts * sizeof(int));
    return 1;
}

static void FreeWork()
{
    delete[] gVertDone;
    delete[] gXf;
    delete[] gVertProjected;
    delete[] gScr;
    delete[] gFaceEntries;
    gVertDone = nullptr;
    gXf = nullptr;
    gVertProjected = nullptr;
    gScr = nullptr;
    gFaceEntries = nullptr;
}

static void ResetFaceList()
{
    gNumFaceEntries = 0;
    gListHead.next = &gListTail;
    gListHead.depth = 9999999.0f;
    gListTail.depth = -9999999.0f;
    gListTail.next = nullptr;
}

// Transform vertex idx of the current frame (rotate, optionally scale, tilt).
static void TransformVertex(int idx)
{
    Vec3 tmp;
    if (gVertexScale == 0.0f) {
        RotateY(&gCurFrame.verts[idx], &tmp, gAngle);
    } else {
        Vec3 s;
        s.x = gCurFrame.verts[idx].x * gVertexScale;
        s.y = gCurFrame.verts[idx].y * gVertexScale;
        s.z = gCurFrame.verts[idx].z * gVertexScale;
        RotateY(&s, &tmp, gAngle);
    }
    IsoTilt(&tmp, &gXf[idx]);
    gVertDone[idx] = 1;
}

// 0x40f6d0: draw one animation frame.
void Model::Render(int frame, float angle)
{
    SetRect(&gModelRect, 10000, 10000, -10000, -10000);
    gCurFrame = frames[frame];
    gAngle = angle;
    AllocWork(this, true);
    ResetFaceList();
    for (uint32_t i = 0; i < numFaces; i++) {
        ModelFace *f = &faces[i];
        for (int k = 0; k < 3; k++) {
            int idx = f->f04[k];
            if (!gVertDone[idx]) TransformVertex(idx);
        }
        Vec3 n;
        TriangleNormal(&n, &gXf[f->f04[0]], &gXf[f->f04[1]], &gXf[f->f04[2]]);
        if (n.z > 0.0f) AddFace(i, (int)((double)lightRange * n.z + 0.5), n.z);
    }
    if (tex) gTexShift = (uint8_t)tex->WidthShift();
    gTexShift1 = gTexShift + 1;
    uint8_t *p;
    unsigned long pitch;
    gDDW.Lock(&p, &pitch);
    DrawPasses();
    gDDW.Unlock();
    FreeWork();
}

// 0x40fab0: as Render, but faces reaching above minY (screen space) are
// skipped. Used to draw models partly hidden by the floor.
void Model::RenderCut(int frame, float angle, float minY)
{
    SetRect(&gModelRect, 10000, 10000, -10000, -10000);
    gCurFrame = frames[frame];
    gAngle = angle;
    AllocWork(this, true);
    ResetFaceList();
    for (uint32_t i = 0; i < numFaces; i++) {
        ModelFace *f = &faces[i];
        float lowest = 1000.0f;
        for (int k = 0; k < 3; k++) {
            int idx = f->f04[k];
            if (!gVertDone[idx]) TransformVertex(idx);
            if (gXf[idx].y < lowest) lowest = gXf[idx].y;
        }
        if (lowest < minY) continue;
        Vec3 n;
        TriangleNormal(&n, &gXf[f->f04[0]], &gXf[f->f04[1]], &gXf[f->f04[2]]);
        if (n.z > 0.0f) AddFace(i, (int)((double)lightRange * n.z + 0.5), n.z);
    }
    if (tex) gTexShift = (uint8_t)tex->WidthShift();
    uint8_t *p;
    unsigned long pitch;
    gDDW.Lock(&p, &pitch);
    DrawPasses();
    gDDW.Unlock();
    FreeWork();
}

// Port: draw the sorted faces. In the enhanced mode they are first drawn at
// hi-res over a pixel-multiplied copy of the background, then normally.
void Model::DrawPasses()
{
    if (gHires.enabled()) {
        const int S = gHires.S;
        gInHiresPass = true;
        TargetHiRes();
        // bounds of the hi-res drawing
        int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
        FaceEntry *e = gListHead.next;
        for (unsigned n = 0; n < gNumFaceEntries; n++, e = e->next) {
            ModelFace *f = &faces[e->face];
            for (int k = 0; k < 3; k++) {
                int idx = f->f04[k];
                if (!gVertProjected[idx]) {
                    ScreenPt tmp;
                    gScr[idx] = *Project(&tmp, &gXf[idx]);
                    gVertProjected[idx] = 1;
                }
                ScreenPt &q = gScr[idx];
                if (q.x < x0) x0 = q.x;
                if (q.x > x1) x1 = q.x;
                if (q.y < y0) y0 = q.y;
                if (q.y > y1) y1 = q.y;
            }
        }
        if (gNumFaceEntries) {
            // background: the framebuffer as it is now, except where another
            // model of this frame already put its hi-res pixels
            int lx0 = x0 / S - 2, lx1 = x1 / S + 2, ly0 = y0 / S - 2, ly1 = y1 / S + 2;
            if (lx0 < gLayout.minX()) lx0 = gLayout.minX();
            if (ly0 < gLayout.minY()) ly0 = gLayout.minY();
            if (lx1 > gLayout.maxX()) lx1 = gLayout.maxX();
            if (ly1 > gLayout.maxY()) ly1 = gLayout.maxY();
            uint32_t *own = gHires.BackOwnOrigin();
            uint16_t *hi = gHires.BackImgOrigin();
            const uint32_t tag = gHires.stamp << 16;
            for (int y = ly0; y <= ly1; y++) {
                uint16_t *src = (uint16_t *)(gDDW.surfacePtr + (long)y * gDDW.pitch);
                uint32_t *o = own + (long)y * gLayout.physW;
                for (int x = lx0; x <= lx1; x++) {
                    uint16_t v = src[x];
                    if ((o[x] & 0x7fffffff) == (tag | v)) continue;
                    o[x] = 0x80000000u | tag | v;   // hi-res block valid while the pixel stays v
                    uint16_t *d = hi + (long)y * S * gHires.W + x * S;
                    for (int j = 0; j < S; j++)
                        for (int i = 0; i < S; i++) d[(long)j * gHires.W + i] = v;
                }
            }
            Rasterize();
            gHires.drewThisFrame = true;
            gHires.boxes[gHires.back].push_back(RECT{lx0 + gLayout.ox, ly0 + gLayout.oy, lx1 + gLayout.ox, ly1 + gLayout.oy});
        }
        memset(gVertProjected, 0, numVerts * sizeof(int));
        gInHiresPass = false;
    }
    TargetLowRes();
    Rasterize();
}

// 0x40fef0: insert a face into the depth sorted list (far first).
void Model::AddFace(int face, int shade, float nz)
{
    FaceEntry *e = &gFaceEntries[gNumFaceEntries];
    e->face = face;
    e->shade = shade;
    e->nz = nz;
    ModelFace *f = &faces[face];
    float depth = (gXf[f->f04[0]].z + gXf[f->f04[1]].z + gXf[f->f04[2]].z) * 0.333333f;
    FaceEntry *prev = &gListHead;
    FaceEntry *node = gListHead.next;
    while (depth < node->depth) {
        prev = node;
        node = node->next;
    }
    e->next = node;
    prev->next = e;
    e->depth = depth;
    gNumFaceEntries++;
}

// 0x40ffc0
void Model::Rasterize()
{
    FaceEntry *e = gListHead.next;
    for (unsigned n = 0; n < gNumFaceEntries; n++) {
        ModelFace *f = &faces[e->face];
        if (gModelRGB) {
            // 3dfx: the light colour times the facing of the face
            float z = e->nz;
            int r = (int)(((gModelRGB >> 10) & 31) * z + 0.5f), g = (int)(((gModelRGB >> 5) & 31) * z + 0.5f),
                b = (int)((gModelRGB & 31) * z + 0.5f);
            gRenderShade->SetShadeColor((uint16_t)(r << 10 | g << 5 | b));
        } else {
            gRenderShade->SetShadeLevel(e->shade);
        }
        if (f->type == 2) {
            const uint8_t *rgb = (const uint8_t *)&f->u[0];
            unsigned c = gDDW.MakePixel16(rgb[0], rgb[1], rgb[2]) & 0xffff;
            gFlatColor = gRenderShade->current[c >> 1];
        }
        BeginPoly(f->type);
        SetPolyUV(f->tu, f->tv);
        AddPolyVertex(f->f04[0]);
        AddPolyVertex(f->f04[1]);
        AddPolyVertex(f->f04[2]);
        EndPoly();
        e = e->next;
    }
}

// 0x410090
void Model::SetOrigin(int x, int y)
{
    gOriginX = x;
    gOriginY = y;
}

// 0x4100b0
void Model::SetScale(float s)
{
    if (s > 0.0f) scale = s;
}

// 0x4100d0
void Model::BeginPoly(int type)
{
    gPolyCount = 0;
    gPolyType = type;
}

// 0x4100f0
void Model::SetPolyUV(int32_t *u, int32_t *v)
{
    for (int i = 0; i < 3; i++) {
        gPolyU[i] = u[i];
        gPolyV[i] = v[i];
    }
}

// 0x410130
void Model::AddPolyVertex(int idx)
{
    if (gPolyCount >= 3) return;
    if (!gVertProjected[idx]) {
        ScreenPt tmp;
        gScr[idx] = *Project(&tmp, &gXf[idx]);
        gVertProjected[idx] = 1;
    }
    gPolyIdx[gPolyCount++] = idx;
}

// 0x4101b0
ScreenPt *Model::Project(ScreenPt *out, Vec3 *p)
{
    if (gInHiresPass) {
        const int S = gRT.S;
        out->x = (int)(p->x * scale * S) + gOriginX * S;
        out->y = gOriginY * S - (int)(p->y * scale * S);
        return out;
    }
    int x = (int)(p->x * scale) + gOriginX;
    int y = gOriginY - (int)(p->y * scale);
    if (x < gModelRect.left) gModelRect.left = x;
    if (x > gModelRect.right) gModelRect.right = x;
    if (y < gModelRect.top) gModelRect.top = y;
    if (y > gModelRect.bottom) gModelRect.bottom = y;
    out->x = x;
    out->y = y;
    return out;
}

// 0x410230
void Model::EndPoly()
{
    switch (gPolyType) {
    case 2: FlatPoly(); break;
    case 3: TexturedPoly(); break;
    }
}

// 0x410250
void Model::FlatPoly()
{
    if (ScanFlatEdges()) {
        FillFlatSpans();
        ClearFlatSpans();
    }
}

// 0x410270: build the span tables; polygons not entirely on screen are
// rejected.
int Model::ScanFlatEdges()
{
    ScreenPt *p0 = &gScr[gPolyIdx[0]];
    ScreenPt *p1 = &gScr[gPolyIdx[1]];
    int maxY = p0->y, minY = p0->y;
    int minX = p1->y, maxX = p1->y;   // (seeded from the wrong field, as in the original)
    gPolyMaxY = maxY;
    gPolyMinY = minY;
    gPolyMaxX = maxX;
    gPolyMinX = minX;
    for (int i = 1; i < 3; i++) {
        ScreenPt *q = &gScr[gPolyIdx[i]];
        if (q->y < minY) gPolyMinY = minY = q->y;
        if (q->y > maxY) gPolyMaxY = maxY = q->y;
        if (q->x < minX) gPolyMinX = minX = q->x;
        if (q->x > maxX) gPolyMaxX = maxX = q->x;
    }
    if (minY < gRT.T || minX < gRT.L || maxY > gRT.flatMaxY || maxX >= gRT.R) return 0;
    ScreenPt *a = p0, *b = p1;
    for (int i = 0; i < 3; i++) {
        int ya = a->y, yb = b->y;
        if (ya > yb) {
            ScanEdge(gSpanLeft, a->x, ya, b->x, yb);
        } else if (ya < yb) {
            ScanEdge(gSpanRight, a->x, ya, b->x, yb);
        } else {
            int lo = a->x < b->x ? a->x : b->x;
            int hi = a->x > b->x ? a->x : b->x;
            if (gSpanLeft[ya] != -1) {
                if (gSpanLeft[ya] < lo) lo = gSpanLeft[ya];
            }
            gSpanLeft[ya] = lo;
            if (gSpanRight[ya] != -1) {
                if (gSpanRight[ya] > hi) hi = gSpanRight[ya];
            }
            gSpanRight[ya] = hi;
        }
        a = b;
        b = &gScr[gPolyIdx[(i + 2) % 3]];
    }
    return 1;
}

// 0x410430
void Model::ScanEdge(int *tab, int x0, int y0, int x1, int y1)
{
    int step = (y0 <= y1) ? 1 : -1;
    int n = abs(y1 - y0);
    if (n == 0) return;
    double x = (double)x0;
    double dx = (double)(x1 - x0) / n;
    int *p = &tab[y0];
    for (int i = n; i >= 0; i--) {
        *p = (int)x;
        p += step;
        x += dx;
    }
}

// 0x4104a0
void Model::FillFlatSpans()
{
    for (int y = gPolyMinY; y <= gPolyMaxY; y++) {
        uint16_t *d = RTRow(y) + gSpanLeft[y];
        uint32_t *o = RTOwner(d);
        for (int x = gSpanLeft[y]; x <= gSpanRight[y]; x++) {
            *d++ = gFlatColor;
            if (o) *o++ = gRT.tag | gFlatColor;
        }
    }
}

// 0x410500
void Model::ClearFlatSpans()
{
    for (int y = gPolyMinY; y <= gPolyMaxY; y++) {
        gSpanLeft[y] = -1;
        gSpanRight[y] = -1;
    }
}

// 0x410550: choose a rasteriser for a textured triangle.
void Model::TexturedPoly()
{
    TexVert v[3];
    memset(v, 0, sizeof(v));
    int offL = 0, offR = 0, offT = 0, offB = 0;
    for (int i = 0; i < 3; i++) {
        ScreenPt *s = &gScr[gPolyIdx[i]];
        v[i].x = (float)s->x;
        if (s->x < gRT.L)
            offL++;
        else if (s->x > gRT.R)
            offR++;
        v[i].y = (float)s->y;
        if (s->y < gRT.T)
            offT++;
        else if (!((double)s->y < (double)gRT.viewH))
            offB++;
        v[i].u = (float)gPolyU[i];
        v[i].v = (float)gPolyV[i];
    }
    if (offL == 3 || offR == 3 || offT == 3 || offB == 3) return;
    if (tex) {
        float u0 = v[0].u, u1 = v[0].u, w0 = v[0].v, w1 = v[0].v;
        for (int i = 1; i < 3; i++) {
            u0 = fminf(u0, v[i].u);
            u1 = fmaxf(u1, v[i].u);
            w0 = fminf(w0, v[i].v);
            w1 = fmaxf(w1, v[i].v);
        }
        gFaceU0 = (int)floorf(u0);
        gFaceU1 = (int)ceilf(u1) - 1;
        gFaceV0 = (int)floorf(w0);
        gFaceV1 = (int)ceilf(w1) - 1;
        if (gFaceU1 < gFaceU0) gFaceU1 = gFaceU0;
        if (gFaceV1 < gFaceV0) gFaceV1 = gFaceV0;
        if (gFaceU0 < 0) gFaceU0 = 0;
        if (gFaceV0 < 0) gFaceV0 = 0;
        if (gFaceU1 > tex->w - 1) gFaceU1 = tex->w - 1;
        if (gFaceV1 > tex->h - 1) gFaceV1 = tex->h - 1;
        if (gFaceU0 > gFaceU1) gFaceU0 = gFaceU1;
        if (gFaceV0 > gFaceV1) gFaceV0 = gFaceV1;
    }
    if (!offL && !offR && !offT && !offB) {
        if (gOpaque)
            DrawTexFast(v);
        else
            DrawTexTranslucent(v);
    } else {
        DrawTexClipped(v);
    }
}

// Rotate the vertices until v[0] is the top one (smallest y).
static void SortTop(TexVert *v)
{
    while (!(v[0].y <= v[2].y) || !(v[0].y <= v[1].y)) {
        TexVert t = v[0];
        v[0] = v[1];
        v[1] = v[2];
        v[2] = t;
    }
}

// 0x40ee90: textured triangle, fully on screen, opaque. Hand written x87
// code in the original running in single precision with 16.16 stepping.
void Model::DrawTexFast(TexVert *v)
{
    uint16_t *shadeTab = gRenderShade->GetShadeTablePtr();
    const uint16_t *tex = this->tex->data;
    SortTop(v);
    int left, right;
    if (v[0].y == v[1].y) {
        left = 0;
        right = 1;
    } else {
        left = 0;
        right = 0;
    }
    int leftNext = left - 1;
    if (leftNext < 0) leftNext += 3;
    int rightNext = (right + 1) % 3;

    TexVert *A, *B;
    int leftEndY, rightEndY;
    float xL, uL, vL, dxL, duL, dvL;
    float xR, uR, vR, dxR, duR, dvR;
    int fxL, fuL, fvL, fxR;

    A = &v[left];
    B = &v[leftNext];
    leftEndY = (int)lrintf(B->y);
    {
        float inv = 1.0f / (B->y - A->y + 1.0f);
        dxL = (B->x - A->x) * inv;
        duL = (B->u - A->u) * inv;
        dvL = (B->v - A->v) * inv;
    }
    TexVert *C = &v[right], *D = &v[rightNext];
    rightEndY = (int)lrintf(D->y);
    {
        float inv = 1.0f / (D->y - C->y + 1.0f);
        dxR = (D->x - C->x) * inv;
        duR = (D->u - C->u) * inv;
        dvR = (D->v - C->v) * inv;
    }
    int y = (int)v[0].y;
    int y1 = (int)v[1].y, y2 = (int)v[2].y;
    int yEnd = y1 > y2 ? y1 : y2;

    xL = A->x;
    fxL = (int)lrintf(xL * 65536.0f);
    uL = A->u;
    fuL = (int)lrintf(uL * 65536.0f);
    vL = A->v;
    fvL = (int)lrintf(vL * 65536.0f);
    xR = C->x;
    fxR = (int)lrintf(xR * 65536.0f);
    uR = C->u;
    vR = C->v;
    uint8_t shift = gTexShift;

    for (;;) {
        uint32_t cnt32 = ((uint32_t)(fxR - fxL) >> 16) + 1;
        if ((int16_t)cnt32 > 0) {
            float fc = (float)(int32_t)cnt32;
            int du = (int)lrintf((uR - uL) / fc * 65536.0f);
            int dv = (int)lrintf((vR - vL) / fc * 65536.0f);
            uint16_t *dst = RTRow(y) + (fxL >> 16);
            uint32_t *o = RTOwner(dst);
            uint32_t u = (uint32_t)fuL, vv = (uint32_t)fvL;
            if (gRT.S > 1 && gBilinear) {
                do {
                    *dst++ = SampleBilinear(this->tex, shadeTab, (float)(int32_t)u / 65536.0f, (float)(int32_t)vv / 65536.0f);
                    u += du;
                    vv += dv;
                } while (--cnt32);
                goto spanDone;
            }
            do {
                uint32_t idx = ((vv >> 16) << shift) + (u >> 16);
                u += du;
                uint16_t c = shadeTab[tex[idx] & 0x7fff];
                *dst++ = c;
                if (o) *o++ = gRT.tag | c;
                vv += dv;
            } while (--cnt32);
        }
    spanDone:
        if (y >= yEnd) break;
        // left edge
        if (leftEndY != y) {
            uL += duL;
            fuL = (int)lrintf(uL * 65536.0f);
            vL += dvL;
            fvL = (int)lrintf(vL * 65536.0f);
            xL += dxL;
            fxL = (int)lrintf(xL * 65536.0f);
        } else {
            left = leftNext;
            if (--leftNext < 0) leftNext += 3;
            A = &v[left];
            B = &v[leftNext];
            leftEndY = (int)lrintf(B->y);
            float inv = 1.0f / (B->y - A->y + 1.0f);
            dxL = (B->x - A->x) * inv;
            duL = (B->u - A->u) * inv;
            dvL = (B->v - A->v) * inv;
            xL = A->x;
            fxL = (int)lrintf(xL * 65536.0f);
            uL = A->u;
            fuL = (int)lrintf(uL * 65536.0f);
            vL = A->v;
            fvL = (int)lrintf(vL * 65536.0f);
        }
        // right edge
        if (rightEndY != y) {
            uR += duR;
            vR += dvR;
            xR += dxR;
            fxR = (int)lrintf(xR * 65536.0f);
        } else {
            right = rightNext;
            if (++rightNext >= 3) rightNext -= 3;
            C = &v[right];
            D = &v[rightNext];
            rightEndY = (int)lrintf(D->y);
            float inv = 1.0f / (D->y - C->y + 1.0f);
            dxR = (D->x - C->x) * inv;
            duR = (D->u - C->u) * inv;
            dvR = (D->v - C->v) * inv;
            xR = C->x;
            fxR = (int)lrintf(xR * 65536.0f);
            uR = C->u;
            vR = C->v;
        }
        y++;
    }
}

// 0x4106f0: textured triangle with screen clipping (opaque).
void Model::DrawTexClipped(TexVert *v)
{
    uint16_t *shadeTab = gRenderShade->current;
    const uint16_t *tex = this->tex->data;
    SortTop(v);
    int s = (v[0].y == v[1].y) ? 1 : 0;
    int rNext = (s + 1) % 3;
    TexVert *S = &v[s];
    TexVert *R = &v[rNext];

    float h = v[2].y - v[0].y + 1.0f;
    float dxL = (v[2].x - v[0].x) / h;
    float duL = (v[2].u - v[0].u) / h;
    float dvL = (v[2].v - v[0].v) / h;
    float hr = R->y - S->y + 1.0f;
    float dxR = (R->x - S->x) / hr;
    float duR = (R->u - S->u) / hr;
    float dvR = (R->v - S->v) / hr;

    int yStart = (int)v[0].y;
    int y1 = (int)v[1].y, y2 = (int)v[2].y;
    int yEnd = y1 > y2 ? y1 : y2;
    if (yEnd > gRT.cap) yEnd = gRT.cap;
    gPolyMinY = yStart;
    gPolyMaxY = yEnd;

    float xL = v[0].x, uL = v[0].u, vL = v[0].v;
    float xR = S->x;
    double uR = S->u, vR = S->v;   // kept on the FPU stack
    int lOff = 2;                  // left edge target vertex
    float rEndY = R->y;
    for (int y = yStart; y <= yEnd; y++) {
        int x0 = (int)xL;
        int x1 = (int)xR;
        int cnt = x1 - x0 + 1;
        float du = (float)((uR - uL) / cnt);
        float dv = (float)((vR - vL) / cnt);
        double u = uL, vv = vL;
        if (y >= gRT.T) {
            int sx;
            if (xL < (float)gRT.L) {
                int skip = gRT.L - x0;
                u = (double)du * skip + uL;
                vv = (double)dv * skip + vL;
                cnt -= skip;
                sx = gRT.L;
            } else {
                sx = x0;
            }
            if (xR > (float)gRT.R) cnt += gRT.R - x1;
            if (cnt > 0) {
                uint16_t *d = RTRow(y) + sx;
                uint32_t *o = RTOwner(d);
                if (gRT.S > 1 && gBilinear) {
                    for (int n = cnt; n; n--) {
                        *d++ = SampleBilinear(this->tex, shadeTab, (float)u, (float)vv);
                        u += du;
                        vv += dv;
                    }
                    cnt = 0;
                }
                for (int n = cnt; n > 0; n--) {
                    int idx = ((int)vv << gTexShift) + (int)u;
                    uint16_t c = shadeTab[tex[idx]];
                    *d++ = c;
                    if (o) *o++ = gRT.tag | c;
                    u += du;
                    vv += dv;
                }
            }
        }
        // left edge
        if (y == (int)v[lOff].y) {
            int old = lOff;
            lOff = lOff - 1;
            if (lOff < 0) lOff += 3;
            TexVert *O = &v[old], *N = &v[lOff];
            float hh = N->y - O->y;
            dxL = (N->x - O->x) / hh;
            duL = (N->u - O->u) / hh;
            dvL = (N->v - O->v) / hh;
            uL = O->u;
            vL = O->v;
            xL = O->x;
        } else {
            uL += duL;
            vL += dvL;
            xL += dxL;
        }
        // right edge
        if (y == (int)rEndY) {
            int old = rNext;
            if (++rNext == 3) rNext = 0;
            TexVert *O = &v[old], *N = &v[rNext];
            float hh = N->y - O->y;
            dxR = (N->x - O->x) / hh;
            duR = (N->u - O->u) / hh;
            dvR = (N->v - O->v) / hh;
            rEndY = N->y;
            xR = O->x;
            uR = O->u;
            vR = O->v;
        } else {
            xR += dxR;
            uR += duR;
            vR += dvR;
        }
    }
}

// 0x410b80: textured triangle blended 50% with the screen (not clipped).
void Model::DrawTexTranslucent(TexVert *v)
{
    unsigned mask = gDDW.MakePixel16(0x7f, 0x7f, 0x7f) & 0xffff;
    uint16_t *shadeTab = gRenderShade->current;
    const uint16_t *tex = this->tex->data;
    SortTop(v);
    int s = (v[0].y == v[1].y) ? 1 : 0;
    int rNext = (s + 1) % 3;
    TexVert *S = &v[s];
    TexVert *R = &v[rNext];
    float *rEndY = &R->y;

    float h = v[2].y - v[0].y + 1.0f;
    float hr = R->y - S->y + 1.0f;
    float dxL = (v[2].x - v[0].x) / h;
    float dxR = (R->x - S->x) / hr;
    float duL = (v[2].u - v[0].u) / h;
    float duR = (R->u - S->u) / hr;
    float dvL = (v[2].v - v[0].v) / h;
    float dvR = (R->v - S->v) / hr;

    gPolyMinY = (int)v[0].y;
    int a = (int)v[2].y, b = (int)v[1].y;
    gPolyMaxY = b > a ? b : a;

    float xL = v[0].x, xR = S->x;
    float uL = v[0].u, uR = S->u;
    float vL = v[0].v, vR = S->v;
    int lOff = 2;
    for (int y = gPolyMinY; y <= gPolyMaxY; y++) {
        int x0 = (int)xL;
        int cnt = (int)xR - x0;
        float du = (uR - uL) / (float)cnt;
        float dv = (vR - vL) / (float)cnt;
        if (cnt > 0) {
            float u = uL, vv = vL;
            int iv = (int)lrintf(vv);
            int iu = (int)lrintf(u);
            uint16_t *d = RTRow(y) + x0;
            uint32_t *o = RTOwner(d);
            for (; cnt; cnt--) {
                vv = vv + dv;
                int idx = (iv << gTexShift) + iu;
                iv = (int)lrintf(vv);
                uint16_t t = tex[idx];
                if (gColorLight) {
                    // 3dfx: alpha 128, rounded average per channel
                    unsigned a = *d, b = shadeTab[t];
                    *d = (uint16_t)((a & b) + (((a ^ b) & 0xf7de) >> 1));
                } else {
                    *d = (uint16_t)((*d >> 1) & mask);
                    *d = (uint16_t)(*d + ((shadeTab[t] >> 1) & mask));
                }
                if (o) *o++ = gRT.tag | *d;
                u = u + du;
                d++;
                iu = (int)lrintf(u);
            }
        }
        if (y == (int)v[lOff].y) {
            int old = lOff;
            lOff = lOff - 1;
            if (lOff < 0) lOff += 3;
            TexVert *O = &v[old], *N = &v[lOff];
            float hh = N->y - O->y;
            dxL = (N->x - O->x) / hh;
            uL = O->u;
            duL = (N->u - O->u) / hh;
            vL = O->v;
            dvL = (N->v - O->v) / hh;
            xL = O->x;
        } else {
            uL += duL;
            vL += dvL;
            xL += dxL;
        }
        if (y == (int)*rEndY) {
            int old = rNext;
            if (++rNext == 3) rNext = 0;
            TexVert *O = &v[old], *N = &v[rNext];
            rEndY = &N->y;
            float hh = N->y - O->y;
            dxR = (N->x - O->x) / hh;
            uR = O->u;
            duR = (N->u - O->u) / hh;
            dvR = (N->v - O->v) / hh;
            xR = O->x;
            vR = O->v;
        } else {
            uR += duR;
            vR += dvR;
            xR += dxR;
        }
    }
}

// ---------------------------------------------------------------------------
// Shadows: the model is projected from a light onto the floor; the covered
// area is collected as merged spans per row and darkened once.
// ---------------------------------------------------------------------------

struct ShadowSpan {
    int x0, x1;
    ShadowSpan *next;
};
struct ShadowRow {
    int f0, f4;
    ShadowSpan *head;
};
static ShadowRow gShadowRowTab[480 - kSpanMin]; // 0x4b9a78 (400 rows in the original)
static ShadowRow *const gShadowRows = gShadowRowTab - kSpanMin;
static ShadowSpan *gShadowAllocs[0x400]; // 0x4bad50
static int gNumShadowAllocs;           // 0x4bad38
static int gNumShadowFrees;            // 0x4bad4c
static float gShadowLightH;            // 0x4b9a70
static float gShadowLightX, gShadowLightY, gShadowLightZ; // 0x4bbcf0..
static float gShadowWorldX, gShadowWorldY, gShadowWorldZ; // 0x4bad40..

static ShadowSpan *NewSpan(int x0, int x1, ShadowSpan *next)
{
    ShadowSpan *s = new ShadowSpan;
    if (gNumShadowAllocs < 0x400) gShadowAllocs[gNumShadowAllocs++] = s;
    s->x0 = x0;
    s->x1 = x1;
    s->next = next;
    return s;
}

int gShadowAlpha;   // port: shadow strength /256 (0: the original halving)

// 0x410f30
void Model::DrawShadow(int sx, int sy, float wx, float wy, float wz, int frame,
                       float angle, float lightX, float lightY, float lightH)
{
    if (!frames || !numFrames || frame < 0 || (uint32_t)frame >= numFrames) return;   // (port)
    gNumShadowFrees = 0;
    gNumShadowAllocs = 0;
    memset(gShadowRowTab, 0, sizeof(gShadowRowTab));
    SetOrigin(sx, sy);
    gShadowLightH = lightH * 1.33333f;
    gShadowLightX = lightX;
    gShadowLightZ = lightH;
    gShadowWorldX = wx;
    gShadowWorldZ = wz;
    gShadowLightY = lightY;
    gAngle = -angle;
    gShadowWorldY = wy;
    ShadowFrame(frame);
}

// 0x410fd0
void Model::ShadowFrame(int frame)
{
    SetRect(&gModelRect, 10000, 10000, -10000, -10000);
    gCurFrame = frames[frame];
    AllocWork(this, false);
    for (uint32_t i = 0; i < numFaces; i++) {
        ModelFace *f = &faces[i];
        int ok = 1;
        for (int k = 0; k < 3; k++) {
            int idx = f->f04[k];
            if (gVertProjected[idx]) continue;
            Vec3 p = gCurFrame.verts[idx];
            p.x = -p.x;
            RotateY(&p, &p, gAngle);
            if (!ShadowProject(&p, &gScr[idx])) {
                ok = 0;   // "skipping"
                break;
            }
            gVertProjected[idx] = 1;
        }
        if (ok) ShadowSpans(f->f04);
    }
    ShadowApply();
    FreeWork();
}

// 0x411210
int Model::ShadowProject(Vec3 *v, ScreenPt *out)
{
    float d = gShadowLightH - v->y;
    double f;
    if (gShadowAlpha) {
        // smooth light model: a light close to the model stretches the
        // shadow a long way (or past infinity, dropping faces); cap it
        const float m = gShadowLightH / 2.5f;
        if (d < m) d = m;
        if (d < 1.0f) d = 1.0f;
        f = (double)gShadowLightH / d;
    } else {
        if (d < 1.0f) return 0;
        f = (double)gShadowLightH / d;
    }
    double zw = (double)v->z * 0.0140845f + gShadowWorldY;
    double xw = (double)v->x * 0.0140845f + gShadowWorldX;
    float Z = (float)((zw - gShadowLightY) * f + gShadowLightY);
    float X = (float)((xw - gShadowLightX) * f + gShadowLightX);
    int sx, sy;
    WorldToScreen(X, Z, &sx, &sy);
    out->x = sx + 0x20;
    out->y = sy;
    return 1;
}

// 0x4112d0: add the triangle's rows to the span lists.
void Model::ShadowSpans(int32_t *vi)
{
    ScreenPt p[3] = {gScr[vi[0]], gScr[vi[1]], gScr[vi[2]]};
    while (p[0].y > p[2].y || p[0].y > p[1].y) {
        ScreenPt t = p[0];
        p[0] = p[1];
        p[1] = p[2];
        p[2] = t;
    }
    if (p[1].x < p[2].x) {
        ScreenPt t = p[1];
        p[1] = p[2];
        p[2] = t;
    }
    int bIdx;
    if (p[0].y == p[1].y) {
        if (p[0].x > p[1].x) p[0] = p[1];
        bIdx = 1;
    } else {
        if (p[1].x < p[2].x || (p[1].x == p[2].x && p[1].y > p[2].y)) p[2] = p[1];
        bIdx = 0;
    }
    int bNext = (bIdx + 1) % 3;
    int yMax = p[1].y > p[2].y ? p[1].y : p[2].y;
    int y = p[0].y;
    if (y > ViewCap() || yMax < ViewT()) return;
    if (yMax > ViewCap() - 1) yMax = ViewCap() - 1;
    float fA = (float)p[0].x;
    float fB = (float)p[bIdx].x;
    if (y < gModelRect.top) gModelRect.top = y < ViewT() ? ViewT() : y;
    if (yMax > gModelRect.bottom) gModelRect.bottom = yMax;
    float slopeA = (float)(p[2].x - p[0].x) / ((float)(p[2].y - p[0].y) + 1.0f);
    int endB = p[bNext].y;
    float slopeB = (float)(p[bNext].x - p[bIdx].x) / ((float)(p[bNext].y - p[bIdx].y) + 1.0f);
    if (y > yMax) return;
    int aIdx = 2;
    for (; y <= yMax; y++) {
        int x0 = (int)fA, x1 = (int)fB;
        if (x0 > x1) {
            int t = x0;
            x0 = x1;
            x1 = t;
        }
        if (y >= ViewT() && x0 <= ViewR() && x1 >= ViewL()) {
            if (x0 < ViewL()) x0 = ViewL();
            if (x1 > ViewR()) x1 = ViewR();
            ShadowRow *row = &gShadowRows[y];
            if (!row->head) {
                row->head = NewSpan(x0, x1, nullptr);
            } else {
                ShadowSpan **prevNext = &row->head;
                ShadowSpan *node = row->head;
                bool done = false;
                while (x0 > node->x1 + 1) {
                    if (!node->next) {
                        node->next = NewSpan(x0, x1, nullptr);
                        done = true;
                        break;
                    }
                    prevNext = &node->next;
                    node = node->next;
                }
                if (!done) {
                    bool inserted = false;
                    if (x0 < node->x0) {
                        if (x1 < node->x0 - 1) {
                            *prevNext = NewSpan(x0, x1, node);
                            inserted = true;
                        } else {
                            node->x0 = x0;
                        }
                    }
                    if (!inserted && x1 > node->x1) {
                        ShadowSpan *n = node->next;
                        int end = x1;
                        while (n && x1 >= n->x0 - 1) {
                            end = n->x1;
                            n = n->next;
                        }
                        if (x1 > end) end = x1;
                        node->x1 = end;
                        node->next = n;
                    }
                }
            }
        }
        // edge A (left side, walking backwards through the vertices)
        if (y == p[aIdx].y) {
            int old = aIdx;
            aIdx = aIdx - 1;
            if (aIdx < 0) aIdx += 3;
            fA = (float)p[old].x;
            slopeA = (float)(p[aIdx].x - p[old].x) / ((float)(p[aIdx].y - p[old].y) + 1.0f);
        } else {
            fA += slopeA;
        }
        // edge B
        if (y == endB) {
            int old = bNext;
            if (++bNext == 3) bNext = 0;
            endB = p[bNext].y;
            fB = (float)p[old].x;
            slopeB = (float)(p[bNext].x - p[old].x) / ((float)(p[bNext].y - p[old].y) + 1.0f);
        } else {
            fB += slopeB;
        }
    }
}

// 0x411710: darken the collected spans.
void Model::ShadowApply()
{
    unsigned mask = gDDW.MakePixel16(0x7f, 0x7f, 0x7f) & 0xffff;
    // smooth light model: darken by the shadow's strength instead of half
    const uint32_t rbMask = gDDW.MakePixel16(0xff, 0, 0xff) & 0xffff, gMask = gDDW.MakePixel16(0, 0xff, 0) & 0xffff;
    const uint32_t keep = (uint32_t)((256 - gShadowAlpha) >> 3);   // 0..32
    uint8_t *p;
    unsigned long pitch;
    gDDW.Lock(&p, &pitch);
    int y = gModelRect.top > ViewT() ? gModelRect.top : ViewT();
    for (; y < gModelRect.bottom; y++) {
        int last = ViewL();
        for (ShadowSpan *s = gShadowRows[y].head; s; s = s->next) {
            int start = last > s->x0 ? last : s->x0;
            uint16_t *d = (uint16_t *)(gDDW.surfacePtr + (long)gDDW.pitch * y) + start;
            int n = s->x1 - start + 1;
            if (gShadowAlpha) {
                for (int i = 0; i < n; i++) {
                    uint32_t px = d[i];
                    d[i] = (uint16_t)((((px & rbMask) * keep >> 5) & rbMask) | (((px & gMask) * keep >> 5) & gMask));
                }
            } else {
                for (int i = 0; i < n; i++) d[i] = (uint16_t)((d[i] >> 1) & mask);
            }
            last = s->x1 + 1;
        }
        gShadowRows[y].head = nullptr;
    }
    // (port: the last row is not darkened, as in the original, but its spans
    // are freed below, so forget them too)
    if (y >= ViewT() && y < ViewCap() + 1 && y - kSpanMin < (int)(sizeof gShadowRowTab / sizeof gShadowRowTab[0]))
        gShadowRows[y].head = nullptr;
    for (int i = 0; i < gNumShadowAllocs; i++) {
        gNumShadowFrees++;
        delete gShadowAllocs[i];
        gShadowAllocs[i] = nullptr;
    }
    gNumShadowAllocs = 0;
    gDDW.Unlock();
}
