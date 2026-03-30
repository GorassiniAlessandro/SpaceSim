#include "spacesim/render/Renderer3DOpenGL.hpp"

#ifdef SPACESIM_ENABLE_OPENGL

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

#include <GLFW/glfw3.h>

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

struct Renderer3DOpenGL::Impl {
    GLFWwindow* window = nullptr;
    bool initFailed = false;
    bool closeCommandSent = false;
    std::array<unsigned char, GLFW_KEY_LAST + 1> keyDown{};
    float yawDeg = 35.0f;
    float pitchDeg = 20.0f;
    float cameraDistance = 3.5f;
    float panX = 0.0f;
    float panY = 0.0f;
    float zoom = 1.0f;
};

Renderer3DOpenGL::Renderer3DOpenGL() : impl_(new Impl{}) {}

Renderer3DOpenGL::~Renderer3DOpenGL() {
    if (impl_->window != nullptr) {
        glfwDestroyWindow(impl_->window);
    }
    delete impl_;
}

void Renderer3DOpenGL::render(const core::World& world) {
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
        impl_->window = glfwCreateWindow(1280, 720, "SpaceSim OpenGL 3D", nullptr, nullptr);
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

    if (glfwGetKey(impl_->window, GLFW_KEY_J) == GLFW_PRESS) {
        impl_->yawDeg -= 1.2f;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_L) == GLFW_PRESS) {
        impl_->yawDeg += 1.2f;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_I) == GLFW_PRESS) {
        impl_->pitchDeg += 1.0f;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_K) == GLFW_PRESS) {
        impl_->pitchDeg -= 1.0f;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_U) == GLFW_PRESS) {
        impl_->cameraDistance -= 0.05f;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_O) == GLFW_PRESS) {
        impl_->cameraDistance += 0.05f;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        impl_->panX -= 0.02f;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        impl_->panX += 0.02f;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_UP) == GLFW_PRESS) {
        impl_->panY += 0.02f;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        impl_->panY -= 0.02f;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_Z) == GLFW_PRESS) {
        impl_->zoom *= 1.02f;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_X) == GLFW_PRESS) {
        impl_->zoom *= 0.98f;
    }
    if (glfwGetKey(impl_->window, GLFW_KEY_C) == GLFW_PRESS) {
        impl_->yawDeg = 35.0f;
        impl_->pitchDeg = 20.0f;
        impl_->cameraDistance = 3.5f;
        impl_->panX = 0.0f;
        impl_->panY = 0.0f;
        impl_->zoom = 1.0f;
    }

    if (impl_->pitchDeg > 89.0f) {
        impl_->pitchDeg = 89.0f;
    }
    if (impl_->pitchDeg < -89.0f) {
        impl_->pitchDeg = -89.0f;
    }
    if (impl_->cameraDistance < 0.6f) {
        impl_->cameraDistance = 0.6f;
    }
    if (impl_->cameraDistance > 40.0f) {
        impl_->cameraDistance = 40.0f;
    }
    if (impl_->zoom < 0.1f) {
        impl_->zoom = 0.1f;
    }
    if (impl_->zoom > 50.0f) {
        impl_->zoom = 50.0f;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(impl_->window, &width, &height);
    if (width <= 0 || height <= 0) {
        return;
    }

    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const auto& bodies = world.bodies();
    if (bodies.empty()) {
        glfwSwapBuffers(impl_->window);
        return;
    }

    double centerX = 0.0;
    double centerY = 0.0;
    double centerZ = 0.0;
    for (const auto& body : bodies) {
        centerX += body.position.x;
        centerY += body.position.y;
        centerZ += body.position.z;
    }
    centerX /= static_cast<double>(bodies.size());
    centerY /= static_cast<double>(bodies.size());
    centerZ /= static_cast<double>(bodies.size());

    double maxRadius = 1.0;
    for (const auto& body : bodies) {
        const double dx = body.position.x - centerX;
        const double dy = body.position.y - centerY;
        const double dz = body.position.z - centerZ;
        maxRadius = std::max(maxRadius, std::sqrt((dx * dx) + (dy * dy) + (dz * dz)));
    }

    const double inv = (1.0 / maxRadius) * impl_->zoom;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const float fov = 60.0f;
    const float nearPlane = 0.1f;
    const float farPlane = 100.0f;
    const float top = std::tan((fov * 0.5f) * 3.1415926535f / 180.0f) * nearPlane;
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(impl_->panX, impl_->panY, -impl_->cameraDistance);
    glRotatef(impl_->pitchDeg, 1.0f, 0.0f, 0.0f);
    glRotatef(impl_->yawDeg, 0.0f, 1.0f, 0.0f);

    // 3D floor grid on XZ plane for depth cues.
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    constexpr int gridMin = -100;
    constexpr int gridMax = 100;
    constexpr float gridStep = 0.1f;
    const float gZoom = impl_->zoom;
    const float gExtent = 2.5f * gZoom;

    for (int i = gridMin; i <= gridMax; ++i) {
        const float t = static_cast<float>(i) * gridStep * gZoom;
        const bool major = (i % 5 == 0);

        if (i == 0) {
            glColor3f(0.20f, 0.28f, 0.45f);
        } else if (major) {
            glColor3f(0.14f, 0.18f, 0.30f);
        } else {
            glColor3f(0.09f, 0.12f, 0.20f);
        }

        // Lines parallel to Z axis
        glVertex3f(t, 0.0f, -gExtent);
        glVertex3f(t, 0.0f, gExtent);
        // Lines parallel to X axis
        glVertex3f(-gExtent, 0.0f, t);
        glVertex3f(gExtent, 0.0f, t);
    }

    // Axis guides (X red, Y green, Z blue)
    const float axisExtent = 2.8f * gZoom;
    glColor3f(0.75f, 0.25f, 0.25f);
    glVertex3f(-axisExtent, 0.0f, 0.0f);
    glVertex3f(axisExtent, 0.0f, 0.0f);

    glColor3f(0.25f, 0.75f, 0.25f);
    glVertex3f(0.0f, -axisExtent, 0.0f);
    glVertex3f(0.0f, axisExtent, 0.0f);

    glColor3f(0.25f, 0.45f, 0.85f);
    glVertex3f(0.0f, 0.0f, -axisExtent);
    glVertex3f(0.0f, 0.0f, axisExtent);
    glEnd();

    glPointSize(6.0f);
    glBegin(GL_POINTS);
    for (const auto& body : bodies) {
        const float x = static_cast<float>((body.position.x - centerX) * inv);
        const float y = static_cast<float>((body.position.y - centerY) * inv);
        const float z = static_cast<float>((body.position.z - centerZ) * inv);

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

        glVertex3f(x, y, z);
    }
    glEnd();

    glfwSwapBuffers(impl_->window);
}

const char* Renderer3DOpenGL::name() const {
    return "3D OpenGL";
}

} // namespace spacesim::render

#else

namespace spacesim::render {

struct Renderer3DOpenGL::Impl {};

Renderer3DOpenGL::Renderer3DOpenGL() : impl_(new Impl{}) {}
Renderer3DOpenGL::~Renderer3DOpenGL() { delete impl_; }
void Renderer3DOpenGL::render(const core::World&) {}
const char* Renderer3DOpenGL::name() const { return "3D OpenGL (disabled)"; }

} // namespace spacesim::render

#endif
