#include "spacesim/core/PhysicsEngine.hpp"

#include <vector>

namespace spacesim::core {

void PhysicsEngine::step(World& world, double dt) const {
    auto& bodies = world.bodies();
    std::vector<Vec3> accelerations(bodies.size(), Vec3{});

    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].isStatic) {
            continue;
        }

        for (size_t j = 0; j < bodies.size(); ++j) {
            if (i == j) {
                continue;
            }

            Vec3 delta = bodies[j].position - bodies[i].position;
            const double distanceSq = delta.lengthSquared() + kSoftening;
            const double distance = std::sqrt(distanceSq);

            if (distance <= 0.0) {
                continue;
            }

            const double accelMagnitude =
                (kGravitationalConstant * bodies[j].mass) / distanceSq;
            accelerations[i] += (delta * (accelMagnitude / distance));
        }
    }

    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].isStatic) {
            continue;
        }

        bodies[i].velocity += accelerations[i] * dt;
        bodies[i].position += bodies[i].velocity * dt;
    }
}

} // namespace spacesim::core
