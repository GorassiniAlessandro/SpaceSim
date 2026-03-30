#pragma once

#include "spacesim/core/World.hpp"

namespace spacesim::render {

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void render(const core::World& world) = 0;
    virtual const char* name() const = 0;
};

} // namespace spacesim::render
