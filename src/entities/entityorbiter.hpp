#pragma once

#include "entities/entity.hpp"

class entityOrbiter : public entity
{
  public:
    entityOrbiter();

    void run() override;
    void spawnTransition() override;

    float mOrbitAngle { 0.0f };
    float mOrbitRadius { 12.0f };
};
