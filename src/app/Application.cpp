#include "spacesim/app/Application.hpp"

#include <iostream>
#include <memory>
#include <string>

#include "spacesim/render/Renderer2D.hpp"
#include "spacesim/render/Renderer3D.hpp"

namespace spacesim::app {

Application::Application()
    : renderer_(std::make_unique<render::Renderer2D>()),
      mode_(RenderMode::TwoD),
      running_(true) {
    seedDemoBodies();
}

void Application::run() {
    std::cout << "SpaceSim avviato. Modalita iniziale: " << renderer_->name() << "\n";
    std::cout << "Comandi: [invio]=step, 2=2D, 3=3D, q=quit\n";

    constexpr double dt = 60.0;
    while (running_) {
        processInput();
        if (!running_) {
            break;
        }

        physics_.step(world_, dt);
        renderer_->render(world_);
    }

    std::cout << "SpaceSim terminato.\n";
}

void Application::seedDemoBodies() {
    core::Body sun;
    sun.name = "Sun";
    sun.mass = 1.989e30;
    sun.position = {0.0, 0.0, 0.0};
    sun.velocity = {0.0, 0.0, 0.0};
    sun.isStatic = true;

    core::Body earth;
    earth.name = "Earth";
    earth.mass = 5.972e24;
    earth.position = {1.496e11, 0.0, 0.0};
    earth.velocity = {0.0, 29780.0, 0.0};

    core::Body blackHole;
    blackHole.name = "BlackHole";
    blackHole.mass = 8.0e30;
    blackHole.position = {-3.5e11, 0.0, 1.0e10};
    blackHole.velocity = {0.0, 0.0, 0.0};
    blackHole.isStatic = true;

    world_.addBody(sun);
    world_.addBody(earth);
    world_.addBody(blackHole);
}

void Application::processInput() {
    std::cout << "\n> ";
    std::string input;
    std::getline(std::cin, input);

    if (input == "q") {
        running_ = false;
        return;
    }

    if (input == "2") {
        switchMode(RenderMode::TwoD);
    } else if (input == "3") {
        switchMode(RenderMode::ThreeD);
    }
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

} // namespace spacesim::app
