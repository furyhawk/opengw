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
            const float dx = p->posStream[0].x - apoint.x;
            const float dy = p->posStream[0].y - apoint.y;
            const float distanceSquared = dx * dx + dy * dy;
            const float radiusSquared = a.radius * a.radius;

            if (distanceSquared < radiusSquared) {
                const float distance = std::sqrt(distanceSquared < 1.0e-5f ? 1.0e-5f : distanceSquared);
                const float r = 1.0f / (distance * distance);
                const float angle = std::atan2(dy, dx);

                // Add a slight curving vector to the gravity
                Point3d gravityVector(-r * a.strength * .5f, 0.0f, 0.0f);                  // .5
                Point3d g = mathutils::rotate2dPoint(gravityVector, angle + .25f); // .35 , .7

                speed.x += g.x;
                speed.y += g.y;
            }
        }
    }

    return speed;
}
