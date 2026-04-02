#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "spacesim/core/SimulationConfig.hpp"
#include "spacesim/core/World.hpp"

namespace spacesim::core {

struct AbsorptionEvent {
    std::string absorberName;
    std::string absorbedName;
    double absorbedMass = 0.0;
};

struct SystemDiagnostics {
    double totalMass = 0.0;
    double kineticEnergy = 0.0;
    double potentialEnergy = 0.0;
    double totalEnergy = 0.0;
    double virialRatio = 0.0;
    Vec3 totalMomentum{};
    Vec3 totalAngularMomentum{};
    Vec3 centerOfMass{};
    Vec3 centerOfMassVelocity{};
    double minDistance = 0.0;
    double maxSpeed = 0.0;
    double maxAcceleration = 0.0;
};

struct StepReport {
    std::size_t absorbedBodies = 0;
    double absorbedMass = 0.0;
    std::vector<AbsorptionEvent> absorptionEvents;
    std::size_t substepsUsed = 0;
    double integratedDt = 0.0;
    SystemDiagnostics diagnostics{};
};

class PhysicsEngine {
public:
    explicit PhysicsEngine(SimulationConfig config = {});
    StepReport step(World& world, double dt) const;
    [[nodiscard]] SystemDiagnostics measure(const World& world) const;
    void setConfig(const SimulationConfig& config);
    [[nodiscard]] const SimulationConfig& config() const;

private:
    [[nodiscard]] std::vector<Vec3> computeAccelerations(const std::vector<Body>& bodies) const;
    [[nodiscard]] SystemDiagnostics computeDiagnostics(const std::vector<Body>& bodies) const;
    StepReport applyCollisionHandling(World& world) const;
    StepReport applyBlackHoleAbsorption(World& world) const;

    SimulationConfig config_;
};

} // namespace spacesim::core
