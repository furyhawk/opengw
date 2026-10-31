#pragma once

#include "entities/entity.hpp"

class entityDart : public entity
{
  public:
    entityDart();

    void run() override;
    void spawnTransition() override;
};
