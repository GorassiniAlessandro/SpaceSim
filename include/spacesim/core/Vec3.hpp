#pragma once

#include <cmath>

namespace spacesim::core {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vec3& operator-=(const Vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    Vec3& operator*=(double scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    [[nodiscard]] double lengthSquared() const {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] double length() const {
        return std::sqrt(lengthSquared());
    }
};

inline Vec3 operator+(Vec3 lhs, const Vec3& rhs) {
    lhs += rhs;
    return lhs;
}

inline Vec3 operator-(Vec3 lhs, const Vec3& rhs) {
    lhs -= rhs;
    return lhs;
}

inline Vec3 operator*(Vec3 v, double scalar) {
    v *= scalar;
    return v;
}

inline Vec3 operator*(double scalar, Vec3 v) {
    v *= scalar;
    return v;
}

} // namespace spacesim::core
