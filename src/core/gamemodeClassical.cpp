#include "core/gamemodeClassical.hpp"

#include "core/camera.hpp"
#include "core/controls.hpp"
#include "core/classicalparams.hpp"
#include "core/game.hpp"
#include "core/settings.hpp"

#include "entities/blackholes.hpp"
#include "entities/bomb.hpp"
#include "entities/enemies.hpp"
#include "entities/particle.hpp"
#include "entities/players.hpp"
#include "entities/spawner.hpp"
#include "entities/attractor.hpp"

#include "render/grid.hpp"
#include "render/scene.hpp"
#include "render/stars.hpp"

#include "render/gl3.h"

// The Classical match rules formerly lived directly in the game shell
// (game::startGame / the GAMEMODE_PLAYING tick / game::endGame / the world
// render inside game::draw). They are encapsulated here so the shell only
// dispatches begin/end/update/draw to its active gameplay_mode.

const char* classical_mode::name() const
{
    return "Classical";
}

void classical_mode::begin_match(game& owner)
{
    const auto& p = classical_params::get();

    owner.mBrightness = p.matchBrightnessStart; // we fade in the grid on start game

    owner.mCamera->center();
    owner.mCamera->mCurrentZoom = 0;

    mLevel = 0;

    mSkillLevel = 0;

    owner.mSpawner->init();

    // Fire up the players
    if (owner.getPlayer1()->mJoined) {
        owner.getPlayer1()->initPlayerForGame();
    }
    if (owner.getPlayer2()->mJoined) {
        owner.getPlayer2()->initPlayerForGame();
    }
    if (owner.getPlayer3()->mJoined) {
        owner.getPlayer3()->initPlayerForGame();
    }
    if (owner.getPlayer4()->mJoined) {
        owner.getPlayer4()->initPlayerForGame();
    }

    if (owner.numPlayers() > 1) {
        // Shared lives and bombs
        mSharedLives = p.matchCoopLives;
        mSharedBombs = p.matchCoopBombs;
    } else {
        // Single player keeps lives/bombs on the player objects.
        mSharedLives = 0;
        mSharedBombs = 0;
    }

    mMusicSpeed = p.matchMusicNormalSpeed;
    mMusicSpeedTarget = p.matchMusicNormalSpeed;

    owner.mSound->stopTrack(SOUNDID_MENU_MUSICLOOP);
    owner.mSound->playTrack(SOUNDID_MUSICLOOP);
    owner.mSound->setTrackSpeed(SOUNDID_MUSICLOOP, mMusicSpeed);

    owner.mSound->playTrack(SOUNDID_BACKGROUND_NOISELOOP);

    owner.mParticles->killAll();
}

void classical_mode::end_match(game& owner)
{
    // Doesn't actually end the game, just does some work that happens after the last player life is used

    owner.mSound->stopAllTracks();

    owner.mSound->playTrack(SOUNDID_PLAYERDEAD);
    owner.mSound->playTrack(SOUNDID_MENU_MUSICLOOP);

    // Kill all players
    owner.getPlayer1()->setState(entity::ENTITY_STATE_INACTIVE);
    owner.getPlayer2()->setState(entity::ENTITY_STATE_INACTIVE);
    owner.getPlayer3()->setState(entity::ENTITY_STATE_INACTIVE);
    owner.getPlayer4()->setState(entity::ENTITY_STATE_INACTIVE);

    owner.getPlayer1()->deinitPlayerForGame();
    owner.getPlayer2()->deinitPlayerForGame();
    owner.getPlayer3()->deinitPlayerForGame();
    owner.getPlayer4()->deinitPlayerForGame();

    // Kill all enemies
    owner.mEnemies->disableAllEnemies();
    owner.mEnemies->disableAllLines();

    // Kill all attractors
    owner.mAttractors->clearAll();
}

