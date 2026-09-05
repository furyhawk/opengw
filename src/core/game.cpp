#include "entities/attractor.hpp"
#include "entities/blackholes.hpp"
#include "entities/bomb.hpp"
#include "core/camera.hpp"
#include "core/controls.hpp"
#include "math/defines.hpp"
#include "entities/enemies.hpp"
#include "entities/entity.hpp"
#include "render/font.hpp"
#include "core/game.hpp"
#include "core/gamemodeClassical.hpp"
#include "render/grid.hpp"
#include "core/highscore.hpp"
#include "entities/particle.hpp"
#include "entities/players.hpp"
#include "render/scene.hpp"
#include "core/settings.hpp"
#include "entities/spawner.hpp"
#include "render/stars.hpp"
#include "ui/menuMain.hpp"
#include "ui/menuPause.hpp"
#include "ui/menuSelectGameType.hpp"
#include "ui/menuSettings.hpp"

#include <cstdio>
#include <memory>

#include "render/gl3.h"

// Statics
//
//settings game::mSettings;
//sound game::mSound;
// particle game::mParticles;
//attractor game::mAttractors;
//controls game::mControls;
//blackholes game::mBlackHoles;
//spawner game::mSpawner;
//bomb game::mBomb;
//highscore game::mHighscore;
// grid game::mGrid;

#define NUM_POINT_DISPLAYS 40
#define TIME_POINT_DISPLAY 50

extern std::unique_ptr<scene> oglScene;

game::GameMode game::mGameMode;
game::GameType game::mGameType;
std::vector<game::PointDisplay> game::mPointDisplays;
bool game::mPaused = false;
int game::mCredits = 0;
bool game::mQuitRequested = false;
game::MenuScreen game::mMenuScreen = game::MENU_NONE;

