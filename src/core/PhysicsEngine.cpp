#include "spacesim/core/PhysicsEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
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

    // Calcola accelerazione massima per adaptive timestep
    const auto accelerationsStart = computeAccelerations(bodies);
    
    double maxAccel = 0.0;
    for (const auto& accel : accelerationsStart) {
        const double len = accel.length();
        if (len > maxAccel) maxAccel = len;
    }
    
    // Limita timestep in base all'accelerazione massima per stabilità
    // Formula: dt_safe <= sqrt(2 * error_tol / max_accel)
    // Usiamo constraint conservativo: max dt = 1 giorno quando accel è alta
    double safedt = dt;
    if (maxAccel > 1e-10) {
        const double maxDtForAccel = 1e8;  // ~1.15 giorni massimo
        if (safedt > maxDtForAccel) {
            safedt = maxDtForAccel;
        }
    }
    
    // Cap finale: mai più di 1 miliardo di secondi
    safedt = std::min(safedt, 1e9);

    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].isStatic) {
            continue;
        }

        // Velocity-Verlet: x(t+dt) = x(t) + v(t)*dt + 0.5*a(t)*dt^2
        const Vec3 displacement = (bodies[i].velocity * safedt) + (accelerationsStart[i] * (0.5 * safedt * safedt));
        bodies[i].position += displacement;
    }

    const auto accelerationsEnd = computeAccelerations(bodies);

    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].isStatic) {
            continue;
        }

        // v(t+dt) = v(t) + 0.5*(a(t) + a(t+dt))*dt
        bodies[i].velocity += (accelerationsStart[i] + accelerationsEnd[i]) * (0.5 * safedt);
        
        // Applica minimo damping per ridurre drift numerico senza rubare energia
        bodies[i].velocity *= config_.numericalDampingFactor;
    }

    return applyBlackHoleAbsorption(world);
}

std::vector<Vec3> PhysicsEngine::computeAccelerations(const std::vector<Body>& bodies) const {
    std::vector<Vec3> accelerations(bodies.size(), Vec3{});

    const bool useGalilean =
        config_.gravityModel == GravityModel::Galilean ||
        config_.gravityModel == GravityModel::Hybrid;
    const bool useNewtonianPairwise =
        config_.gravityModel == GravityModel::Newtonian ||
        config_.gravityModel == GravityModel::Relativistic ||
        config_.gravityModel == GravityModel::Hybrid;
    const bool useRelativisticCorrection =
        config_.gravityModel == GravityModel::Relativistic ||
        config_.gravityModel == GravityModel::Hybrid;

    if (useGalilean) {
        for (size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].isStatic) {
                continue;
            }
            accelerations[i] += config_.galileanField;
        }
    }

    if (!useNewtonianPairwise) {
        return accelerations;
    }

    const double c2 = config_.lightSpeed * config_.lightSpeed;

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

            const double accelMagnitude =
                ((config_.gravitationalConstant * bodies[j].mass) / distanceSq) * correction;
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
