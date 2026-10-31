#include "core/gamemodePressureLadder.hpp"

#include "core/camera.hpp"
#include "core/game.hpp"
#include "entities/enemies.hpp"
#include "entities/entity.hpp"
#include "entities/particle.hpp"
#include "entities/player.hpp"
#include "entities/players.hpp"
#include "entities/spawner.hpp"
#include "math/defines.hpp"
#include "math/mathutils.hpp"
#include "render/grid.hpp"

#include <algorithm>

namespace {

constexpr float kLogicStepSeconds = 1.0f / 60.0f;

bool anyActivePlayer(const game& owner)
{
    const player* players[4] = {
        owner.getPlayer1(),
        owner.getPlayer2(),
        owner.getPlayer3(),
        owner.getPlayer4(),
    };

    for (int i = 0; i < 4; ++i) {
        if (players[i] && players[i]->mJoined && players[i]->getState() == entity::ENTITY_STATE_RUNNING)
            return true;
    }

    return false;
}

float tierAccel(int tier)
{
    switch (tier) {
    case 1:
        return 0.004f;
    case 2:
        return 0.006f;
    case 3:
        return 0.008f;
    case 4:
        return 0.010f;
    default:
        return 0.012f;
    }
}

float cadenceForTier(int tier)
{
    if (tier >= 5)
        return 14.0f;
    if (tier >= 4)
        return 18.0f;
    if (tier >= 3)
        return 22.0f;
    return 30.0f;
}

} // namespace

const char* pressure_ladder_mode::name() const
{
    return "Pressure Ladder";
}

void pressure_ladder_mode::begin_match(game& owner)
{
    mClassical.begin_match(owner);

    owner.mGrid->setResolution(299, 233);
    owner.mCamera->mCurrentZoom = 0.0f;
    owner.mCamera->mTargetZoom = 96.0f;

    mTier = 1;
    mTierTimer = 0.0f;
    mEliteTimer = 0.0f;
    mPulseTimer = 6.0f;
    mEliteWaveTimer = 0.0f;
    mScoreMultiplier = 1.0f;
    mEliteWaveActive = false;
}

void pressure_ladder_mode::end_match(game& owner)
{
    mClassical.end_match(owner);
}

void pressure_ladder_mode::triggerPressurePulse(game& owner)
{
    Point3d center(owner.mGrid->extentX() * 0.5f, owner.mGrid->extentY() * 0.5f, 0.0f);
    Point3d delta(0.0f, 0.0f, 0.0f);

    vector::pen core(1.0f, 0.63f, 0.18f, 1.0f, 6.0f);
    owner.mParticles->emitter(&center, &delta, 6.0f, 2.0f * PI, 220, &core, 20, true, false, 0.92f, true);

    vector::pen flare(1.0f, 0.9f, 0.7f, 0.8f, 8.0f);
    owner.mParticles->emitter(&center, &delta, 4.5f, 2.0f * PI, 140, &flare, 24, true, false, 0.90f, true);

    owner.mSound->setTrackSpeed(SOUNDID_MULTIPLIERADVANCE, 0.95 + (mTier * 0.12));
    owner.mSound->playTrack(SOUNDID_MULTIPLIERADVANCE);
    owner.mSound->playTrack(SOUNDID_EXTRABOMB);

    owner.mCamera->mTargetZoom = std::max(owner.mCamera->mTargetZoom, 96.0f + (mTier * 5.0f));
    owner.mCamera->mCurrentZoom = std::min(owner.mCamera->mCurrentZoom + 12.0f, owner.mCamera->mTargetZoom);
}

void pressure_ladder_mode::awardTierBonus(game& owner)
{
    const int bonus = static_cast<int>((1000.0f + (mTier * 300.0f)) * mScoreMultiplier);
    player* players[4] = { owner.getPlayer1(), owner.getPlayer2(), owner.getPlayer3(), owner.getPlayer4() };

    for (int i = 0; i < 4; ++i) {
        if (!players[i] || !players[i]->mJoined)
            continue;
        players[i]->addPointsNoMultiplier(bonus);
        players[i]->addAchievementProgress(26 + (mTier * 2));

        char msg[32];
        snprintf(msg, sizeof(msg), "TIER %d", mTier);
        game::showMessageAtLocation(msg, players[i]->getPos(), players[i]->getFontPen());
    }
}

void pressure_ladder_mode::awardEliteBonus(game& owner)
{
    const int bonus = static_cast<int>((1500.0f + (mTier * 400.0f)) * mScoreMultiplier);
    player* players[4] = { owner.getPlayer1(), owner.getPlayer2(), owner.getPlayer3(), owner.getPlayer4() };

    for (int i = 0; i < 4; ++i) {
        if (!players[i] || !players[i]->mJoined)
            continue;
        players[i]->addPointsNoMultiplier(bonus);
        players[i]->addAchievementProgress(32 + (mTier * 3));

        char msg[32];
        snprintf(msg, sizeof(msg), "ELITE %d", mTier);
        game::showMessageAtLocation(msg, players[i]->getPos(), players[i]->getFontPen());
    }
}

