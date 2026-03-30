#pragma once

#include <vector>

#include "spacesim/core/Body.hpp"

namespace spacesim::core {

class World {
public:
    void addBody(const Body& body) {
        bodies_.push_back(body);
    }

    [[nodiscard]] std::vector<Body>& bodies() {
        return bodies_;
    }

    [[nodiscard]] const std::vector<Body>& bodies() const {
        return bodies_;
    }

private:
    std::vector<Body> bodies_;
};

} // namespace spacesim::core
