#pragma once

#include "spacesim/core/World.hpp"

namespace spacesim::core {

class PhysicsEngine {
public:
    void step(World& world, double dt) const;

private:
    static constexpr double kGravitationalConstant = 6.67430e-11;
    static constexpr double kSoftening = 1e3;
};

} // namespace spacesim::core
