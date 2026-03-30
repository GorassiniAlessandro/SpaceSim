#include "spacesim/render/Renderer3D.hpp"

#include <iostream>

namespace spacesim::render {

void Renderer3D::render(const core::World& world) {
    std::cout << "\n[Renderer 3D] Bodies: " << world.bodies().size() << "\n";
    for (const auto& body : world.bodies()) {
        std::cout << " - " << body.name << " @ ("
                  << body.position.x << ", "
                  << body.position.y << ", "
                  << body.position.z << ")\n";
    }
}

const char* Renderer3D::name() const {
    return "3D";
}

} // namespace spacesim::render
