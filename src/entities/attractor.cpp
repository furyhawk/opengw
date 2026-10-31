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
        if (!a.enabled || !a.attractsParticles) {
            continue;
        }

        const Point3d& apoint = a.pos;
        const float dx = apoint.x - p->posStream[0].x;
        const float dy = apoint.y - p->posStream[0].y;
        const float distanceSquared = dx * dx + dy * dy;
        const float radiusSquared = a.radius * a.radius;

        if (distanceSquared < radiusSquared) {
            const float clampedDistanceSquared = (distanceSquared > 0.0f) ? distanceSquared : radiusSquared;
            const float inverseDistanceSquared = 1.0f / clampedDistanceSquared;
            const float angle = std::atan2(dy, dx) + .25f;
            const float force = a.strength * .5f * inverseDistanceSquared;

            // Keep the force direction identical to the existing attractor math,
            // but avoid the extra temporary point and rotate2d helper call on the
            // hot path. The cost of two trig calls is much lower than the extra
            // object churn and helper overhead across thousands of particles.
            speed.x -= std::cos(angle) * force;
            speed.y -= std::sin(angle) * force;
        }
    }

    return speed;
}
