#include "spacesim/core/PhysicsEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <vector>

namespace spacesim::core {

PhysicsEngine::PhysicsEngine(SimulationConfig config) : config_(config) {}

void PhysicsEngine::setConfig(const SimulationConfig& config) {
    config_ = config;
}

const SimulationConfig& PhysicsEngine::config() const {
    return config_;
}

StepReport PhysicsEngine::step(World& world, double dt) const {
    auto& bodies = world.bodies();
    if (bodies.empty()) {
        return {};
    }

    StepReport stepReport;

    // Clamp globale del dt richiesto per evitare overflow numerici.
    const double requestedDt = std::min(std::max(dt, 0.0), 1e9);
    double remainingDt = requestedDt;
    if (remainingDt <= 0.0) {
        stepReport.diagnostics = computeDiagnostics(bodies);
        return stepReport;
    }

    constexpr std::size_t kMaxSubsteps = 2048;
    std::size_t substeps = 0;

    while (remainingDt > 0.0 && substeps < kMaxSubsteps) {
        const auto accelerationsStart = computeAccelerations(bodies);

        double maxAccel = 0.0;
        double maxSpeed = 0.0;
        double minDynamicalDt = 1e9;
        for (size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].isStatic) {
                continue;
            }
            maxAccel = std::max(maxAccel, accelerationsStart[i].length());
            maxSpeed = std::max(maxSpeed, bodies[i].velocity.length());
        }

        double minDistance = std::numeric_limits<double>::max();
        double maxPairMass = 0.0;
        // Calculate minDistance between ANY pair of bodies (including static ones)
        // This is needed for collision detection and proper timestep adjustment
        for (size_t i = 0; i < bodies.size(); ++i) {
            for (size_t j = i + 1; j < bodies.size(); ++j) {
                const double d = (bodies[j].position - bodies[i].position).length();
                if (d > 0.0 && d < minDistance) {
                    minDistance = d;
                }
                // Only count pairs where at least one is dynamic
                if (!bodies[i].isStatic || !bodies[j].isStatic) {
                    const double pairMass = bodies[i].mass + bodies[j].mass;
                    if (pairMass > 0.0) {
                        maxPairMass = std::max(maxPairMass, pairMass);
                    }
                }
            }
        }
        if (minDistance == std::numeric_limits<double>::max()) {
            minDistance = std::sqrt(config_.softeningLengthSquared);
        }
        if (maxPairMass > 0.0 && minDistance > 0.0) {
            minDynamicalDt = 0.02 * std::sqrt((minDistance * minDistance * minDistance) /
                                              (config_.gravitationalConstant * maxPairMass));
        }

        const double dtAccel =
            (maxAccel > 1e-12)
                ? std::sqrt((0.02 * std::max(config_.softeningLengthSquared, 1.0)) / maxAccel)
                : 1e9;
        const double dtSpeed =
            (maxSpeed > 1e-9)
                ? (0.01 * std::max(minDistance, 1.0) / maxSpeed)
                : 1e9;

        // Limiti pratici: minimo 1 ms, massimo 1 giorno per substep.
        double subDt = std::min(remainingDt, std::min(dtAccel, std::min(dtSpeed, minDynamicalDt)));
        subDt = std::clamp(subDt, 1e-3, 86400.0);

        for (size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].isStatic) {
                continue;
            }

            const Vec3 displacement =
                (bodies[i].velocity * subDt) + (accelerationsStart[i] * (0.5 * subDt * subDt));
            bodies[i].position += displacement;
        }

        const auto accelerationsEnd = computeAccelerations(bodies);

        for (size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].isStatic) {
                continue;
            }

            bodies[i].velocity += (accelerationsStart[i] + accelerationsEnd[i]) * (0.5 * subDt);

            if (config_.numericalDampingFactor < 1.0) {
                const double baseDt = std::max(config_.fixedTimeStepSeconds, 1.0);
                const double dampingExponent = subDt / baseDt;
                const double damping = std::pow(config_.numericalDampingFactor, dampingExponent);
                bodies[i].velocity *= damping;
            }
        }

        remainingDt -= subDt;
        ++substeps;
    }

    stepReport.substepsUsed = substeps;
    stepReport.integratedDt = requestedDt - remainingDt;

    // Apply collision merging first, then black hole absorption
    const auto collisionReport = applyCollisionHandling(world);
    stepReport.absorbedBodies = collisionReport.absorbedBodies;
    stepReport.absorbedMass = collisionReport.absorbedMass;
    stepReport.absorptionEvents = collisionReport.absorptionEvents;

    // Then apply black hole absorption (if enabled)
    const auto absorptionReport = applyBlackHoleAbsorption(world);
    stepReport.absorbedBodies += absorptionReport.absorbedBodies;
    stepReport.absorbedMass += absorptionReport.absorbedMass;
    for (const auto& evt : absorptionReport.absorptionEvents) {
        stepReport.absorptionEvents.push_back(evt);
    }

    stepReport.diagnostics = computeDiagnostics(world.bodies());

    return stepReport;
}

