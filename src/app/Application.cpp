#include "spacesim/app/Application.hpp"

#include <cmath>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/select.h>
#include <unistd.h>
#endif

#include "spacesim/core/ScenarioLoader.hpp"
#include "spacesim/render/HudState.hpp"
#include "spacesim/render/Renderer2D.hpp"
#include "spacesim/render/Renderer3D.hpp"
#include "spacesim/render/WindowCommandBridge.hpp"
#ifdef SPACESIM_ENABLE_OPENGL
#include "spacesim/render/Renderer2DOpenGL.hpp"
#include "spacesim/render/Renderer3DOpenGL.hpp"
#endif

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

std::unique_ptr<render::IRenderer> makeRenderer(RenderBackend backend, RenderMode mode) {
#ifdef SPACESIM_ENABLE_OPENGL
    if (backend == RenderBackend::OpenGL) {
        if (mode == RenderMode::TwoD) {
            return std::make_unique<render::Renderer2DOpenGL>();
        }
        return std::make_unique<render::Renderer3DOpenGL>();
    }
#else
    (void)backend;
#endif

    if (mode == RenderMode::TwoD) {
        return std::make_unique<render::Renderer2D>();
    }
    return std::make_unique<render::Renderer3D>();
}

const char* gravityModelName(core::GravityModel model) {
    switch (model) {
    case core::GravityModel::Relativistic:
        return "Relativistic";
    case core::GravityModel::Hybrid:
        return "Hybrid";
    case core::GravityModel::Newtonian:
    default:
        return "Newtonian";
    }
}

bool parseHubPanel(const std::string& token, render::hud::Panel& panel) {
    if (token == "overview") {
        panel = render::hud::Panel::Overview;
        return true;
    }
    if (token == "kin" || token == "kinematics") {
        panel = render::hud::Panel::Kinematics;
        return true;
    }
    if (token == "distance" || token == "dist") {
        panel = render::hud::Panel::Distance;
        return true;
    }
    if (token == "energy" || token == "ek") {
        panel = render::hud::Panel::Energy;
        return true;
    }
    return false;
}

bool hasStdinInputAvailable() {
#if defined(__linux__) || defined(__APPLE__)
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    const int result = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &tv);
    return result > 0 && FD_ISSET(STDIN_FILENO, &readfds);
#else
    return std::cin.rdbuf()->in_avail() > 0;
#endif
}

} // namespace

Application::Application()
        : physics_(config_),
            renderer_(nullptr),
      mode_(RenderMode::TwoD),
      backend_(RenderBackend::Ascii),
      running_(true),
      paused_(true),
      stepRequested_(false),
      timeScale_(1.0),
    commandPromptShown_(false),
      currentScenario_("solar_blackhole_demo"),
      totalAbsorbedBodies_(0),
            totalAbsorbedMass_(0.0),
            diagnosticsReferenceSet_(false),
            diagnosticsReference_(),
            lastDiagnostics_() {
        renderer_ = makeRenderer(backend_, mode_);
        loadInitialScenario();
}

