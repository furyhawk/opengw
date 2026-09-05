#pragma once

#include "entities/entity.hpp"

class entityProton : public entity
{
  public:
    entityProton(void);

    void run() override;
    void spawnTransition() override;

  private:
    void drawRing();
};
