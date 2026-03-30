#include "spacesim/core/PhysicsEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace spacesim::core {

PhysicsEngine::PhysicsEngine(SimulationConfig config) : config_(config) {}

StepReport PhysicsEngine::step(World& world, double dt) const {
    auto& bodies = world.bodies();
    if (bodies.empty()) {
        return {};
    }

    const auto accelerationsStart = computeAccelerations(bodies);

    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].isStatic) {
            continue;
        }

        const Vec3 displacement = (bodies[i].velocity * dt) + (accelerationsStart[i] * (0.5 * dt * dt));
        bodies[i].position += displacement;
    }

    const auto accelerationsEnd = computeAccelerations(bodies);

    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].isStatic) {
            continue;
        }

        bodies[i].velocity += (accelerationsStart[i] + accelerationsEnd[i]) * (0.5 * dt);
    }

    return applyBlackHoleAbsorption(world);
}

std::vector<Vec3> PhysicsEngine::computeAccelerations(const std::vector<Body>& bodies) const {
    std::vector<Vec3> accelerations(bodies.size(), Vec3{});

    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].isStatic) {
            continue;
        }

        for (size_t j = 0; j < bodies.size(); ++j) {
            if (i == j) {
                continue;
            }

            const Vec3 delta = bodies[j].position - bodies[i].position;
            const double distanceSq = delta.lengthSquared() + config_.softeningLengthSquared;
            const double distance = std::sqrt(distanceSq);

            if (distance <= 0.0) {
                continue;
            }

            const double accelMagnitude = (config_.gravitationalConstant * bodies[j].mass) / distanceSq;
            accelerations[i] += delta * (accelMagnitude / distance);
        }
    }

    return accelerations;
}

StepReport PhysicsEngine::applyBlackHoleAbsorption(World& world) const {
    auto& bodies = world.bodies();
    StepReport report;

    if (bodies.size() < 2) {
        return report;
    }

    struct PendingAbsorption {
        std::size_t absorberIndex;
        std::size_t absorbedIndex;
        double absorbedMass;
        std::string absorberName;
        std::string absorbedName;
    };

    std::vector<size_t> absorbedIndexes;
    std::vector<PendingAbsorption> pendingAbsorptions;

    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].kind != BodyKind::BlackHole || bodies[i].eventHorizonRadius <= 0.0) {
            continue;
        }

        for (size_t j = 0; j < bodies.size(); ++j) {
            if (i == j || bodies[j].kind == BodyKind::BlackHole) {
                continue;
            }

            const Vec3 delta = bodies[j].position - bodies[i].position;
            if (delta.length() <= bodies[i].eventHorizonRadius) {
                pendingAbsorptions.push_back(
                    {i, j, bodies[j].mass, bodies[i].name, bodies[j].name});
                bodies[i].mass += bodies[j].mass;
                absorbedIndexes.push_back(j);
            }
        }
    }

    if (absorbedIndexes.empty()) {
        return report;
    }

    std::sort(absorbedIndexes.begin(), absorbedIndexes.end());
    absorbedIndexes.erase(std::unique(absorbedIndexes.begin(), absorbedIndexes.end()), absorbedIndexes.end());

    report.absorptionEvents.reserve(pendingAbsorptions.size());
    for (const auto& absorption : pendingAbsorptions) {
        report.absorptionEvents.push_back(
            {absorption.absorberName, absorption.absorbedName, absorption.absorbedMass});
        report.absorbedMass += absorption.absorbedMass;
    }
    report.absorbedBodies = report.absorptionEvents.size();

    for (auto it = absorbedIndexes.rbegin(); it != absorbedIndexes.rend(); ++it) {
        bodies.erase(bodies.begin() + static_cast<std::vector<Body>::difference_type>(*it));
    }

    return report;
}

} // namespace spacesim::core
