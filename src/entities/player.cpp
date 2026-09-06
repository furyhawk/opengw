#include "entities/attractor.hpp"
#include "entities/bomb.hpp"
#include "core/classicalparams.hpp"
#include "core/controls.hpp"
#include "math/defines.hpp"
#include "core/game.hpp"
#include "core/gamemode.hpp"
#include "entities/player.hpp"

#include <algorithm>
#include <cstdio>

#include "render/gl3.h"

player::player()
    : entity()
{
    mDrawSheild = false;

    mScoreValue = 0;

    const auto& p = classical_params::get();

    mSpawnTime = p.playerSpawnTime;

    mSheildTimer = p.playerShieldTime;

    mJoined = false;

    mDestroyTime = p.playerDestroyTime;

    mExhaustSpreadIndex = mathutils::randFromTo(0, 1);

    setState(ENTITY_STATE_INACTIVE);

    // Create our missiles
    missiles.resize(500);
    for (std::size_t i = 0; i < missiles.size(); i++) {
        entityPlayerMissile* missile = &missiles[i];
        missile->setEnabled(false);
    }

    initPlayerForGame();
}

player::~player()
{
}

void player::initPlayerForGame()
{
    const auto& p = classical_params::get();

    mNumBombs = p.playerStartBombs;
    mNumLives = p.playerStartLives;

    mWeaponCounter = 0;
    mBombCounter = 0;
    mLifeCounter = 0;

    mScore = 0;

    mMultiplier = 1;

    mShieldCharges = 0;

    mHasHoming = false;
    mHomingTimer = 0;

    mFiringTimer = 0;

    mKillCounter = 0;
    mLevelAdvanceCounter = 0;

    mCurrentWeapon = 0;

    // Every weapon starts the match at power level 1.
    for (int w = 0; w < WEAPON_COUNT; ++w) {
        mWeaponPower[w] = 1;
    }

    mAngle = 0;

    // Set all missiles to inactive
    for (std::size_t i = 0; i < missiles.size(); i++) {
        entityPlayerMissile* missile = &missiles[i];
        missile->setEnabled(false);
        missile->mLife = 0;
    }

    setState(entity::ENTITY_STATE_SPAWN_TRANSITION);

    mJoined = true;
}

void player::deinitPlayerForGame()
{
    mJoined = true;
}

