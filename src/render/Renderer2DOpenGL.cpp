#include "spacesim/render/Renderer2DOpenGL.hpp"

#ifdef SPACESIM_ENABLE_OPENGL

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

#include <GLFW/glfw3.h>

#include "spacesim/render/TerminalRenderCommon.hpp"
#include "spacesim/render/WindowCommandBridge.hpp"

namespace spacesim::render {

namespace {

struct GlfwRuntime {
    GlfwRuntime() {
        ok = glfwInit() == GLFW_TRUE;
    }

    ~GlfwRuntime() = default;

    bool ok = false;
};

GlfwRuntime& runtime() {
    static GlfwRuntime instance;
    return instance;
}

} // namespace

struct Renderer2DOpenGL::Impl {
    GLFWwindow* window = nullptr;
    bool initFailed = false;
    bool closeCommandSent = false;
    std::array<unsigned char, GLFW_KEY_LAST + 1> keyDown{};
    double panX = 0.0;
    double panY = 0.0;
    double zoom = 1.0;
};

Renderer2DOpenGL::Renderer2DOpenGL() : impl_(new Impl{}) {}

Renderer2DOpenGL::~Renderer2DOpenGL() {
    if (impl_->window != nullptr) {
        glfwDestroyWindow(impl_->window);
    }
    delete impl_;
}

void Renderer2DOpenGL::render(const core::World& world) {
    if (impl_->initFailed) {
        return;
    }

    auto& rt = runtime();
    if (!rt.ok) {
        impl_->initFailed = true;
        std::cout << "[OpenGL] glfwInit fallita, fallback consigliato su renderer ASCII.\n";
        return;
    }

    if (impl_->window == nullptr) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        impl_->window = glfwCreateWindow(1280, 720, "SpaceSim OpenGL 2D", nullptr, nullptr);
        if (impl_->window == nullptr) {
            impl_->initFailed = true;
            const char* errStr = nullptr;
            const int errCode = glfwGetError(&errStr);
            std::cout << "[OpenGL] creazione finestra fallita (" << errCode
                      << "): " << (errStr != nullptr ? errStr : "errore sconosciuto")
                      << ". fallback consigliato su renderer ASCII.\n";
            window_commands::enqueue("gfx ascii");
            return;
        }
    }

    if (glfwWindowShouldClose(impl_->window) == GLFW_TRUE) {
        if (!impl_->closeCommandSent) {
            window_commands::enqueue("gfx ascii");
            impl_->closeCommandSent = true;
        }
        return;
    }

    glfwMakeContextCurrent(impl_->window);
    glfwSwapInterval(1);
    glfwPollEvents();

    const auto queueKey = [&](int key, const char* command) {
        if (key < 0 || key > GLFW_KEY_LAST) {
            return;
        }
        const bool pressed = glfwGetKey(impl_->window, key) == GLFW_PRESS;
        if (pressed && impl_->keyDown[static_cast<std::size_t>(key)] == 0) {
            window_commands::enqueue(command);
        }
        impl_->keyDown[static_cast<std::size_t>(key)] = pressed ? 1 : 0;
    };

    queueKey(GLFW_KEY_SPACE, "s");
    queueKey(GLFW_KEY_P, "p");
    queueKey(GLFW_KEY_2, "2");
    queueKey(GLFW_KEY_3, "3");
    queueKey(GLFW_KEY_EQUAL, "+");
    queueKey(GLFW_KEY_KP_ADD, "+");
    queueKey(GLFW_KEY_MINUS, "-");
    queueKey(GLFW_KEY_KP_SUBTRACT, "-");
    queueKey(GLFW_KEY_R, "r");
    queueKey(GLFW_KEY_M, "m");
    queueKey(GLFW_KEY_H, "h");
    queueKey(GLFW_KEY_ESCAPE, "q");
    queueKey(GLFW_KEY_Q, "q");

