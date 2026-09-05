#include "entities/powerups.hpp"

#include "core/game.hpp"
#include "entities/entity.hpp"
#include "entities/player.hpp"
#include "entities/players.hpp"
#include "math/mathutils.hpp"
#include "math/sincos.hpp"
#include "render/grid.hpp"

#include <cstdio>

#include "render/gl3.h"

const float powerups::kCollectRadius = 8.0f;

namespace {

player* playerByIndex(const game& owner, int index)
{
    switch (index) {
    case 0:
        return owner.getPlayer1();
    case 1:
        return owner.getPlayer2();
    case 2:
        return owner.getPlayer3();
    default:
        return owner.getPlayer4();
    }
}

} // namespace

void powerups::reset()
{
    for (int i = 0; i < kPoolSize; ++i) {
        mPool[i].enabled = false;
        mPool[i].ttl = 0;
    }
    mActive = 0;
}

void powerups::disable(Pickup& pk)
{
    pk.enabled = false;
    if (mActive > 0)
        --mActive;
}

void powerups::spawnAt(game& owner, PickupType type, const Point3d& pos)
{
    if (mActive >= kPoolSize)
        return;

    Pickup* pk = nullptr;
    for (int i = 0; i < kPoolSize; ++i) {
        if (!mPool[i].enabled) {
            pk = &mPool[i];
            break;
        }
    }
    if (!pk)
        return;

    pk->enabled = true;
    pk->type = type;
    pk->pos = pos;
    pk->phase = mathutils::frandFrom0To1() * (2.0f * 3.14159f);
    pk->ttl = kLifetime;
    ++mActive;

    (void)owner;
}

void powerups::spawnRandom(game& owner)
{
    if (mActive >= kMaxOnScreen)
        return;

    // Weighted pick: mostly weapon upgrades, some shields, rarer lasers.
    const float roll = mathutils::frandFrom0To1() * 100.0f;
    PickupType type = PICKUP_WEAPON;
    if (roll >= 45.0f && roll < 80.0f)
        type = PICKUP_SHIELD;
    else if (roll >= 80.0f)
        type = PICKUP_LASER;

    // Find a spot on the grid away from every joined, running player.
    const float margin = 30.0f;
    const float leftEdge = margin;
    const float bottomEdge = margin;
    const float rightEdge = (owner.mGrid->extentX() - 1.0f) - margin;
    const float topEdge = (owner.mGrid->extentY() - 1.0f) - margin;

    Point3d pos(0, 0, 0);
    for (int attempt = 0; attempt < 10; ++attempt) {
        pos.x = (mathutils::frandFrom0To1() * (rightEdge - leftEdge)) + leftEdge;
        pos.y = (mathutils::frandFrom0To1() * (topEdge - bottomEdge)) + bottomEdge;

        bool tooClose = false;
        for (int i = 0; i < 4; ++i) {
            player* p = playerByIndex(owner, i);
            if (p && p->mJoined && p->getState() == entity::ENTITY_STATE_RUNNING) {
                if (mathutils::calculate2dDistance(pos, p->getPos()) < 60.0f) {
                    tooClose = true;
                    break;
                }
            }
        }
        if (!tooClose)
            break;
    }

    spawnAt(owner, type, pos);
}

void powerups::update(game& owner)
{
    for (int i = 0; i < kPoolSize; ++i) {
        Pickup& pk = mPool[i];
        if (!pk.enabled)
            continue;

        --pk.ttl;
        pk.phase += 0.1f;
        if (pk.ttl <= 0) {
            disable(pk);
            continue;
        }

        // Collection: any joined, running player within range.
        for (int pi = 0; pi < 4; ++pi) {
            player* p = playerByIndex(owner, pi);
            if (!p || !p->mJoined || p->getState() != entity::ENTITY_STATE_RUNNING)
                continue;
            if (mathutils::calculate2dDistance(pk.pos, p->getPos()) <= kCollectRadius) {
                collect(owner, pk, p);
                break;
            }
        }
    }
}