void player::run()
{
    const auto& p = classical_params::get();

    if (getEnabled()) {
        // Read the trigger
        if (theGame->mGameType == game::GAMETYPE_SINGLEPLAYER) {
            bool trigger = theGame->mControls->getTriggerButton(mPlayerAssignment);
            if (trigger) {
                if (getNumBombs() > 0 && mBombInterimTimer <= 0) {
                    // Fire off a bomb
                    takeBomb();
                    mBombInterimTimer = p.playerBombCooldown;

                    theGame->mBomb->startBomb(mPos, p.bombRingRadius, p.bombRingThickness, p.bombRingSpeed, p.bombRingTimeToLive, vector::pen(1, 1, 1, .3, 4));
                    theGame->mSound->playTrack(SOUNDID_BOMB);
                    theGame->startBomb();
                }
            }
            if (mBombInterimTimer > 0) {
                --mBombInterimTimer;
            }
        }

        Point3d playerSpeed(0, 0, 0);

        // Move the player
        Point3d leftStick = theGame->mControls->getLeftStick(mPlayerAssignment);
        float distance = mathutils::calculate2dDistance(Point3d(0, 0, 0), leftStick);
        if (distance > p.playerStickDeadZone) {
            //
            // The movement stick is being used
            //

            if (distance > p.playerStickFullZone) {
                distance = 1;
            } else
                distance = .5;

            float angle = mathutils::calculate2dAngle(Point3d(0, 0, 0), leftStick) + mathutils::DegreesToRads(90);

            angle -= mathutils::DegreesToRads(90);

            // Rotate to the correct angle
            angle = mathutils::wrapRadians(angle);
            float currentAngle = mathutils::wrapRadians(this->getAngle());
            float diff = mathutils::diffAngles(angle, currentAngle);
            currentAngle += diff * p.playerMoveTurnRate;
            this->setAngle(currentAngle);

            // Move
            Point3d thrust(distance * p.playerMoveSpeed, 0, 0);
            thrust = mathutils::rotate2dPoint(thrust, currentAngle + mathutils::DegreesToRads(90));
            playerSpeed = thrust;
            this->setPos(this->getPos() + thrust);

            // Emit exhaust particles
            Point3d exhaustAngle = currentAngle + mathutils::DegreesToRads(180);
            Point3d exhaustOffset;

            float speed = .8; // .5
            int num = 1;
            int timeToLive = 200;
            vector::pen pen = this->getExhaustPen();
            pen.a = 100;

            // Main stream
            {
                float spread = .1;
                exhaustOffset = mathutils::rotate2dPoint(Point3d(0, -2, 0), currentAngle);
                exhaustOffset += getPos();
                theGame->mParticles->emitter(&exhaustOffset, &exhaustAngle, speed, spread, num, &pen, timeToLive, true, true, .92, false);
            }
            // First swirl
            {
                exhaustOffset = mathutils::rotate2dPoint(Point3d(0, -3, 0), currentAngle + (get_sin(mExhaustSpreadIndex) * .3));
                exhaustOffset += getPos();

                float spread = 0;
                theGame->mParticles->emitter(&exhaustOffset, &exhaustAngle, speed, spread, num, &pen, timeToLive, true, true, .92, false);
            }
            // Second swirl
            {
                exhaustOffset = mathutils::rotate2dPoint(Point3d(0, -3, 0), currentAngle + (get_sin(-mExhaustSpreadIndex) * .3));
                exhaustOffset += getPos();

                float spread = 0;
                theGame->mParticles->emitter(&exhaustOffset, &exhaustAngle, speed, spread, num, &pen, timeToLive, true, true, .92, false);
            }
            mExhaustSpreadIndex += .18;

            if (!theGame->mSound->isTrackPlaying(SOUNDID_PLAYERTHRUST))
                theGame->mSound->playTrack(SOUNDID_PLAYERTHRUST);
        } else {
            theGame->mSound->stopTrack(SOUNDID_PLAYERTHRUST);
        }

        // Firing
        Point3d rightStick = theGame->mControls->getRightStick(mPlayerAssignment);
        distance = mathutils::calculate2dDistance(Point3d(0, 0, 0), rightStick);
        if (distance > p.playerStickDeadZone) {
            //
            // The firing stick is being used
            //

            switch (mCurrentWeapon) {
            case 0:
                firePattern1(rightStick, playerSpeed);
                {
                    if (!theGame->mSound->isTrackPlaying(SOUNDID_PLAYERFIRE1))
                        theGame->mSound->playTrack(SOUNDID_PLAYERFIRE1);

                    theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE2);
                    theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE3);
                }
                break;
            case 1:
                firePattern2(rightStick, playerSpeed);
                {
                    if (!theGame->mSound->isTrackPlaying(SOUNDID_PLAYERFIRE2))
                        theGame->mSound->playTrack(SOUNDID_PLAYERFIRE2);

                    theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE1);
                    theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE3);
                }
                break;
            case 2:
                firePattern3(rightStick, playerSpeed);
                {
                    if (!theGame->mSound->isTrackPlaying(SOUNDID_PLAYERFIRE3))
                        theGame->mSound->playTrack(SOUNDID_PLAYERFIRE3);

                    theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE1);
                    theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE2);
                }
                break;
            case 3:
                firePattern4(rightStick, playerSpeed);
                {
                    if (!theGame->mSound->isTrackPlaying(SOUNDID_PLAYERFIRE3))
                        theGame->mSound->playTrack(SOUNDID_PLAYERFIRE3);

                    theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE1);
                    theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE2);
                }
                break;
            }

            // Secondary weapon: when equipped, homing missiles auto-launch
            // alongside the main weapon while the fire stick is held.
            if (mHasHoming && --mHomingTimer <= 0) {
                mHomingTimer = 16;
                fireHomingMissile(rightStick, playerSpeed);
            }
        } else {
            theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE1);
            theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE2);
            theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE3);
        }
    }

    runMissiles();

    mDrawSheild = false;

    if (mSheildTimer > 0)
        --mSheildTimer;

    if (mSheildTimer < p.playerShieldWarnTime) {
        mDrawSheild = (mSheildTimer / 6) & 1;
    } else
        mDrawSheild = true;

    if (mSheildTimer == p.playerShieldWarnTime) {
        theGame->mSound->playTrack(SOUNDID_SHIELDSLOST);
    }

    if (mDrawSheild) {
        attractor::Attractor* att = theGame->mAttractors->getAttractor();
        if (att) {
            att->strength = 100;
            att->radius = 3.5;
            att->pos = mPos;
            att->enabled = true;
            att->attractsParticles = false;
        }
    }

    entity::run();
}

void player::indicating()
{
    if (--mStateTimer <= 0) {
        setState(ENTITY_STATE_INACTIVE);
    }
}

void player::runMissiles()
{
    for (std::size_t i = 0; i < missiles.size(); i++) {
        entityPlayerMissile* missile = &missiles[i];
        if (missile->getEnabled()) {
            switch (missile->getState()) {
            case entity::ENTITY_STATE_SPAWN_TRANSITION:
                missile->spawnTransition();
                break;
            case entity::ENTITY_STATE_SPAWNING:
                missile->spawn();
                break;
            case entity::ENTITY_STATE_RUN_TRANSITION:
                missile->runTransition();
                break;
            case entity::ENTITY_STATE_RUNNING:
                missile->run();
                break;
            case entity::ENTITY_STATE_DESTROY_TRANSITION:
                missile->destroyTransition();
                break;
            case entity::ENTITY_STATE_DESTROYED:
                missile->destroy();
                break;
            default:
                printf("Unhandled missile state\n");
                break;
            }
        }
    }
}

