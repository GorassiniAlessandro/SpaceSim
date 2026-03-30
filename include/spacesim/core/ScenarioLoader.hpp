#pragma once

#include <string>

#include "spacesim/core/World.hpp"

namespace spacesim::core {

class ScenarioLoader {
public:
    bool loadFromFile(const std::string& path, World& world) const;

private:
    static bool parseBodyKind(const std::string& token, BodyKind& kind);
};

} // namespace spacesim::core