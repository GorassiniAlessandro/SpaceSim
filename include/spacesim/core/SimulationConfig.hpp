#pragma once

namespace spacesim::core {

struct SimulationConfig {
    double gravitationalConstant = 6.67430e-11;
    double softeningLengthSquared = 1e6;
    double fixedTimeStepSeconds = 60.0;
};

} // namespace spacesim::core