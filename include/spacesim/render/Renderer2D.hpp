#pragma once

#include "spacesim/render/IRenderer.hpp"

namespace spacesim::render {

class Renderer2D final : public IRenderer {
public:
    void render(const core::World& world) override;
    const char* name() const override;
};

} // namespace spacesim::render
