#include "entities/entitydart.hpp"
#include "core/classicalparams.hpp"
#include "core/game.hpp"
#include "entities/players.hpp"

entityDart::entityDart()
    : entity()
{
    mScale = 1.2f;
    mRadius = 2.4f;

    mScoreValue = classical_params::get().scoreDart;

    mType = ENTITY_TYPE_DART;
    setState(ENTITY_STATE_INACTIVE);

    mPen = vector::pen(1.0f, 0.65f, 0.2f, 0.8f, 12);

    int i = 0;

    mModel.mVertexList.resize(5);
    mModel.mVertexList[i++] = Point3d(0.0f, 1.4f);
    mModel.mVertexList[i++] = Point3d(1.1f, 0.2f);
    mModel.mVertexList[i++] = Point3d(0.6f, -1.1f);
    mModel.mVertexList[i++] = Point3d(-0.6f, -1.1f);
    mModel.mVertexList[i++] = Point3d(-1.1f, 0.2f);

    i = 0;

    mModel.mEdgeList.resize(5);
    mModel.mEdgeList[i].from = 0;
    mModel.mEdgeList[i++].to = 1;
    mModel.mEdgeList[i].from = 1;
    mModel.mEdgeList[i++].to = 2;
    mModel.mEdgeList[i].from = 2;
    mModel.mEdgeList[i++].to = 3;
    mModel.mEdgeList[i].from = 3;
    mModel.mEdgeList[i++].to = 4;
    mModel.mEdgeList[i].from = 4;
    mModel.mEdgeList[i++].to = 0;
}

void entityDart::run()
{
    if (this->getEnabled()) {
        player* target = theGame->mPlayers->getPlayerClosestToPosition(mPos);
        if (target) {
            float angle = mathutils::calculate2dAngle(mPos, target->getPos());
            Point3d moveVector(1, 0, 0);
            moveVector = mathutils::rotate2dPoint(moveVector, angle);
            moveVector = mathutils::clamp2dVector(moveVector, .16f);
            mSpeed += moveVector * .6f;
            mSpeed = mathutils::clamp2dVector(mSpeed, .8f * mAggression);
            mSpeed *= .93f;

            mAngle = angle + mathutils::DegreesToRads(90);
            mRotationRate += (mathutils::frandFrom0To1() - .5f) * .04f;
            mRotationRate *= .85f;
        }
    }

    entity::run();
}

void entityDart::spawnTransition()
{
    entity::spawnTransition();
    mRotationRate = .15f;
    theGame->mSound->playTrack(SOUNDID_ENEMYSPAWN2);
}