void player::draw()
{
    // Draw the missiles
    for (std::size_t i = 0; i < missiles.size(); i++) {
        entityPlayerMissile* missile = &missiles[i];
        if (missile->getEnabled())
            missile->draw();
    }

    if (getEnabled()) {
        // Draw the shields
        if (mDrawSheild) {
            float delta_theta = 0.05;
            float r = 2.5;

            glColor4f(mPen.r, mPen.g, mPen.b, mPen.a);

            glBegin(GL_LINE_LOOP);

            for (float angle = 0; angle < 2 * PI; angle += delta_theta)
                glVertex3f(mPos.x + (r * get_cos(angle)), mPos.y + (r * get_sin(angle)), 0);

            glEnd();
        }

        // Dashed ring showing remaining shield charges (Endless pickups).
        if ((mShieldCharges > 0) && (getState() == entity::ENTITY_STATE_RUNNING)) {
            float r = 3.0f;
            glLineWidth(2);
            glColor4f(mPen.r * .5f + .5f, mPen.g * .5f + .5f, mPen.b * .5f + .5f, .9f);
            glBegin(GL_LINES);
            for (float angle = 0; angle < 2 * PI; angle += 0.05f) {
                bool solid = (static_cast<int>(angle / 0.25f) & 1) == 0;
                if (!solid)
                    continue;
                glVertex3f(mPos.x + (r * get_cos(angle)), mPos.y + (r * get_sin(angle)), 0);
                glVertex3f(mPos.x + (r * get_cos(angle + 0.05f)), mPos.y + (r * get_sin(angle + 0.05f)), 0);
            }
            glEnd();
        }
    }

    if (getState() != entity::ENTITY_STATE_DESTROYED)
        entity::draw();
}

void player::spawnTransition()
{
    const auto& p = classical_params::get();
    mSheildTimer = p.playerShieldTime;

    float angle = getAngle();
    entity::spawnTransition();
    setAngle(angle);

    mDrawSheild = true;

    theGame->mSound->playTrack(SOUNDID_PLAYERSPAWN);
}

void player::spawn()
{
    entity::spawn();

    mDrawSheild = true;

    // Rez-up grid distortion

    float b = (float)mStateTimer / mSpawnTime;
    b = 1 - b;

    attractor::Attractor* att = theGame->mAttractors->getAttractor();
    if (att) {
        att->strength = 20;
        att->radius = 28 * b;
        att->pos = mPos;
        att->enabled = true;
        att->attractsParticles = true;
    }
    att = theGame->mAttractors->getAttractor();
    if (att) {
        att->strength = -20;
        att->radius = 30 * b;
        att->pos = mPos;
        att->enabled = true;
        att->attractsParticles = true;
    }
}

void player::firePattern1(const Point3d& fireAngle, const Point3d& playerSpeed)
{
    const auto& p = classical_params::get();

    // Powered-up twin: a widening fan of bullets and a slightly faster cadence.
    const int power = weaponPower();
    if (power > 1) {
        if (--mFiringTimer <= 0) {
            mFiringTimer = std::max(3, p.weapon0Interval - (power - 1));
            const float speedMul = 1.0f + (0.08f * (power - 1));
            const float half = p.weapon0Spread * (1.0f + (0.22f * (power - 1)));
            launchFan(fireAngle, playerSpeed, 0, power * 2, half, p.weapon0MissileSpeed * speedMul, p.weapon0InheritSpeed);
        }
        return;
    }

    if (--mFiringTimer <= 0) {
        mFiringTimer = p.weapon0Interval;

        // Find an unused missile
        entityPlayerMissile* missile1 = nullptr;
        for (std::size_t i = 0; i < missiles.size(); i++) {
            if (!missiles[i].getEnabled()) {
                missile1 = &missiles[i];
                missile1->setState(ENTITY_STATE_SPAWN_TRANSITION);
                missile1->mType = 0;
                missile1->mPlayerSource = mPlayerAssignment;
                break;
            }
        }
        entityPlayerMissile* missile2 = nullptr;
        for (std::size_t i = 0; i < missiles.size(); i++) {
            if (!missiles[i].getEnabled()) {
                missile2 = &missiles[i];
                missile2->setState(ENTITY_STATE_SPAWN_TRANSITION);
                missile2->mType = 0;
                missile2->mPlayerSource = mPlayerAssignment;
                break;
            }
        }

        if (missile1 && missile2) {
            float angle = mathutils::calculate2dAngle(Point3d(0, 0, 0), fireAngle) + mathutils::DegreesToRads(90);

            float speed = p.weapon0MissileSpeed;
            float spread = p.weapon0Spread;
            float missileAngle1 = (angle + spread);
            float missileAngle2 = (angle - spread);

            {
                Point3d missilePos;
                Point3d missileSpeedVector(speed, 0, 0);
                Point3d missileOffsetVector(2, 0, 0);

                missilePos = this->getPos() + mathutils::rotate2dPoint(missileOffsetVector, missileAngle1);
                missileSpeedVector = mathutils::rotate2dPoint(missileSpeedVector, angle);

                missile1->setPos(missilePos);
                missile1->setAngle(angle - mathutils::DegreesToRads(90));
                missile1->setSpeed(missileSpeedVector + (playerSpeed * p.weapon0InheritSpeed));
                missile1->mVelocity = speed;
            }

            // ***************************************

            {
                Point3d missilePos;
                Point3d missileSpeedVector(speed, 0, 0);
                Point3d missileOffsetVector(2, 0, 0);

                missilePos = this->getPos() + mathutils::rotate2dPoint(missileOffsetVector, missileAngle2);
                missileSpeedVector = mathutils::rotate2dPoint(missileSpeedVector, angle);

                missile2->setPos(missilePos);
                missile2->setAngle(angle - mathutils::DegreesToRads(90));
                missile2->setSpeed(missileSpeedVector + (playerSpeed * p.weapon0InheritSpeed));
                missile2->mVelocity = speed;
            }

            // ***************************************
        }
    }
}

