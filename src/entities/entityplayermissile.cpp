#include "entities/attractor.hpp"
#include "math/defines.hpp"
#include "entities/entityplayermissile.hpp"
#include "entities/enemies.hpp"
#include "entities/entityblackhole.hpp"
#include "entities/entityrepulsor.hpp"
#include "core/game.hpp"
#include "render/grid.hpp"
#include "render/scene.hpp"
#include "entities/particle.hpp"
#include "entities/players.hpp"

#include "render/gl3.h"

#include <cmath>

namespace
{
constexpr float kTwoPi = 2.0f * PI;
constexpr int kLaunchFlashFrames = 9;

float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

Point3d normalize2d(const Point3d& v, const Point3d& fallback)
{
    const float len = mathutils::calculate2dDistance(Point3d(0, 0, 0), v);
    if (len < 0.0001f)
        return fallback;
    return Point3d(v.x / len, v.y / len, 0.0f);
}

// Additive radial glow: hot centre fading to a transparent rim. Cheap enough
// to use for muzzle flashes and beam heads (one triangle fan each).
void drawGlowOrb(float x, float y, float radius, float r, float g, float b, float alpha)
{
    if ((alpha <= 0.002f) || (radius <= 0.0f))
        return;

    constexpr int kSegments = 16;
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(r, g, b, clamp01(alpha));
    glVertex3f(x, y, 0.0f);
    glColor4f(r, g, b, 0.0f);
    for (int i = 0; i <= kSegments; ++i) {
        const float a = (kTwoPi * i) / kSegments;
        glVertex3f(x + (std::cos(a) * radius), y + (std::sin(a) * radius), 0.0f);
    }
    glEnd();
}

// One tapered streak segment. The per-vertex alpha makes a beam or exhaust
// plume fade smoothly instead of ending in a hard edge.
void drawTaperedSegment(const Point3d& from, const Point3d& to, float width, const vector::pen& pen, float aFrom, float aTo)
{
    glLineWidth(width);
    glBegin(GL_LINES);
    glColor4f(pen.r, pen.g, pen.b, clamp01(aFrom));
    glVertex3f(from.x, from.y, 0.0f);
    glColor4f(pen.r, pen.g, pen.b, clamp01(aTo));
    glVertex3f(to.x, to.y, 0.0f);
    glEnd();
}

void drawRing(float x, float y, float radius, const vector::pen& pen, float alpha)
{
    if (alpha <= 0.002f)
        return;

    constexpr int kSegments = 20;
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glColor4f(pen.r, pen.g, pen.b, clamp01(alpha));
    for (int i = 0; i < kSegments; ++i) {
        const float a = (kTwoPi * i) / kSegments;
        glVertex3f(x + (std::cos(a) * radius), y + (std::sin(a) * radius), 0.0f);
    }
    glEnd();
}
} // namespace

entityPlayerMissile::entityPlayerMissile()
    : entity()
{
    mScale = .16;
    mRadius = 1;

    mPlayerSource = 1;

    mScoreValue = 0;

    mSpawnTime = 0;
    mDestroyTime = 0;

    mType = ENTITY_TYPE_PLAYER_MISSILE;

    int i = 0;

    mModel.mVertexList.resize(3);
    mModel.mVertexList[i++] = Point3d(0, 7.5);
    mModel.mVertexList[i++] = Point3d(2, -5.2);
    mModel.mVertexList[i++] = Point3d(-2, -5.2);

    i = 0;

    mModel.mEdgeList.resize(3);
    mModel.mEdgeList[i].from = 0;
    mModel.mEdgeList[i++].to = 1;
    mModel.mEdgeList[i].from = 1;
    mModel.mEdgeList[i++].to = 2;
    mModel.mEdgeList[i].from = 2;
    mModel.mEdgeList[i++].to = 0;
}

