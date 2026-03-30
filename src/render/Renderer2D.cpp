#include "spacesim/render/Renderer2D.hpp"

#include <iostream>

namespace spacesim::render {

void Renderer2D::render(const core::World& world) {
    std::cout << "\n[Renderer 2D] Bodies: " << world.bodies().size() << "\n";
    for (const auto& body : world.bodies()) {
        std::cout << " - " << body.name << " @ ("
                  << body.position.x << ", "
                  << body.position.y << ")\n";
    }
}

const char* Renderer2D::name() const {
    return "2D";
}

} // namespace spacesim::render
