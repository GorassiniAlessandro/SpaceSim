#pragma once

#include "spacesim/render/IRenderer.hpp"

namespace spacesim::render {

class Renderer3DOpenGL final : public IRenderer {
public:
    Renderer3DOpenGL();
    ~Renderer3DOpenGL() override;

    void render(const core::World& world) override;
    const char* name() const override;

private:
    struct Impl;
    Impl* impl_;
};

} // namespace spacesim::render
