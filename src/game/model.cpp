// Animated model sets (.AMT), models (.OMT) and PCX textures (.TEX).
// AnimSet 0x40aa20-0x40ae60, Model 0x40e980-0x40ee90, Texture 0x41d180-0x41d5c0.
#include "game.h"

// ---------------------------------------------------------------------------
// AnimSet
// ---------------------------------------------------------------------------

// 0x40aa20
void AnimSet::Construct()
{
    count = 0;
    models = nullptr;
    cur = nullptr;
    f2c = 0;
    f00 = 0;
    mode = 2;
    outModel = nullptr;
    nextIndex = 0;
}

// 0x40aa40
void AnimSet::Free()
{
    if (models) {
        for (uint32_t i = 0; i < count; i++) models[i].Free();
    }
    models = nullptr;   // (the array itself is never deleted by the original)
    count = 0;
}

// 0x40aa80: play sequence 'index' looping.
Model *AnimSet::Loop(uint32_t index)
{
    if (index >= count) index = defaultIndex;
    curIndex = index;
    mode = 2;
    frame = 0;
    cur = &models[index];
    numFrames = cur->numFrames;
    return cur;
}

// 0x40aac0: loop starting at a given frame.
Model *AnimSet::LoopFrom(uint32_t index, uint32_t startFrame)
{
    if (index >= count) index = defaultIndex;
    curIndex = index;
    mode = 2;
    cur = &models[index];
    numFrames = cur->numFrames;
    if (startFrame > (uint32_t)numFrames) startFrame = numFrames - 1;
    frame = startFrame;
    return cur;
}

// 0x40ab00: play once, then switch *out to sequence 'next' (looping).
Model *AnimSet::Once(uint32_t index, Model **out, int next)
{
    if (index >= count) index = defaultIndex;
    curIndex = index;
    mode = 1;
    frame = 0;
    cur = &models[index];
    numFrames = cur->numFrames;
    outModel = out;
    nextIndex = next;
    return cur;
}

// 0x40ab50: play once and hold the last frame.
Model *AnimSet::Hold(uint32_t index)
{
    if (index >= count) index = defaultIndex;
    curIndex = index;
    mode = 4;
    frame = 0;
    cur = &models[index];
    numFrames = cur->numFrames;
    return cur;
}

// 0x40ab90: loop backwards.
Model *AnimSet::ReverseLoop(uint32_t index)
{
    if (index >= count) index = defaultIndex;
    curIndex = index;
    mode = 0x12;
    cur = &models[index];
    frame = 1;
    numFrames = cur->numFrames;
    return cur;
}

// 0x40abd0: play backwards once, then switch *out to 'next'.
Model *AnimSet::Reverse(uint32_t index, Model **out, int next)
{
    if (index >= count) index = defaultIndex;
    curIndex = index;
    mode = 0x11;
    cur = &models[index];
    numFrames = cur->numFrames;
    frame = numFrames - 1;
    outModel = out;
    nextIndex = next;
    return cur;
}

// 0x40ac10: advance one frame; returns the new frame number.
int AnimSet::Advance()
{
    uint8_t m = mode;
    if (m & 0x10) {
        if (--frame >= 0) return frame;
        switch (m & 0xef) {
        case 1:
            *outModel = Loop(nextIndex);
            return frame;
        case 2:
            frame = numFrames - 1;
            return frame;
        }
        return frame;
    }
    if (++frame != numFrames) return frame;
    switch (m) {
    case 1:
        *outModel = Loop(nextIndex);
        break;
    case 2:
        frame = 0;
        return frame;
    case 4:
        frame--;
        return frame;
    }
    return frame;
}

// 0x40aca0
int AnimSet::AtEnd()
{
    if (mode & 0x10) return frame == 0;
    return frame == numFrames - 1;
}

// 0x40acd0: load an .AMT file: a 0x30 byte header followed by OMT models.
int AnimSet::Load(const char *file)
{
    int fd = w_open(file, W_O_RDONLY | W_O_BINARY);
    if (fd < 0) return 0;
    uint32_t hdr[12];
    if (w_read(fd, hdr, 0x30) != 0x30) {
        w_close(fd);
        return 0;
    }
    // The header is copied over the start of the object in the original.
    f00 = hdr[0];
    count = hdr[1];
    f0c = hdr[3];
    f10 = hdr[4];
    curIndex = hdr[5];
    cur = nullptr;                 // (a stale pointer value in the file)
    defaultIndex = hdr[7];
    frame = hdr[8];
    numFrames = hdr[9];
    f28 = hdr[10];
    f2c = hdr[11];
    Model *m = new Model[count];
    for (uint32_t i = 0; i < count; i++) m[i].Construct();
    models = m;
    for (uint32_t i = 0; i < count; i++) models[i].Load(fd);
    w_close(fd);                   // (the original leaks the handle)
    return 1;
}

