#include "spacesim/render/Renderer2D.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>

#include "spacesim/render/TerminalRenderCommon.hpp"

namespace spacesim::render {

void Renderer2D::render(const core::World& world) {
    clearTerminalFrame();

    TerminalCanvas canvas;
    const auto& bodies = world.bodies();

    if (bodies.empty()) {
        std::cout << "[Renderer 2D] Nessun corpo in simulazione.\n";
        return;
    }

    double centerX = 0.0;
    double centerY = 0.0;
    for (const auto& body : world.bodies()) {
        centerX += body.position.x;
        centerY += body.position.y;
    }
    centerX /= static_cast<double>(bodies.size());
    centerY /= static_cast<double>(bodies.size());

    double maxRange = 1.0;
    for (const auto& body : bodies) {
        const double rx = std::abs(body.position.x - centerX);
        const double ry = std::abs(body.position.y - centerY);
        maxRange = std::max(maxRange, std::max(rx, ry));
    }

    const double halfSpan = static_cast<double>(std::min(canvas.width, canvas.height)) * 0.45;
    const double scale = halfSpan / maxRange;

    for (const auto& body : bodies) {
        const double localX = body.position.x - centerX;
        const double localY = body.position.y - centerY;

        const int sx = static_cast<int>(std::llround((canvas.width * 0.5) + (localX * scale)));
        const int sy = static_cast<int>(std::llround((canvas.height * 0.5) - (localY * scale)));

        canvas.put(sx, sy, symbolForBodyKind(body.kind));
    }

    std::cout << "[Renderer 2D] corpi=" << bodies.size()
              << "  scala~" << std::scientific << (1.0 / scale) << std::defaultfloat << " m/cella\n";
    std::cout << "Legenda: S=Star P=Planet a=Asteroid @=BlackHole o=Generic\n";

    for (const auto& row : canvas.rows) {
        std::cout << '|' << row << "|\n";
    }
}

const char* Renderer2D::name() const {
    return "2D";
}

} // namespace spacesim::render
