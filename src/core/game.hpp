#pragma once

#include "math/point3d.hpp"
#include "audio/sound.hpp"
#include "math/vector.hpp"

#include <memory>
#include <vector>

// Forward declare pointers
class attractor;
class blackholes;
class bomb;
class camera;
class classical_mode;
class controls;
class enemies;
class entity;
class gameplay_mode;
class grid;
class highscore;
class particle;
class player;
class players;
class spawner;
class stars;

enum
{
    SOUNDID_MUSICLOOP = 0,

    SOUNDID_MENU_MUSICLOOP,

    SOUNDID_MENU_SELECT,

    SOUNDID_BACKGROUND_NOISELOOP,

    SOUNDID_PLAYERSPAWN,
    SOUNDID_PLAYERHIT,
    SOUNDID_PLAYERDEAD,
    SOUNDID_SHIELDSLOST,
    SOUNDID_PLAYERTHRUST,

    SOUNDID_EXTRALIFE,
    SOUNDID_EXTRABOMB,

    SOUNDID_BOMB,

    SOUNDID_MULTIPLIERADVANCE,

    SOUNDID_MISSILEHITWALL,

    SOUNDID_REPULSORA,
    SOUNDID_REPULSORB,
    SOUNDID_REPULSORC,
    SOUNDID_REPULSORD,

    SOUNDID_GRAVITYWELLDESTROYED,
    SOUNDID_GRAVITYWELLABSORBED,
    SOUNDID_GRAVITYWELLHIT,
    SOUNDID_GRAVITYWELLALERT,
    SOUNDID_GRAVITYWELLEXPLODE,

    SOUNDID_GRAVITYWELL_HUMLOOPA,
    SOUNDID_GRAVITYWELL_HUMLOOPB,
    SOUNDID_GRAVITYWELL_HUMLOOPC,
    SOUNDID_GRAVITYWELL_HUMLOOPD,
    SOUNDID_GRAVITYWELL_HUMLOOPE,
    SOUNDID_GRAVITYWELL_HUMLOOPF,

    SOUNDID_ENEMYSPAWN1,
    SOUNDID_ENEMYSPAWN2,
    SOUNDID_ENEMYSPAWN3,
    SOUNDID_ENEMYSPAWN4,
    SOUNDID_ENEMYSPAWN5,
    SOUNDID_ENEMYSPAWN6,

    SOUNDID_ENEMYHIT,

    SOUNDID_MAYFLIES,

    SOUNDID_PLAYERFIRE1,
    SOUNDID_PLAYERFIRE2,
    SOUNDID_PLAYERFIRE3,

    SOUNDID_LAST
};

class game
{
  public:
    typedef enum
    {
        GAMEMODE_ATTRACT = 0,
        GAMEMODE_CREDITED,
        GAMEMODE_CHOOSE_GAMETYPE,
        GAMEMODE_PLAYING,
        GAMEMODE_HIGHSCORES_CHECK,
        GAMEMODE_HIGHSCORES,
        GAMEMODE_GAMEOVER_TRANSITION,
        GAMEMODE_GAMEOVER
    } GameMode;

    // Which front-end overlay menu (if any) is currently shown. When one is up
    // the world is frozen while it is driven; the menu screen decides what to
    // do (resume, change settings, quit).
    typedef enum
    {
        MENU_NONE = 0,
        MENU_TITLE,       // main menu on the title screen
        MENU_PAUSE,       // pause menu over a running match
        MENU_SETTINGS,    // combined settings screen (from title or pause)
        MENU_HIGHSCORES   // top-scores screen (from the title main menu)
    } MenuScreen;

    typedef enum
    {
        GAMETYPE_SINGLEPLAYER = 0,
        GAMETYPE_MULTIPLAYER_COOP,
        GAMETYPE_MULTIPLAYER_VS
    } GameType;

    game();
    ~game();
    void quitThreads();

    void run();
    void draw(int pass);

    void startGame(GameType gameType);
    void endGame();

    // Abandon the running match (from the pause menu) and return to attract /
    // title mode. Stops the match music, releases any paused tracks and drops
    // the active gameplay mode.
    void abandonMatch();

    static void showMessageAtLocation(char* message, const Point3d& pos, const vector::pen& pen);

    void startBomb();

    int numPlayers() const;

    player* getPlayer1() const;
    player* getPlayer2() const;
    player* getPlayer3() const;
    player* getPlayer4() const;

    // The gameplay mode currently running a match (null when no match is
    // active, e.g. in attract mode / menus).
    gameplay_mode* activeMode() const { return mMode.get(); }

    std::unique_ptr<sound> mSound;
    std::unique_ptr<grid> mGrid;
    std::unique_ptr<enemies> mEnemies;
    std::unique_ptr<particle> mParticles;
    std::unique_ptr<camera> mCamera;
    std::unique_ptr<attractor> mAttractors;
    std::unique_ptr<controls> mControls;
    std::unique_ptr<stars> mStars;
    std::unique_ptr<players> mPlayers;
    std::unique_ptr<blackholes> mBlackHoles;
    std::unique_ptr<spawner> mSpawner;
    std::unique_ptr<bomb> mBomb;
    std::unique_ptr<highscore> mHighscore;

    static GameMode mGameMode;

    static GameType mGameType;

    static bool mPaused;

    static int mCredits;

    // Set to request a clean shutdown of the whole application (checked by the
    // host main loop in OpenGW.cpp).
    static bool mQuitRequested;

    // The front-end overlay currently shown (see MenuScreen).
    static MenuScreen mMenuScreen;

  private:
    // classical_mode is granted access so it can drive the shared subsystems
    // (players, spawner, ...) and read the shell's brightness/fade state.
    friend class classical_mode;

    typedef struct
    {
        Point3d pos;
        vector::pen pen;
        char message[128];
        int timer;
        bool enabled;
    } PointDisplay;

    static std::vector<PointDisplay> mPointDisplays;

    void runPointDisplays();
    void drawPointDisplays();
    void clearPointDisplays();

    // Always-on world FX shared between a running match (drawn via the active
    // mode) and the attract-mode fireworks behind the menus.
    void drawParticles(int pass);
    void drawStars(int pass);

    // The active gameplay mode (set by startGame(), cleared when the run
    // returns to attract mode).
    std::unique_ptr<gameplay_mode> mMode;

    int mGameOverTimer { 0 };

    float mBrightness;

    bool mDebounce { false };

    std::unique_ptr<entity> mAttractModeBlackHoles[4];
};

extern std::unique_ptr<game> theGame;
