#include "entities/attractor.hpp"
#include "entities/blackholes.hpp"
#include "entities/enemies.hpp"
#include "entities/entityblackhole.hpp"
#include "core/game.hpp"
#include "entities/players.hpp"

#include <algorithm>
#include <vector>

blackholes::blackholes()
{
}

void blackholes::run()
{
    std::vector<entity*> activeEnemies;
    activeEnemies.reserve(NUM_ENEMIES);
    for (int i = 0; i < NUM_ENEMIES; ++i) {
        entity* enemy = theGame->mEnemies->mEnemies[i];
        if ((enemy->getState() == entity::ENTITY_STATE_RUNNING) && (enemy->getType() != entity::ENTITY_TYPE_UNDEF)) {
            activeEnemies.push_back(enemy);
        }
    }

    // Suck enemies and particles into the black holes.
    for (int i = 0; i < NUM_ENEMIES; i++) {
        entity* blackHoleEntity = theGame->mEnemies->mEnemies[i];
        if ((blackHoleEntity->getType() != entity::ENTITY_TYPE_BLACKHOLE) || (blackHoleEntity->getState() != entity::ENTITY_STATE_RUNNING)) {
            continue;
        }

        entityBlackHole* blackHole = static_cast<entityBlackHole*>(blackHoleEntity);
        if (!blackHole->mActivated) {
            continue;
        }

        const Point3d blackHolePos = blackHole->getPos();
        const float blackHoleRadius = blackHole->getRadius();
        const float blackHoleRadiusSq = blackHoleRadius * blackHoleRadius;

        // Players
        for (int p = 0; p < 2; p++) {
            entity* player = (p == 0) ? theGame->getPlayer1() : theGame->getPlayer2();
            if (player->getEnabled()) {
                const Point3d delta = player->getPos() - blackHolePos;
                const float distanceSquared = (delta.x * delta.x) + (delta.y * delta.y);
                if (distanceSquared < blackHoleRadiusSq) {
                    // Destroy the player
                } else {
                    float distance = std::sqrt(distanceSquared);
                    const float strength = 2.4f;
                    if (distance < blackHoleRadius) {
                        distance = blackHoleRadius;
                    }

                    float r = 1.0f / (distance * distance);

                    Point3d gravityVector(r * strength, 0, 0);
                    float angle = mathutils::calculate2dAngle(player->getPos(), blackHolePos);
                    Point3d g = mathutils::rotate2dPoint(gravityVector, angle);

                    Point3d speed = player->getDrift();
                    speed.x += g.x;
                    speed.y += g.y;
                    player->setDrift(speed);
                }
            }
        }

        for (entity* enemy : activeEnemies) {
            if (enemy == blackHoleEntity) {
                continue;
            }

            const Point3d delta = enemy->getPos() - blackHolePos;
            const float distanceSquared = (delta.x * delta.x) + (delta.y * delta.y);
            const float angle = mathutils::calculate2dAngle(enemy->getPos(), blackHolePos);
            const float distance = std::sqrt(distanceSquared);

            if (enemy->getType() == entity::ENTITY_TYPE_BLACKHOLE) {
                // It's another black hole. Keep it at the proper distance or combine it.
                entityBlackHole* blackHole2 = static_cast<entityBlackHole*>(enemy);
                const float totalRadius = blackHoleRadius + blackHole2->getRadius();
                if (distance < 6.0f) {
                    Point3d avgPos = (blackHolePos + blackHole2->getPos()) * .5f;
                    float strength = std::max(blackHole->mStrength, blackHole2->mStrength);
                    int feedCount = blackHole->mFeedCount + blackHole2->mFeedCount;

                    blackHole->setPos(avgPos);
                    blackHole->mStrength = strength;
                    blackHole->mFeedCount = feedCount;
                    blackHole->feed(blackHole2->mPoints);
                    blackHole->feed(0);

                    blackHole2->setState(entity::ENTITY_STATE_DESTROY_TRANSITION);
                    theGame->mSound->playTrack(SOUNDID_GRAVITYWELLHIT);
                } else if (distance < totalRadius) {
                    float strength = 6;
                    float safeDistance = std::max(distance, blackHoleRadius);
                    float r = 1.0f / (safeDistance * safeDistance);

                    Point3d gravityVector(r * -strength, 0, 0);
                    Point3d g = mathutils::rotate2dPoint(gravityVector, angle);

                    Point3d speed = enemy->getDrift();
                    speed.x += g.x;
                    speed.y += g.y;
                    enemy->setDrift(speed);
                }
            }

            if (distance < blackHoleRadius) {
                Point3d drift = enemy->getDrift();
                drift *= .25f;
                blackHole->setDrift(blackHole->getDrift() + drift);

                enemy->hit(blackHole);
                blackHole->feed(enemy->getScoreValue());

                attractor::Attractor* att = theGame->mAttractors->getAttractor();
                if (att) {
                    att->strength = 1.5f;
                    att->radius = 30;
                    att->pos = blackHolePos;
                    att->enabled = true;
                    att->attractsParticles = false;
                }
            } else {
                float strength = 16;
                float safeDistance = std::max(distance, blackHoleRadius);
                float r = 1.0f / (safeDistance * safeDistance);

                Point3d gravityVector(r * strength, 0, 0);
                Point3d g = mathutils::rotate2dPoint(gravityVector, angle + .4f);

                Point3d speed = enemy->getDrift();
                speed.x += g.x;
                speed.y += g.y;
                enemy->setDrift(speed);
            }
        }
    }

    // Kill any particles that fly into the black hole
    /*
        for (int i=0; i<NUM_ENEMIES; i++)
        {
            if ((game::mEnemies.mEnemies[i]->getType() == entity::ENTITY_TYPE_BLACKHOLE) && (game::mEnemies.mEnemies[i]->getState() == entity::ENTITY_STATE_RUNNING))
            {
                entityBlackHole* blackHole = static_cast<entityBlackHole*>(game::mEnemies.mEnemies[i]);
                if (blackHole->mActivated)
                {
                    for (int p=0; p<game::mParticles.mNumParticles; p++)
                    {
                        if (game::mParticles.mParticles[p].timeToLive > 0)
                        {
                            // This particle is active
                            particle::PARTICLE* particle = &game::mParticles.mParticles[p];
                            if (particle->gravity)
                            {
                                float distance = mathutils::calculate2dDistance(particle->posStream[0], blackHole->getPos());
                                if (distance < blackHole->getRadius())
                                {
                                    particle->timeToLive = 0;
                                }
                            }
                        }
                    }
                }
            }
        }
    */
}