void player::firePattern2(const Point3d& fireAngle, const Point3d& playerSpeed)
{
    const auto& p = classical_params::get();

    // Powered-up heavy: denser double-tap bursts.
    const int power = weaponPower();
    if (power > 1) {
        if (--mFiringTimer <= 0) {
            static bool alt = true;
            alt = !alt;
            if (alt)
                mFiringTimer = p.weapon1IntervalA;
            else
                mFiringTimer = p.weapon1IntervalB;

            const float speedMul = 1.0f + (0.08f * (power - 1));
            if (alt) {
                // Denser spread burst.
                launchFan(fireAngle, playerSpeed, 1, power + 1, p.weapon1Spread * (0.5f + (0.15f * (power - 1))), p.weapon1MissileSpeed * speedMul, p.weapon1InheritSpeed);
            } else {
                // Central single heavy slug.
                launchFan(fireAngle, playerSpeed, 1, 1, 0.0f, p.weapon1MissileSpeed * speedMul, p.weapon1InheritSpeed);
            }
        }
        return;
    }

    if (--mFiringTimer <= 0) {
        static bool alternate = true;
        alternate = !alternate;
        if (alternate)
            mFiringTimer = p.weapon1IntervalA;
        else
            mFiringTimer = p.weapon1IntervalB;

        entityPlayerMissile* missile1 = nullptr;
        entityPlayerMissile* missile2 = nullptr;
        entityPlayerMissile* missile3 = nullptr;

        // Find an unused missile
        if (alternate) {
            for (std::size_t i = 0; i < missiles.size(); i++) {
                if (!missiles[i].getEnabled()) {
                    missile1 = &missiles[i];
                    missile1->setState(ENTITY_STATE_SPAWN_TRANSITION);
                    missile1->mType = 1;
                    missile1->mPlayerSource = mPlayerAssignment;
                    break;
                }
            }
            for (std::size_t i = 0; i < missiles.size(); i++) {
                if (!missiles[i].getEnabled()) {
                    missile2 = &missiles[i];
                    missile2->setState(ENTITY_STATE_SPAWN_TRANSITION);
                    missile2->mType = 1;
                    missile2->mPlayerSource = mPlayerAssignment;
                    break;
                }
            }
        } else {
            for (std::size_t i = 0; i < missiles.size(); i++) {
                if (!missiles[i].getEnabled()) {
                    missile3 = &missiles[i];
                    missile3->setState(ENTITY_STATE_SPAWN_TRANSITION);
                    missile3->mType = 1;
                    missile3->mPlayerSource = mPlayerAssignment;
                    break;
                }
            }
        }

        float angle = mathutils::calculate2dAngle(Point3d(0, 0, 0), fireAngle) + mathutils::DegreesToRads(90);
        float speed = p.weapon1MissileSpeed;

        if (missile3) {
            Point3d missilePos;
            Point3d missileSpeedVector(speed, 0, 0);
            Point3d missileOffsetVector(.5, 0, 0);

            missilePos = this->getPos() + mathutils::rotate2dPoint(missileOffsetVector, angle);
            missileSpeedVector = mathutils::rotate2dPoint(missileSpeedVector, angle);

            missile3->setPos(missilePos);
            missile3->setAngle(angle - mathutils::DegreesToRads(90));
            missile3->setSpeed(missileSpeedVector + (playerSpeed * p.weapon1InheritSpeed));
            missile3->mVelocity = speed;
        } else if (missile1 && missile2) {
            float spread = p.weapon1Spread;
            float missileAngle1 = (angle + spread);
            float missileAngle2 = (angle - spread);

            {
                Point3d missilePos;
                Point3d missileSpeedVector(speed, 0, 0);
                Point3d missileOffsetVector(.5, 0, 0);

                missilePos = this->getPos() + mathutils::rotate2dPoint(missileOffsetVector, missileAngle1);
                missileSpeedVector = mathutils::rotate2dPoint(missileSpeedVector, angle + .06);

                missile1->setPos(missilePos);
                missile1->setAngle(angle - mathutils::DegreesToRads(90));
                missile1->setSpeed(missileSpeedVector + (playerSpeed * p.weapon1InheritSpeed));
                missile1->mVelocity = speed;
            }

            // ***************************************

            {
                Point3d missilePos;
                Point3d missileSpeedVector(speed, 0, 0);
                Point3d missileOffsetVector(.5, 0, 0);

                missilePos = this->getPos() + mathutils::rotate2dPoint(missileOffsetVector, missileAngle2);
                missileSpeedVector = mathutils::rotate2dPoint(missileSpeedVector, angle - .06);

                missile2->setPos(missilePos);
                missile2->setAngle(angle - mathutils::DegreesToRads(90));
                missile2->setSpeed(missileSpeedVector + (playerSpeed * p.weapon1InheritSpeed));
                missile2->mVelocity = speed;
            }
        }
    }
}

