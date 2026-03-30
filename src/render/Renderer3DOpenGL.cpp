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

// Disegna una sfera usando coordinate sferiche corrette (latitude/longitude)
void drawSphere(float radius, int lats = 16, int lons = 16) {
    const float pi = 3.14159265359f;

    for (int i = 0; i < lats; ++i) {
        const float phi1 = (pi * static_cast<float>(i)) / static_cast<float>(lats);
        const float phi2 = (pi * static_cast<float>(i + 1)) / static_cast<float>(lats);

        const float sinPhi1 = std::sin(phi1);
        const float cosPhi1 = std::cos(phi1);
        const float sinPhi2 = std::sin(phi2);
        const float cosPhi2 = std::cos(phi2);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= lons; ++j) {
            const float theta = (2.0f * pi * static_cast<float>(j)) / static_cast<float>(lons);
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);

            // Prima fascia: phi1
            glVertex3f(
                radius * sinPhi1 * cosTheta,
                radius * cosPhi1,
                radius * sinPhi1 * sinTheta
            );

            // Seconda fascia: phi2
            glVertex3f(
                radius * sinPhi2 * cosTheta,
                radius * cosPhi2,
                radius * sinPhi2 * sinTheta
            );
        }
        glEnd();
    }
}

// Disegna un disco (per accretion disk di buchi neri)
void drawDisk(float innerRadius, float outerRadius, int segments = 32) {
    const float pi2 = 6.283185307f;
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(segments)) * pi2;
        const float cosA = std::cos(angle);
        const float sinA = std::sin(angle);

        glVertex3f(innerRadius * cosA, 0.0f, innerRadius * sinA);
        glVertex3f(outerRadius * cosA, 0.0f, outerRadius * sinA);
    }
    glEnd();
}

