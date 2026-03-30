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

enum class RenderBackend {
    Ascii,
    OpenGL
};

class Application {
public:
    Application();
    void run();

private:
    void loadInitialScenario();
    bool loadScenarioByName(const std::string& scenarioName);
    void seedFallbackBodies();
    void processWindowCommands();
    void processInput();
    void executeCommand(const std::string& rawInput);
    void switchMode(RenderMode mode);
    void switchBackend(RenderBackend backend);
    void printCommandBoard() const;
    void printStatus() const;
    void printMetrics() const;

    core::SimulationConfig config_;
    core::World world_;
    core::PhysicsEngine physics_;
    std::unique_ptr<render::IRenderer> renderer_;
    RenderMode mode_;
    RenderBackend backend_;
    bool running_;
    bool paused_;
    bool stepRequested_;
    double timeScale_;
    bool commandPromptShown_;
    std::string currentScenario_;
    std::size_t totalAbsorbedBodies_;
    double totalAbsorbedMass_;
};

} // namespace spacesim::app