// 0x40ade0
void AnimSet::SetTexture(Texture *t)
{
    if (!models) return;
    for (uint32_t i = 0; i < count; i++) models[i].SetTexture(t);
}

// 0x40ae20
void AnimSet::CopyTo(AnimSet *dst)
{
    *dst = *this;
}

// 0x40ae40
void AnimSet::Share(AnimSet *src, Model **out)
{
    models = src->models;
    outModel = out;
}

// ---------------------------------------------------------------------------
// Model
// ---------------------------------------------------------------------------

// 0x40e980
void Model::Construct()
{
    memset(magic, 0, sizeof(magic));
    version = hFrames = hFaces = hVerts = h14 = h18 = 0;
    numVerts = 0;
    numFrames = 0;
    numFaces = 0;
    f34 = 0;
    f38 = 0;
    tex = nullptr;
    faces = nullptr;
    frames = nullptr;
    gModelError = 0;
    scale = 1.0f;
    lightRange = 0;        // (left uninitialised by the original)
}

// 0x40e9c0
int Model::AllocFrames(uint32_t n)
{
    gModelError = 0;
    if (frames) return 0;
    numFrames = n;
    ModelFrame *f = new ModelFrame[n];
    for (uint32_t i = 0; i < n; i++) {   // 0x40ee80
        f[i].count = 0;
        f[i].verts = nullptr;
    }
    frames = f;
    if (n == 0) {
        gModelError = 1;
        return 0;
    }
    return 1;
}

// Shared body of the two OMT loaders.
static int ReadOMT(Model *m, int fd)
{
    w_read(fd, m, 0x1c);            // header: magic, version, frames, faces, verts
    if (strcmp(m->magic, "OMT") != 0) {
        w_close(fd);
        gModelError = 3;
        return 0;
    }
    if (m->version < 0x100) {
        w_close(fd);
        gModelError = 4;
        return 0;
    }
    m->numFaces = m->hFaces;
    m->numFrames = m->hFrames;
    m->numVerts = m->hVerts;
    if (!m->AllocFrames(m->hFrames)) {
        w_close(fd);
        gModelError = 1;
        return 0;
    }
    m->faces = new ModelFace[m->numFaces];
    w_read(fd, m->faces, m->numFaces * 0x4c);
    for (uint32_t i = 0; i < m->numFrames; i++) {
        m->frames[i].count = m->numVerts;
        m->frames[i].verts = new Vec3[m->numVerts];
        w_read(fd, m->frames[i].verts, m->numVerts * 12);
    }
    return 1;
}

// 0x40ea70
int Model::Load(const char *file)
{
    gModelError = 0;
    int fd = w_open(file, W_O_RDONLY | W_O_BINARY);
    if (fd < 0) {
        gModelError = 2;
        return 0;
    }
    if (!ReadOMT(this, fd)) return 0;
    w_close(fd);
    return 1;
}

// 0x40ec00: load from an already open file (inside an .AMT).
int Model::Load(int fd)
{
    gModelError = 0;
    return ReadOMT(this, fd);
}

// 0x40ed60: attach a texture and convert the face UVs into texel units.
void Model::SetTexture(Texture *t)
{
    tex = t;
    if (!t) return;
    for (uint32_t i = 0; i < numFaces; i++) {
        ModelFace *f = &faces[i];
        if (f->type != 3 && f->type != 4) continue;
        for (int k = 0; k < 3; k++) {
            f->tu[k] = (int)(f->u[k] * tex->fw + 0.5f);
            f->tv[k] = (int)(tex->fh * f->v[k] + 0.5f);
        }
    }
}

