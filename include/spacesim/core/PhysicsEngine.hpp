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

struct StepReport {
    std::size_t absorbedBodies = 0;
    double absorbedMass = 0.0;
    std::vector<AbsorptionEvent> absorptionEvents;
};

class PhysicsEngine {
public:
    explicit PhysicsEngine(SimulationConfig config = {});
    StepReport step(World& world, double dt) const;
    void setConfig(const SimulationConfig& config);
    [[nodiscard]] const SimulationConfig& config() const;

private:
    [[nodiscard]] std::vector<Vec3> computeAccelerations(const std::vector<Body>& bodies) const;
    StepReport applyBlackHoleAbsorption(World& world) const;

    SimulationConfig config_;
};

} // namespace spacesim::core
