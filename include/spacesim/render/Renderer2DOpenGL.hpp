#pragma once

#include "spacesim/render/IRenderer.hpp"

namespace spacesim::render {

class Renderer2DOpenGL final : public IRenderer {
public:
    Renderer2DOpenGL();
    ~Renderer2DOpenGL() override;

    void render(const core::World& world) override;
    const char* name() const override;

private:
    struct Impl;
    Impl* impl_;
};

} // namespace spacesim::render