void powerups::collect(game& owner, Pickup& pk, player* p)
{
    vector::pen pen(1.0f, 1.0f, 1.0f, 1.0f, 3);

    switch (pk.type) {
    case PICKUP_WEAPON: {
        pen = vector::pen(0.3f, 1.0f, 1.0f, 1.0f, 3);
        if (p->getWeapon() < p->maxWeapon()) {
            const bool toLaser = (p->getWeapon() == p->maxWeapon() - 1);
            p->upgradeWeapon();
            theGame->mSound->playTrack(SOUNDID_MULTIPLIERADVANCE);
            char msg[64];
            snprintf(msg, sizeof(msg), toLaser ? "LASER ONLINE!" : "WEAPON UPGRADED");
            game::showMessageAtLocation(msg, pk.pos, pen);
        } else {
            // Weapon already maxed: a spare bomb (single player) or points.
            if (owner.numPlayers() == 1) {
                p->addBomb();
                theGame->mSound->playTrack(SOUNDID_EXTRABOMB);
                char msg[64];
                snprintf(msg, sizeof(msg), "EXTRA BOMB");
                game::showMessageAtLocation(msg, pk.pos, pen);
            } else {
                p->addPointsNoMultiplier(1000);
                theGame->mSound->playTrack(SOUNDID_MULTIPLIERADVANCE);
                char msg[64];
                snprintf(msg, sizeof(msg), "WEAPON MAX +1000");
                game::showMessageAtLocation(msg, pk.pos, pen);
            }
        }
        break;
    }
    case PICKUP_SHIELD: {
        pen = vector::pen(0.3f, 1.0f, 0.45f, 1.0f, 3);
        if (p->shieldCharges() < p->maxShieldCharges()) {
            p->addShieldCharges(1);
            theGame->mSound->playTrack(SOUNDID_PLAYERSPAWN);
            char msg[64];
            snprintf(msg, sizeof(msg), "SHIELD UP");
            game::showMessageAtLocation(msg, pk.pos, pen);
        } else {
            if (owner.numPlayers() == 1) {
                p->addBomb();
                theGame->mSound->playTrack(SOUNDID_EXTRABOMB);
                char msg[64];
                snprintf(msg, sizeof(msg), "SHIELD MAX +BOMB");
                game::showMessageAtLocation(msg, pk.pos, pen);
            } else {
                p->addPointsNoMultiplier(1000);
                theGame->mSound->playTrack(SOUNDID_MULTIPLIERADVANCE);
                char msg[64];
                snprintf(msg, sizeof(msg), "SHIELD MAX +1000");
                game::showMessageAtLocation(msg, pk.pos, pen);
            }
        }
        break;
    }
    case PICKUP_LASER:
    default: {
        pen = vector::pen(1.0f, 0.35f, 0.2f, 1.0f, 3);
        if (p->getWeapon() < p->maxWeapon()) {
            p->setWeapon(p->maxWeapon());
            theGame->mSound->playTrack(SOUNDID_MULTIPLIERADVANCE);
            char msg[64];
            snprintf(msg, sizeof(msg), "LASER ONLINE!");
            game::showMessageAtLocation(msg, pk.pos, pen);
        } else {
            p->addPointsNoMultiplier(1000);
            theGame->mSound->playTrack(SOUNDID_MULTIPLIERADVANCE);
            char msg[64];
            snprintf(msg, sizeof(msg), "+1000");
            game::showMessageAtLocation(msg, pk.pos, pen);
        }
        break;
    }
    }

    disable(pk);
}

void powerups::draw(game& owner, int pass)
{
    (void)owner;

    if (pass != 0)
        return; // only the primary world pass

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (int i = 0; i < kPoolSize; ++i) {
        const Pickup& pk = mPool[i];
        if (!pk.enabled)
            continue;

        // Blink as the pickup is about to expire.
        if (pk.ttl < kBlinkTime && ((pk.ttl / 4) & 1))
            continue;

        const float bob = 1.0f + (0.15f * get_sin(pk.phase));
        const float x = pk.pos.x;
        const float y = pk.pos.y + (1.5f * get_sin(pk.phase));

        switch (pk.type) {
        case PICKUP_WEAPON: {
            // A little "up arrow" (weapon upgrade).
            const float s = 3.0f * bob;
            vector::pen pen(0.3f, 1.0f, 1.0f, 0.85f, 2);

            glLineWidth(4);
            glColor4f(pen.r, pen.g, pen.b, 0.25f);
            glBegin(GL_LINES);
            glVertex3f(x, y - s, 0);
            glVertex3f(x, y + s * 0.6f, 0);
            glVertex3f(x - s * 0.55f, y - s * 0.2f, 0);
            glVertex3f(x, y + s * 0.6f, 0);
            glVertex3f(x + s * 0.55f, y - s * 0.2f, 0);
            glVertex3f(x, y + s * 0.6f, 0);
            glEnd();

            glLineWidth(2);
            glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
            glBegin(GL_LINES);
            glVertex3f(x, y - s, 0);
            glVertex3f(x, y + s * 0.6f, 0);
            glEnd();
            break;
        }
        case PICKUP_SHIELD: {
            // A dashed ring (shield charge).
            const float r = 3.2f * bob;
            vector::pen pen(0.3f, 1.0f, 0.45f, 0.85f, 2);

            glLineWidth(4);
            glColor4f(pen.r, pen.g, pen.b, 0.25f);
            glBegin(GL_LINES);
            for (float a = 0; a < 2.0f * 3.14159f; a += 0.06f) {
                const bool solid = (static_cast<int>(a / 0.3f) & 1) == 0;
                if (!solid)
                    continue;
                glVertex3f(x + r * get_cos(a), y + r * get_sin(a), 0);
                glVertex3f(x + r * get_cos(a + 0.06f), y + r * get_sin(a + 0.06f), 0);
            }
            glEnd();

            glLineWidth(2);
            glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
            glBegin(GL_LINES);
            for (float a = 0; a < 2.0f * 3.14159f; a += 0.06f) {
                const bool solid = (static_cast<int>(a / 0.3f) & 1) == 0;
                if (!solid)
                    continue;
                glVertex3f(x + r * get_cos(a), y + r * get_sin(a), 0);
                glVertex3f(x + r * get_cos(a + 0.06f), y + r * get_sin(a + 0.06f), 0);
            }
            glEnd();
            break;
        }
        case PICKUP_LASER:
        default: {
            // A short horizontal beam (laser).
            const float s = 3.4f * bob;
            vector::pen pen(1.0f, 0.35f, 0.2f, 0.9f, 2);

            glLineWidth(5);
            glColor4f(pen.r, pen.g, pen.b, 0.3f);
            glBegin(GL_LINES);
            glVertex3f(x - s, y, 0);
            glVertex3f(x + s, y, 0);
            glEnd();

            glLineWidth(2);
            glColor4f(1.0f, 1.0f, 1.0f, 0.95f);
            glBegin(GL_LINES);
            glVertex3f(x - s * 0.6f, y, 0);
            glVertex3f(x + s * 0.6f, y, 0);
            glEnd();
            break;
        }
        }
    }
}
