#include "math/defines.hpp"
#include "entities/entityorbiter.hpp"
#include "core/classicalparams.hpp"
#include "core/game.hpp"
#include "entities/players.hpp"

#include <cmath>

entityOrbiter::entityOrbiter()
    : entity()
{
    mScale = 1.6f;
    mRadius = 2.8f;

    mScoreValue = classical_params::get().scoreOrbiter;

    mType = ENTITY_TYPE_ORBITER;
    setState(ENTITY_STATE_INACTIVE);

    mPen = vector::pen(.25f, .75f, 1.0f, .8f, 12);

    int i = 0;

    mModel.mVertexList.resize(8);
    mModel.mVertexList[i++] = Point3d(0.0f, 1.5f);
    mModel.mVertexList[i++] = Point3d(1.0f, 0.7f);
    mModel.mVertexList[i++] = Point3d(1.5f, 0.0f);
    mModel.mVertexList[i++] = Point3d(1.0f, -0.7f);
    mModel.mVertexList[i++] = Point3d(0.0f, -1.5f);
    mModel.mVertexList[i++] = Point3d(-1.0f, -0.7f);
    mModel.mVertexList[i++] = Point3d(-1.5f, 0.0f);
    mModel.mVertexList[i++] = Point3d(-1.0f, 0.7f);

    i = 0;

    mModel.mEdgeList.resize(8);
    for (int j = 0; j < 7; ++j) {
        mModel.mEdgeList[i].from = j;
        mModel.mEdgeList[i++].to = j + 1;
    }
    mModel.mEdgeList[i].from = 7;
    mModel.mEdgeList[i++].to = 0;
}

void entityOrbiter::run()
{
    if (this->getEnabled()) {
        player* target = theGame->mPlayers->getPlayerClosestToPosition(mPos);
        if (target) {
            mOrbitAngle += 0.06f + (mAggression * 0.02f);
            mOrbitRadius = 10.0f + (std::sin(mOrbitAngle * 1.7f) * 2.0f);

            Point3d offset(mOrbitRadius, 0, 0);
            offset = mathutils::rotate2dPoint(offset, mOrbitAngle);
            Point3d desiredPos = target->getPos() + offset;

            Point3d toTarget = desiredPos - mPos;
            const float dist = mathutils::calculate2dDistance(Point3d(0, 0, 0), toTarget);
            if (dist > 0.001f) {
                toTarget.x /= dist;
                toTarget.y /= dist;
            }

            mSpeed += toTarget * .18f;
            mSpeed = mathutils::clamp2dVector(mSpeed, .52f * mAggression);
            mSpeed *= .96f;

            mAngle = mOrbitAngle + mathutils::DegreesToRads(90);
            mRotationRate += .08f;
            mRotationRate *= .85f;
        }
    }

    entity::run();
}

void entityOrbiter::spawnTransition()
{
    entity::spawnTransition();
    mOrbitAngle = mathutils::frandFrom0To1() * (2 * PI);
    mOrbitRadius = 10.0f + (mathutils::frandFrom0To1() * 12.0f);
    mRotationRate = .12f;
    theGame->mSound->playTrack(SOUNDID_ENEMYSPAWN3);
}
