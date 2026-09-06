#pragma once

#include "entities/entity.hpp"
#include "entities/entityplayermissile.hpp"

#include <vector>

class player : public entity
{
  public:
    player();
    ~player();

    virtual void initPlayerForGame();
    virtual void deinitPlayerForGame();

    void run() override;
    void spawn() override;
    void spawnTransition() override;
    void destroyTransition() override;
    void destroy() override;
    void indicating() override;

    void draw() override;

    std::vector<entityPlayerMissile> missiles;

    int mScore;
    int mMultiplier;

    int mPlayerAssignment;

    bool mJoined;

    void addKillAtLocation(int points, Point3d pos);

    void runMissiles();

    bool shields() { return mSheildTimer > 0; }

    int getNumLives();
    int getNumBombs();

    void addLife();
    void takeLife();
    void addBomb();
    void takeBomb();
    void switchWeapons();

    // ---- Selectable weapons & per-weapon power levels ---------------------
    // There are four weapon types (WEAPON_*). Each type keeps its own power
    // level from 1 up to WEAPON_POWER_MAX (5): a higher level fires more
    // bullets and a touch faster (see the firePattern* implementations).
    // Classical keeps every weapon at level 1 and only rerolls the *type* at
    // score milestones (switchWeapons). Endless never rerolls by score;
    // "WEAPON UPGRADE" pickups raise the power of the equipped weapon, and the
    // "LASER" pickup switches the equipped type to the laser.
    enum
    {
        WEAPON_TWIN = 0,
        WEAPON_HEAVY,
        WEAPON_FIVE_WAY,
        WEAPON_LASER,
        WEAPON_COUNT,
        WEAPON_POWER_MAX = 5 // maximum power level of any weapon
    };

    // Equipped weapon type (0..WEAPON_LASER).
    int getWeapon() const { return mCurrentWeapon; }
    void setWeapon(int weapon); // switch equipped type (keeps its own power)

    // Power level (1..WEAPON_POWER_MAX) of the equipped weapon.
    int weaponPower() const { return weaponPower(mCurrentWeapon); }
    int weaponPower(int weapon) const;
    int weaponPowerMax() const { return WEAPON_POWER_MAX; }
    // Raise the equipped weapon's power level by one (up to WEAPON_POWER_MAX).
    void upgradeWeapon();

    // ---- Shield charges (Endless "shield upgrade") ------------------------
    // Each charge absorbs one enemy collision without destroying the player
    // (see players.cpp). Charges are lost on death. Modes that don't hand
    // these out (Classical) simply keep zero, so behaviour is unchanged.
    int shieldCharges() const { return mShieldCharges; }
    int maxShieldCharges() const { return 3; }
    void addShieldCharges(int amount);
    bool useShieldCharge();

    // ---- Secondary weapon: homing missiles --------------------------------
    // Granted as a power-up in Endless. While the player is firing, homing
    // missiles auto-launch alongside the main weapon and steer at enemies.
    bool hasHomingMissiles() const { return mHasHoming; }
    void enableHomingMissiles() { mHasHoming = true; }

    void addPointsNoMultiplier(int points);

    vector::pen getExhaustPen() const { return mExhaustPen; }
    vector::pen getMissilesPen() const { return mMissilesPen; }
    vector::pen getFontPen() const { return mFontPen; }

  protected:
    void firePattern1(const Point3d& fireAngle, const Point3d& playerSpeed);
    void firePattern2(const Point3d& fireAngle, const Point3d& playerSpeed);
    void firePattern3(const Point3d& fireAngle, const Point3d& playerSpeed);
    void firePattern4(const Point3d& fireAngle, const Point3d& playerSpeed); // laser
    void fireHomingMissile(const Point3d& fireAngle, const Point3d& playerSpeed);

    // Reserve the next free missile of the given subtype (0 twin, 1 heavy,
    // 2 5-way, 3 laser, 4 homing) and mark it as about to spawn.
    entityPlayerMissile* allocMissile(int type);
    // Fire `count` missiles of `type` spread across +-halfAngle around the
    // aim direction, each aimed along its own angle. Used by powered-up
    // weapons (power > 1).
    void launchFan(const Point3d& fireAngle, const Point3d& playerSpeed, int type, int count, float halfAngle, float speed, float inherit);

    int mCurrentWeapon;

    // Power level (1..WEAPON_POWER_MAX) of each weapon type.
    int mWeaponPower[WEAPON_COUNT] { 1, 1, 1, 1 };

    int mWeaponCounter;
    int mBombCounter;
    int mLifeCounter;

    bool mDrawSheild;

    int mFiringTimer;
    int mSheildTimer;
    int mBombInterimTimer = 0;

    int mKillCounter;
    int mLevelAdvanceCounter;

    int mNumLives;
    int mNumBombs;

    int mShieldCharges { 0 };

    // Secondary weapon: homing missile state.
    bool mHasHoming { false };
    int mHomingTimer { 0 };

    float mExhaustSpreadIndex;

    vector::pen mExhaustPen;
    vector::pen mMissilesPen;
    vector::pen mFontPen;
};
