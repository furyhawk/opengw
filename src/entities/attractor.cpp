#include "entities/attractor.hpp"
#include "core/game.hpp"
#include "math/mathutils.hpp"
#include "core/settings.hpp"

#include <cmath>

attractor::attractor()
{
    // PERFORMANCE: If you set this too high things will start to slow down in grid::run()!!
    mAttractors.resize(settings::get().mAttractors);
}

attractor::Attractor* attractor::getAttractor()
{
    for (auto& a: mAttractors) {
        if (!a.enabled) {
            return &a;
        }
    }

    return nullptr;
}

void attractor::clearAll()
{
    for (auto& a: mAttractors) {
        a.enabled = false;
    }
}

Point3d attractor::evaluateParticle(particle::PARTICLE* p)
{
    Point3d speed(0.0f, 0.0f, 0.0f);

    for (const auto& a: mAttractors) {
        if (a.enabled && a.attractsParticles) {
            const Point3d& apoint = a.pos;
            const float dx = apoint.x - p->posStream[0].x;
            const float dy = apoint.y - p->posStream[0].y;
            const float distanceSquared = dx * dx + dy * dy;
            const float radiusSquared = a.radius * a.radius;

            if (distanceSquared < radiusSquared) {
                constexpr float minDistanceSquared = 1.0e-5f;
                const float clampedDistanceSquared = (distanceSquared > minDistanceSquared) ? distanceSquared : minDistanceSquared;
                const float r = 1.0f / clampedDistanceSquared;
                const float angle = std::atan2(dy, dx);

                // Keep the previous near-origin force clamp while still avoiding
                // per-particle sqrt/reciprocal-sqrt work.
                Point3d gravityVector(-r * a.strength * .5f, 0.0f, 0.0f);
                Point3d g = mathutils::rotate2dPoint(gravityVector, angle + .25f);

                speed.x += g.x;
                speed.y += g.y;
            }
        }
    }

    return speed;
}
