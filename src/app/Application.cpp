#include "spacesim/app/Application.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "spacesim/core/ScenarioLoader.hpp"
#include "spacesim/render/Renderer2D.hpp"
#include "spacesim/render/Renderer3D.hpp"

namespace spacesim::app {

namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

} // namespace

Application::Application()
        : physics_(config_),
            renderer_(std::make_unique<render::Renderer2D>()),
      mode_(RenderMode::TwoD),
      running_(true),
      paused_(true),
      stepRequested_(false),
      timeScale_(1.0),
      currentScenario_("solar_blackhole_demo"),
      totalAbsorbedBodies_(0),
      totalAbsorbedMass_(0.0) {
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
            const auto stepReport = physics_.step(world_, dt);

            if (stepReport.absorbedBodies > 0) {
                totalAbsorbedBodies_ += stepReport.absorbedBodies;
                totalAbsorbedMass_ += stepReport.absorbedMass;

                for (const auto& event : stepReport.absorptionEvents) {
                    std::cout << "[ABSORB] " << event.absorberName
                              << " ha assorbito " << event.absorbedName
                              << " (massa=" << event.absorbedMass << ")\n";
                }
            }

            renderer_->render(world_);
        }

        stepRequested_ = false;
    }

    std::cout << "SpaceSim terminato.\n";
}

void Application::loadInitialScenario() {
    if (loadScenarioByName(currentScenario_)) {
        return;
    }

    std::cout << "Scenario file non trovato o non valido, uso fallback interno.\n";
    seedFallbackBodies();
}

bool Application::loadScenarioByName(const std::string& scenarioName) {
    std::string normalizedName = trim(scenarioName);
    if (normalizedName.empty()) {
        return false;
    }

    if (normalizedName.size() < 4 || normalizedName.substr(normalizedName.size() - 4) != ".txt") {
        normalizedName += ".txt";
    }

    const std::string fullPath = "objects/scenarios/" + normalizedName;
    const core::ScenarioLoader loader;

    if (!loader.loadFromFile(fullPath, world_)) {
        return false;
    }

    currentScenario_ = normalizedName.substr(0, normalizedName.size() - 4);
    totalAbsorbedBodies_ = 0;
    totalAbsorbedMass_ = 0.0;
    std::cout << "Scenario caricato da file: " << fullPath << "\n";
    return true;
}

void Application::seedFallbackBodies() {
    world_.bodies().clear();
    totalAbsorbedBodies_ = 0;
    totalAbsorbedMass_ = 0.0;

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

    if (input.rfind("load ", 0) == 0) {
        const std::string requestedScenario = trim(input.substr(5));
        if (loadScenarioByName(requestedScenario)) {
            std::cout << "Scenario cambiato con successo.\n";
        } else {
            std::cout << "Scenario non trovato o invalido: " << requestedScenario << "\n";
        }
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

    if (input == "m" || input == "metrics") {
        printMetrics();
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
              << "  m            -> mostra metriche fisiche (energia e momento)\n"
              << "  r            -> ricarica lo scenario dal file\n"
              << "  load <nome>  -> carica uno scenario da objects/scenarios/<nome>.txt\n"
              << "  h            -> mostra questa guida\n"
              << "  q            -> esci\n";
}

void Application::printStatus() const {
    std::cout << "Stato simulazione:\n"
              << "  renderer: " << renderer_->name() << "\n"
              << "  paused: " << (paused_ ? "true" : "false") << "\n"
              << "  timeScale: x" << timeScale_ << "\n"
              << "  dt base: " << config_.fixedTimeStepSeconds << " s\n"
              << "  scenario: " << currentScenario_ << "\n"
              << "  corpi attivi: " << world_.bodies().size() << "\n"
              << "  assorbimenti totali: " << totalAbsorbedBodies_ << "\n"
              << "  massa assorbita totale: " << totalAbsorbedMass_ << "\n";

    for (const auto& body : world_.bodies()) {
        if (body.kind == core::BodyKind::BlackHole) {
            std::cout << "  black hole: " << body.name << " massa=" << body.mass
                      << " eventHorizon=" << body.eventHorizonRadius << "\n";
        }
    }
}

void Application::printMetrics() const {
    const auto& bodies = world_.bodies();
    double kineticEnergy = 0.0;
    double potentialEnergy = 0.0;
    core::Vec3 totalMomentum{};

    for (const auto& body : bodies) {
        const double speedSq = body.velocity.lengthSquared();
        kineticEnergy += 0.5 * body.mass * speedSq;
        totalMomentum += body.velocity * body.mass;
    }

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const core::Vec3 delta = bodies[j].position - bodies[i].position;
            const double distance = std::sqrt(delta.lengthSquared() + config_.softeningLengthSquared);
            if (distance <= 0.0) {
                continue;
            }

            potentialEnergy -= (config_.gravitationalConstant * bodies[i].mass * bodies[j].mass) / distance;
        }
    }

    const double totalEnergy = kineticEnergy + potentialEnergy;

    std::cout << std::setprecision(10)
              << "Metriche fisiche:\n"
              << "  energia cinetica: " << kineticEnergy << "\n"
              << "  energia potenziale: " << potentialEnergy << "\n"
              << "  energia totale: " << totalEnergy << "\n"
              << "  momento totale: (" << totalMomentum.x << ", "
              << totalMomentum.y << ", " << totalMomentum.z << ")\n";
}

} // namespace spacesim::app
