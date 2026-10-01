// Port: settings of the port (ancientevil.cfg), key bindings and control
// schemes. The game's own preferences stay in PREFS.CFG (gPrefs).
#pragma once
#include <stdint.h>

enum ControlScheme { kSchemeClassic = 0, kSchemeDiablo = 1, kSchemeWASD = 2, kNumSchemes = 3 };

// Rebindable actions. "Press" actions stand for one of the original keys;
// "hold" actions are read while held down.
enum Action {
    // movement (hold)
    ACT_MOVE_UP, ACT_MOVE_LEFT, ACT_MOVE_DOWN, ACT_MOVE_RIGHT,   // WASD scheme
    ACT_WALK_FORWARD, ACT_WALK_BACK, ACT_TURN_LEFT, ACT_TURN_RIGHT,
    ACT_RUN, ACT_FACE, ACT_ATTACK_HOLD, ACT_ATTACK_IN_PLACE, ACT_JUMP,
    // modes and commands (press)
    ACT_ATTACK_MODE, ACT_CAST_MODE, ACT_JUMP_MODE,
    ACT_INVENTORY, ACT_MAP, ACT_MEMORISE, ACT_SPELL_HOTKEYS, ACT_HEALTH,
    ACT_OPEN_DOORS, ACT_ELIXIR, ACT_DROP, ACT_USE, ACT_IDENTIFY, ACT_SEARCH,
    ACT_CENTRE, ACT_STATUS_BAR, ACT_MOUSE_SPEED, ACT_GAMMA_DOWN, ACT_GAMMA_UP,
    ACT_QUICKSAVE, ACT_QUICKLOAD,
    kNumActions
};

struct ActionInfo {
    const char *name;
    bool hold;            // read while held (not through the key queue)
    uint8_t ch, vk;       // the original key event it stands for (press actions)
    uint8_t def[kNumSchemes][2];   // default keys per scheme (VK codes, 0 none)
    uint8_t schemes;      // bit mask of schemes it applies to
};
extern const ActionInfo kActions[kNumActions];

struct PortSettings {
    // display
    int enhanced = 1;       // widescreen 1080p mode (restart)
    int hiresModels = 1;    // (restart)
    int textureFilter = 1;
    int colorLight = 1;
    int smoothLight = 1;    // ease the drawn light instead of flickering at its edges
    int lightModel = 1;     // coloured light: 0 per tile (3dfx), 1 smooth falloff and soft shadows
    int fullscreen = 0;
    int windowScale = 2;    // window size, multiple of the game picture
    int scaling = 0;        // 0 sharp, 1 sharp integer steps, 2 smooth
    // game
    int tick = 0;           // 0 Win95/98, 1 NT 4, 2 2000/XP, 3 1 ms clock
    int pauseOnFocus = 1;
    // controls
    int scheme = kSchemeClassic;
    int wasdRelative = 1;   // WASD moves relative to the cursor (0: screen directions)
    int followCamera = 1;   // Diablo / WASD: keep the view centred on the player
    uint8_t bind[kNumSchemes][kNumActions][2];
};
extern PortSettings gSettings;

void SettingsInit();          // defaults, then ancientevil.cfg, then environment overrides
void SettingsSave();
void SettingsApplyLive();     // push the live settings into the engine
void ResetBindings(int scheme);
double TickLengthMs(int tick);
const char *TickName(int tick);
const char *SchemeName(int scheme);
const char *KeyName(int vk);
const char *BindText(int action);     // bound keys for hints, or "Unbound"

// Input through the bindings
bool ActionHeld(int action);          // any bound key is down
bool ActionAny(int action);
void InputFrameBegin();               // once per game frame: note the keys pressed since the last           // down, or pressed since the last check
bool ActionApplies(int action);       // used by the current scheme
int  TranslateGameKey(uint8_t *buf);  // key queue entry -> original key; 0 drops it
int  KeyEventToVk(uint8_t ch, uint8_t vk);
bool KeyIsCommand(uint8_t ch, uint8_t vk);   // a queue entry that gives a command   // one key queue entry -> VK code (0: ignore)

// Control schemes (controls.cpp)
void ControlsUpdate();        // start of UpdatePlayer, normal play
bool ControlsOwnMouse();      // the scheme handles clicks itself
void ControlsReset();
void ControlsCamera();
uint16_t SmoothLight(int tile, uint16_t target);
constexpr int kVertStride = 66;
extern uint16_t gVertRGB[kVertStride * 71];   // smooth model: light at tile corners
bool SmoothLightModel();
extern int gShadowAlpha;
int LightLevelOf(int i);   // level of a placed light's brightness index   // the smooth light model is drawing this frame
uint16_t LightRGBAt(float x, float y);   // drawn light at a world position
bool ControlsJumpHeld();
extern int gCtlRightClick;

// Options screen (optionsmenu.cpp)
void OptionsScreen(bool inGame);