SystemDiagnostics PhysicsEngine::measure(const World& world) const {
    return computeDiagnostics(world.bodies());
}

std::vector<Vec3> PhysicsEngine::computeAccelerations(const std::vector<Body>& bodies) const {
    std::vector<Vec3> accelerations(bodies.size(), Vec3{});

    const bool useNewtonianPairwise =
        config_.gravityModel == GravityModel::Newtonian ||
        config_.gravityModel == GravityModel::Relativistic ||
        config_.gravityModel == GravityModel::Hybrid;
    const bool useRelativisticCorrection =
        config_.gravityModel == GravityModel::Relativistic ||
        config_.gravityModel == GravityModel::Hybrid;

    if (!useNewtonianPairwise) {
        return accelerations;
    }

    const double c2 = config_.lightSpeed * config_.lightSpeed;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const Vec3 delta = bodies[j].position - bodies[i].position;
            const double distanceSq = delta.lengthSquared() + config_.softeningLengthSquared;
            const double distance = std::sqrt(distanceSq);

            if (distance <= 0.0) {
                continue;
            }

            double correction = 1.0;
            if (useRelativisticCorrection && c2 > 0.0) {
                const Vec3 vRel = bodies[i].velocity - bodies[j].velocity;
                const Vec3 h = cross(delta, vRel);
                const double h2 = h.lengthSquared();
                correction += (3.0 * h2) / (distanceSq * c2);

                // Keep correction bounded for numerical stability in this simplified model.
                if (correction > 50.0) {
                    correction = 50.0;
                }
            }

            const double invDistance = 1.0 / distance;
            const double invDistanceCubed = invDistance / distanceSq;
            const Vec3 accelToJ = delta * (config_.gravitationalConstant * bodies[i].mass * correction * invDistanceCubed);
            const Vec3 accelToI = delta * (-config_.gravitationalConstant * bodies[j].mass * correction * invDistanceCubed);

            if (!bodies[i].isStatic) {
                accelerations[i] += accelToJ;
            }
            if (!bodies[j].isStatic) {
                accelerations[j] += accelToI;
            }
        }
    }

    return accelerations;
}

SystemDiagnostics PhysicsEngine::computeDiagnostics(const std::vector<Body>& bodies) const {
    SystemDiagnostics diagnostics;
    if (bodies.empty()) {
        return diagnostics;
    }

    diagnostics.minDistance = std::numeric_limits<double>::infinity();

    for (const auto& body : bodies) {
        diagnostics.totalMass += body.mass;
        diagnostics.kineticEnergy += 0.5 * body.mass * body.velocity.lengthSquared();
        diagnostics.totalMomentum += body.velocity * body.mass;
        diagnostics.totalAngularMomentum += cross(body.position, body.velocity * body.mass);
        diagnostics.centerOfMass += body.position * body.mass;
        diagnostics.centerOfMassVelocity += body.velocity * body.mass;
        diagnostics.maxSpeed = std::max(diagnostics.maxSpeed, body.velocity.length());
    }

    if (diagnostics.totalMass > 0.0) {
        diagnostics.centerOfMass *= (1.0 / diagnostics.totalMass);
        diagnostics.centerOfMassVelocity *= (1.0 / diagnostics.totalMass);
    }

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const Vec3 delta = bodies[j].position - bodies[i].position;
            const double distance = std::sqrt(delta.lengthSquared() + config_.softeningLengthSquared);
            if (distance <= 0.0) {
                continue;
            }

            diagnostics.minDistance = std::min(diagnostics.minDistance, delta.length());
            diagnostics.potentialEnergy -=
                (config_.gravitationalConstant * bodies[i].mass * bodies[j].mass) / distance;
        }
    }

    if (!std::isfinite(diagnostics.minDistance)) {
        diagnostics.minDistance = 0.0;
    }

    const auto accelerations = computeAccelerations(bodies);
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].isStatic) {
            continue;
        }
        diagnostics.maxAcceleration = std::max(diagnostics.maxAcceleration, accelerations[i].length());
    }

    diagnostics.totalEnergy = diagnostics.kineticEnergy + diagnostics.potentialEnergy;
    diagnostics.virialRatio =
        (std::abs(diagnostics.potentialEnergy) > 0.0)
            ? ((2.0 * diagnostics.kineticEnergy) / std::abs(diagnostics.potentialEnergy))
            : 0.0;
    return diagnostics;
}

