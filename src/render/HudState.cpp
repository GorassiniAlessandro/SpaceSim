#include "spacesim/render/HudState.hpp"

#include <mutex>
#include <sstream>

namespace spacesim::render::hud {

namespace {

struct HudStateData {
    bool enabled = true;
    bool overview = true;
    bool kinematics = true;
    bool distance = true;
    bool energy = true;
};

std::mutex& hudMutex() {
    static std::mutex m;
    return m;
}

HudStateData& hudState() {
    static HudStateData s;
    return s;
}

} // namespace

void setEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(hudMutex());
    hudState().enabled = enabled;
}

bool isEnabled() {
    std::lock_guard<std::mutex> lock(hudMutex());
    return hudState().enabled;
}

void setPanelEnabled(Panel panel, bool enabled) {
    std::lock_guard<std::mutex> lock(hudMutex());
    auto& s = hudState();
    switch (panel) {
    case Panel::Overview:
        s.overview = enabled;
        break;
    case Panel::Kinematics:
        s.kinematics = enabled;
        break;
    case Panel::Distance:
        s.distance = enabled;
        break;
    case Panel::Energy:
        s.energy = enabled;
        break;
    }
}

bool isPanelEnabled(Panel panel) {
    std::lock_guard<std::mutex> lock(hudMutex());
    const auto& s = hudState();
    switch (panel) {
    case Panel::Overview:
        return s.overview;
    case Panel::Kinematics:
        return s.kinematics;
    case Panel::Distance:
        return s.distance;
    case Panel::Energy:
        return s.energy;
    }
    return false;
}

void resetDefaults() {
    std::lock_guard<std::mutex> lock(hudMutex());
    hudState() = HudStateData{};
}

std::string summary() {
    std::lock_guard<std::mutex> lock(hudMutex());
    const auto& s = hudState();

    std::ostringstream out;
    out << "hub=" << (s.enabled ? "on" : "off")
        << " panels:[overview=" << (s.overview ? "on" : "off")
        << ", kinematics=" << (s.kinematics ? "on" : "off")
        << ", distance=" << (s.distance ? "on" : "off")
        << ", energy=" << (s.energy ? "on" : "off")
        << "]";
    return out.str();
}

} // namespace spacesim::render::hud
