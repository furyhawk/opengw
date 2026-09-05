#pragma once

#include "entities/entity.hpp"

class entityWeaver : public entity
{
  public:
    entityWeaver();

    void run() override;
    void spawnTransition() override;

    float mAnimationIndex;
};
