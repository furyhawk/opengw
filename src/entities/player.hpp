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

    // ---- Selectable weapon ladder (shared with the Endless mode) ----------
    // Weapon 0 "twin", 1 "heavy alternating" and 2 "5-way" are the original
    // three; 3 is the Endless-mode laser. Classical advances/rerolls among
    // 0..2 at score milestones; Endless hands out upgrades as pickups.
    enum
    {
        WEAPON_TWIN = 0,
        WEAPON_HEAVY,
        WEAPON_FIVE_WAY,
        WEAPON_LASER,
        WEAPON_COUNT
    };

    int getWeapon() const { return mCurrentWeapon; }
    int maxWeapon() const { return WEAPON_LASER; }
    void setWeapon(int weapon);
    // Advance to the next weapon on the ladder (up to WEAPON_LASER).
    void upgradeWeapon();

    // ---- Shield charges (Endless "shield upgrade") ------------------------
    // Each charge absorbs one enemy collision without destroying the player
    // (see players.cpp). Charges are lost on death. Modes that don't hand
    // these out (Classical) simply keep zero, so behaviour is unchanged.
    int shieldCharges() const { return mShieldCharges; }
    int maxShieldCharges() const { return 3; }
    void addShieldCharges(int amount);
    bool useShieldCharge();

    void addPointsNoMultiplier(int points);

    vector::pen getExhaustPen() const { return mExhaustPen; }
    vector::pen getMissilesPen() const { return mMissilesPen; }
    vector::pen getFontPen() const { return mFontPen; }

  protected:
    void firePattern1(const Point3d& fireAngle, const Point3d& playerSpeed);
    void firePattern2(const Point3d& fireAngle, const Point3d& playerSpeed);
    void firePattern3(const Point3d& fireAngle, const Point3d& playerSpeed);
    void firePattern4(const Point3d& fireAngle, const Point3d& playerSpeed); // laser

    int mCurrentWeapon;

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

    float mExhaustSpreadIndex;

    vector::pen mExhaustPen;
    vector::pen mMissilesPen;
    vector::pen mFontPen;
};
