#pragma once

#include <memory>

#include "spacesim/core/World.hpp"
#include "spacesim/core/PhysicsEngine.hpp"
#include "spacesim/render/IRenderer.hpp"

namespace spacesim::app {

enum class RenderMode {
    TwoD,
    ThreeD
};

class Application {
public:
    Application();
    void run();

private:
    void seedDemoBodies();
    void processInput();
    void switchMode(RenderMode mode);

    core::World world_;
    core::PhysicsEngine physics_;
    std::unique_ptr<render::IRenderer> renderer_;
    RenderMode mode_;
    bool running_;
};

} // namespace spacesim::app
