#include "entities/bomb.hpp"
#include "core/classicalparams.hpp"
#include "math/defines.hpp"
#include "entities/spawner.hpp"
#include "entities/enemies.hpp"
#include "core/game.hpp"
#include "render/grid.hpp"
#include "entities/players.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

spawner::spawner(void)
{
}

void spawner::init()
{
    const auto& p = classical_params::get();

    mNumWavesAllowed = 0;
    mSpawnIndex = 0;
    mLastSpawnIndex = 0;

    mSpawnCheckTimer = p.spawnerScatterInterval;
    mSpawnWaitTimer = p.spawnerRespawnWaitTimer;
    mWaveStartTimer = 0;

    clearWaveData();
    transition();
}

int spawner::getSpawnIndex() const
{
    return floor(mSpawnIndex);
}

void spawner::accelerateSpawnIndex(float amount)
{
    const auto& p = classical_params::get();

    if (amount <= 0 || getSpawnIndex() >= p.spawnerMaxIndex)
        return;

    mSpawnIndex += amount;
    if (mSpawnIndex > p.spawnerMaxIndex)
        mSpawnIndex = p.spawnerMaxIndex;

    if (getSpawnIndex() > mLastSpawnIndex) {
        mLastSpawnIndex = getSpawnIndex();
        mSpawnProgress = mSpawnIndex / p.spawnerMaxIndex;
        if (mSpawnProgress > 1)
            mSpawnProgress = 1;
        transition();
    }
}

