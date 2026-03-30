#include "spacesim/render/Renderer3DOpenGL.hpp"

#ifdef SPACESIM_ENABLE_OPENGL

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <GLFW/glfw3.h>

#include "spacesim/render/HudState.hpp"
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

// Disegna una freccia robusta: asta + due alette finali ben visibili.
void drawArrow(float startX, float startY, float startZ, float endX, float endY, float endZ, float arrowSize = 0.1f) {
    const float dx = endX - startX;
    const float dy = endY - startY;
    const float dz = endZ - startZ;
    const float len = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    if (len < 1e-6f) {
        return;
    }

    const float ux = dx / len;
    const float uy = dy / len;
    const float uz = dz / len;

    // Asse di appoggio per costruire un vettore perpendicolare stabile.
    float ax = 0.0f;
    float ay = 1.0f;
    float az = 0.0f;
    if (std::abs(uy) > 0.95f) {
        ax = 1.0f;
        ay = 0.0f;
    }

    // perp = u x a
    float px = (uy * az) - (uz * ay);
    float py = (uz * ax) - (ux * az);
    float pz = (ux * ay) - (uy * ax);
    float plen = std::sqrt((px * px) + (py * py) + (pz * pz));
    if (plen < 1e-6f) {
        return;
    }
    px /= plen;
    py /= plen;
    pz /= plen;

    const float headLen = arrowSize;
    const float headWidth = arrowSize * 0.55f;

    // Linea principale
    glBegin(GL_LINES);
    glVertex3f(startX, startY, startZ);
    glVertex3f(endX, endY, endZ);

    // Aletta 1
    glVertex3f(endX, endY, endZ);
    glVertex3f(
        endX - (ux * headLen) + (px * headWidth),
        endY - (uy * headLen) + (py * headWidth),
        endZ - (uz * headLen) + (pz * headWidth));

    // Aletta 2
    glVertex3f(endX, endY, endZ);
    glVertex3f(
        endX - (ux * headLen) - (px * headWidth),
        endY - (uy * headLen) - (py * headWidth),
        endZ - (uz * headLen) - (pz * headWidth));
    glEnd();
}

} // namespace

struct Renderer3DOpenGL::Impl {
    GLFWwindow* window = nullptr;
    bool initFailed = false;
    bool closeCommandSent = false;
    std::array<unsigned char, GLFW_KEY_LAST + 1> keyDown{};
    unsigned int hudFrameCounter = 0;
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
    
    // Scorciatoie HUD: Numpad 1-4 per toggle pannelli, Tab per on/off
    queueKey(GLFW_KEY_KP_1, "hub toggle overview");
    queueKey(GLFW_KEY_KP_2, "hub toggle kinematics");
    queueKey(GLFW_KEY_KP_3, "hub toggle distance");
    queueKey(GLFW_KEY_KP_4, "hub toggle energy");
    queueKey(GLFW_KEY_TAB, "hub toggle-all");

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
        glfwSetWindowTitle(impl_->window, "SpaceSim OpenGL 3D | N:0");
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