void entityPlayerMissile::run()
{
    mLastPos = mPos;

    // Lasers fly straight: they ignore repulsor deflection and black-hole
    // warping so they read as a clean beam.
    if (mType != 3) {

    // Check for repulsors that may effect us.
    for (int i = 0; i < NUM_ENEMIES; i++) {
        entity* enemy = theGame->mEnemies->mEnemies[i];
        if ((enemy->getType() == entity::ENTITY_TYPE_REPULSOR) && (enemy->getState() == entity::ENTITY_STATE_RUNNING)) {
            entityRepulsor* repulsor = static_cast<entityRepulsor*>(enemy);
            if (repulsor) {
                repulsor->repelEntity(this);
            }
        }
    }

    // Check for black holes that may effect us. Use a squared-distance check to
    // avoid the expensive sqrt while the missile is still far away.
    bool warped = false;
    for (int i = 0; i < NUM_ENEMIES; i++) {
        entity* enemy = theGame->mEnemies->mEnemies[i];
        if ((enemy->getType() == entity::ENTITY_TYPE_BLACKHOLE) && (enemy->getState() == entity::ENTITY_STATE_RUNNING)) {
            entityBlackHole* blackHole = static_cast<entityBlackHole*>(enemy);
            if (blackHole->mActivated) {
                const Point3d delta = mPos - blackHole->getPos();
                const float distanceSquared = (delta.x * delta.x) + (delta.y * delta.y);
                if (distanceSquared < (20.0f * 20.0f)) {
                    Point3d v2 = blackHole->getPos();
                    Point3d v1 = mPos;

                    float angle = mathutils::wrapRadians(mathutils::calculate2dAngle(v1, v2));
                    float heading = mathutils::wrapRadians(getAngle() + mathutils::DegreesToRads(90));

                    float targetingAngle = mathutils::diffAngles(angle, heading);
                    float targetingOffset = fabs(targetingAngle);

                    if (targetingOffset < 0.8) {
                        mRotationRate += targetingAngle * .1;
                        warped = true;
                    }
                }
            }
        }
    }

    mRotationRate *= .5;

    if (warped) {
        Point3d vector(mVelocity, 0, 0);
        mSpeed = mathutils::rotate2dPoint(vector, mAngle + mathutils::DegreesToRads(90));
    }

    } // end: lasers fly straight

    // Homing missiles (type 4): expire after their life, otherwise steer
    // toward the nearest live enemy so they arc onto a target.
    if (mType == 4) {
        if (--mLife <= 0) {
            setState(ENTITY_STATE_DESTROY_TRANSITION);
            return;
        }

        entity* target = nullptr;
        float best = 1e30f;
        for (int i = 0; i < NUM_ENEMIES; i++) {
            entity* e = theGame->mEnemies->mEnemies[i];
            if (!e || e->getState() != entity::ENTITY_STATE_RUNNING)
                continue;
            const entity::EntityType t = e->getType();
            if (t == entity::ENTITY_TYPE_BLACKHOLE || t == entity::ENTITY_TYPE_REPULSOR || t == entity::ENTITY_TYPE_REPULSOR_SHIELD)
                continue;
            const float d = mathutils::calculate2dDistanceSquared(mPos, e->getPos());
            if (d < best) {
                best = d;
                target = e;
            }
        }

        if (target) {
            Point3d toTarget(target->getPos().x - mPos.x, target->getPos().y - mPos.y, 0);
            const float dist = mathutils::calculate2dDistance(Point3d(0, 0, 0), toTarget);
            if (dist > 0.001f) {
                toTarget.x /= dist;
                toTarget.y /= dist;

                // Blend the current heading toward the target so the missile
                // banks into a turn instead of snapping instantly.
                Point3d cur = mSpeed;
                const float curLen = mathutils::calculate2dDistance(Point3d(0, 0, 0), cur);
                if (curLen > 0.001f) {
                    cur.x /= curLen;
                    cur.y /= curLen;
                    constexpr float kTurn = 0.10f;
                    cur.x += (toTarget.x - cur.x) * kTurn;
                    cur.y += (toTarget.y - cur.y) * kTurn;
                    const float len = mathutils::calculate2dDistance(Point3d(0, 0, 0), cur);
                    if (len > 0.001f) {
                        cur.x /= len;
                        cur.y /= len;
                    }
                } else {
                    cur = toTarget;
                }

                mSpeed = Point3d(cur.x * mVelocity, cur.y * mVelocity, 0);
                mAngle = mathutils::wrapRadians(mathutils::calculate2dAngle(Point3d(0, 0, 0), cur) - mathutils::DegreesToRads(90));
            }
        }
    }

    mPos += mSpeed;
    mPos += mDrift;
    mAngle -= mRotationRate;
    mAngle = fmodf(mAngle, 2.0f * PI);

    // Update the model's matrix
    mModel.Identity();
    mModel.Scale(mScale);
    mModel.Rotate(mAngle);
    mModel.Translate(mPos);

    mDrift *= .95;

    ++mAnimTick;
    if (mFlashTimer > 0)
        --mFlashTimer;

    if ((mState == ENTITY_STATE_RUNNING) && (mType != 3)) {
        // A short, throttled particle trail on top of the procedural plume.
        // Beams are drawn as a single continuous streak instead: particles
        // left behind by a fast-moving bolt read as beads, not as light.
        const int trailInterval = (mType == 4) ? 5 : 6;
        if ((mAnimTick % trailInterval) == 0) {
            emitExhaustParticle();
        }
    }

    if (this->getEnabled()) {
        // Hit test 10 interpolated positions against enemies
        bool hit = false;
        for (int i = 0; i < 10; i++) {
            float amount1 = i / 10.0f;
            float amount2 = 1 - amount1;

            Point3d pos((mPos.x * amount1) + (mLastPos.x * amount2), (mPos.y * amount1) + (mLastPos.y * amount2), 0);

            entity* enemy = theGame->mEnemies->hitTestEnemiesAtPosition(pos, getRadius(), true);
            if (enemy) {
                // Lasers pierce: they damage every enemy in their path and
                // only stop when they leave the grid.
                if (mType != 3) {
                    setState(ENTITY_STATE_DESTROY_TRANSITION);
                }
                enemy->hit(this);

                // Sparks at the impact point give the hit immediate feedback.
                emitImpactSparks(pos, (mType == 3) ? 3 : 5);
            }
        }

        if (!hit) {
            if (theGame->mGrid->hitTest(mPos, 0)) {
                // Hit the edge of the grid - destroy it
                setState(ENTITY_STATE_DESTROY_TRANSITION);
                mSpeed = Point3d(0, 0, 0);

                theGame->mSound->playTrack(SOUNDID_MISSILEHITWALL);
            }

            // Here be an attractor
            if (mType == 0) {
                attractor::Attractor* att = theGame->mAttractors->getAttractor();
                if (att) {
                    att->strength = 10;
                    att->radius = 5;
                    att->pos = mPos;
                    att->enabled = true;
                    att->attractsParticles = false;
                }
            } else if (mType == 1) {
                attractor::Attractor* att = theGame->mAttractors->getAttractor();
                if (att) {
                    att->strength = 20;
                    att->radius = 5;
                    att->pos = mPos;
                    att->enabled = true;
                    att->attractsParticles = false;
                }
            } else if (mType == 2) {
                attractor::Attractor* att = theGame->mAttractors->getAttractor();
                if (att) {
                    att->strength = 30;
                    att->radius = 5;
                    att->pos = mPos;
                    att->enabled = true;
                    att->attractsParticles = false;
                }
            }
        }
    }
}

