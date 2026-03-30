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
    double radius = 1.0;  // Raggio fisico del corpo
    Vec3 position{};
    Vec3 velocity{};
    bool isStatic = false;
    double eventHorizonRadius = 0.0;
    
    // Colore RGBA per rendering (0.0-1.0)
    float colorR = 1.0f;
    float colorG = 1.0f;
    float colorB = 1.0f;
    float colorA = 1.0f;
};

} // namespace spacesim::core
