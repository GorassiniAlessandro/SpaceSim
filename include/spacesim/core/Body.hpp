#pragma once

#include <string>

#include "spacesim/core/Vec3.hpp"

namespace spacesim::core {

struct Body {
    std::string name;
    double mass = 1.0;
    Vec3 position{};
    Vec3 velocity{};
    bool isStatic = false;
};

} // namespace spacesim::core