void entityPlayerMissile::spawnTransition()
{
    spawn();
    setState(ENTITY_STATE_RUNNING);
    mRotationRate = 0;

    // Reset the per-shot visual state and kick off the launch flash.
    mAnimTick = 0;
    mFlashTimer = kLaunchFlashFrames;
    syncPenFromPlayer();

    run();
}

void entityPlayerMissile::destroyTransition()
{
    mStateTimer = mDestroyTime;
    entity::destroy();

    setState(ENTITY_STATE_INACTIVE); // kill it off immediately
    mLife = 0;                       // no leftover life when the slot is reused
    mFlashTimer = 0;

    // Throw out a denser, brighter burst so weapon impacts feel snappy.
    Point3d pos(this->mPos);
    Point3d angle(0, 0, 0);
    float spread = kTwoPi;

    float speed = 3.0f;
    int num = 26;
    int timeToLive = 220;

    // Beams are pure energy: a fast, short-lived flash. Homing missiles carry
    // more fuel, so they get a slightly bigger and longer-lived fireball.
    if (mType == 3) {
        speed = 4.6f;
        num = 18;
        timeToLive = 130;
    } else if (mType == 4) {
        speed = 3.4f;
        num = 30;
        timeToLive = 240;
    }

    vector::pen pen = mPen;
    pen.lineRadius = 6;
    pen.a = .85f;
    theGame->mParticles->emitter(&pos, &angle, speed, spread, num, &pen, timeToLive);

    // White-hot core flash on top of the coloured debris.
    vector::pen core(1.0f, 0.94f, 0.78f, 9.0f, 5.0f);
    theGame->mParticles->emitter(&pos, &angle, speed * 1.8f, spread, 8, &core, timeToLive / 2, false, false, .90f, true);
}

