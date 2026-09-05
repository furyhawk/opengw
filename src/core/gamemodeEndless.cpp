#include "core/gamemodeEndless.hpp"

#include "core/game.hpp"
#include "entities/entity.hpp"
#include "entities/player.hpp"
#include "entities/powerups.hpp"
#include "entities/spawner.hpp"

#include <algorithm>

namespace {

bool anyActivePlayer(const game& owner)
{
    const player* players[4] = { owner.getPlayer1(), owner.getPlayer2(), owner.getPlayer3(), owner.getPlayer4() };
    for (int i = 0; i < 4; ++i) {
        if (players[i] && players[i]->mJoined && players[i]->getState() == entity::ENTITY_STATE_RUNNING)
            return true;
    }
    return false;
}

} // namespace

endless_mode::endless_mode()
    : mPowerUps(std::make_unique<powerups>())
{
}

endless_mode::~endless_mode() = default;

const char* endless_mode::name() const
{
    return "Endless";
}

void endless_mode::begin_match(game& owner)
{
    // Run the normal Classical match setup (players, shared pool, music,
    // spawner) and then start this mode's own systems fresh.
    mClassical.begin_match(owner);

    mPowerUps->reset();
    mDropTimer = 60 * 4; // first drop lands ~4 seconds in
}

void endless_mode::end_match(game& owner)
{
    mClassical.end_match(owner);
}

int endless_mode::dropInterval(const game& owner) const
{
    // Drops get more frequent as the difficulty index climbs: ~8 seconds at
    // index 0 down to ~2 seconds near the cap.
    int idx = owner.mSpawner->getSpawnIndex();
    int frames = 480 - (idx * 12);
    return std::max(frames, 110);
}

void endless_mode::update(game& owner)
{
    mClassical.update(owner);

    // Fast pace: push the difficulty clock forward faster than the spawner's
    // own indexRate so the action escalates quickly.
    owner.mSpawner->accelerateSpawnIndex(mDifficultyAccel);

    // Drop power-ups periodically while somebody is actually playing. The
    // pickup pool updates every frame regardless so drops expire/are caught.
    if (anyActivePlayer(owner)) {
        if (--mDropTimer <= 0) {
            mDropTimer = dropInterval(owner);
            mPowerUps->spawnRandom(owner);
        }
    }

    mPowerUps->update(owner);
}

void endless_mode::draw(game& owner, int pass)
{
    mClassical.draw(owner, pass);
    mPowerUps->draw(owner, pass);
}
