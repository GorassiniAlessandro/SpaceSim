#include "spacesim/core/ScenarioLoader.hpp"

#include <fstream>
#include <sstream>

namespace spacesim::core {

bool ScenarioLoader::loadFromFile(const std::string& path, World& world) const {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

    world.bodies().clear();

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream stream(line);
        Body body;
        std::string kindToken;

        if (!(stream >> body.name >> kindToken >> body.mass >> body.position.x >> body.position.y >>
              body.position.z >> body.velocity.x >> body.velocity.y >> body.velocity.z >> std::boolalpha >>
              body.isStatic >> body.eventHorizonRadius)) {
            continue;
        }

        BodyKind kind = BodyKind::Generic;
        if (!parseBodyKind(kindToken, kind)) {
            continue;
        }

        body.kind = kind;
        world.addBody(body);
    }

    return !world.bodies().empty();
}

bool ScenarioLoader::parseBodyKind(const std::string& token, BodyKind& kind) {
    if (token == "star") {
        kind = BodyKind::Star;
        return true;
    }

    if (token == "planet") {
        kind = BodyKind::Planet;
        return true;
    }

    if (token == "asteroid") {
        kind = BodyKind::Asteroid;
        return true;
    }

    if (token == "blackhole") {
        kind = BodyKind::BlackHole;
        return true;
    }

    if (token == "generic") {
        kind = BodyKind::Generic;
        return true;
    }

    return false;
}

} // namespace spacesim::core