void pressure_ladder_mode::advanceTier(game& owner)
{
    ++mTier;
    mTierTimer = 0.0f;
    mEliteTimer = 0.0f;
    mScoreMultiplier = 1.0f + (mTier * 0.25f);

    triggerPressurePulse(owner);
    owner.mCamera->mTargetZoom = std::max(owner.mCamera->mTargetZoom, 96.0f + (mTier * 1.5f));

    awardTierBonus(owner);
    applyArenaModifier(owner);
    mPulseTimer = 16.0f;
}

void pressure_ladder_mode::applyArenaModifier(game& owner)
{
    // Push the arena wider and more dangerous as the pressure rises, but keep it
    // in the same overall visual language as the rest of the game.
    if (mTier >= 5) {
        owner.mGrid->setResolution(310, 238);
        owner.mCamera->mTargetZoom = std::max(owner.mCamera->mTargetZoom, 100.0f + (mTier * 2.0f));
    } else if (mTier >= 3) {
        owner.mGrid->setResolution(299, 233);
        owner.mCamera->mTargetZoom = std::max(owner.mCamera->mTargetZoom, 96.0f + (mTier * 1.5f));
    }
}

void pressure_ladder_mode::triggerEliteWave(game& owner)
{
    mEliteWaveActive = true;
    mEliteWaveTimer = 12.0f + (mTier * 1.5f);

    const int count = std::min(8 + (mTier * 2), 16);
    const entity::EntityType choices[] = {
        entity::ENTITY_TYPE_WANDERER,
        entity::ENTITY_TYPE_WEAVER,
        entity::ENTITY_TYPE_GRUNT,
        entity::ENTITY_TYPE_SNAKE,
        entity::ENTITY_TYPE_SPINNER
    };

    Point3d center(owner.mGrid->extentX() * 0.5f, owner.mGrid->extentY() * 0.5f, 0.0f);

    for (int i = 0; i < count; ++i) {
        const entity::EntityType type = choices[i % (sizeof(choices) / sizeof(choices[0]))];
        entity* enemy = owner.mEnemies->getUnusedEnemyOfType(type);
        if (!enemy)
            continue;

        const float angle = (static_cast<float>(i) / static_cast<float>(count)) * (2.0f * PI)
            + mathutils::frandFrom0To1() * 0.7f;
        const float radius = 55.0f + mathutils::frandFrom0To1() * 18.0f;
        Point3d pos(radius * cosf(angle), radius * sinf(angle), 0.0f);
        pos += center;

        player* target = owner.mPlayers->getPlayerClosestToPosition(pos);
        Point3d delta(0.0f, 0.0f, 0.0f);
        if (target) {
            delta = target->getPos() - pos;
            float len = mathutils::calculate2dDistance(Point3d(0.0f, 0.0f, 0.0f), delta);
            if (len > 0.0001f) {
                delta *= 1.0f / len;
            }
        }

        enemy->setPos(pos);
        enemy->setSpeed(delta * (2.5f + (mTier * 0.35f)));
        enemy->setDrift(Point3d(0.0f, 0.0f, 0.0f));
        enemy->setEnabled(true);
    }
}

void pressure_ladder_mode::update(game& owner)
{
    mClassical.update(owner);

    owner.mCamera->mTargetZoom = std::max(owner.mCamera->mTargetZoom, 96.0f);
    owner.mSpawner->accelerateSpawnIndex(tierAccel(mTier));

    mTierTimer += kLogicStepSeconds;
    mEliteTimer += kLogicStepSeconds;
    mPulseTimer -= kLogicStepSeconds;

    if (mEliteWaveActive) {
        mEliteWaveTimer -= kLogicStepSeconds;
        if (mEliteWaveTimer <= 0.0f) {
            mEliteWaveActive = false;
            awardEliteBonus(owner);
        }
    }

    if (mTierTimer >= 30.0f && anyActivePlayer(owner)) {
        advanceTier(owner);
    }

    const float cadence = cadenceForTier(mTier);
    if (mEliteTimer >= cadence && anyActivePlayer(owner)) {
        triggerEliteWave(owner);
        mEliteTimer = 0.0f;
    }

    if (mPulseTimer <= 0.0f && anyActivePlayer(owner)) {
        applyArenaModifier(owner);
        mPulseTimer = 16.0f;
    }
}

void pressure_ladder_mode::draw(game& owner, int pass)
{
    mClassical.draw(owner, pass);

    // This mode intentionally leaves the live HUD text for a later pass. The
    // actual match rendering is already handled by the classical mode's draw
    // path, while the tier / pressure state can be overlaid in scene.cpp.
    (void)owner;
    (void)pass;
}
