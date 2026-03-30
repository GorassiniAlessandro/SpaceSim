#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "spacesim/core/PhysicsEngine.hpp"
#include "spacesim/core/SimulationConfig.hpp"
#include "spacesim/core/World.hpp"
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
    void loadInitialScenario();
    bool loadScenarioByName(const std::string& scenarioName);
    void seedFallbackBodies();
    void processInput();
    void switchMode(RenderMode mode);
    void printCommandBoard() const;
    void printStatus() const;
    void printMetrics() const;

    core::SimulationConfig config_;
    core::World world_;
    core::PhysicsEngine physics_;
    std::unique_ptr<render::IRenderer> renderer_;
    RenderMode mode_;
    bool running_;
    bool paused_;
    bool stepRequested_;
    double timeScale_;
    std::string currentScenario_;
    std::size_t totalAbsorbedBodies_;
    double totalAbsorbedMass_;
};

} // namespace spacesim::app
