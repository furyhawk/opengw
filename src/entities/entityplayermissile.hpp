#pragma once

#include "entities/entity.hpp"

class entityPlayerMissile : public entity
{
  public:
    entityPlayerMissile();

    void run() override;
    void spawnTransition() override;
    void destroyTransition() override;

    void draw() override;

    // Subtype: 0 twin, 1 heavy, 2 five-way, 3 laser, 4 homing.
    int mType; // TODO: entity class also has mType
    int mPlayerSource; // 0..3 = owning player
    float mVelocity { 0.0f };
    int mLife { 0 }; // >0 = frames left before the missile self-destructs

  private:
    void syncPenFromPlayer();
    void drawLaser();
    void drawHomingMissile();
    void drawStandardMissile();
    void drawLaunchFlash();
    void emitExhaustParticle();
    void emitImpactSparks(const Point3d& pos, int count);

    Point3d mLastPos;

    // Visual-only state. mAnimTick advances once per logic frame (unlike
    // draw(), which runs once per render pass) so both the primary and glow
    // passes see the same animation phase.
    int mAnimTick { 0 };
    int mFlashTimer { 0 }; // frames left on the launch flash
};