game::game()
{
    mGrid = std::make_unique<grid>();
    mSound = std::make_unique<sound>();
    mParticles = std::make_unique<particle>();
    mAttractors = std::make_unique<attractor>();
    mControls = std::make_unique<controls>();
    mBlackHoles = std::make_unique<blackholes>();
    mSpawner = std::make_unique<spawner>();
    mBomb = std::make_unique<bomb>();
    mHighscore = std::make_unique<highscore>();

    //
    // Load our sounds
    //

    mSound->loadTrack("assets/sounds/musicloop.wav", SOUNDID_MUSICLOOP, .2, true);
    mSound->loadTrack("assets/sounds/menumusicloop.wav", SOUNDID_MENU_MUSICLOOP, .2, true);
    mSound->loadTrack("assets/sounds/menuselect.wav", SOUNDID_MENU_SELECT, .2, false);

    mSound->loadTrack("assets/sounds/backgroundnoiseloop.wav", SOUNDID_BACKGROUND_NOISELOOP, .2, true);

    mSound->loadTrack("assets/sounds/playerspawn.wav", SOUNDID_PLAYERSPAWN, 1, false);
    mSound->loadTrack("assets/sounds/playerhit.wav", SOUNDID_PLAYERHIT, .5, false);
    mSound->loadTrack("assets/sounds/playerdead.wav", SOUNDID_PLAYERDEAD, .5, false);
    mSound->loadTrack("assets/sounds/sheildsdown.wav", SOUNDID_SHIELDSLOST, .7, false);
    mSound->loadTrack("assets/sounds/playerthrust.wav", SOUNDID_PLAYERTHRUST, .2, true);

    mSound->loadTrack("assets/sounds/extralife.wav", SOUNDID_EXTRALIFE, .3, false);
    mSound->loadTrack("assets/sounds/extrabomb.wav", SOUNDID_EXTRABOMB, .3, false);

    mSound->loadTrack("assets/sounds/bomb.wav", SOUNDID_BOMB, .5, false);

    mSound->loadTrack("assets/sounds/multiplieradvance.wav", SOUNDID_MULTIPLIERADVANCE, .3, false);

    mSound->loadTrack("assets/sounds/missilehitwall.wav", SOUNDID_MISSILEHITWALL, .3, false);

    mSound->loadTrack("assets/sounds/gravitywelldestroyed.wav", SOUNDID_GRAVITYWELLDESTROYED, .3, false);
    mSound->loadTrack("assets/sounds/gravitywelldestroyed.wav", SOUNDID_GRAVITYWELLABSORBED, .1, false);
    mSound->loadTrack("assets/sounds/gravitywellhit.wav", SOUNDID_GRAVITYWELLHIT, .3, false);
    mSound->loadTrack("assets/sounds/gravitywellalert.wav", SOUNDID_GRAVITYWELLALERT, .5, false);
    mSound->loadTrack("assets/sounds/gravitywellexplode.wav", SOUNDID_GRAVITYWELLEXPLODE, .5, false);

    mSound->loadTrack("assets/sounds/repulsor.wav", SOUNDID_REPULSORA, .4, true);
    mSound->loadTrack("assets/sounds/repulsor.wav", SOUNDID_REPULSORB, .4, true);
    mSound->loadTrack("assets/sounds/repulsor.wav", SOUNDID_REPULSORC, .4, true);
    mSound->loadTrack("assets/sounds/repulsor.wav", SOUNDID_REPULSORD, .4, true);

    mSound->loadTrack("assets/sounds/gravitywellhumloop.wav", SOUNDID_GRAVITYWELL_HUMLOOPA, .3, true);
    mSound->loadTrack("assets/sounds/gravitywellhumloop.wav", SOUNDID_GRAVITYWELL_HUMLOOPB, .3, true);
    mSound->loadTrack("assets/sounds/gravitywellhumloop.wav", SOUNDID_GRAVITYWELL_HUMLOOPC, .3, true);
    mSound->loadTrack("assets/sounds/gravitywellhumloop.wav", SOUNDID_GRAVITYWELL_HUMLOOPD, .3, true);
    mSound->loadTrack("assets/sounds/gravitywellhumloop.wav", SOUNDID_GRAVITYWELL_HUMLOOPE, .3, true);
    mSound->loadTrack("assets/sounds/gravitywellhumloop.wav", SOUNDID_GRAVITYWELL_HUMLOOPF, .3, true);

    mSound->loadTrack("assets/sounds/enemyspawn1.wav", SOUNDID_ENEMYSPAWN1, .5, false);
    mSound->loadTrack("assets/sounds/enemyspawn2.wav", SOUNDID_ENEMYSPAWN2, .5, false);
    mSound->loadTrack("assets/sounds/enemyspawn3.wav", SOUNDID_ENEMYSPAWN3, .5, false);
    mSound->loadTrack("assets/sounds/enemyspawn4.wav", SOUNDID_ENEMYSPAWN4, .5, false);
    mSound->loadTrack("assets/sounds/enemyspawn5.wav", SOUNDID_ENEMYSPAWN5, .3, false);
    mSound->loadTrack("assets/sounds/enemyspawn6.wav", SOUNDID_ENEMYSPAWN6, .3, false);

    mSound->loadTrack("assets/sounds/enemyhit.wav", SOUNDID_ENEMYHIT, .5, false);

    mSound->loadTrack("assets/sounds/mayflies.wav", SOUNDID_MAYFLIES, .1, true);

    mSound->loadTrack("assets/sounds/playerfire1.wav", SOUNDID_PLAYERFIRE1, .9, true);
    mSound->loadTrack("assets/sounds/playerfire2.wav", SOUNDID_PLAYERFIRE2, .5, true);
    mSound->loadTrack("assets/sounds/playerfire3.wav", SOUNDID_PLAYERFIRE3, .8, true);

    mSound->startSound();

    // Apply the stored master volume.
    mSound->setMasterVolume(settings::get().mSoundVolume / 100.0f);

    mBrightness = 0;

    mCamera = std::make_unique<camera>(*this);
    mCamera->center();

    mPointDisplays.resize(NUM_POINT_DISPLAYS);
    for (int i = 0; i < NUM_POINT_DISPLAYS; i++) {
        mPointDisplays[i].enabled = false;
    }

    // Tag 4 black holes for attract mode
    for (int i = 0; i < 4; i++) {
        mAttractModeBlackHoles[i] = std::make_unique<entity>();
        mAttractModeBlackHoles[i]->setPos(Point3d(mathutils::frandFrom0To1() * mGrid->extentX(), mathutils::frandFrom0To1() * mGrid->extentY(), 0));
        mAttractModeBlackHoles[i]->setEdgeBounce(false);

        static float heading = mathutils::frandFrom0To1() * (2 * PI);
        if ((mathutils::frandFrom0To1() * 40) < 1) {
            // Pick a random direction
            heading += mathutils::frandFrom0To1() * (2 * PI);
        }

        Point3d speedVector = Point3d(1, 0, 0);
        speedVector = mathutils::rotate2dPoint(speedVector, heading);
        mAttractModeBlackHoles[i]->setDrift(speedVector);
    }

    mGameMode = GAMEMODE_ATTRACT;

    mSound->playTrack(SOUNDID_MENU_MUSICLOOP);

    mStars = std::make_unique<stars>(*this);
    mPlayers = std::make_unique<players>(*this);
    mEnemies = std::make_unique<enemies>(*this);
}

