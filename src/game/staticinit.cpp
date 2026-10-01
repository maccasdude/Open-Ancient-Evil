// C++ static initialisers of the original (the CRT table at 0x43a004).
// Constructors of engine objects (sprites, PCX, fonts, sprite tables...) run
// automatically in the port; the ones that set up game data are here or in
// their modules, and RunStaticInitializers() calls them in the original order.
#include "game.h"

// 0x41e110
void StaticInit_PlayerAnim() { gPlayerAnim.Construct(); }

// 0x41e130
void StaticInit_PlayerTexture() { gPlayerTexture.Construct(); }

// 0x41e150
void StaticInit_ProjectileModels()
{
    gBoltModel.Construct();
    gFireballModel.Construct();
    gLightningModel.Construct();
    gMissileModel.Construct();
}

// 0x41e190
void StaticInit_ProjectileTextures()
{
    gBoltTex.Construct();
    gFireballTex.Construct();
    gLightningTex.Construct();
    gMissileTex.Construct();
}

// 0x41e3a0 (CDAudio constructor)
void StaticInit_CD() { gCD.opened = 0; }

// 0x41e3c0
void StaticInit_SummonModels()
{
    gHordeModel.Construct();
    gSummonModel.Construct();
}

// 0x41e3f0
void StaticInit_SummonTextures()
{
    gHordeTex.Construct();
    gSummonTex.Construct();
}

// 0x41e420
void StaticInit_ServantAnims()
{
    for (int i = 0; i < 10; i++) gServants[i].anim.Construct();
}

static void CloseLog() { gLog.Destroy(); }

// 0x41e450 / 0x41e460
void StaticInit_Log()
{
    gLog.Construct();
    atexit(CloseLog);
}

// 0x42a7c0: the save / load slot menu (texts and positions are filled in by
// ReadSaveHeaders). Item 10 is an invisible close box that answers ESC.
void StaticInit_SaveMenu()
{
    for (int i = 0; i < 11; i++) {
        MenuItem &m = gSaveMenu[i];
        m.text = nullptr;
        m.color = gColorWhite;
        m.hotkey = (int8_t)(i < 9 ? '1' + i : i == 9 ? '0' : 0x1b);
        m.hiColor = gColorAzure;
        m.id = i < 10 ? i : -1;
        m.x = -1;
        m.y = 0x96;
        m.left = 0xb4;
        m.top = 0x96;
        m.right = 0x1cc;
        m.bottom = 0xa9;
    }
    MenuItem &c = gSaveMenu[10];
    c.left = 0x19a;
    c.top = 0x73;
    c.right = 0x1ae;
    c.bottom = 0x87;
}

// The CRT initialiser table (0x43a004-0x43a0ec) in order. Entries that only
// construct engine objects (PCX, Text, Font, Shade16, sprite tables) have no
// counterpart: the port's objects are constructed by C++ before main().
void RunStaticInitializers()
{
    StaticInit_HotkeyMenu();
    StaticInit_Monsters();
    StaticInit_MonsterAnims();
    StaticInit_MonsterTextures();
    StaticInit_RatModel();
    StaticInit_RatTex();
    StaticInit_ServantAnim();
    StaticInit_ServantTex();
    StaticInit_Cursors();
    StaticInit_CurFrame();
    StaticInit_Particles();
    StaticInit_CGModels();
    StaticInit_CGTex();
    StaticInit_CastMenu();
    StaticInit_SpellLevelMenu();
    StaticInit_ActionMenu();
    StaticInit_PlayerAnim();
    StaticInit_PlayerTexture();
    StaticInit_ProjectileModels();
    StaticInit_ProjectileTextures();
    StaticInit_CD();
    StaticInit_SummonModels();
    StaticInit_SummonTextures();
    StaticInit_ServantAnims();
    StaticInit_Log();
    StaticInit_SaveMenu();
}