void classical_mode::update(game& owner)
{
    const auto& p = classical_params::get();

    if ((owner.numPlayers() > 1) && (mSharedLives > 0)) {
        if (owner.mControls->getStartButton(0) && !owner.getPlayer1()->mJoined) {
            owner.getPlayer1()->takeLife();
            owner.getPlayer1()->initPlayerForGame();
        }
        if (owner.mControls->getStartButton(1) && !owner.getPlayer2()->mJoined) {
            owner.getPlayer2()->takeLife();
            owner.getPlayer2()->initPlayerForGame();
        }
        if (owner.mControls->getStartButton(2) && !owner.getPlayer3()->mJoined) {
            owner.getPlayer3()->takeLife();
            owner.getPlayer3()->initPlayerForGame();
        }
        if (owner.mControls->getStartButton(3) && !owner.getPlayer4()->mJoined) {
            owner.getPlayer4()->takeLife();
            owner.getPlayer4()->initPlayerForGame();
        }
    }

    owner.mCamera->followPlayer();
    owner.mStars->run();
    owner.mBlackHoles->run();
    owner.mPlayers->run();
    owner.mBomb->run();
    owner.mSpawner->run();

    // Brightness
    if (owner.mBrightness < 1) {
        owner.mBrightness += p.matchBrightnessRamp;
    }

    // Music speed

    mMusicSpeedTarget = p.matchMusicNormalSpeed;

    // Slow the music down when someone is respawning
    if (owner.getPlayer1()->mJoined && (owner.getPlayer1()->getState() == entity::ENTITY_STATE_DESTROYED)) {
        mMusicSpeedTarget = 0;
        mMusicSpeed = p.matchMusicRespawnSpeed;
    }
    if (owner.getPlayer2()->mJoined && (owner.getPlayer2()->getState() == entity::ENTITY_STATE_DESTROYED)) {
        mMusicSpeedTarget = 0;
        mMusicSpeed = p.matchMusicRespawnSpeed;
    }
    if (owner.getPlayer3()->mJoined && (owner.getPlayer3()->getState() == entity::ENTITY_STATE_DESTROYED)) {
        mMusicSpeedTarget = 0;
        mMusicSpeed = p.matchMusicRespawnSpeed;
    }
    if (owner.getPlayer4()->mJoined && (owner.getPlayer4()->getState() == entity::ENTITY_STATE_DESTROYED)) {
        mMusicSpeedTarget = 0;
        mMusicSpeed = p.matchMusicRespawnSpeed;
    }

    if (mMusicSpeed < mMusicSpeedTarget) {
        mMusicSpeed += p.matchMusicSpeedUpStep;
        if (mMusicSpeed > mMusicSpeedTarget)
            mMusicSpeed = mMusicSpeedTarget;
    } else if (mMusicSpeed > mMusicSpeedTarget) {
        mMusicSpeed -= p.matchMusicSpeedDownStep;
        if (mMusicSpeed < mMusicSpeedTarget)
            mMusicSpeed = mMusicSpeedTarget;
    }

    owner.mSound->setTrackSpeed(SOUNDID_MUSICLOOP, mMusicSpeed);
}

void classical_mode::draw(game& owner, int pass)
{
    // The camera transform has already been applied by the game shell.

    // Grid
    if (((owner.mGameMode == game::GAMEMODE_PLAYING) || (owner.mGameMode == game::GAMEMODE_GAMEOVER) || (owner.mGameMode == game::GAMEMODE_GAMEOVER_TRANSITION))
#ifndef GRID_GLOW
        && (pass == scene::RENDERPASS_PRIMARY)
#endif
    ) {
        if (settings::get().mGridSmoothing) {
            glEnable(GL_LINE_SMOOTH);
            glEnable(GL_MULTISAMPLE);
        }

        glLineWidth(6);
        owner.mGrid->brightness = owner.mBrightness;
        owner.mGrid->draw();

        if (settings::get().mGridSmoothing) {
            glDisable(GL_MULTISAMPLE);
            glDisable(GL_LINE_SMOOTH);
        }
    }

    // Particles
    owner.drawParticles(pass);

    // Enemies
    {
        glLineWidth(4);

        if (pass == scene::RENDERPASS_PRIMARY) {
            if (settings::get().mEnemySmoothing) {
                glEnable(GL_LINE_SMOOTH);
                glEnable(GL_MULTISAMPLE);
            }

            owner.mEnemies->draw();

            if (settings::get().mEnemySmoothing) {
                glDisable(GL_MULTISAMPLE);
                glDisable(GL_LINE_SMOOTH);
            }
        } else {
            owner.mEnemies->draw();
        }
    }

    // Players
    if (owner.mGameMode == game::GAMEMODE_PLAYING) {
        glLineWidth(4);
        glPointSize(4 / 2);

        if (pass == scene::RENDERPASS_PRIMARY) {
            if (settings::get().mPlayerSmoothing) {
                glEnable(GL_LINE_SMOOTH);
                glEnable(GL_MULTISAMPLE);
            }

            owner.mPlayers->draw();

            if (settings::get().mPlayerSmoothing) {
                glDisable(GL_MULTISAMPLE);
                glDisable(GL_LINE_SMOOTH);
            }
        } else {
            owner.mPlayers->draw();
        }
    }

    // Stars
    owner.drawStars(pass);

    // Bombs
    {
        glLineWidth(4);
        owner.mBomb->draw();
    }

    // Point displays
    {
        glLineWidth(4);

        if (pass == scene::RENDERPASS_PRIMARY) {
            glEnable(GL_LINE_SMOOTH);
            glEnable(GL_MULTISAMPLE);

            owner.drawPointDisplays();

            glDisable(GL_MULTISAMPLE);
            glDisable(GL_LINE_SMOOTH);
        } else {
            owner.drawPointDisplays();
        }
    }
}