    // HUD live: metriche essenziali aggiornate periodicamente nel titolo finestra.
    if ((impl_->hudFrameCounter++ % 10U) == 0U) {
        const bool hubEnabled = hud::isEnabled();

        double totalMass = 0.0;
        double totalKinetic = 0.0;
        double sumSpeed = 0.0;
        double maxSpeedValue = 0.0;
        double minDistance = std::numeric_limits<double>::infinity();

        for (const auto& body : bodies) {
            const double speed = body.velocity.length();
            totalMass += body.mass;
            sumSpeed += speed;
            maxSpeedValue = std::max(maxSpeedValue, speed);
            totalKinetic += 0.5 * body.mass * speed * speed;
        }

        for (std::size_t i = 0; i < bodies.size(); ++i) {
            for (std::size_t j = i + 1; j < bodies.size(); ++j) {
                const core::Vec3 delta = bodies[j].position - bodies[i].position;
                minDistance = std::min(minDistance, delta.length());
            }
        }
        if (!std::isfinite(minDistance)) {
            minDistance = 0.0;
        }

        const double avgSpeed = sumSpeed / static_cast<double>(bodies.size());

        std::ostringstream hudTitle;
        hudTitle << "SpaceSim OpenGL 3D"
                 << " | HUB:" << (hubEnabled ? "on" : "off");

        if (hubEnabled) {
            if (hud::isPanelEnabled(hud::Panel::Overview)) {
                hudTitle << " | N:" << bodies.size();
            }
            if (hud::isPanelEnabled(hud::Panel::Kinematics)) {
                hudTitle << " | v_avg:" << std::fixed << std::setprecision(2) << (avgSpeed / 1000.0) << " km/s"
                         << " | v_max:" << std::fixed << std::setprecision(2) << (maxSpeedValue / 1000.0) << " km/s";
            }
            if (hud::isPanelEnabled(hud::Panel::Distance)) {
                hudTitle << " | d_min:" << std::fixed << std::setprecision(3) << (minDistance / 1.0e9) << " Gm";
            }
            if (hud::isPanelEnabled(hud::Panel::Energy)) {
                hudTitle << " | Ek:" << std::scientific << std::setprecision(3) << totalKinetic << " J";
            }
        }

        glfwSetWindowTitle(impl_->window, hudTitle.str().c_str());
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

    // Renderizza vettori di velocita/gravita e traiettoria predetta.
    // Disegno in overlay (senza depth test) per evitare che sfere/griglia nascondano i vettori.
    const double G = 6.67430e-11;
    const double c2 = 299792458.0 * 299792458.0;
    const double softeningSq = 1e8;

    const auto computeAccelAt = [&](const core::Body& self,
                                    const core::Vec3& pos,
                                    const core::Vec3& vel) {
        core::Vec3 totalAccel{};
        for (const auto& other : bodies) {
            if (other.name == self.name) {
                continue;
            }

            const core::Vec3 delta = other.position - pos;
            const double distanceSq = delta.lengthSquared() + softeningSq;
            const double distance = std::sqrt(distanceSq);
            if (distance <= 0.0) {
                continue;
            }

            double correction = 1.0;
            const core::Vec3 vRel = vel - other.velocity;
            const core::Vec3 h = cross(delta, vRel);
            const double h2 = h.lengthSquared();
            correction += (3.0 * h2) / (distanceSq * c2);

            const double accelMag = ((G * other.mass) / distanceSq) * correction;
            totalAccel += delta * (accelMag / distance);
        }
        return totalAccel;
    };

    double totalMass = 0.0;
    core::Vec3 comVelocity{};
    for (const auto& body : bodies) {
        totalMass += body.mass;
        comVelocity += body.velocity * body.mass;
    }
    if (totalMass > 0.0) {
        comVelocity *= (1.0 / totalMass);
    }

    double maxSpeed = 0.0;
    double maxAccel = 0.0;
    for (const auto& body : bodies) {
        const core::Vec3 relVelocity = body.velocity - comVelocity;
        maxSpeed = std::max(maxSpeed, relVelocity.length());
        maxAccel = std::max(maxAccel, computeAccelAt(body, body.position, body.velocity).length());
    }

    if (maxSpeed < 1e-9) {
        maxSpeed = 1.0;
    }
    if (maxAccel < 1e-15) {
        maxAccel = 1.0;
    }

    glDisable(GL_DEPTH_TEST);

    int renderedDynamicBodies = 0;
    for (const auto& body : bodies) {
        if (body.isStatic) {
            continue;
        }
        if (renderedDynamicBodies >= 6) {
            break;
        }

        const float x = static_cast<float>((body.position.x - centerX) * inv);
        const float y = static_cast<float>((body.position.y - centerY) * inv);
        const float z = static_cast<float>((body.position.z - centerZ) * inv);

        const core::Vec3 relVelocity = body.velocity - comVelocity;
        const core::Vec3 accelNow = computeAccelAt(body, body.position, body.velocity);

        // Lunghezze vettori in unita schermo con scala adattiva e minimo visibile.
        const double speedRatio = relVelocity.length() / maxSpeed;
        const double accelRatio = accelNow.length() / maxAccel;
        const float velLen = static_cast<float>(std::max(0.10, 0.45 * speedRatio));
        const float accLen = static_cast<float>(std::max(0.10, 0.45 * accelRatio));

        core::Vec3 velDir = relVelocity;
        const double velMag = velDir.length();
        if (velMag > 1e-12) {
            velDir *= (1.0 / velMag);
        }

        core::Vec3 accDir = accelNow;
        const double accMag = accDir.length();
        if (accMag > 1e-18) {
            accDir *= (1.0 / accMag);
        }

        // Vettore velocita (verde)
        glLineWidth(2.0f);
        glColor3f(0.2f, 1.0f, 0.2f);
        drawArrow(
            x,
            y,
            z,
            x + static_cast<float>(velDir.x) * velLen,
            y + static_cast<float>(velDir.y) * velLen,
            z + static_cast<float>(velDir.z) * velLen,
            0.06f);

        // Vettore accelerazione gravitazionale (rosso)
        glColor3f(1.0f, 0.2f, 0.2f);
        drawArrow(
            x,
            y,
            z,
            x + static_cast<float>(accDir.x) * accLen,
            y + static_cast<float>(accDir.y) * accLen,
            z + static_cast<float>(accDir.z) * accLen,
            0.06f);
        glLineWidth(1.0f);

        // Traiettoria predetta con orizzonte adattivo (frazione del periodo orbitale).
        core::Vec3 pos = body.position;
        core::Vec3 vel = body.velocity;
        const double r = (body.position - core::Vec3{centerX, centerY, centerZ}).length();
        const double v = std::max(body.velocity.length(), 1.0);
        const double orbitalPeriodEstimate = std::max((2.0 * 3.1415926535 * r) / v, 3600.0);
        const double horizonTime = std::min(orbitalPeriodEstimate * 0.60, 365.0 * 24.0 * 3600.0);
        const int predictionSteps = 220;
        const double dtPred = horizonTime / static_cast<double>(predictionSteps);

        glColor3f(0.50f, 0.80f, 1.00f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_STRIP);
        glVertex3f(x, y, z);
        for (int step = 0; step < predictionSteps; ++step) {
            const core::Vec3 accel = computeAccelAt(body, pos, vel);
            pos += vel * dtPred + accel * (0.5 * dtPred * dtPred);
            vel += accel * dtPred;

            const float px = static_cast<float>((pos.x - centerX) * inv);
            const float py = static_cast<float>((pos.y - centerY) * inv);
            const float pz = static_cast<float>((pos.z - centerZ) * inv);
            glVertex3f(px, py, pz);
        }
        glEnd();
        glLineWidth(1.0f);

        renderedDynamicBodies++;
    }

    glEnable(GL_DEPTH_TEST);

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
