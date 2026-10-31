#pragma once

#include "entities/entity.hpp"

class game;

class entityBoss : public entity
{
  public:
    entityBoss(const game& gameRef);

    void run() override;
    void spawnTransition() override;

  private:
    const game& mGame;
    float mPulse { 0.0f };
};