void player::firePattern3(const Point3d& fireAngle, const Point3d& playerSpeed)
{
    const auto& p = classical_params::get();

    // Powered-up 5-way: a wider, denser fan.
    const int power = weaponPower();
    if (power > 1) {
        if (--mFiringTimer <= 0) {
            mFiringTimer = std::max(4, p.weapon2Interval - (power - 1));
            const float speedMul = 1.0f + (0.08f * (power - 1));
            const float half = 0.35f + (0.15f * (power - 1));
            launchFan(fireAngle, playerSpeed, 2, 5 + (2 * (power - 1)), half, p.weapon2MissileSpeed * speedMul, p.weapon2InheritSpeed);
        }
        return;
    }

    if (--mFiringTimer <= 0) {
        mFiringTimer = p.weapon2Interval;

        // Find 5 unused missiles
        entityPlayerMissile* missile0 = nullptr;
        entityPlayerMissile* missile1 = nullptr;
        entityPlayerMissile* missile2 = nullptr;
        entityPlayerMissile* missile3 = nullptr;
        entityPlayerMissile* missile4 = nullptr;

        for (std::size_t i = 0; i < missiles.size(); i++) {
            if (!missiles[i].getEnabled()) {
                missile0 = &missiles[i];
                missile0->setState(ENTITY_STATE_SPAWN_TRANSITION);
                missile0->mType = 2;
                missile0->mPlayerSource = mPlayerAssignment;
                break;
            }
        }
        for (std::size_t i = 0; i < missiles.size(); i++) {
            if (!missiles[i].getEnabled()) {
                missile1 = &missiles[i];
                missile1->setState(ENTITY_STATE_SPAWN_TRANSITION);
                missile1->mType = 2;
                missile1->mPlayerSource = mPlayerAssignment;
                break;
            }
        }
        for (std::size_t i = 0; i < missiles.size(); i++) {
            if (!missiles[i].getEnabled()) {
                missile2 = &missiles[i];
                missile2->setState(ENTITY_STATE_SPAWN_TRANSITION);
                missile2->mType = 2;
                missile2->mPlayerSource = mPlayerAssignment;
                break;
            }
        }
        for (std::size_t i = 0; i < missiles.size(); i++) {
            if (!missiles[i].getEnabled()) {
                missile3 = &missiles[i];
                missile3->setState(ENTITY_STATE_SPAWN_TRANSITION);
                missile3->mType = 2;
                missile3->mPlayerSource = mPlayerAssignment;
                break;
            }
        }
        for (std::size_t i = 0; i < missiles.size(); i++) {
            if (!missiles[i].getEnabled()) {
                missile4 = &missiles[i];
                missile4->setState(ENTITY_STATE_SPAWN_TRANSITION);
                missile4->mType = 2;
                missile4->mPlayerSource = mPlayerAssignment;
                break;
            }
        }

        if (missile0 && missile1 && missile2 && missile3 && missile4) {
            float angle = mathutils::calculate2dAngle(Point3d(0, 0, 0), fireAngle) + mathutils::DegreesToRads(90);

            float speed = p.weapon2MissileSpeed;
            float start1 = p.weapon2StartInner;
            float start2 = p.weapon2StartOuter;
            float spread1 = p.weapon2SpreadInner;
            float spread2 = p.weapon2SpreadOuter;
            float missileStart1 = (angle + start1);
            float missileStart2 = (angle - start1);
            float missileStart3 = (angle + start2);
            float missileStart4 = (angle - start2);
            float missileSpread1 = (angle + spread1);
            float missileSpread2 = (angle - spread1);
            float missileSpread3 = (angle + spread2);
            float missileSpread4 = (angle - spread2);

            // Missile 0 just fires out straight
            {
                Point3d missilePos;
                Point3d missileSpeedVector(speed, 0, 0);
                Point3d missileOffsetVector(2, 0, 0);

                missilePos = this->getPos() + mathutils::rotate2dPoint(missileOffsetVector, angle);
                missileSpeedVector = mathutils::rotate2dPoint(missileSpeedVector, angle);

                missile0->setPos(missilePos);
                missile0->setAngle(angle - mathutils::DegreesToRads(90));
                missile0->setSpeed(missileSpeedVector + (playerSpeed * p.weapon2InheritSpeed));
                missile0->mVelocity = speed;
            }

            // Missile 1 and 2 fire at a medium spread but aimed not far off from center

            {
                Point3d missilePos;
                Point3d missileSpeedVector(speed, 0, 0);
                Point3d missileOffsetVector(2, 0, 0);

                missilePos = this->getPos() + mathutils::rotate2dPoint(missileOffsetVector, missileStart1);
                missileSpeedVector = mathutils::rotate2dPoint(missileSpeedVector, missileSpread1);

                missile1->setPos(missilePos);
                missile1->setAngle(angle - mathutils::DegreesToRads(90));
                missile1->setSpeed(missileSpeedVector + (playerSpeed * p.weapon2InheritSpeed));
                missile1->mVelocity = speed;
            }

            {
                Point3d missilePos;
                Point3d missileSpeedVector(speed, 0, 0);
                Point3d missileOffsetVector(2, 0, 0);

                missilePos = this->getPos() + mathutils::rotate2dPoint(missileOffsetVector, missileStart2);
                missileSpeedVector = mathutils::rotate2dPoint(missileSpeedVector, missileSpread2);

                missile2->setPos(missilePos);
                missile2->setAngle(angle - mathutils::DegreesToRads(90));
                missile2->setSpeed(missileSpeedVector + (playerSpeed * p.weapon2InheritSpeed));
                missile2->mVelocity = speed;
            }

            // Missile 3 and 4 fire at a far spread

            {
                Point3d missilePos;
                Point3d missileSpeedVector(speed, 0, 0);
                Point3d missileOffsetVector(1, 0, 0);

                missilePos = this->getPos() + mathutils::rotate2dPoint(missileOffsetVector, missileStart3);
                missileSpeedVector = mathutils::rotate2dPoint(missileSpeedVector, missileSpread3);

                missile3->setPos(missilePos);
                missile3->setAngle(angle - mathutils::DegreesToRads(90));
                missile3->setSpeed(missileSpeedVector + (playerSpeed * p.weapon2InheritSpeed));
                missile3->mVelocity = speed;
            }

            {
                Point3d missilePos;
                Point3d missileSpeedVector(speed, 0, 0);
                Point3d missileOffsetVector(1, 0, 0);

                missilePos = this->getPos() + mathutils::rotate2dPoint(missileOffsetVector, missileStart4);
                missileSpeedVector = mathutils::rotate2dPoint(missileSpeedVector, missileSpread4);

                missile4->setPos(missilePos);
                missile4->setAngle(angle - mathutils::DegreesToRads(90));
                missile4->setSpeed(missileSpeedVector + (playerSpeed * p.weapon2InheritSpeed));
                missile4->mVelocity = speed;
            }
        }
    }
}