game::~game()
{
    // TODO: mParticles uses theGame pointer, which would be nil now.
    // mParticles.reset();
    // mGrid.reset();
}

void game::quitThreads()
{
    printf("%s\n", __func__);
    mParticles.reset();
    mGrid.reset();
}

void game::run()
{
    // Credits
    static bool creditButtonLast = false;
    bool creditButton = false; // mControls->getCreditButton();
    if (creditButton && !creditButtonLast) {
        ++mCredits;
    }
    creditButtonLast = creditButton;

    if (settings::get().mCreditsPerGame == 0) {
        mCredits = 4;
    }

    // ---- Pause: opening the pause menu during a match ---------------------
    if ((mGameMode == GAMEMODE_PLAYING) && (mMenuScreen == MENU_NONE)) {
        static bool pauseLast = false;
        bool pause = mControls->getPauseButton(0) || mControls->getPauseButton(1) || mControls->getPauseButton(2) || mControls->getPauseButton(3);
        if (pause && !pauseLast) {
            game::mSound->playTrack(SOUNDID_MENU_SELECT);
            mPaused = true;
            mMenuScreen = MENU_PAUSE;
            menuPause::init();
            mSound->pauseAllTracksBut(SOUNDID_MENU_SELECT);
        }
        pauseLast = pause;
    }

    // ---- While a match is paused, freeze the world and drive the menu -----
    if (mPaused) {
        switch (mMenuScreen) {
        case MENU_PAUSE:
            menuPause::run();
            break;
        case MENU_SETTINGS:
            menuSettings::run();
            break;
        default:
            break;
        }
        return;
    }

    // Run the camera
    mCamera->run();

    // Run the point displays
    runPointDisplays();

    // Run the enemies
    mEnemies->run();

    switch (mGameMode) {
    case GAMEMODE_ATTRACT:
    {
        if (mCredits > 0) {
            mGameMode = GAMEMODE_CREDITED;
            mDebounce = true;
        }
    } break;
    case GAMEMODE_CREDITED:
        // Show the title / main menu. Entered once while no menu is up; the
        // menu itself is driven from the attract block below so the fireworks
        // keep animating behind the title.
        if (mMenuScreen == MENU_NONE) {
            menuMain::init();
            mMenuScreen = MENU_TITLE;
        }
        break;
    case GAMEMODE_CHOOSE_GAMETYPE:
        // Handled in menuSelectGameType.cpp
        break;
    case GAMEMODE_PLAYING:
    {
        // The active gameplay mode (e.g. classical_mode) drives the match.
        if (mMode) {
            mMode->update(*this);
        }
    } break;
    case GAMEMODE_HIGHSCORES:
        mHighscore->run();
        break;
    case GAMEMODE_HIGHSCORES_CHECK:
        if (mHighscore->isHighScore(getPlayer1()->mScore) == true) {
            mGameMode = GAMEMODE_HIGHSCORES;
            mHighscore->init();
            break;
        }
        oglScene->showHighScores();
        break;
    case GAMEMODE_GAMEOVER_TRANSITION:
        mGameMode = GAMEMODE_GAMEOVER;
        mGameOverTimer = 0;
        break;
    case GAMEMODE_GAMEOVER:

        if (mBrightness > 0) {
            mBrightness *= .98;
        }

        ++mGameOverTimer;
        if (mGameOverTimer > 180) {
            // Back to attract: the match (and its gameplay mode) is over.
            mMode.reset();
            mGameMode = GAMEMODE_ATTRACT;
            mCamera->mCurrentZoom = 1;
        }
        break;
    }

    if (mGameMode == GAMEMODE_ATTRACT || mGameMode == GAMEMODE_CREDITED) {
        if (mBrightness > 0) {
            mBrightness *= .98;
        }
    }

    if ((game::mGameMode == game::GAMEMODE_HIGHSCORES_CHECK) || (game::mGameMode == game::GAMEMODE_HIGHSCORES)) {
    } else if (mGameMode == GAMEMODE_ATTRACT || mGameMode == GAMEMODE_CREDITED || mGameMode == GAMEMODE_CHOOSE_GAMETYPE) {
        static int explosionTimer = 0;

        ++explosionTimer;
        if (explosionTimer > 1000) {
            explosionTimer = 0;
        }
        if (explosionTimer == 980) {
            game::mSound->playTrack(SOUNDID_GRAVITYWELLEXPLODE);
        }

        mCamera->center();

        // Drive the menus that sit in front of the attract field: the game
        // type selector, the title main menu and (from the title) the settings
        // screen. The pause / settings-from-pause paths are handled above
        // while the world is frozen.
        if (mGameMode == GAMEMODE_CHOOSE_GAMETYPE) {
            menuSelectGameType::run();
        } else if (mMenuScreen == MENU_TITLE) {
            menuMain::run();
        } else if (mMenuScreen == MENU_SETTINGS) {
            menuSettings::run();
        }

        // Attractors to wander around the fireworks display

        int sizex = mGrid->extentX();
        int sizey = mGrid->extentY();

        for (int i = 0; i < 4; i++) {
            mAttractModeBlackHoles[i]->setState(entity::ENTITY_STATE_RUNNING);
            mAttractModeBlackHoles[i]->run();

            Point3d pos = mAttractModeBlackHoles[i]->getPos();

            attractor::Attractor* att = mAttractors->getAttractor();
            if (att) {
                att->strength = (explosionTimer > 980) ? 1000 : -40;
                att->radius = 40;
                att->pos = pos;
                att->enabled = true;
                att->attractsParticles = true;

                if (mGameMode == GAMEMODE_CHOOSE_GAMETYPE) {
                    // static float breathValue = 0;
                    att->strength = 200; // sin(breathValue) * 10;
                    // breathValue += .002;
                }
            }

            for (int j = 0; j < 4; j++) {
                if (i == j)
                    continue;

                float angle = mathutils::calculate2dAngle(mAttractModeBlackHoles[j]->getPos(), mAttractModeBlackHoles[i]->getPos());
                float distance = mathutils::calculate2dDistance(mAttractModeBlackHoles[j]->getPos(), mAttractModeBlackHoles[i]->getPos());

                float strength = 2;
                if (distance < mAttractModeBlackHoles[i]->getRadius()) {
                    distance = mAttractModeBlackHoles[i]->getRadius();
                }

                float r = 1.0 / (distance * distance);

                // Add a slight curving vector to the gravity
                Point3d gravityVector(r * strength, 0, 0);
                Point3d g = mathutils::rotate2dPoint(gravityVector, angle + .4);

                Point3d speed = mAttractModeBlackHoles[j]->getDrift();
                speed.x += g.x;
                speed.y += g.y;
                mAttractModeBlackHoles[j]->setDrift(speed);
            }

            // float heading;
            if ((mathutils::frandFrom0To1() * 40) < 1) {
                // Pick a random direction
                // heading + mathutils::frandFrom0To1() * (2*PI);
            }

            // Change direction when we hit the grid edges

            Point3d mSpeed = mAttractModeBlackHoles[i]->getDrift();
            Point3d mPos = mAttractModeBlackHoles[i]->getPos();

            const float leftEdge = 2;
            const float bottomEdge = 2;
            const float rightEdge = (sizex - 2) - 1;
            const float topEdge = (sizey - 2) - 1;

            if (mPos.x <= leftEdge) {
                mSpeed.x = -mSpeed.x;
                mPos.x = leftEdge;
            } else if (mPos.x >= rightEdge) {
                mSpeed.x = -mSpeed.x;
                mPos.x = rightEdge;
            }
            if (mPos.y <= bottomEdge) {
                mSpeed.y = -mSpeed.y;
                mPos.y = bottomEdge;
            } else if (mPos.y >= topEdge) {
                mSpeed.y = -mSpeed.y;
                mPos.y = topEdge;
            }

            mAttractModeBlackHoles[i]->setDrift(mSpeed);
            mAttractModeBlackHoles[i]->setPos(mPos);
        }

        if (mGameMode != GAMEMODE_CHOOSE_GAMETYPE && mMenuScreen != MENU_SETTINGS) {
            // Fireworks display
            static int fw = 99999;
            ++fw;
            if (fw >= 5) {
                fw = 0;
                static float colorTimer = 0;
                colorTimer += .08;

                Point3d pos(mathutils::frandFrom0To1() * sizex, mathutils::frandFrom0To1() * sizey);

                {
                    Point3d angle(0, 0, 0);
                    float speed = mathutils::frandFrom0To1() * 4;
                    float spread = (2 * PI);
                    int num = 50;
                    int timeToLive = 500;
                    vector::pen pen;

                    pen.r = get_sin(colorTimer + ((2 * PI) / 1));
                    pen.g = get_sin(colorTimer + ((2 * PI) / 2));
                    pen.b = get_sin(colorTimer + ((2 * PI) / 3));

                    if (pen.r < 0)
                        pen.r = 0;
                    if (pen.g < 0)
                        pen.g = 0;
                    if (pen.b < 0)
                        pen.b = 0;

                    pen.r += .2;
                    pen.g += .2;
                    pen.b += .2;

                    pen.a = 100;
                    pen.lineRadius = 4;
                    mParticles->emitter(&pos, &angle, speed, spread, num, &pen, timeToLive, true, false, .98, true);
                }
            }
        }
    }

    mGrid->run();
    mParticles->run();
}