void entityPlayerMissile::syncPenFromPlayer()
{
    switch (mPlayerSource) {
    case 0:
        mPen = theGame->getPlayer1()->getMissilesPen();
        break;
    case 1:
        mPen = theGame->getPlayer2()->getMissilesPen();
        break;
    case 2:
        mPen = theGame->getPlayer3()->getMissilesPen();
        break;
    case 3:
        mPen = theGame->getPlayer4()->getMissilesPen();
        break;
    default:
        break;
    }
}

void entityPlayerMissile::emitExhaustParticle()
{
    if (!theGame->mParticles)
        return;

    const Point3d dir = normalize2d(mSpeed, Point3d(1, 0, 0));
    Point3d pos = mPos - (dir * 1.6f);

    vector::pen pen = mPen;
    float spread = 0.3f;
    float speed = 0.35f;

    int timeToLive = 24;
    float drag = .84f;

    if (mType == 4) {
        pen = vector::pen(1.0f, 0.62f, 0.22f, 2.0f, 4.0f);
        spread = 0.25f;
        timeToLive = 32;
        drag = .86f;
    } else {
        pen.a = 1.6f;
        pen.lineRadius = 3.0f;
    }

    Point3d angle(mathutils::calculate2dAngle(Point3d(0, 0, 0), mSpeed) + PI);
    theGame->mParticles->emitter(&pos, &angle, speed, spread, 1, &pen, timeToLive, false, false, drag, true);
}

void entityPlayerMissile::emitImpactSparks(const Point3d& pos, int count)
{
    if (!theGame->mParticles)
        return;

    Point3d center(pos);
    Point3d angle(0, 0, 0);

    // White-hot sparks thrown straight out of the hit point.
    vector::pen spark(1.0f, 0.95f, 0.78f, 7.0f, 4.0f);
    theGame->mParticles->emitter(&center, &angle, 3.4f, kTwoPi, count, &spark, 70, false, false, .88f, true);

    // A couple of the weapon's own colour for team identity.
    vector::pen tint = mPen;
    tint.a = 4.5f;
    tint.lineRadius = 3.0f;
    theGame->mParticles->emitter(&center, &angle, 2.4f, kTwoPi, 2, &tint, 55, false, false, .90f, true);
}

void entityPlayerMissile::drawLaunchFlash()
{
    if (mFlashTimer <= 0)
        return;

    const float progress = clamp01(1.0f - (static_cast<float>(mFlashTimer) / kLaunchFlashFrames));
    const float fade = 1.0f - progress;
    const float radius = 0.8f + (3.2f * progress);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    drawGlowOrb(mPos.x, mPos.y, radius * 0.6f, 1.0f, 1.0f, 1.0f, 0.40f * fade);
    drawGlowOrb(mPos.x, mPos.y, radius, mPen.r, mPen.g, mPen.b, 0.20f * fade);
    drawRing(mPos.x, mPos.y, radius * 1.25f, mPen, 0.35f * fade);
}

void entityPlayerMissile::draw()
{
    if (getState() != entity::ENTITY_STATE_RUNNING)
        return;

    syncPenFromPlayer();

    switch (mType) {
    case 3:
        drawLaser();
        break;
    case 4:
        drawHomingMissile();
        break;
    default:
        drawStandardMissile();
        break;
    }

    drawLaunchFlash();
}