    if (glfwGetKey(impl_->window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        impl_->panX -= 0.02;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        impl_->panX += 0.02;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_UP) == GLFW_PRESS) {
        impl_->panY += 0.02;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        impl_->panY -= 0.02;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_U) == GLFW_PRESS) {
        impl_->zoom *= 1.02;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_O) == GLFW_PRESS) {
        impl_->zoom *= 0.98;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_C) == GLFW_PRESS) {
        impl_->panX = 0.0;
        impl_->panY = 0.0;
        impl_->zoom = 1.0;
    }

    if (impl_->zoom < 0.1) {
        impl_->zoom = 0.1;
    }
    if (impl_->zoom > 50.0) {
        impl_->zoom = 50.0;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(impl_->window, &width, &height);
    if (width <= 0 || height <= 0) {
        return;
    }

    glViewport(0, 0, width, height);
    glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const auto& bodies = world.bodies();
    if (bodies.empty()) {
        glfwSwapBuffers(impl_->window);
        return;
    }

    double centerX = 0.0;
    double centerY = 0.0;
    for (const auto& body : bodies) {
        centerX += body.position.x;
        centerY += body.position.y;
    }
    centerX /= static_cast<double>(bodies.size());
    centerY /= static_cast<double>(bodies.size());

    double maxRange = 1.0;
    for (const auto& body : bodies) {
        const double rx = std::abs(body.position.x - centerX);
        const double ry = std::abs(body.position.y - centerY);
        maxRange = std::max(maxRange, std::max(rx, ry));
    }

    const double inv = (1.0 / maxRange) * impl_->zoom;

    glLineWidth(1.0f);
    glBegin(GL_LINES);
    constexpr int gridMin = -100;
    constexpr int gridMax = 100;
    constexpr float gridStep = 0.1f;
    const float zoom = static_cast<float>(impl_->zoom);
    const float panX = static_cast<float>(impl_->panX);
    const float panY = static_cast<float>(impl_->panY);
    const float baseExtent = 2.5f;

    for (int i = gridMin; i <= gridMax; ++i) {
        const float t = static_cast<float>(i) * gridStep;
        const bool major = (i % 5 == 0);

        if (i == 0) {
            glColor3f(0.25f, 0.35f, 0.55f);
        } else if (major) {
            glColor3f(0.16f, 0.20f, 0.30f);
        } else {
            glColor3f(0.10f, 0.13f, 0.20f);
        }

        const float gx = (t * zoom) + panX;
        const float gy = (t * zoom) + panY;
        const float minX = (-baseExtent * zoom) + panX;
        const float maxX = (baseExtent * zoom) + panX;
        const float minY = (-baseExtent * zoom) + panY;
        const float maxY = (baseExtent * zoom) + panY;

        // Vertical grid lines
        glVertex2f(gx, minY);
        glVertex2f(gx, maxY);
        // Horizontal grid lines
        glVertex2f(minX, gy);
        glVertex2f(maxX, gy);
    }
    glEnd();

    glPointSize(6.0f);
    glBegin(GL_POINTS);
    for (const auto& body : bodies) {
        const double x = ((body.position.x - centerX) * inv) + impl_->panX;
        const double y = ((body.position.y - centerY) * inv) + impl_->panY;

        switch (body.kind) {
        case core::BodyKind::Star:
            glColor3f(1.0f, 0.95f, 0.45f);
            break;
        case core::BodyKind::Planet:
            glColor3f(0.4f, 0.7f, 1.0f);
            break;
        case core::BodyKind::Asteroid:
            glColor3f(0.7f, 0.7f, 0.7f);
            break;
        case core::BodyKind::BlackHole:
            glColor3f(0.8f, 0.2f, 0.95f);
            break;
        case core::BodyKind::Generic:
        default:
            glColor3f(0.9f, 0.9f, 0.9f);
            break;
        }

        glVertex2f(static_cast<float>(x), static_cast<float>(y));
    }
    glEnd();

    glfwSwapBuffers(impl_->window);
}

const char* Renderer2DOpenGL::name() const {
    return "2D OpenGL";
}

} // namespace spacesim::render

#else

namespace spacesim::render {

struct Renderer2DOpenGL::Impl {};

Renderer2DOpenGL::Renderer2DOpenGL() : impl_(new Impl{}) {}
Renderer2DOpenGL::~Renderer2DOpenGL() { delete impl_; }
void Renderer2DOpenGL::render(const core::World&) {}
const char* Renderer2DOpenGL::name() const { return "2D OpenGL (disabled)"; }

} // namespace spacesim::render

#endif