void player::firePattern4(const Point3d& fireAngle, const Point3d& playerSpeed)
{
    // Powered-up laser: fire `power` parallel piercing beams in a wall.
    const int power = weaponPower();
    if (power > 1) {
        constexpr float kBeamSpeed = 1.6f;
        constexpr float kBeamInherit = 0.5f;
        constexpr float kBeamGap = 2.0f;

        if (--mFiringTimer <= 0) {
            mFiringTimer = std::max(2, 4 - (power - 1));
            const float speed = kBeamSpeed * (1.0f + (0.06f * (power - 1)));
            const float angle = mathutils::calculate2dAngle(Point3d(0, 0, 0), fireAngle) + mathutils::DegreesToRads(90);

            Point3d fwd = mathutils::rotate2dPoint(Point3d(1, 0, 0), angle);
            Point3d side = mathutils::rotate2dPoint(Point3d(0, 1, 0), angle);

            for (int k = 0; k < power; ++k) {
                entityPlayerMissile* beam = allocMissile(3);
                if (!beam)
                    continue;

                const float off = (k - (power - 1) * 0.5f) * kBeamGap;
                Point3d missilePos = this->getPos() + (fwd * 1.0f) + (side * off);
                Point3d missileSpeedVector = mathutils::rotate2dPoint(Point3d(speed, 0, 0), angle);

                beam->setPos(missilePos);
                beam->setAngle(angle - mathutils::DegreesToRads(90));
                beam->setSpeed(missileSpeedVector + (playerSpeed * kBeamInherit));
                beam->mVelocity = speed;
            }
        }
        return;
    }

    // Laser: a fast, piercing straight beam. A bolt keeps going until it
    // leaves the grid, so one at a time is plenty.
    constexpr int kLaserInterval = 4;
    constexpr float kLaserSpeed = 1.6f;
    constexpr float kLaserOffset = 1.0f;
    constexpr float kLaserInherit = 0.5f;

    if (--mFiringTimer <= 0) {
        mFiringTimer = kLaserInterval;

        entityPlayerMissile* missile = nullptr;
        for (std::size_t i = 0; i < missiles.size(); i++) {
            if (!missiles[i].getEnabled()) {
                missile = &missiles[i];
                missile->setState(ENTITY_STATE_SPAWN_TRANSITION);
                missile->mType = 3;
                missile->mPlayerSource = mPlayerAssignment;
                break;
            }
        }

        if (missile) {
            float angle = mathutils::calculate2dAngle(Point3d(0, 0, 0), fireAngle) + mathutils::DegreesToRads(90);

            Point3d missilePos;
            Point3d missileSpeedVector(kLaserSpeed, 0, 0);
            Point3d missileOffsetVector(kLaserOffset, 0, 0);

            missilePos = this->getPos() + mathutils::rotate2dPoint(missileOffsetVector, angle);
            missileSpeedVector = mathutils::rotate2dPoint(missileSpeedVector, angle);

            missile->setPos(missilePos);
            missile->setAngle(angle - mathutils::DegreesToRads(90));
            missile->setSpeed(missileSpeedVector + (playerSpeed * kLaserInherit));
            missile->mVelocity = kLaserSpeed;
        }
    }
}

