#include "spacesim/app/Application.hpp"

#include <iostream>
#include <memory>
#include <string>

#include "spacesim/core/ScenarioLoader.hpp"
#include "spacesim/render/Renderer2D.hpp"
#include "spacesim/render/Renderer3D.hpp"

namespace spacesim::app {

Application::Application()
        : physics_(config_),
            renderer_(std::make_unique<render::Renderer2D>()),
      mode_(RenderMode::TwoD),
      running_(true),
      paused_(true),
      stepRequested_(false),
      timeScale_(1.0) {
        loadInitialScenario();
}

void Application::run() {
    std::cout << "SpaceSim avviato. Modalita iniziale: " << renderer_->name() << "\n";
    printCommandBoard();

    while (running_) {
        processInput();
        if (!running_) {
            break;
        }

        const bool shouldStep = (!paused_) || stepRequested_;
        if (shouldStep) {
            const double dt = config_.fixedTimeStepSeconds * timeScale_;
            physics_.step(world_, dt);
            renderer_->render(world_);
        }

        stepRequested_ = false;
    }

    std::cout << "SpaceSim terminato.\n";
}

void Application::loadInitialScenario() {
    const core::ScenarioLoader loader;
    if (loader.loadFromFile("objects/scenarios/solar_blackhole_demo.txt", world_)) {
        std::cout << "Scenario caricato da file.\n";
        return;
    }

    std::cout << "Scenario file non trovato o non valido, uso fallback interno.\n";
    seedFallbackBodies();
}

void Application::seedFallbackBodies() {
    world_.bodies().clear();

    core::Body sun;
    sun.name = "Sun";
    sun.kind = core::BodyKind::Star;
    sun.mass = 1.989e30;
    sun.position = {0.0, 0.0, 0.0};
    sun.velocity = {0.0, 0.0, 0.0};
    sun.isStatic = true;

    core::Body earth;
    earth.name = "Earth";
    earth.kind = core::BodyKind::Planet;
    earth.mass = 5.972e24;
    earth.position = {1.496e11, 0.0, 0.0};
    earth.velocity = {0.0, 29780.0, 0.0};

    core::Body blackHole;
    blackHole.name = "BlackHole";
    blackHole.kind = core::BodyKind::BlackHole;
    blackHole.mass = 8.0e30;
    blackHole.position = {-3.5e11, 0.0, 1.0e10};
    blackHole.velocity = {0.0, 0.0, 0.0};
    blackHole.isStatic = true;
    blackHole.eventHorizonRadius = 8.0e9;

    world_.addBody(sun);
    world_.addBody(earth);
    world_.addBody(blackHole);
}

void Application::processInput() {
    std::cout << "\n(" << (paused_ ? "paused" : "running") << ", x" << timeScale_ << ") > ";
    std::string input;
    std::getline(std::cin, input);

    if (input.empty() || input == "s") {
        stepRequested_ = true;
        return;
    }

    if (input == "q") {
        running_ = false;
        return;
    }

    if (input == "h" || input == "help") {
        printCommandBoard();
        return;
    }

    if (input == "p") {
        paused_ = !paused_;
        std::cout << (paused_ ? "Simulazione in pausa.\n" : "Simulazione in esecuzione.\n");
        return;
    }

    if (input == "r") {
        loadInitialScenario();
        std::cout << "Scenario resettato.\n";
        return;
    }

    if (input == "+") {
        timeScale_ *= 2.0;
        if (timeScale_ > 1024.0) {
            timeScale_ = 1024.0;
        }
        std::cout << "Time scale impostata a x" << timeScale_ << "\n";
        return;
    }

    if (input == "-") {
        timeScale_ *= 0.5;
        if (timeScale_ < 0.125) {
            timeScale_ = 0.125;
        }
        std::cout << "Time scale impostata a x" << timeScale_ << "\n";
        return;
    }

    if (input == "st") {
        printStatus();
        return;
    }

    if (input == "2") {
        switchMode(RenderMode::TwoD);
        return;
    }

    if (input == "3") {
        switchMode(RenderMode::ThreeD);
        return;
    }

    std::cout << "Comando non riconosciuto. Digita h per help.\n";
}

void Application::switchMode(RenderMode mode) {
    if (mode == mode_) {
        return;
    }

    mode_ = mode;
    if (mode_ == RenderMode::TwoD) {
        renderer_ = std::make_unique<render::Renderer2D>();
    } else {
        renderer_ = std::make_unique<render::Renderer3D>();
    }

    std::cout << "Modalita cambiata a " << renderer_->name() << "\n";
}

void Application::printCommandBoard() const {
    std::cout << "Comandi disponibili:\n"
              << "  [invio] o s  -> esegue un singolo step\n"
              << "  p            -> toggle pausa/esecuzione\n"
              << "  2            -> renderer 2D\n"
              << "  3            -> renderer 3D\n"
              << "  +            -> raddoppia velocita simulazione\n"
              << "  -            -> dimezza velocita simulazione\n"
              << "  st           -> mostra stato corrente\n"
              << "  r            -> ricarica lo scenario dal file\n"
              << "  h            -> mostra questa guida\n"
              << "  q            -> esci\n";
}

void Application::printStatus() const {
    std::cout << "Stato simulazione:\n"
              << "  renderer: " << renderer_->name() << "\n"
              << "  paused: " << (paused_ ? "true" : "false") << "\n"
              << "  timeScale: x" << timeScale_ << "\n"
              << "  dt base: " << config_.fixedTimeStepSeconds << " s\n"
              << "  corpi attivi: " << world_.bodies().size() << "\n";
}

} // namespace spacesim::app