void spawner::run(void)
{
    const auto& p = classical_params::get();

    // Update our spawn index
    if (getSpawnIndex() < p.spawnerMaxIndex) {
        mSpawnIndex += p.spawnerIndexRate;
        if (mSpawnIndex > p.spawnerMaxIndex)
            mSpawnIndex = p.spawnerMaxIndex;
        if (getSpawnIndex() > mLastSpawnIndex) {
            mLastSpawnIndex = getSpawnIndex();
            mSpawnProgress = mSpawnIndex / p.spawnerMaxIndex;
            if (mSpawnProgress > 1)
                mSpawnProgress = 1;
            transition();
        }
    }

    int numPlayersActive = 0;
    if (theGame->getPlayer1()->mJoined) {
        if (theGame->getPlayer1()->getState() == entity::ENTITY_STATE_RUNNING)
            ++numPlayersActive;
    }
    if (theGame->getPlayer2()->mJoined) {
        if (theGame->getPlayer2()->getState() == entity::ENTITY_STATE_RUNNING)
            ++numPlayersActive;
    }
    if (theGame->getPlayer3()->mJoined) {
        if (theGame->getPlayer3()->getState() == entity::ENTITY_STATE_RUNNING)
            ++numPlayersActive;
    }
    if (theGame->getPlayer4()->mJoined) {
        if (theGame->getPlayer4()->getState() == entity::ENTITY_STATE_RUNNING)
            ++numPlayersActive;
    }

    // Respawn the players as needed

    static int player1SpawnTimer = 0;
    static int player2SpawnTimer = 0;
    static int player3SpawnTimer = 0;
    static int player4SpawnTimer = 0;

    // Monitor players and respawn as needed
    for (int i = 0; i < 4; i++) {
        player* player;
        int* timer;
        switch (i) {
        case 0:
            player = theGame->getPlayer1();
            timer = &player1SpawnTimer;
            break;
        case 1:
            player = theGame->getPlayer2();
            timer = &player2SpawnTimer;
            break;
        case 2:
            player = theGame->getPlayer3();
            timer = &player3SpawnTimer;
            break;
        case 3:
            player = theGame->getPlayer4();
            timer = &player4SpawnTimer;
            break;
        }

        if (player->mJoined && (player->getState() == entity::ENTITY_STATE_INACTIVE)) {
            if (theGame->numPlayers() == 1) {
                mSpawnWaitTimer = p.spawnerRespawnWaitTimer;
            }

            if (++(*timer) >= p.spawnerRespawnDelay) {
                *timer = 0;

                player->takeLife();
                if (player->getNumLives() > 0) {
                    player->setState(entity::ENTITY_STATE_SPAWN_TRANSITION);
                } else {
                    if (theGame->numPlayers() == 1) {
                        game::mGameMode = game::GAMEMODE_GAMEOVER_TRANSITION;
                    } else {
                        // Multiplayer
                        // Wait until no more players are active
                        if (numPlayersActive == 0) {
                            game::mGameMode = game::GAMEMODE_GAMEOVER_TRANSITION;
                        }
                    }
                }
            }
        }
    }

    if (mSpawnWaitTimer > 0)
        --mSpawnWaitTimer;

    if (!numPlayersActive) {
        mWaveStartTimer = 0;
    }

    /*
        if (numWaveData() == 0)
        {
            newWave(WAVETYPE_SWARM, entity::ENTITY_TYPE_BLACKHOLE, 1);
        }
        runWaves();
        return;
    */

    if ((mSpawnWaitTimer <= 0) && numPlayersActive && !theGame->mBomb->isBombing()) {
        //
        // Randomly spawn enemies here and there
        //
        int index = getSpawnIndex();

        if ((++mSpawnCheckTimer > p.spawnerScatterInterval) || (index > p.spawnerScatterEveryFrameIndex)) {
            mSpawnCheckTimer = 0;

            // Wanderers
            if (index <= 1)
                spawnEntities(entity::ENTITY_TYPE_WANDERER, p.spawnerWandererEarly);
            else if (index > 1 && index < 3)
                spawnEntities(entity::ENTITY_TYPE_WANDERER, p.spawnerWandererMid);

            // Grunts
            spawnEntities(entity::ENTITY_TYPE_GRUNT, (index <= 1) ? p.spawnerGruntEarly : p.spawnerGruntLate);

            // Spinners
            if (index >= 1) {
                spawnEntities(entity::ENTITY_TYPE_SPINNER, p.spawnerSpinnerScatter);
            }

            // Weavers
            if (index >= 1) {
                spawnEntities(entity::ENTITY_TYPE_WEAVER, p.spawnerWeaverScatter);
            }

            // Black holes
            if (index >= p.spawnerBlackholeIndex) {
                if (mathutils::frandFrom0To1() * 100 < p.spawnerBlackholeChance) {
                    spawnEntities(entity::ENTITY_TYPE_BLACKHOLE, 1);
                }
            }
            /*
                        if (game::mEnemies.getNumActiveEnemiesOfType(entity::ENTITY_TYPE_BLACKHOLE) == 0)
                        {
                            spawnEntities(entity::ENTITY_TYPE_BLACKHOLE, 1);
                        }
            */
        }

        mNumWavesAllowed = 0;
        if (index > p.spawnerWaveCap1Index) {
            mNumWavesAllowed = 1;
        }
        if (index > p.spawnerWaveCap2Index) {
            mNumWavesAllowed = 2;
        }
        if (index > p.spawnerWaveUnlimitedIndex) {
            // Make it rain!!
            mNumWavesAllowed = 9999;
        }

        if ((++mWaveStartTimer >= p.spawnerWaveCadence) && (numWaveData() < mNumWavesAllowed)) {
            mWaveStartTimer = 0;

            switch ((int)(mathutils::frandFrom0To1() * p.spawnerWaveChoices)) {
            //
            // SWARM TYPE
            //
            case 0:
                newWave(WAVETYPE_SWARM, entity::ENTITY_TYPE_GRUNT, std::max(p.waveMinGrunt, (int)ceil(p.wavePopGrunt * mSpawnProgress)));
                break;
            case 1:
                newWave(WAVETYPE_SWARM, entity::ENTITY_TYPE_WEAVER, std::max(p.waveMinWeaver, (int)ceil(p.wavePopWeaver * mSpawnProgress)));
                break;
            case 2:
                if (index > p.waveSnakeIndex) {
                    newWave(WAVETYPE_SWARM, entity::ENTITY_TYPE_SNAKE, std::max(p.waveMinSnake, (int)ceil(p.wavePopSnake * mSpawnProgress)));
                }
                break;
            case 3:
                newWave(WAVETYPE_SWARM, entity::ENTITY_TYPE_SPINNER, std::max(p.waveMinSpinner, (int)ceil(p.wavePopSpinner * mSpawnProgress)));
                break;
            case 4:
                if (index > p.waveBlackholeIndex) {
                    newWave(WAVETYPE_SWARM, entity::ENTITY_TYPE_BLACKHOLE, std::max(p.waveMinBlackhole, (int)ceil(mathutils::frandFrom0To1() * p.wavePopBlackhole * mSpawnProgress)));
                }
                break;
            case 5:
                if (index > p.waveMayflyIndex) {
                    newWave(WAVETYPE_SWARM, entity::ENTITY_TYPE_MAYFLY, std::max(p.waveMinMayfly, (int)ceil(p.wavePopMayfly * mSpawnProgress)));
                }
                break;
            //
            // RUSH TYPE
            //
            case 6:
                newWave(WAVETYPE_RUSH, entity::ENTITY_TYPE_GRUNT, std::max(p.waveMinGrunt, (int)ceil(p.wavePopGrunt * mSpawnProgress) / p.waveRushDivide));
                break;
            case 7:
                newWave(WAVETYPE_RUSH, entity::ENTITY_TYPE_WEAVER, std::max(p.waveMinWeaver, (int)ceil(p.wavePopWeaver * mSpawnProgress) / p.waveRushDivide));
                break;
            case 8:
                if (index > p.waveSnakeIndex) {
                    newWave(WAVETYPE_RUSH, entity::ENTITY_TYPE_SNAKE, std::max(p.waveMinSnake, (int)ceil(p.wavePopSnake * mSpawnProgress) / p.waveRushDivide));
                }
                break;
            case 9:
                newWave(WAVETYPE_RUSH, entity::ENTITY_TYPE_SPINNER, std::max(p.waveMinSpinner, (int)ceil(p.wavePopSpinner * mSpawnProgress) / p.waveRushDivide));
                break;
            case 10:
                //                    newWave(WAVETYPE_RUSH, entity::ENTITY_TYPE_BLACKHOLE, ceil(mathutils::frandFrom0To1() * numEnemyBlackHole * mSpawnProgress) / 2);
                break;
            case 11:
                if (index > p.waveRepulsorIndex) {
                    newWave(WAVETYPE_RUSH, entity::ENTITY_TYPE_REPULSOR, ceil(mathutils::frandFrom0To1() * p.wavePopRepulsor * mSpawnProgress) / p.waveRushDivide);
                }
                break;
            }
        }

        runWaves();
    } else {
        mSpawnCheckTimer = 0;
    }

    // Run the mayfly sound loop
    // This is a dumb place for this to live
    if (theGame->mEnemies->getNumActiveEnemiesOfType(entity::ENTITY_TYPE_MAYFLY) > 0) {
        if (!theGame->mSound->isTrackPlaying(SOUNDID_MAYFLIES))
            theGame->mSound->playTrack(SOUNDID_MAYFLIES);
    } else {
        if (theGame->mSound->isTrackPlaying(SOUNDID_MAYFLIES))
            theGame->mSound->stopTrack(SOUNDID_MAYFLIES);
    }
}

