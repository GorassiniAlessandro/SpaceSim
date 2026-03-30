#pragma once

#include <vector>

#include "spacesim/core/SimulationConfig.hpp"
#include "spacesim/core/World.hpp"

namespace spacesim::core {

class PhysicsEngine {
public:
    explicit PhysicsEngine(SimulationConfig config = {});
    void step(World& world, double dt) const;

private:
    [[nodiscard]] std::vector<Vec3> computeAccelerations(const std::vector<Body>& bodies) const;
    void applyBlackHoleAbsorption(World& world) const;

    SimulationConfig config_;
};

} // namespace spacesim::core
