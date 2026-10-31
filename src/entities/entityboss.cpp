#include "entities/entityboss.hpp"
#include "core/classicalparams.hpp"
#include "core/game.hpp"
#include "entities/players.hpp"
#include "math/defines.hpp"

#include <cmath>

entityBoss::entityBoss(const game& gameRef)
    : entity(), mGame(gameRef)
{
    mScale = 2.8f;
    mRadius = 6.0f;

    mScoreValue = classical_params::get().scoreBoss;

    mType = ENTITY_TYPE_BOSS;
    setState(ENTITY_STATE_INACTIVE);

    mPen = vector::pen(.9f, .35f, 1.0f, .9f, 20);

    mModel.mVertexList.resize(12);
    for (int i = 0; i < 12; ++i) {
        const float angle = (2.0f * PI * i) / 12.0f;
        const float radius = (i % 2 == 0) ? 1.8f : 1.0f;
        mModel.mVertexList[i] = Point3d(std::cos(angle) * radius, std::sin(angle) * radius, 0.0f);
    }

    mModel.mEdgeList.resize(12);
    for (int i = 0; i < 11; ++i) {
        mModel.mEdgeList[i].from = i;
        mModel.mEdgeList[i].to = i + 1;
    }
    mModel.mEdgeList[11].from = 11;
    mModel.mEdgeList[11].to = 0;
}

void entityBoss::run()
{
    if (this->getEnabled()) {
        player* target = mGame.mPlayers->getPlayerClosestToPosition(mPos);
        if (target) {
            const float angle = mathutils::calculate2dAngle(mPos, target->getPos());
            Point3d moveVector(1, 0, 0);
            moveVector = mathutils::rotate2dPoint(moveVector, angle);
            mSpeed += moveVector * .03f;
            mSpeed = mathutils::clamp2dVector(mSpeed, .55f * mAggression + .15f);
            mSpeed *= .96f;

            mPulse += 0.12f + (mAggression * 0.03f);
            mScale.x = 3.0f + (std::sin(mPulse) * 0.35f);
            mScale.y = 3.0f + (std::cos(mPulse * 1.3f) * 0.35f);
            mAngle += 0.05f + (mAggression * 0.02f);
        }
    }

    entity::run();
}

void entityBoss::spawnTransition()
{
    entity::spawnTransition();
    mPulse = mathutils::frandFrom0To1() * (2 * PI);
    mRotationRate = .08f;
    mGame.mSound->playTrack(SOUNDID_ENEMYSPAWN6);
}
