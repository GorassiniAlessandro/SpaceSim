#pragma once

#include <string>

namespace spacesim::render::hud {

enum class Panel {
    Overview,
    Kinematics,
    Distance,
    Energy
};

void setEnabled(bool enabled);
[[nodiscard]] bool isEnabled();

void setPanelEnabled(Panel panel, bool enabled);
[[nodiscard]] bool isPanelEnabled(Panel panel);

void resetDefaults();
[[nodiscard]] std::string summary();

} // namespace spacesim::render::hud