// #define GRID_GLOW // PERFORMANCE: Making the grid glow causes us to have to draw it twice, which is slower
#define PARTICLE_GLOW // PERFORMANCE: Uncomment to get glowing particles, but it has a performance hit!

void game::draw(int pass)
{
    // The camera
    {
        glTranslatef(-mCamera->mCurrentPos.x, -mCamera->mCurrentPos.y, -mCamera->mCurrentPos.z);
    }

    if (mMode) {
        // A match is running (playing or fading out): the active gameplay
        // mode renders its arena/world.
        mMode->draw(*this, pass);
    } else if ((mGameMode == GAMEMODE_ATTRACT) || (mGameMode == GAMEMODE_CREDITED) || (mGameMode == GAMEMODE_CHOOSE_GAMETYPE)
               || (mGameMode == GAMEMODE_HIGHSCORES_CHECK) || (mGameMode == GAMEMODE_HIGHSCORES)) {
        // No match running: back the menus with the attract-mode FX.
        drawParticles(pass);

        // Stars stay visible (frozen while no match is running).
        drawStars(pass);

        if (mGameMode == GAMEMODE_CHOOSE_GAMETYPE) {
            menuSelectGameType::draw();
        }
    }
}

void game::drawParticles(int pass)
{
    // Particles
    if (pass == scene::RENDERPASS_PRIMARY) {
        if (settings::get().mParticleSmoothing) {
            glEnable(GL_LINE_SMOOTH);
            glEnable(GL_MULTISAMPLE);
        }

        glLineWidth(4);

        mParticles->draw();

        if (settings::get().mParticleSmoothing) {
            glDisable(GL_MULTISAMPLE);
            glDisable(GL_LINE_SMOOTH);
        }
    } else {
#ifdef PARTICLE_GLOW
        glLineWidth(10);
        mParticles->draw();
#endif
    }
}

