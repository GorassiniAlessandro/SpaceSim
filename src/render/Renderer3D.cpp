#include "spacesim/render/Renderer3D.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>

#include "spacesim/render/TerminalRenderCommon.hpp"

namespace spacesim::render {

void Renderer3D::render(const core::World& world) {
    clearTerminalFrame();

    TerminalCanvas canvas;
    const auto& bodies = world.bodies();

    if (bodies.empty()) {
        std::cout << "[Renderer 3D] Nessun corpo in simulazione.\n";
        return;
    }

    double centerX = 0.0;
    double centerY = 0.0;
    double centerZ = 0.0;
    for (const auto& body : bodies) {
        centerX += body.position.x;
        centerY += body.position.y;
        centerZ += body.position.z;
    }
    centerX /= static_cast<double>(bodies.size());
    centerY /= static_cast<double>(bodies.size());
    centerZ /= static_cast<double>(bodies.size());

    double maxRadius = 1.0;
    for (const auto& body : bodies) {
        const double dx = body.position.x - centerX;
        const double dy = body.position.y - centerY;
        const double dz = body.position.z - centerZ;
        maxRadius = std::max(maxRadius, std::sqrt((dx * dx) + (dy * dy) + (dz * dz)));
    }

    const double yaw = 0.7;
    const double pitch = 0.35;
    const double cosY = std::cos(yaw);
    const double sinY = std::sin(yaw);
    const double cosX = std::cos(pitch);
    const double sinX = std::sin(pitch);
    const double cameraDistance = maxRadius * 3.0;
    const double focal = static_cast<double>(std::min(canvas.width, canvas.height)) * 0.65;

    for (const auto& body : bodies) {
        const double dx = body.position.x - centerX;
        const double dy = body.position.y - centerY;
        const double dz = body.position.z - centerZ;

        const double x1 = (cosY * dx) - (sinY * dz);
        const double z1 = (sinY * dx) + (cosY * dz);
        const double y2 = (cosX * dy) - (sinX * z1);
        const double z2 = (sinX * dy) + (cosX * z1);

        const double denom = z2 + cameraDistance;
        if (denom <= 1.0) {
            continue;
        }

        const double perspective = focal / denom;
        const int sx = static_cast<int>(std::llround((canvas.width * 0.5) + (x1 * perspective)));
        const int sy = static_cast<int>(std::llround((canvas.height * 0.5) - (y2 * perspective)));

        canvas.put(sx, sy, symbolForBodyKind(body.kind));
    }

    std::cout << "[Renderer 3D] corpi=" << bodies.size()
              << "  cameraDist~" << std::scientific << cameraDistance << std::defaultfloat << " m\n";
    std::cout << "Legenda: S=Star P=Planet a=Asteroid @=BlackHole o=Generic\n";

    for (const auto& row : canvas.rows) {
        std::cout << '|' << row << "|\n";
    }
}

const char* Renderer3D::name() const {
    return "3D";
}

} // namespace spacesim::render
