#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "spacesim/core/Body.hpp"

namespace spacesim::render {

struct TerminalCanvas {
    int width = 100;
    int height = 36;
    std::vector<std::string> rows;

    TerminalCanvas() : rows(static_cast<std::size_t>(height), std::string(static_cast<std::size_t>(width), ' ')) {}

    void put(int x, int y, char c) {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return;
        }

        rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = c;
    }
};

inline char symbolForBodyKind(core::BodyKind kind) {
    switch (kind) {
    case core::BodyKind::Star:
        return 'S';
    case core::BodyKind::Planet:
        return 'P';
    case core::BodyKind::Asteroid:
        return 'a';
    case core::BodyKind::BlackHole:
        return '@';
    case core::BodyKind::Generic:
    default:
        return 'o';
    }
}

inline void clearTerminalFrame() {
    // ANSI clear + cursor home; works in modern terminals (including WSL).
    std::cout << "\x1B[2J\x1B[H";
}

} // namespace spacesim::render