// Disegna una freccia dal punto start al punto end
void drawArrow(float startX, float startY, float startZ, float endX, float endY, float endZ, float arrowSize = 0.1f) {
    // Linea principale
    glBegin(GL_LINES);
    glVertex3f(startX, startY, startZ);
    glVertex3f(endX, endY, endZ);
    glEnd();
    
    // Punta della freccia (cone semplice)
    glPushMatrix();
    glTranslatef(endX, endY, endZ);
    
    // Direzione della freccia
    float dx = endX - startX;
    float dy = endY - startY;
    float dz = endZ - startZ;
    float len = std::sqrt(dx*dx + dy*dy + dz*dz);
    
    if (len > 0.001f) {
        dx /= len;
        dy /= len;
        dz /= len;
        
        // Piccolo cono come punta
        glBegin(GL_TRIANGLES);
        // Base del cono (tre vertici)
        const float coneBase = arrowSize * 0.15f;
        glVertex3f(0, 0, 0);
        glVertex3f(coneBase * dy, coneBase * dz, -coneBase * dx);
        glVertex3f(-coneBase * dy, -coneBase * dz, coneBase * dx);
        glEnd();
    }
    
    glPopMatrix();
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

    // Renderizza corpi come sfere 3D con geometria realistica
    for (const auto& body : bodies) {
        const float x = static_cast<float>((body.position.x - centerX) * inv);
        const float y = static_cast<float>((body.position.y - centerY) * inv);
        const float z = static_cast<float>((body.position.z - centerZ) * inv);

        // Raggio della sfera: proporzionale alle posizioni per mantenere rapporti costanti
        // Moltiplicatore aumentato per migliore visibilità
        float radiusDisplay = static_cast<float>(std::max(0.015 * maxRadius, body.radius * 1.5) * inv);

        glPushMatrix();
        glTranslatef(x, y, z);

        // Colori e geometrie per tipo di corpo
        switch (body.kind) {
        case core::BodyKind::Star: {
            // Stella: giallo-arancione brillante
            glColor3f(1.0f, 0.9f, 0.3f);
            drawSphere(radiusDisplay, 20, 20);
            
            // Alone attorno a stelle (raggio doppio, trasparente)
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glColor4f(1.0f, 0.6f, 0.0f, 0.2f);
            drawSphere(radiusDisplay * 1.8f, 12, 12);
            glDisable(GL_BLEND);
            break;
        }
        case core::BodyKind::Planet: {
            // Pianeta: azzurro con atmosfera
            glColor3f(0.3f, 0.6f, 1.0f);
            drawSphere(radiusDisplay, 20, 20);
            
            // Atmosfera come fascia sottile sulla superficie
            glColor3f(0.5f, 0.8f, 1.0f);
            drawSphere(radiusDisplay * 1.1f, 16, 16);
            break;
        }
        case core::BodyKind::Asteroid: {
            // Asteroide: grigio-marrone, leggermente irregolare
            glColor3f(0.65f, 0.6f, 0.5f);
            drawSphere(radiusDisplay, 12, 12);
            
            // Variazione cromatica per aspetto irregolare
            glColor3f(0.75f, 0.65f, 0.55f);
            drawSphere(radiusDisplay * 0.95f, 10, 10);
            break;
        }
        case core::BodyKind::BlackHole: {
            // Buco nero: sfera nera con disco di accrezione
            glColor3f(0.1f, 0.05f, 0.1f);
            drawSphere(radiusDisplay, 24, 24);
            
            // Disco di accrezione attorno al buco nero
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDisable(GL_DEPTH_TEST);
            
            const float diskInner = radiusDisplay * 0.9f;
            const float diskOuter = radiusDisplay * 3.5f;
            glColor4f(1.0f, 0.4f, 0.1f, 0.4f);
            drawDisk(diskInner, diskOuter, 32);
            
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            break;
        }
        case core::BodyKind::Generic:
        default: {
            // Corpo generico: bianco
            glColor3f(0.9f, 0.9f, 0.9f);
            drawSphere(radiusDisplay, 16, 16);
            break;
        }
        }

        glPopMatrix();
    }

    // Renderizza vettori di velocità e traiettoria predetta
    // Solo per i primi 2 corpi per chiarezza
    int bodyCount = 0;
    for (const auto& body : bodies) {
        if (bodyCount >= 2 || body.isStatic) {
            bodyCount++;
            continue;
        }

        const float x = static_cast<float>((body.position.x - centerX) * inv);
        const float y = static_cast<float>((body.position.y - centerY) * inv);
        const float z = static_cast<float>((body.position.z - centerZ) * inv);

        // Vettore di velocità (verde)
        const float velScale = 1e-6f;  // Aumentato per visibilità
        const float velX = x + static_cast<float>(body.velocity.x * velScale * inv);
        const float velY = y + static_cast<float>(body.velocity.y * velScale * inv);
        const float velZ = z + static_cast<float>(body.velocity.z * velScale * inv);
        
        glLineWidth(2.0f);
        glColor3f(0.2f, 1.0f, 0.2f);
        drawArrow(x, y, z, velX, velY, velZ, 0.05f);
        glLineWidth(1.0f);

        // Calcola accelerazione gravitazionale dovuta a tutti gli altri corpi
        spacesim::core::Vec3 totalAccel{0, 0, 0};
        const double G = 6.67430e-11;
        const double c2 = 299792458.0 * 299792458.0;
        
        for (const auto& other : bodies) {
            if (other.name == body.name) continue;
            
            const spacesim::core::Vec3 delta = other.position - body.position;
            const double distanceSq = delta.lengthSquared() + 1e6;
            const double distance = std::sqrt(distanceSq);
            
            if (distance > 0) {
                double correction = 1.0;
                const spacesim::core::Vec3 vRel = body.velocity - other.velocity;
                const spacesim::core::Vec3 h = cross(delta, vRel);
                const double h2 = h.lengthSquared();
                if (c2 > 0.0) correction += (3.0 * h2) / (distanceSq * c2);
                
                const double accelMag = ((G * other.mass) / distanceSq) * correction;
                totalAccel += delta * (static_cast<float>(accelMag / distance));
            }
        }
        
        // Vettore di accelerazione (rosso)
        const float accelScale = 1e-10f;  // Aumentato per visibilità
        const float accelX = x + static_cast<float>(totalAccel.x * accelScale * inv);
        const float accelY = y + static_cast<float>(totalAccel.y * accelScale * inv);
        const float accelZ = z + static_cast<float>(totalAccel.z * accelScale * inv);
        
        glLineWidth(2.0f);
        glColor3f(1.0f, 0.2f, 0.2f);
        drawArrow(x, y, z, accelX, accelY, accelZ, 0.05f);
        glLineWidth(1.0f);

        // Traiettoria predetta (simula i prossimi 100 passi)
        spacesim::core::Vec3 pos = body.position;
        spacesim::core::Vec3 vel = body.velocity;
        const double dt = 0.5;
        const int predictionSteps = 100;
        
        glColor3f(0.5f, 0.8f, 1.0f);
        glLineWidth(1.5f);
        glBegin(GL_LINE_STRIP);
        
        for (int step = 0; step < predictionSteps; ++step) {
            // Calcola accelerazione per questa posizione
            spacesim::core::Vec3 accel{0, 0, 0};
            for (const auto& other : bodies) {
                if (other.name == body.name) continue;
                
                const spacesim::core::Vec3 delta = other.position - pos;
                const double distanceSq = delta.lengthSquared() + 1e6;
                const double distance = std::sqrt(distanceSq);
                
                if (distance > 0) {
                    double correction = 1.0;
                    const spacesim::core::Vec3 vRel = vel - other.velocity;
                    const spacesim::core::Vec3 h = cross(delta, vRel);
                    const double h2 = h.lengthSquared();
                    if (c2 > 0.0) correction += (3.0 * h2) / (distanceSq * c2);
                    
                    const double accelMag = ((G * other.mass) / distanceSq) * correction;
                    accel += delta * (accelMag / distance);
                }
            }
            
            // Velocity-Verlet step
            pos += vel * dt + accel * (0.5 * dt * dt);
            vel += accel * dt;
            
            // Disegna punto della traiettoria
            const float px = static_cast<float>((pos.x - centerX) * inv);
            const float py = static_cast<float>((pos.y - centerY) * inv);
            const float pz = static_cast<float>((pos.z - centerZ) * inv);
            glVertex3f(px, py, pz);
        }
        
        glEnd();
        glLineWidth(1.0f);

        bodyCount++;
    }

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