void game::drawStars(int pass)
{
    // Stars
    if (pass == scene::RENDERPASS_PRIMARY) {
        if (settings::get().mStarSmoothing) {
            glEnable(GL_POINT_SMOOTH);
            glEnable(GL_MULTISAMPLE);
        }

        mStars->draw();

        if (settings::get().mStarSmoothing) {
            glDisable(GL_MULTISAMPLE);
            glDisable(GL_POINT_SMOOTH);
        }
    }
}

void game::startGame(GameType gameType)
{
    mGameType = gameType;

    // Enter the selected gameplay mode. Only "Classical" exists so far; more
    // modes can be created and selected here later.
    mMode = std::make_unique<classical_mode>();
    printf("Starting gameplay mode: %s\n", mMode->name());

    mMode->begin_match(*this);

    mGameMode = GAMEMODE_PLAYING;
}

void game::endGame()
{
    // Doesn't actually end the game, just does some work that happens after the last player life is used
    if (mMode) {
        mMode->end_match(*this);
    }
}

void game::abandonMatch()
{
    // Abandon a running match from the pause menu and return to the attract /
    // title flow. Release any paused audio, then let the mode tear itself down.
    mSound->unpauseAllTracks();
    mPaused = false;
    mMenuScreen = MENU_NONE;

    if (mMode) {
        mMode->end_match(*this);
    }
    mMode.reset();

    mCamera->mCurrentZoom = 1;
    mGameMode = GAMEMODE_ATTRACT;
}

