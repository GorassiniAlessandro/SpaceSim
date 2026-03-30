#pragma once

#include "spacesim/core/Vec3.hpp"

namespace spacesim::core {

enum class GravityModel {
    Newtonian,
    Galilean,
    Relativistic,
    Hybrid
};

struct SimulationConfig {
    // Costanti fisiche fondamentali - valori realistici
    double gravitationalConstant = 6.67430e-11;
    
    // Softening per evitare singolarità: aumentato per tollerare grandi timestep
    double softeningLengthSquared = 1e8;  // 10000 m^2
    
    // Timestep per simulazioni astronomiche: 30 minuti = 1800 secondi
    // Piccolo timestep per integrazione stabile anche con timeScale
    double fixedTimeStepSeconds = 1800.0;  // 30 minuti
    
    GravityModel gravityModel = GravityModel::Newtonian;

    // Galileo-style uniform gravitational field (e.g. near-planet approximation).
    Vec3 galileanField{0.0, -9.81, 0.0};

    // Speed of light for relativistic correction term.
    double lightSpeed = 299792458.0;
    
    // Fattore di damping numerico (0.0-1.0): riduce drift energetico senza affettare fis
    // 0.9999 = minimo damping, 0.99 = moderato
    double numericalDampingFactor = 0.9999;
};

} // namespace spacesim::core