void player::fireHomingMissile(const Point3d& fireAngle, const Point3d& playerSpeed)
{
    // Secondary weapon: a single seeker launched toward the aim direction.
    // It steers itself at the nearest enemy (see entityPlayerMissile mType 4)
    // and self-destructs after a while if nothing is around to catch.
    constexpr float kHomingSpeed = 0.9f;
    constexpr float kHomingOffset = 1.6f;
    constexpr float kHomingInherit = 0.3f;
    constexpr int kHomingLife = 60 * 4; // ~4 seconds before self-destruct

    entityPlayerMissile* missile = nullptr;
    for (std::size_t i = 0; i < missiles.size(); i++) {
        if (!missiles[i].getEnabled()) {
            missile = &missiles[i];
            break;
        }
    }
    if (!missile)
        return;

    float angle = mathutils::calculate2dAngle(Point3d(0, 0, 0), fireAngle) + mathutils::DegreesToRads(90);

    missile->setState(ENTITY_STATE_SPAWN_TRANSITION);
    missile->mType = 4; // homing
    missile->mPlayerSource = mPlayerAssignment;

    Point3d missilePos;
    Point3d missileSpeedVector(kHomingSpeed, 0, 0);
    Point3d missileOffsetVector(kHomingOffset, 0, 0);

    missilePos = this->getPos() + mathutils::rotate2dPoint(missileOffsetVector, angle);
    missileSpeedVector = mathutils::rotate2dPoint(missileSpeedVector, angle);

    missile->setPos(missilePos);
    missile->setAngle(angle - mathutils::DegreesToRads(90));
    missile->setSpeed(missileSpeedVector + (playerSpeed * kHomingInherit));
    missile->mVelocity = kHomingSpeed;
    missile->mLife = kHomingLife;
}

void player::destroyTransition()
{
    entity::destroyTransition();

    mStateTimer = mDestroyTime;

    // Reset the multipler stuff
    mMultiplier = 1;
    mKillCounter = 0;

    // Shield charges (Endless) are also spent on death.
    mShieldCharges = 0;

    attractor::Attractor* att = theGame->mAttractors->getAttractor();
    if (att) {
        att->strength = 200;
        att->radius = 30;
        att->pos = mPos;
        att->enabled = true;
        att->attractsParticles = true;
    }

    // Throw out some particles
    Point3d pos(this->mPos);
    Point3d angle(0, 0, 0);
    float speed = 2.0;
    float spread = 2 * PI;
    int num = 500;
    int timeToLive = 200;
    vector::pen pen = mPen;
    pen.r *= 1.2;
    pen.g *= 1.2;
    pen.b *= 1.2;
    pen.a = 200;
    pen.lineRadius = 5;
    theGame->mParticles->emitter(&pos, &angle, speed, spread, num, &pen, timeToLive, true, true, .97, true);

    setState(ENTITY_STATE_DESTROYED);

    theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE1);
    theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE2);
    theGame->mSound->stopTrack(SOUNDID_PLAYERFIRE3);

    if (getNumLives() <= 1) {
        theGame->endGame();
    } else
        theGame->mSound->playTrack(SOUNDID_PLAYERHIT);
}

void player::destroy()
{
    if (--mStateTimer <= 0) {
        setState(ENTITY_STATE_INACTIVE);
    }
}

void player::addKillAtLocation(int points, Point3d pos)
{
    const auto& p = classical_params::get();

    int pointsEarned = points * mMultiplier;
    mScore += pointsEarned;
    mWeaponCounter += pointsEarned;
    mBombCounter += pointsEarned;
    mLifeCounter += pointsEarned;

    /*
    For every enemy you destroy you receive that enemy's base score times your
    multiplier. Your multiplier begins at x1. After destroying 25 enemies in
    one life your multiplier becomes x2. At 50 enemies x3, and so on, up to
    x10 with 2,000 enemies destroyed in one life. If you die your multiplier
    returns to zero.

    Every 10,000 points there is a chance your main weapon will change. Which
    weapon you receive is random, but if you receive the same weapon you are
    currently using then it's like nothing happened.

    You will receive an extra life every 75,000 point interval on the score.

    You will receive an extra bomb every 100,000 point interval on the score.
    */

    if (mWeaponCounter >= p.playerWeaponChangeScore) {
        mWeaponCounter = 0;
        // Weapon changes at score milestones are a Classical rule. Modes with
        // pickup-driven upgrades (Endless) leave weapon choice to the player.
        gameplay_mode* mode = theGame->activeMode();
        if (!mode || mode->weapon_auto_advances()) {
            switchWeapons();
        }
    }

    if (mBombCounter >= p.playerExtraBombScore) {
        mBombCounter = 0;
        addBomb();
        theGame->mSound->playTrack(SOUNDID_EXTRABOMB);
    }
    if (mLifeCounter >= p.playerExtraLifeScore) {
        mLifeCounter = 0;
        addLife();
        theGame->mSound->playTrack(SOUNDID_EXTRALIFE);
    }

    bool showMultiplier = false;

    ++mKillCounter;
    if (mKillCounter >= p.playerMultiplierKillCount) {
        // Increment the multiplier and display a message
        mKillCounter = 0;
        if (mMultiplier < p.playerMultiplierMax) {
            ++mMultiplier;
            showMultiplier = true;
        }
    }

    vector::pen pen = this->getFontPen();

    if (showMultiplier) {
        // Show the multiplier message
        char message[256];
        snprintf(message, sizeof(message), "Multiplier x%d", mMultiplier);
        game::showMessageAtLocation(message, pos, pen);
        theGame->mSound->playTrack(SOUNDID_MULTIPLIERADVANCE);
    } else {
        // Just display the point value
        char message[128];
        snprintf(message, sizeof(message), "%d", pointsEarned);
        game::showMessageAtLocation(message, pos, pen);
    }
}