void game::showMessageAtLocation(char* message, const Point3d& pos, const vector::pen& pen)
{
    // Find an empty diplay
    for (int i = 0; i < NUM_POINT_DISPLAYS; i++) {
        if (!mPointDisplays[i].enabled) {
            mPointDisplays[i].enabled = true;
            mPointDisplays[i].pos = pos;
            mPointDisplays[i].pen = pen;
            snprintf(mPointDisplays[i].message, sizeof(mPointDisplays[i].message), "%s", message);
            mPointDisplays[i].timer = TIME_POINT_DISPLAY;
            break;
        }
    }
}

void game::startBomb()
{
}

void game::runPointDisplays()
{
    for (int i = 0; i < NUM_POINT_DISPLAYS; i++) {
        if (mPointDisplays[i].enabled) {
            mPointDisplays[i].timer--;
            if (mPointDisplays[i].timer <= 0) {
                mPointDisplays[i].enabled = false;
            }
        }
    }
}

void game::drawPointDisplays()
{
    for (int i = 0; i < NUM_POINT_DISPLAYS; i++) {
        if (mPointDisplays[i].enabled) {
            float percent = (float)mPointDisplays[i].timer / TIME_POINT_DISPLAY;
            percent *= 2;
            if (percent > 1)
                percent = 1;

            float scale = .9 * percent;
            float alpha = 1 * percent;

            vector::pen pen = mPointDisplays[i].pen;
            pen.a = alpha;
            pen.lineRadius = 3;

            font::AlphanumericsPrint(font::ALIGN_CENTER, scale, mPointDisplays[i].pos.x, mPointDisplays[i].pos.y, &pen, mPointDisplays[i].message);
        }
    }
}

void game::clearPointDisplays()
{
    for (int i = 0; i < NUM_POINT_DISPLAYS; i++) {
        mPointDisplays[i].enabled = false;
    }
}

int game::numPlayers() const
{
    int numPlayers = 0;

    if (getPlayer1()->mJoined) {
        ++numPlayers;
    }
    if (getPlayer2()->mJoined) {
        ++numPlayers;
    }
    if (getPlayer3()->mJoined) {
        ++numPlayers;
    }
    if (getPlayer4()->mJoined) {
        ++numPlayers;
    }

    return numPlayers;
}

player* game::getPlayer1() const { return mPlayers->getPlayer1(); }
player* game::getPlayer2() const { return mPlayers->getPlayer2(); }
player* game::getPlayer3() const { return mPlayers->getPlayer3(); }
player* game::getPlayer4() const { return mPlayers->getPlayer4(); }