void entityPlayerMissile::drawLaser()
{
    const Point3d dir = normalize2d(mSpeed, Point3d(1, 0, 0));
    const Point3d perp(-dir.y, dir.x, 0.0f);

    // Energy pulse/flicker driven by the logic frame counter so both render
    // passes stay in sync.
    const float pulse = 0.92f + (0.08f * std::sin(mAnimTick * 0.55f));
    const float flicker = 0.85f + (0.15f * std::sin((mAnimTick * 1.7f) + 0.9f));
    const float passBoost = (scene::mPass == scene::RENDERPASS_BLUR) ? 2.0f : 1.0f;

    constexpr float kLength = 30.0f;
    const Point3d tip = mPos;
    const Point3d tail = tip - (dir * kLength);
    const Point3d mid = tip - (dir * (kLength * 0.55f));
    const Point3d coreStart = tip - (dir * (kLength * 0.78f));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    // Wide colour halo: invisible at the tail, brightest at the bolt head.
    drawTaperedSegment(tail, mid, 14.0f * pulse * passBoost, mPen, 0.0f, 0.09f);
    drawTaperedSegment(mid, tip, 14.0f * pulse * passBoost, mPen, 0.09f, 0.18f);

    // Inner glow.
    drawTaperedSegment(tail, mid, 6.0f * passBoost, mPen, 0.0f, 0.26f);
    drawTaperedSegment(mid, tip, 6.0f * passBoost, mPen, 0.26f, 0.55f);

    // White-hot core, only over the forward part of the beam.
    const vector::pen white(1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
    drawTaperedSegment(coreStart, mid, 2.4f, white, 0.0f, 0.70f);
    drawTaperedSegment(mid, tip, 2.4f, white, 0.70f, 1.0f);

    // Scrolling energy ticks running down the beam sell its speed.
    glLineWidth(1.6f);
    glBegin(GL_LINES);
    const float scroll = std::fmod(mAnimTick * 2.2f, 7.0f);
    for (int i = 0; i < 4; ++i) {
        const float behind = scroll + (i * 7.0f);
        if (behind > kLength)
            continue;

        const float fade = 1.0f - (behind / kLength);
        const Point3d p = tip - (dir * behind);
        const float half = 0.9f + (0.5f * fade);

        glColor4f(1.0f, 1.0f, 1.0f, 0.55f * fade);
        glVertex3f(p.x + (perp.x * half), p.y + (perp.y * half), 0.0f);
        glColor4f(mPen.r, mPen.g, mPen.b, 0.35f * fade);
        glVertex3f(p.x - (perp.x * half), p.y - (perp.y * half), 0.0f);
    }
    glEnd();

    // Bright bolt head with a soft coloured bloom around it.
    drawGlowOrb(tip.x, tip.y, 3.4f * flicker * passBoost, mPen.r, mPen.g, mPen.b, 0.45f);
    drawGlowOrb(tip.x, tip.y, 1.5f * flicker, 1.0f, 1.0f, 1.0f, 0.95f);

    // Little leading-edge star.
    const Point3d lead = tip + (dir * 0.7f);
    glLineWidth(1.4f);
    glBegin(GL_LINES);
    glColor4f(1.0f, 1.0f, 1.0f, 0.85f);
    glVertex3f(lead.x + (perp.x * 1.6f), lead.y + (perp.y * 1.6f), 0.0f);
    glVertex3f(lead.x - (perp.x * 1.6f), lead.y - (perp.y * 1.6f), 0.0f);
    glVertex3f(lead.x - (dir.x * 1.1f), lead.y - (dir.y * 1.1f), 0.0f);
    glVertex3f(lead.x + (dir.x * 1.4f), lead.y + (dir.y * 1.4f), 0.0f);
    glEnd();
}

void entityPlayerMissile::drawStandardMissile()
{
    const Point3d dir = normalize2d(mSpeed, Point3d(1, 0, 0));
    const Point3d perp(-dir.y, dir.x, 0.0f);
    const float passBoost = (scene::mPass == scene::RENDERPASS_BLUR) ? 2.0f : 1.0f;

    const float flicker = 0.70f + (0.30f * std::sin(mAnimTick * 1.9f));
    const float pulse = 0.80f + (0.20f * std::sin((mAnimTick * 0.7f) + 2.0f));

    const Point3d nozzle = mPos - (dir * 1.35f);
    const Point3d plumeTip = nozzle - (dir * (4.5f + (2.5f * flicker))) + (perp * (0.45f * std::sin(mAnimTick * 2.7f)));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    // Exhaust: a soft colour halo, a tapered outer flame and a hot inner core.
    drawGlowOrb(nozzle.x, nozzle.y, (1.1f + (0.3f * flicker)) * passBoost, mPen.r, mPen.g, mPen.b, 0.11f * flicker);
    drawTaperedSegment(nozzle, plumeTip, 7.0f * pulse * passBoost, mPen, 0.40f * flicker, 0.0f);
    drawTaperedSegment(nozzle, nozzle - (dir * (2.0f + (1.4f * flicker))), 3.0f, vector::pen(1.0f, 0.95f, 0.80f, 1.0f, 0.0f), 0.90f, 0.0f);

    // Soft glow silhouette, then the solid body with a white-hot centre line.
    vector::pen glowPen = mPen;
    glowPen.lineRadius = 18.0f;
    glowPen.a = 0.20f * pulse;

    mModel.Identity();
    mModel.Scale(mScale * 1.55f);
    mModel.Rotate(mAngle);
    mModel.Translate(mPos);
    mModel.draw(glowPen);

    vector::pen bodyPen = mPen;
    bodyPen.r = (mPen.r * 0.7f) + 0.3f;
    bodyPen.g = (mPen.g * 0.7f) + 0.3f;
    bodyPen.b = (mPen.b * 0.7f) + 0.3f;
    bodyPen.a = 1.0f;
    bodyPen.lineRadius = 8.0f;
    mPen = bodyPen;
    entity::draw();

    const Point3d tail = mPos - (dir * 0.9f);
    const Point3d nose = mPos + (dir * 1.2f);
    drawTaperedSegment(tail, nose, 1.3f, vector::pen(1.0f, 1.0f, 1.0f, 1.0f, 0.0f), 0.35f, 0.90f);

    // Exhaust sparks tumbling off the plume.
    glPointSize(2.0f + (1.5f * flicker));
    glBegin(GL_POINTS);
    glColor4f(1.0f, 0.85f, 0.55f, 0.70f * flicker);
    glVertex3f(nozzle.x + (perp.x * 0.5f), nozzle.y + (perp.y * 0.5f), 0.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 0.50f * flicker);
    glVertex3f(plumeTip.x - (perp.x * 0.4f), plumeTip.y - (perp.y * 0.4f), 0.0f);
    glEnd();
}

void entityPlayerMissile::drawHomingMissile()
{
    const Point3d dir = normalize2d(mSpeed, Point3d(1, 0, 0));
    const Point3d perp(-dir.y, dir.x, 0.0f);
    const float passBoost = (scene::mPass == scene::RENDERPASS_BLUR) ? 2.0f : 1.0f;

    const float flicker = 0.70f + (0.30f * std::sin(mAnimTick * 2.3f));
    const Point3d nozzle = mPos - (dir * 1.2f);
    const Point3d plumeTip = nozzle - (dir * (5.0f + (2.2f * flicker))) + (perp * (0.55f * std::sin(mAnimTick * 3.1f)));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    // Smoke haze trailing behind the flame.
    drawGlowOrb(plumeTip.x, plumeTip.y, (3.0f + (0.8f * flicker)) * passBoost, 0.55f, 0.55f, 0.55f, 0.10f);
    drawGlowOrb((nozzle.x + plumeTip.x) * 0.5f, (nozzle.y + plumeTip.y) * 0.5f, 2.2f * passBoost, 0.68f, 0.68f, 0.68f, 0.07f);

    // Amber flame with a white-hot core.
    drawTaperedSegment(nozzle, plumeTip, 4.5f * passBoost, vector::pen(1.0f, 0.55f, 0.15f, 1.0f, 0.0f), 0.80f, 0.0f);
    drawTaperedSegment(nozzle, nozzle - (dir * (2.6f + (1.2f * flicker))), 1.8f, vector::pen(1.0f, 0.92f, 0.75f, 1.0f, 0.0f), 0.95f, 0.0f);
    drawGlowOrb(nozzle.x, nozzle.y, 1.0f * flicker * passBoost, 1.0f, 0.60f, 0.20f, 0.30f);

    // Body glow and solid body.
    vector::pen glowPen = mPen;
    glowPen.lineRadius = 18.0f;
    glowPen.a = 0.20f;

    mModel.Identity();
    mModel.Scale(mScale * 1.55f);
    mModel.Rotate(mAngle);
    mModel.Translate(mPos);
    mModel.draw(glowPen);

    vector::pen bodyPen = mPen;
    bodyPen.r = (mPen.r * 0.65f) + 0.35f;
    bodyPen.g = (mPen.g * 0.65f) + 0.35f;
    bodyPen.b = (mPen.b * 0.65f) + 0.35f;
    bodyPen.a = 1.0f;
    bodyPen.lineRadius = 10.0f;
    mPen = bodyPen;
    entity::draw();

    // Blinking seeker beacon on the nose.
    const Point3d nose = mPos + (dir * 1.4f);
    if ((mAnimTick / 8) & 1) {
        drawGlowOrb(nose.x, nose.y, 1.4f * passBoost, 1.0f, 0.20f, 0.15f, 0.70f);
        drawGlowOrb(nose.x, nose.y, 0.55f, 1.0f, 1.0f, 1.0f, 0.95f);
    } else {
        drawGlowOrb(nose.x, nose.y, 0.7f, 1.0f, 0.20f, 0.15f, 0.25f);
    }
}