int player::getNumLives()
{
    if (theGame->numPlayers() == 1)
        return mNumLives;
    // Co-op: lives come from the active mode's shared pool.
    gameplay_mode* mode = theGame->activeMode();
    return mode ? mode->shared_lives() : 0;
}

int player::getNumBombs()
{
    if (theGame->numPlayers() == 1)
        return mNumBombs;
    // Co-op: bombs come from the active mode's shared pool.
    gameplay_mode* mode = theGame->activeMode();
    return mode ? mode->shared_bombs() : 0;
}

void player::addLife()
{
    if (theGame->numPlayers() == 1)
        ++mNumLives;
    else if (gameplay_mode* mode = theGame->activeMode())
        mode->add_shared_life();
}

void player::takeLife()
{
    if (theGame->numPlayers() == 1)
        --mNumLives;
    else if (gameplay_mode* mode = theGame->activeMode())
        mode->take_shared_life();

    if (mNumLives < 0)
        mNumLives = 0;
}

void player::addBomb()
{
    if (theGame->numPlayers() == 1)
        ++mNumBombs;
    // No bombs on 2 player game
}

void player::takeBomb()
{
    if (theGame->numPlayers() == 1)
        --mNumBombs;
    // No bombs on 2 player game
}

void player::addPointsNoMultiplier(int points)
{
    int pointsEarned = points;
    mScore += pointsEarned;
    mBombCounter += pointsEarned;
    mLifeCounter += pointsEarned;
}

void player::switchWeapons()
{
    const auto& p = classical_params::get();

    if (mCurrentWeapon == 0) {
        mCurrentWeapon = 1;
    } else {
        mCurrentWeapon = (mathutils::frandFrom0To1() * 100) < p.playerWeapon1Chance ? 1 : 2;
    }
}

void player::setWeapon(int weapon)
{
    if (weapon < WEAPON_TWIN)
        weapon = WEAPON_TWIN;
    if (weapon > WEAPON_LASER)
        weapon = WEAPON_LASER;
    mCurrentWeapon = weapon;
}

int player::weaponPower(int weapon) const
{
    if (weapon < WEAPON_TWIN || weapon >= WEAPON_COUNT)
        return 1;
    return mWeaponPower[weapon];
}

void player::upgradeWeapon()
{
    // Raise the power level of the currently equipped weapon, not the type.
    if (mWeaponPower[mCurrentWeapon] < WEAPON_POWER_MAX) {
        ++mWeaponPower[mCurrentWeapon];
    }
}

entityPlayerMissile* player::allocMissile(int type)
{
    for (auto& missile : missiles) {
        if (!missile.getEnabled()) {
            missile.setState(ENTITY_STATE_SPAWN_TRANSITION);
            missile.mType = type;
            missile.mPlayerSource = mPlayerAssignment;
            return &missile;
        }
    }
    return nullptr;
}

void player::launchFan(const Point3d& fireAngle, const Point3d& playerSpeed, int type, int count, float halfAngle, float speed, float inherit)
{
    const float center = mathutils::calculate2dAngle(Point3d(0, 0, 0), fireAngle) + mathutils::DegreesToRads(90);
    const float spreadStep = (count > 1) ? ((2.0f * halfAngle) / (count - 1)) : 0.0f;

    for (int i = 0; i < count; ++i) {
        entityPlayerMissile* missile = allocMissile(type);
        if (!missile)
            continue;

        const float missileAngle = center - halfAngle + (spreadStep * i);

        Point3d missilePos;
        Point3d missileSpeedVector(speed, 0, 0);
        Point3d missileOffsetVector(2, 0, 0);

        missilePos = this->getPos() + mathutils::rotate2dPoint(missileOffsetVector, missileAngle);
        missileSpeedVector = mathutils::rotate2dPoint(missileSpeedVector, missileAngle);

        missile->setPos(missilePos);
        missile->setAngle(mathutils::wrapRadians(missileAngle - mathutils::DegreesToRads(90)));
        missile->setSpeed(missileSpeedVector + (playerSpeed * inherit));
        missile->mVelocity = speed;
    }
}

void player::addShieldCharges(int amount)
{
    mShieldCharges += amount;
    if (mShieldCharges > maxShieldCharges())
        mShieldCharges = maxShieldCharges();
}

bool player::useShieldCharge()
{
    if (mShieldCharges <= 0)
        return false;
    --mShieldCharges;
    return true;
}