// Gets called every time we go from one spawn index to another
// Not currently used
void spawner::transition()
{
    printf("New spawnIndex = %d, amount %f\n", getSpawnIndex(), mSpawnProgress);
}

void spawner::spawnEntities(entity::EntityType type, int numWanted)
{
    const auto& p = classical_params::get();

    const float margin = p.spawnerScatterMargin;
    const float leftEdge = margin;
    const float bottomEdge = margin;
    const float rightEdge = (theGame->mGrid->extentX() - 1) - margin;
    const float topEdge = (theGame->mGrid->extentY() - 1) - margin;

    int numHave = theGame->mEnemies->getNumActiveEnemiesOfType(type);
    if (numHave < numWanted) {
        entity* enemy = theGame->mEnemies->getUnusedEnemyOfType(type);
        if (enemy) {
            // Spawn somewhere random but not too close to the player
            float x = (mathutils::frandFrom0To1() * (rightEdge - leftEdge)) + leftEdge;
            float y = (mathutils::frandFrom0To1() * (topEdge - bottomEdge)) + bottomEdge;
            Point3d spawnPoint(x, y, 0);

            Point3d hitPoint;
            if (theGame->mGrid->hitTest(spawnPoint, enemy->getRadius(), &hitPoint)) {
                spawnPoint = hitPoint;
            }

            enemy->setPos(spawnPoint);

            // TODO - FIX THIS SO ENEMIES ARE SPAWNED OFF A RADIAL AT A MIN DISTANCE RATHER THAN
            // JUST USING RANDOM XY COORDS ON THE GRID?
            // float distance = mathutils::calculate2dDistance(spawnPoint, game::getPlayer1()->getPos());

            enemy->setState(entity::ENTITY_STATE_SPAWN_TRANSITION);
        }
    }
}