// 0x40ee00
void Model::Free()
{
    memset(magic, 0, sizeof(magic));
    version = hFrames = hFaces = hVerts = h14 = h18 = 0;
    for (uint32_t i = 0; i < numFrames; i++) {
        if (frames[i].verts) {
            delete[] frames[i].verts;
            frames[i].verts = nullptr;
        }
        frames[i].count = 0;
    }
    if (frames) delete[] frames;
    frames = nullptr;
    if (faces) delete[] faces;
    faces = nullptr;
    numFrames = 0;
    numVerts = 0;
    numFaces = 0;
    f34 = 0;
    tex = nullptr;
}

// ---------------------------------------------------------------------------
// Texture
// ---------------------------------------------------------------------------

// 0x41d180
void Texture::Construct()
{
    h = 0;
    w = 0;
    data = nullptr;
}

// 0x41d190
int Texture::Load(const char *file)
{
    uint8_t *pixels, *palette;
    uint8_t *buf = LoadFile(file, &pixels, &palette);
    if (!buf) return 0;
    Free();
    SetPalette(palette);
    SetSize(buf);
    data = new uint16_t[(size_t)w * h];
    if (WidthShift() == -1 || !Decode(pixels)) {
        delete[] buf;
        return 0;
    }
    delete[] buf;
    return 1;
}

// 0x41d250: read a 256-colour PCX file into memory.
uint8_t *Texture::LoadFile(const char *file, uint8_t **pixels, uint8_t **palette)
{
    int fd = w_open(file, W_O_RDONLY | W_O_BINARY);
    if (fd < 0) return nullptr;
    int size = w_filelength(fd);
    uint8_t *buf = new uint8_t[size];
    int got = w_read(fd, buf, size);
    w_close(fd);
    if (buf[3] == 8 && buf[0x41] == 1 && got == size) {
        *pixels = buf + 0x80;
        uint8_t *pal = buf + size - 0x301;
        *palette = pal;
        if (*pal == 0x0c) {
            *palette = pal + 1;
            return buf;
        }
    }
    delete[] buf;
    return nullptr;
}

// 0x41d310: convert the PCX palette to RGB555.
void Texture::SetPalette(const uint8_t *pal)
{
    for (int i = 0; i < 256; i++, pal += 3) {
        unsigned r = pal[0] & 0xf8, g = pal[1] & 0xf8, b = pal[2];
        gPalette555[i] = (uint16_t)((((r << 5) | g) << 2) | ((b >> 3) & 0x1f));
    }
}

// 0x41d360
void Texture::SetSize(const uint8_t *hdr)
{
    w = *(const int16_t *)(hdr + 8) + 1;
    h = *(const int16_t *)(hdr + 10) + 1;
    fw = (float)(uint32_t)w;
    fh = (float)(uint32_t)h;
}

// 0x41d3a0: PCX run length decoding through the palette.
int Texture::Decode(const uint8_t *src)
{
    uint32_t total = (uint32_t)(w * h);
    uint32_t n = 0;
    uint16_t *dst = data;
    while (n < total) {
        uint8_t c = *src++;
        if ((c & 0xc0) == 0xc0) {
            unsigned run = c & 0x3f;
            uint16_t px = gPalette555[*src++];
            if (run) {
                for (unsigned i = 0; i < run; i++) dst[i] = px;
                dst += run;
                n += run;
            }
        } else {
            *dst++ = gPalette555[c];
            n++;
        }
    }
    return 1;
}

// 0x41d470: log2 of the width, -1 if not a power of two.
int Texture::WidthShift()
{
    if (!data) return 0;
    int s = 0;
    int x = w;
    while (!(x & 1)) {
        x >>= 1;
        s++;
    }
    if ((1 << s) != w) return -1;
    return s;
}

// 0x41d4b0
void Texture::Free()
{
    if (data) {
        delete[] data;
        data = nullptr;
    }
}

// 0x41d4d0: copy the non-zero pixels of src to (x,y).
int Texture::Blit(Texture *src, int x, int y)
{
    if ((uint32_t)w < (uint32_t)(src->w + x)) return 0;
    const uint16_t *s = src->data;
    for (int r = 0; r < src->h; r++, y++) {
        uint16_t *d = data + (w * y + x);
        for (int i = 0; i < src->w; i++)
            if (s[i]) d[i] = s[i];
        s += src->w;
    }
    return 1;
}

// 0x41d560
void Texture::GetPtr(uint8_t **ptr, int *pitch)
{
    *ptr = (uint8_t *)data;
    *pitch = w * 2;
}

// 0x41d580
void Texture::ToGrey()
{
    uint16_t *p = data;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++, p++) *p = Convert555ToGrey(*p);
}
