#pragma once

#include "spacesim/core/Vec3.hpp"

namespace spacesim::core {

enum class GravityModel {
    Newtonian,
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

    // Speed of light for relativistic correction term.
    double lightSpeed = 299792458.0;
    
    // Fattore di damping numerico opzionale.
    // Per un Newtoniano conservativo la scelta corretta e' 1.0.
    double numericalDampingFactor = 1.0;

    // Collision handling: distance threshold for merging (in meters)
    // Bodies merge if center-to-center distance <= collisionDistanceThreshold
    // Typical value: ~2.0 * sqrt(softeningLengthSquared)
    // For stellar scales: 1e9 m (1 million km) is reasonable
    double collisionDistanceThreshold = 1.0e9;  // 1 billion meters (1 million km)
    
    // Enable/disable collision merging
    bool enableCollisionMerging = true;
};

} // namespace spacesim::core