void spawner::runWaves()
{
    const auto& p = classical_params::get();

    for (int i = 0; i < NUM_WAVEDATA; i++) {
        WAVEDATA* wd = &mWaveData[i];
        if (wd->mWaveType != WAVETYPE_UNUSED) {
            const float margin = p.spawnerWaveMargin;
            const float leftEdge = margin;
            const float bottomEdge = margin;
            const float rightEdge = (theGame->mGrid->extentX() - 1) - margin;
            const float topEdge = (theGame->mGrid->extentY() - 1) - margin;

            if (wd->mWaveType == WAVETYPE_RUSH) {
                // Pick a player to attack
                player* randPlayer = theGame->mPlayers->getRandomActivePlayer();
                if (!randPlayer)
                    return;
                Point3d playerPos = randPlayer->getPos();

                // Keep cranking out enemies until the spawn counter reaches 0
                while (wd->spawnCount-- > 0) {
                    entity* enemy = theGame->mEnemies->getUnusedEnemyOfType(wd->entityType);
                    if (enemy) {
                        // Add the enemy to our tracking list
                        addEntityToWaveTracker(wd, enemy);

                        float rx = (mathutils::frandFrom0To1() * p.spawnerRushJitter) - (p.spawnerRushJitter * .5f);
                        float ry = (mathutils::frandFrom0To1() * p.spawnerRushJitter) - (p.spawnerRushJitter * .5f);

                        Point3d spawnPoint(wd->entityType == entity::ENTITY_TYPE_BLACKHOLE ? p.spawnerRushRadiusBlackhole : p.spawnerRushRadiusPlayer, 0, 0);
                        spawnPoint = mathutils::rotate2dPoint(spawnPoint, mathutils::frandFrom0To1() * (2 * PI));
                        spawnPoint.x += playerPos.x + rx;
                        spawnPoint.y += playerPos.y + ry;

                        // Keep it on the grid
                        Point3d hitPoint;
                        if (theGame->mGrid->hitTest(spawnPoint, enemy->getRadius(), &hitPoint)) {
                            spawnPoint = hitPoint;
                        }

                        enemy->setPos(spawnPoint);
                        enemy->setState(entity::ENTITY_STATE_SPAWN_TRANSITION);
                    }
                };

            } else {
                if (wd->timer == 0) {
                    static int corner = 0;

                    for (int n = 0; n < 4; n++) {
                        // Keep cranking out enemies until the spawn counter reaches 0
                        if (wd->spawnCount-- > 0) {
                            entity* enemy = theGame->mEnemies->getUnusedEnemyOfType(wd->entityType);
                            if (enemy) {
                                // Add the enemy to our tracking list
                                addEntityToWaveTracker(wd, enemy);

                                float rx = (mathutils::frandFrom0To1() * p.spawnerSwarmJitter) - (p.spawnerSwarmJitter * .5f);
                                float ry = (mathutils::frandFrom0To1() * p.spawnerSwarmJitter) - (p.spawnerSwarmJitter * .5f);

                                Point3d spawnPoint;
                                switch (corner % 4) {
                                case 0:
                                    spawnPoint = Point3d(leftEdge + rx, topEdge + ry);
                                    break;
                                case 1:
                                    spawnPoint = Point3d(rightEdge + rx, topEdge + ry);
                                    break;
                                case 2:
                                    spawnPoint = Point3d(rightEdge + rx, bottomEdge + ry);
                                    break;
                                case 3:
                                    spawnPoint = Point3d(leftEdge + rx, bottomEdge + ry);
                                    break;
                                }

                                {
                                    float radius = enemy->getRadius();

                                    if (wd->entityType == entity::ENTITY_TYPE_BLACKHOLE)
                                        radius = 20;

                                    const float leftEdge = radius;
                                    const float bottomEdge = radius;
                                    const float rightEdge = (theGame->mGrid->extentX() - radius) - 1;
                                    const float topEdge = (theGame->mGrid->extentY() - radius) - 1;

                                    if (spawnPoint.x < leftEdge) {
                                        spawnPoint.x = leftEdge;
                                    } else if (spawnPoint.x > rightEdge) {
                                        spawnPoint.x = rightEdge;
                                    }
                                    if (spawnPoint.y < bottomEdge) {
                                        spawnPoint.y = bottomEdge;
                                    } else if (spawnPoint.y > topEdge) {
                                        spawnPoint.y = topEdge;
                                    }
                                }

                                // Keep it on the grid
                                Point3d hitPoint;
                                if (theGame->mGrid->hitTest(spawnPoint, enemy->getRadius(), &hitPoint)) {
                                    spawnPoint = hitPoint;
                                }

                                enemy->setPos(spawnPoint);
                                enemy->setState(entity::ENTITY_STATE_SPAWN_TRANSITION);
                            }
                        }

                        ++corner;
                    }
                }

                ++wd->timer;

                if (wd->timer > p.spawnerSwarmWaveCadence) {
                    wd->timer = 0;
                }
            }
        }

        if (wd->spawnCount <= 0) {
            // Keep tracking previously spawned entities so we know when the wave is completed
            bool running = false;
            for (int n = 0; n < NUM_WAVEITEMTRACKERS; n++) {
                if (wd->mItemTrackers[n].e && (wd->mItemTrackers[n].e->getGenId() == wd->mItemTrackers[n].genId)) {
                    running = true;
                    break;
                }
            }
            if (!running) {
                // No more entities from this wave exist - end the wave
                wd->mWaveType = WAVETYPE_UNUSED;
            }
        }
    }
}

