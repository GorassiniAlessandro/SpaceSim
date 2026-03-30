#pragma once

#include <string>

#include "spacesim/core/Vec3.hpp"

namespace spacesim::core {

enum class BodyKind {
    Generic,
    Star,
    Planet,
    Asteroid,
    BlackHole
};

struct Body {
    std::string name;
    BodyKind kind = BodyKind::Generic;
    double mass = 1.0;
    Vec3 position{};
    Vec3 velocity{};
    bool isStatic = false;
    double eventHorizonRadius = 0.0;
};

} // namespace spacesim::core