StepReport PhysicsEngine::applyCollisionHandling(World& world) const {
    auto& bodies = world.bodies();
    StepReport report;

    if (bodies.size() < 2) {
        return report;
    }

    if (!config_.enableCollisionMerging) {
        // Collision handling disabled
        return report;
    }

    const double collisionDistSq = config_.collisionDistanceThreshold * config_.collisionDistanceThreshold;

    struct PendingCollision {
        std::size_t bodyAIndex;
        std::size_t bodyBIndex;
        double mergedMass;
        std::string nameA;
        std::string nameB;
    };

    std::vector<std::size_t> collisionIndexes;  // All bodies involved in collisions
    std::vector<PendingCollision> pendingCollisions;
    std::vector<bool> isAlreadyAbsorbed(bodies.size(), false);

    // Detect all collisions (symmetric pairwise)
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (isAlreadyAbsorbed[i]) continue;

        for  (size_t j = i + 1; j < bodies.size(); ++j) {
            if (isAlreadyAbsorbed[j]) continue;

            const Vec3 delta = bodies[j].position - bodies[i].position;
            const double distSq = delta.lengthSquared();

            if (distSq <= collisionDistSq) {
                // Collision detected! Merge j into i (i absorbs j)
                // Save absorbed mass BEFORE updating bodies[i]
                const double absorbedMass = bodies[j].mass;
                // Conserve momentum: p_total = m_i*v_i + m_j*v_j = (m_i+m_j)*v_merged
                const double totalMass = bodies[i].mass + bodies[j].mass;
                const Vec3 mergedVelocity = (bodies[i].mass * bodies[i].velocity + bodies[j].mass * bodies[j].velocity) / totalMass;
                
                // Merge position to center of mass
                const Vec3 mergedPosition = (bodies[i].mass * bodies[i].position + bodies[j].mass * bodies[j].position) / totalMass;

                // Update absorber body
                bodies[i].mass = totalMass;
                bodies[i].velocity = mergedVelocity;
                bodies[i].position = mergedPosition;

                // Mark j as absorbed
                isAlreadyAbsorbed[j] = true;
                collisionIndexes.push_back(j);

                pendingCollisions.push_back({i, j, absorbedMass, bodies[i].name, bodies[j].name});
                report.absorbedMass += absorbedMass;
            }
        }
    }

    if (collisionIndexes.empty()) {
        // No collisions detected this step
        return report;
    }

    // Sort and remove duplicates
    std::sort(collisionIndexes.begin(), collisionIndexes.end());
    collisionIndexes.erase(std::unique(collisionIndexes.begin(), collisionIndexes.end()), collisionIndexes.end());

    // Record collision events
    report.absorptionEvents.reserve(pendingCollisions.size());
    for (const auto& collision : pendingCollisions) {
        report.absorptionEvents.push_back({collision.nameA, collision.nameB, collision.mergedMass});
    }
    report.absorbedBodies = report.absorptionEvents.size();

    // Remove absorbed bodies in reverse order to maintain correct indices
    for (auto it = collisionIndexes.rbegin(); it != collisionIndexes.rend(); ++it) {
        bodies.erase(bodies.begin() + static_cast<std::vector<Body>::difference_type>(*it));
    }

    return report;
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
