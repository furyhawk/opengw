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
    mRadius = 8.5f;

    mScoreValue = classical_params::get().scoreBoss;

    mType = ENTITY_TYPE_BOSS;
    setState(ENTITY_STATE_INACTIVE);

    mPen = vector::pen(.9f, .35f, 1.0f, .9f, 20);

    mModel.mVertexList.resize(24);
    for (int i = 0; i < 12; ++i) {
        const float angle = (2.0f * PI * i) / 12.0f;
        const float outerRadius = 1.9f + 0.35f * std::sin(angle * 3.0f);
        const float innerRadius = 0.9f + 0.18f * std::cos(angle * 2.0f + (PI / 12.0f));

        mModel.mVertexList[i] = Point3d(std::cos(angle) * outerRadius, std::sin(angle) * outerRadius, 0.0f);
        mModel.mVertexList[i + 12] = Point3d(std::cos(angle + (PI / 12.0f)) * innerRadius,
                                            std::sin(angle + (PI / 12.0f)) * innerRadius,
                                            0.0f);
    }

    mModel.mEdgeList.resize(36);
    int edgeIndex = 0;
    for (int i = 0; i < 12; ++i) {
        const int next = (i + 1) % 12;
        mModel.mEdgeList[edgeIndex++] = { i, next };
        mModel.mEdgeList[edgeIndex++] = { 12 + i, 12 + next };
        mModel.mEdgeList[edgeIndex++] = { i, 12 + i };
    }
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
            const float pulseX = 0.85f + (0.25f * std::sin(mPulse));
            const float pulseY = 0.85f + (0.25f * std::cos(mPulse * 1.3f));
            mScale.x = 3.0f * pulseX;
            mScale.y = 3.0f * pulseY;
            mAngle += 0.05f + (mAggression * 0.02f);

            for (int i = 0; i < 12; ++i) {
                const float baseAngle = (2.0f * PI * i) / 12.0f + (mPulse * 0.9f);
                const float outerRadius = 1.8f + (0.35f * std::sin(baseAngle * 3.0f + mPulse)) + (0.18f * std::cos(baseAngle * 6.0f));
                const float innerRadius = 0.92f + (0.18f * std::cos(baseAngle * 2.0f - mPulse));

                mModel.mVertexList[i] = Point3d(std::cos(baseAngle) * outerRadius,
                                               std::sin(baseAngle) * outerRadius,
                                               0.0f);
                mModel.mVertexList[i + 12] = Point3d(std::cos(baseAngle + (PI / 12.0f)) * innerRadius,
                                                    std::sin(baseAngle + (PI / 12.0f)) * innerRadius,
                                                    0.0f);
            }
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