void spawner::clearWaveData()
{
    for (int i = 0; i < NUM_WAVEDATA; i++) {
        WAVEDATA* wd = &mWaveData[i];

        wd->mWaveType = WAVETYPE_UNUSED;

        for (int n = 0; n < NUM_WAVEITEMTRACKERS; n++) {
            wd->mItemTrackers[n].e = nullptr;
            wd->mItemTrackers[n].genId = -1;
        }
    }
}

void spawner::newWave(WAVETYPE mWaveType, entity::EntityType entityType, int spawnCount)
{
    WAVEDATA* wd = getUnusedWaveData();
    if (wd) {
        wd->mWaveType = mWaveType;
        wd->entityType = entityType;
        wd->spawnCount = spawnCount;
        wd->timer = 0;

        for (int n = 0; n < NUM_WAVEITEMTRACKERS; n++) {
            wd->mItemTrackers[n].e = nullptr;
            wd->mItemTrackers[n].genId = -1;
        }
    }
}

spawner::WAVEDATA* spawner::getUnusedWaveData()
{
    for (int i = 0; i < NUM_WAVEDATA; i++) {
        if (mWaveData[i].mWaveType == WAVETYPE_UNUSED) {
            return &mWaveData[i];
        }
    }
    return nullptr;
}

int spawner::numWaveData()
{
    int num = 0;

    for (int i = 0; i < NUM_WAVEDATA; i++) {
        if (mWaveData[i].mWaveType != WAVETYPE_UNUSED) {
            ++num;
        }
    }

    return num;
}

void spawner::addEntityToWaveTracker(WAVEDATA* wd, entity* e)
{
    for (int i = 0; i < NUM_WAVEITEMTRACKERS; i++) {
        if (wd->mItemTrackers[i].e == nullptr) {
            wd->mItemTrackers[i].e = e;
            wd->mItemTrackers[i].genId = e->getGenId();
            break;
        }
    }
}