void Application::run() {
    std::cout << "SpaceSim avviato. Modalita iniziale: " << renderer_->name() << "\n";
    printCommandBoard();

    while (running_) {
        processWindowCommands();
        processInput();
        if (!running_) {
            break;
        }

        const bool shouldStep = (!paused_) || stepRequested_;
        if (shouldStep) {
            const double dt = config_.fixedTimeStepSeconds * timeScale_;
            const auto stepReport = physics_.step(world_, dt);
            lastDiagnostics_ = stepReport.diagnostics;

            if (!diagnosticsReferenceSet_ && world_.bodies().size() >= 2) {
                diagnosticsReference_ = stepReport.diagnostics;
                diagnosticsReferenceSet_ = true;
            }

            if (stepReport.absorbedBodies > 0) {
                totalAbsorbedBodies_ += stepReport.absorbedBodies;
                totalAbsorbedMass_ += stepReport.absorbedMass;

                for (const auto& event : stepReport.absorptionEvents) {
                    std::cout << "[ABSORB] " << event.absorberName
                              << " ha assorbito " << event.absorbedName
                              << " (massa=" << event.absorbedMass << ")\n";
                }
            }

            if (stepRequested_) {
                std::cout << "[STEP] dt=" << stepReport.integratedDt
                          << " substeps=" << stepReport.substepsUsed
                          << " a_max=" << stepReport.diagnostics.maxAcceleration
                          << " E=" << stepReport.diagnostics.totalEnergy << "\n";
            }

        }

        const bool shouldRender = (backend_ == RenderBackend::OpenGL) || shouldStep;
        if (shouldRender) {
            renderer_->render(world_);
        }

        stepRequested_ = false;
        if (backend_ == RenderBackend::OpenGL) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    std::cout << "SpaceSim terminato.\n";
}

void Application::processWindowCommands() {
    std::string windowCommand;
    while (render::window_commands::tryPop(windowCommand)) {
        executeCommand(windowCommand);
        if (!running_) {
            break;
        }
    }
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
    resetDiagnosticsReference();
    std::cout << "Scenario caricato da file: " << fullPath << "\n";
    return true;
}

void Application::seedFallbackBodies() {
    world_.bodies().clear();
    totalAbsorbedBodies_ = 0;
    totalAbsorbedMass_ = 0.0;
    resetDiagnosticsReference();

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
    resetDiagnosticsReference();
}

void Application::processInput() {
    if (backend_ == RenderBackend::OpenGL) {
        if (!commandPromptShown_) {
            std::cout << "\n[console] (" << (paused_ ? "paused" : "running") << ", x"
                      << timeScale_ << ") > " << std::flush;
            commandPromptShown_ = true;
        }

        if (!hasStdinInputAvailable()) {
            return;
        }

        std::string input;
        std::getline(std::cin, input);
        commandPromptShown_ = false;
        executeCommand(input);
        return;
    }

    std::cout << "\n(" << (paused_ ? "paused" : "running") << ", x" << timeScale_ << ") > ";
    std::string input;
    std::getline(std::cin, input);
    executeCommand(input);
}

void Application::executeCommand(const std::string& rawInput) {
    const std::string input = trim(rawInput);

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
        if (timeScale_ > 256.0) {  // Limite massimo per stabilità
            timeScale_ = 256.0;
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

    if (input == "hub") {
        std::cout << "Hub interattivo:\n"
                  << "  hub on/off                 -> abilita o disabilita HUD live\n"
                  << "  hub show <panel|all>       -> mostra pannello\n"
                  << "  hub hide <panel|all>       -> nasconde pannello\n"
                  << "  hub toggle <panel|all>     -> toggle pannello\n"
                  << "  hub toggle-all             -> toggle on/off HUD intero\n"
                  << "  hub reset                  -> reset pannelli default\n"
                  << "  hub status                 -> stato attuale hub\n"
                  << "  panel validi: overview | kin | distance | energy\n"
                  << "  Scorciatoie tastiera in OpenGL:\n"
                  << "    Numpad 1 = toggle Overview\n"
                  << "    Numpad 2 = toggle Kinematics\n"
                  << "    Numpad 3 = toggle Distance\n"
                  << "    Numpad 4 = toggle Energy\n"
                  << "    Tab = toggle HUD on/off\n"
                  << "  stato: " << render::hud::summary() << "\n";
        return;
    }

    if (input.rfind("hub ", 0) == 0) {
        const std::string cmd = trim(input.substr(4));

        if (cmd == "on") {
            render::hud::setEnabled(true);
            std::cout << "Hub attivato. " << render::hud::summary() << "\n";
            return;
        }
        if (cmd == "off") {
            render::hud::setEnabled(false);
            std::cout << "Hub disattivato. " << render::hud::summary() << "\n";
            return;
        }
        if (cmd == "reset") {
            render::hud::resetDefaults();
            std::cout << "Hub resettato. " << render::hud::summary() << "\n";
            return;
        }
        if (cmd == "status") {
            std::cout << "Stato hub: " << render::hud::summary() << "\n";
            return;
        }

        if (cmd.rfind("show ", 0) == 0 || cmd.rfind("hide ", 0) == 0) {
            const bool enable = cmd.rfind("show ", 0) == 0;
            const std::string arg = trim(cmd.substr(5));

            if (arg == "all") {
                render::hud::setPanelEnabled(render::hud::Panel::Overview, enable);
                render::hud::setPanelEnabled(render::hud::Panel::Kinematics, enable);
                render::hud::setPanelEnabled(render::hud::Panel::Distance, enable);
                render::hud::setPanelEnabled(render::hud::Panel::Energy, enable);
                std::cout << "Hub aggiornato. " << render::hud::summary() << "\n";
                return;
            }

            render::hud::Panel panel{};
            if (!parseHubPanel(arg, panel)) {
                std::cout << "Panel non valido. Usa: overview | kin | distance | energy | all\n";
                return;
            }

            render::hud::setPanelEnabled(panel, enable);
            std::cout << "Hub aggiornato. " << render::hud::summary() << "\n";
            return;
        }

        if (cmd == "toggle-all") {
            const bool hubCurrentlyEnabled = render::hud::isEnabled();
            render::hud::setEnabled(!hubCurrentlyEnabled);
            std::cout << "Hub " << (hubCurrentlyEnabled ? "disattivato" : "attivato") << ". " 
                      << render::hud::summary() << "\n";
            return;
        }

        if (cmd.rfind("toggle ", 0) == 0) {
            const std::string arg = trim(cmd.substr(7));

            if (arg == "all") {
                // Toggle tutti i pannelli
                const bool overview = render::hud::isPanelEnabled(render::hud::Panel::Overview);
                const bool kinematics = render::hud::isPanelEnabled(render::hud::Panel::Kinematics);
                const bool distance = render::hud::isPanelEnabled(render::hud::Panel::Distance);
                const bool energy = render::hud::isPanelEnabled(render::hud::Panel::Energy);
                
                render::hud::setPanelEnabled(render::hud::Panel::Overview, !overview);
                render::hud::setPanelEnabled(render::hud::Panel::Kinematics, !kinematics);
                render::hud::setPanelEnabled(render::hud::Panel::Distance, !distance);
                render::hud::setPanelEnabled(render::hud::Panel::Energy, !energy);
                std::cout << "Hub aggiornato. " << render::hud::summary() << "\n";
                return;
            }

            render::hud::Panel panel{};
            if (!parseHubPanel(arg, panel)) {
                std::cout << "Panel non valido. Usa: overview | kin | distance | energy | all\n";
                return;
            }

            const bool currentState = render::hud::isPanelEnabled(panel);
            render::hud::setPanelEnabled(panel, !currentState);
            std::cout << "Hub aggiornato. " << render::hud::summary() << "\n";
            return;
        }

        std::cout << "Comando hub non valido. Digita 'hub' per la guida hub.\n";
        return;
    }

    if (input.rfind("phys ", 0) == 0) {
        const std::string mode = trim(input.substr(5));
        if (mode == "newton" || mode == "newtonian") {
            config_.gravityModel = core::GravityModel::Newtonian;
        } else if (mode == "rel" || mode == "relativity" || mode == "relativistic") {
            config_.gravityModel = core::GravityModel::Relativistic;
        } else if (mode == "hybrid") {
            config_.gravityModel = core::GravityModel::Hybrid;
        } else {
            std::cout << "Modello fisico non riconosciuto. Usa: newton | rel | hybrid\n";
            return;
        }

        physics_.setConfig(config_);
        resetDiagnosticsReference();
        std::cout << "Modello fisico impostato su " << gravityModelName(config_.gravityModel) << "\n";
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

    if (input == "gfx ascii") {
        switchBackend(RenderBackend::Ascii);
        return;
    }

    if (input == "gfx opengl" || input == "w") {
        switchBackend(RenderBackend::OpenGL);
        return;
    }

    if (input == "collision on") {
        config_.enableCollisionMerging = true;
        physics_.setConfig(config_);
        std::cout << "Collision merging abilitato (threshold: " << config_.collisionDistanceThreshold 
                  << " m)\n";
        return;
    }

    if (input == "collision off") {
        config_.enableCollisionMerging = false;
        physics_.setConfig(config_);
        std::cout << "Collision merging disabilitato\n";
        return;
    }

    if (input == "collision") {
        std::cout << "Collision merging config:\n"
                  << "  enabled: " << (config_.enableCollisionMerging ? "si" : "no") << "\n"
                  << "  threshold: " << config_.collisionDistanceThreshold << " m\n";
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
        renderer_ = makeRenderer(backend_, RenderMode::TwoD);
    } else {
        renderer_ = makeRenderer(backend_, RenderMode::ThreeD);
    }

    std::cout << "Modalita cambiata a " << renderer_->name() << "\n";
}

void Application::switchBackend(RenderBackend backend) {
    if (backend == backend_) {
        return;
    }

#ifndef SPACESIM_ENABLE_OPENGL
    if (backend == RenderBackend::OpenGL) {
        std::cout << "Backend OpenGL non disponibile in questa build.\n";
        return;
    }
#endif

    backend_ = backend;
    renderer_ = makeRenderer(backend_, mode_);
    commandPromptShown_ = false;
    if (backend_ == RenderBackend::OpenGL) {
        // Force at least one frame so the window is created even if simulation is paused.
        stepRequested_ = true;
        std::cout << "Console comandi attiva nel terminale mentre la finestra e aperta.\n";
    }
    std::cout << "Backend grafico cambiato: "
              << (backend_ == RenderBackend::Ascii ? "ASCII" : "OpenGL") << "\n";
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
              << "  m            -> mostra metriche fisiche complete (E,P,L,CM,drift,a_max)\n"
              << "  hub          -> guida hub interattivo (metriche live separate)\n"
              << "  hub on/off   -> abilita/disabilita HUD live\n"
              << "  hub show/hide <overview|kin|distance|energy|all>\n"
              << "  hub toggle <overview|kin|distance|energy|all>  -> toggle singolo pannello\n"
              << "  hub toggle-all   -> toggle on/off HUD\n"
              << "  hub status   -> stato hub corrente\n"
              << "  phys <mode>  -> cambia modello fisico: newton | rel | hybrid\n"
              << "  r            -> ricarica lo scenario dal file\n"
              << "  load <nome>  -> carica uno scenario da objects/scenarios/<nome>.txt\n"
              << "  gfx ascii    -> usa renderer ASCII in terminale\n"
              << "  gfx opengl   -> usa renderer OpenGL in finestra\n"
              << "  w            -> scorciatoia per entrare in finestra OpenGL\n"              << "  collision    -> mostra config collision\n"
              << "  collision on/off -> abilita/disabilita collision merging\n"              << "  h            -> mostra questa guida\n"
              << "  q            -> esci\n"
              << "Hotkeys visuale in finestra OpenGL:\n"
              << "  2D: frecce=pan, U/O=zoom, C=reset camera\n"
              << "  3D: J/L=yaw, I/K=pitch, U/O=dolly, frecce=pan, Z/X=zoom, C=reset camera\n"
              << "  Numpad 1/2/3/4: toggle pannelli HUD (Overview/Kinematics/Distance/Energy)\n"
              << "  Tab: toggle on/off HUD\n";
}

void Application::printStatus() const {
    std::cout << "Stato simulazione:\n"
              << "  renderer: " << renderer_->name() << "\n"
              << "  paused: " << (paused_ ? "true" : "false") << "\n"
              << "  backend: " << (backend_ == RenderBackend::Ascii ? "ASCII" : "OpenGL") << "\n"
              << "  gravityModel: " << gravityModelName(config_.gravityModel) << "\n"
              << "  timeScale: x" << timeScale_ << "\n"
              << "  dt base: " << config_.fixedTimeStepSeconds << " s\n"
              << "  scenario: " << currentScenario_ << "\n"
              << "  corpi attivi: " << world_.bodies().size() << "\n"
              << "  assorbimenti totali: " << totalAbsorbedBodies_ << "\n"
              << "  massa assorbita totale: " << totalAbsorbedMass_ << "\n"
              << "  E totale: " << lastDiagnostics_.totalEnergy << "\n"
              << "  virial ratio: " << lastDiagnostics_.virialRatio << "\n"
              << "  a max: " << lastDiagnostics_.maxAcceleration << "\n"
              << "  d min: " << lastDiagnostics_.minDistance << "\n"
              << "  hub: " << render::hud::summary() << "\n";

    for (const auto& body : world_.bodies()) {
        if (body.kind == core::BodyKind::BlackHole) {
            std::cout << "  black hole: " << body.name << " massa=" << body.mass
                      << " eventHorizon=" << body.eventHorizonRadius << "\n";
        }
    }
}

void Application::printMetrics() const {
    const auto diagnostics = physics_.measure(world_);
    std::cout << std::setprecision(10)
              << "Metriche fisiche:\n"
              << "  modello: " << gravityModelName(config_.gravityModel) << "\n"
              << "  massa totale: " << diagnostics.totalMass << "\n"
              << "  energia cinetica: " << diagnostics.kineticEnergy << "\n"
              << "  energia potenziale: " << diagnostics.potentialEnergy << "\n"
              << "  energia totale: " << diagnostics.totalEnergy << "\n"
              << "  rapporto viriale: " << diagnostics.virialRatio << "\n"
              << "  momento totale: (" << diagnostics.totalMomentum.x << ", "
              << diagnostics.totalMomentum.y << ", " << diagnostics.totalMomentum.z << ")\n"
              << "  momento angolare totale: (" << diagnostics.totalAngularMomentum.x << ", "
              << diagnostics.totalAngularMomentum.y << ", " << diagnostics.totalAngularMomentum.z << ")\n"
              << "  centro di massa: (" << diagnostics.centerOfMass.x << ", "
              << diagnostics.centerOfMass.y << ", " << diagnostics.centerOfMass.z << ")\n"
              << "  velocita CM: (" << diagnostics.centerOfMassVelocity.x << ", "
              << diagnostics.centerOfMassVelocity.y << ", " << diagnostics.centerOfMassVelocity.z << ")\n"
              << "  d_min: " << diagnostics.minDistance << "\n"
              << "  v_max: " << diagnostics.maxSpeed << "\n"
              << "  a_max: " << diagnostics.maxAcceleration << "\n";

    if (diagnosticsReferenceSet_) {
        const double e0 = std::max(std::abs(diagnosticsReference_.totalEnergy), 1e-30);
        const double driftE = (diagnostics.totalEnergy - diagnosticsReference_.totalEnergy) / e0;

        const core::Vec3 dp = diagnostics.totalMomentum - diagnosticsReference_.totalMomentum;
        const core::Vec3 dl = diagnostics.totalAngularMomentum - diagnosticsReference_.totalAngularMomentum;

        std::cout << "  drift energia relativo: " << driftE << "\n"
                  << "  drift momento: (" << dp.x << ", " << dp.y << ", " << dp.z << ")\n"
                  << "  drift momento angolare: (" << dl.x << ", " << dl.y << ", " << dl.z << ")\n";
    } else {
        std::cout << "  drift: riferimento non ancora inizializzato\n";
    }

}

void Application::resetDiagnosticsReference() {
    lastDiagnostics_ = physics_.measure(world_);
    diagnosticsReference_ = lastDiagnostics_;
    diagnosticsReferenceSet_ = !world_.bodies().empty();
}

} // namespace spacesim::app
