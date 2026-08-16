#pragma once

#include <cmath>
#include <cstring>
#include <array>

namespace vk_math {

constexpr float PI = 3.14159265358979323846f;

inline float radians(float degrees) {
    return degrees * (PI / 180.0f);
}

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }

    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }

    Vec3 cross(const Vec3& o) const {
        return Vec3(
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        );
    }

    Vec3 normalize() const {
        float len = std::sqrt(dot(*this));
        if (len > 0.0f) {
            float inv = 1.0f / len;
            return Vec3(x * inv, y * inv, z * inv);
        }
        return *this;
    }
};

struct Mat4 {
    // Column-major order 4x4 matrix: m[col][row]
    float m[4][4];

    Mat4() {
        std::memset(m, 0, sizeof(m));
    }

    static Mat4 identity() {
        Mat4 res;
        res.m[0][0] = 1.0f;
        res.m[1][1] = 1.0f;
        res.m[2][2] = 1.0f;
        res.m[3][3] = 1.0f;
        return res;
    }

    Mat4 operator*(const Mat4& r) const {
        Mat4 res;
        for (int c = 0; c < 4; ++c) {
            for (int r_idx = 0; r_idx < 4; ++r_idx) {
                res.m[c][r_idx] = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    res.m[c][r_idx] += m[k][r_idx] * r.m[c][k];
                }
            }
        }
        return res;
    }

    static Mat4 translate(const Vec3& v) {
        Mat4 res = identity();
        res.m[3][0] = v.x;
        res.m[3][1] = v.y;
        res.m[3][2] = v.z;
        return res;
    }

    static Mat4 scale(const Vec3& v) {
        Mat4 res = identity();
        res.m[0][0] = v.x;
        res.m[1][1] = v.y;
        res.m[2][2] = v.z;
        return res;
    }

    static Mat4 rotate(float angleRad, const Vec3& axis) {
        Mat4 res = identity();
        Vec3 a = axis.normalize();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        float nc = 1.0f - c;

        res.m[0][0] = c + a.x * a.x * nc;
        res.m[0][1] = a.x * a.y * nc + a.z * s;
        res.m[0][2] = a.x * a.z * nc - a.y * s;

        res.m[1][0] = a.y * a.x * nc - a.z * s;
        res.m[1][1] = c + a.y * a.y * nc;
        res.m[1][2] = a.y * a.z * nc + a.x * s;

        res.m[2][0] = a.z * a.x * nc + a.y * s;
        res.m[2][1] = a.z * a.y * nc - a.x * s;
        res.m[2][2] = c + a.z * a.z * nc;

        return res;
    }

    // Vulkan LookAt Matrix: RH view coordinate system
    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = (center - eye).normalize();
        Vec3 s = f.cross(up).normalize();
        Vec3 u = s.cross(f);

        Mat4 res = identity();
        res.m[0][0] = s.x;
        res.m[1][0] = s.y;
        res.m[2][0] = s.z;

        res.m[0][1] = u.x;
        res.m[1][1] = u.y;
        res.m[2][1] = u.z;

        res.m[0][2] = -f.x;
        res.m[1][2] = -f.y;
        res.m[2][2] = -f.z;

        res.m[3][0] = -s.dot(eye);
        res.m[3][1] = -u.dot(eye);
        res.m[3][2] = f.dot(eye);

        return res;
    }

    // Vulkan Perspective Projection (Depth [0, 1], inverted Y for Vulkan NDC)
    static Mat4 perspective(float fovYRad, float aspect, float zNear, float zFar) {
        float tanHalfFovy = std::tan(fovYRad / 2.0f);

        Mat4 res;
        res.m[0][0] = 1.0f / (aspect * tanHalfFovy);
        res.m[1][1] = 1.0f / (tanHalfFovy);
        res.m[2][2] = zFar / (zNear - zFar);
        res.m[2][3] = -1.0f;
        res.m[3][2] = -(zFar * zNear) / (zFar - zNear);

        // Vulkan clip space has inverted Y compared to OpenGL
        res.m[1][1] *= -1.0f;

        return res;
    }

    Mat4 inverse() const {
        Mat4 inv;
        float invArr[16];
        const float* mArr = &m[0][0];

        invArr[0] = mArr[5]  * mArr[10] * mArr[15] - 
                    mArr[5]  * mArr[11] * mArr[14] - 
                    mArr[9]  * mArr[6]  * mArr[15] + 
                    mArr[9]  * mArr[7]  * mArr[14] +
                    mArr[13] * mArr[6]  * mArr[11] - 
                    mArr[13] * mArr[7]  * mArr[10];

        invArr[4] = -mArr[4]  * mArr[10] * mArr[15] + 
                     mArr[4]  * mArr[11] * mArr[14] + 
                     mArr[8]  * mArr[6]  * mArr[15] - 
                     mArr[8]  * mArr[7]  * mArr[14] - 
                     mArr[12] * mArr[6]  * mArr[11] + 
                     mArr[12] * mArr[7]  * mArr[10];

        invArr[8] = mArr[4]  * mArr[9] * mArr[15] - 
                    mArr[4]  * mArr[11] * mArr[13] - 
                    mArr[8]  * mArr[5] * mArr[15] + 
                    mArr[8]  * mArr[7] * mArr[13] + 
                    mArr[12] * mArr[5] * mArr[11] - 
                    mArr[12] * mArr[7] * mArr[9];

        invArr[12] = -mArr[4]  * mArr[9] * mArr[14] + 
                      mArr[4]  * mArr[10] * mArr[13] +
                      mArr[8]  * mArr[5] * mArr[14] - 
                      mArr[8]  * mArr[6] * mArr[13] - 
                      mArr[12] * mArr[5] * mArr[10] + 
                      mArr[12] * mArr[6] * mArr[9];

        invArr[1] = -mArr[1]  * mArr[10] * mArr[15] + 
                     mArr[1]  * mArr[11] * mArr[14] + 
                     mArr[9]  * mArr[2] * mArr[15] - 
                     mArr[9]  * mArr[3] * mArr[14] - 
                     mArr[13] * mArr[2] * mArr[11] + 
                     mArr[13] * mArr[3] * mArr[10];

        invArr[5] = mArr[0]  * mArr[10] * mArr[15] - 
                    mArr[0]  * mArr[11] * mArr[14] - 
                    mArr[8]  * mArr[2] * mArr[15] + 
                    mArr[8]  * mArr[3] * mArr[14] + 
                    mArr[12] * mArr[2] * mArr[11] - 
                    mArr[12] * mArr[3] * mArr[10];

        invArr[9] = -mArr[0]  * mArr[9] * mArr[15] + 
                     mArr[0]  * mArr[11] * mArr[13] + 
                     mArr[8]  * mArr[1] * mArr[15] - 
                     mArr[8]  * mArr[3] * mArr[13] - 
                     mArr[12] * mArr[1] * mArr[11] + 
                     mArr[12] * mArr[3] * mArr[9];

        invArr[13] = mArr[0]  * mArr[9] * mArr[14] - 
                     mArr[0]  * mArr[10] * mArr[13] - 
                     mArr[8]  * mArr[1] * mArr[14] + 
                     mArr[8]  * mArr[2] * mArr[13] + 
                     mArr[12] * mArr[1] * mArr[10] - 
                     mArr[12] * mArr[2] * mArr[9];

        invArr[2] = mArr[1]  * mArr[6] * mArr[15] - 
                    mArr[1]  * mArr[7] * mArr[14] - 
                    mArr[5]  * mArr[2] * mArr[15] + 
                    mArr[5]  * mArr[3] * mArr[14] + 
                    mArr[13] * mArr[2] * mArr[7] - 
                    mArr[13] * mArr[3] * mArr[6];

        invArr[6] = -mArr[0]  * mArr[6] * mArr[15] + 
                     mArr[0]  * mArr[7] * mArr[14] + 
                     mArr[4]  * mArr[2] * mArr[15] - 
                     mArr[4]  * mArr[3] * mArr[14] - 
                     mArr[12] * mArr[2] * mArr[7] + 
                     mArr[12] * mArr[3] * mArr[6];

        invArr[10] = mArr[0]  * mArr[5] * mArr[15] - 
                     mArr[0]  * mArr[7] * mArr[13] - 
                     mArr[4]  * mArr[1] * mArr[15] + 
                     mArr[4]  * mArr[3] * mArr[13] + 
                     mArr[12] * mArr[1] * mArr[7] - 
                     mArr[12] * mArr[3] * mArr[5];

        invArr[14] = -mArr[0]  * mArr[5] * mArr[14] + 
                      mArr[0]  * mArr[6] * mArr[13] + 
                      mArr[4]  * mArr[1] * mArr[14] - 
                      mArr[4]  * mArr[2] * mArr[13] - 
                      mArr[12] * mArr[1] * mArr[6] + 
                      mArr[12] * mArr[2] * mArr[5];

        invArr[3] = -mArr[1] * mArr[6] * mArr[11] + 
                     mArr[1] * mArr[7] * mArr[10] + 
                     mArr[5] * mArr[2] * mArr[11] - 
                     mArr[5] * mArr[3] * mArr[10] - 
                     mArr[9] * mArr[2] * mArr[7] + 
                     mArr[9] * mArr[3] * mArr[6];

        invArr[7] = mArr[0] * mArr[6] * mArr[11] - 
                    mArr[0] * mArr[7] * mArr[10] - 
                    mArr[4] * mArr[2] * mArr[11] + 
                    mArr[4] * mArr[3] * mArr[10] + 
                    mArr[8] * mArr[2] * mArr[7] - 
                    mArr[8] * mArr[3] * mArr[6];

        invArr[11] = -mArr[0] * mArr[5] * mArr[11] + 
                      mArr[0] * mArr[7] * mArr[9] + 
                      mArr[4] * mArr[1] * mArr[11] - 
                      mArr[4] * mArr[3] * mArr[9] - 
                      mArr[8] * mArr[1] * mArr[7] + 
                      mArr[8] * mArr[3] * mArr[5];

        invArr[15] = mArr[0] * mArr[5] * mArr[10] - 
                     mArr[0] * mArr[6] * mArr[9] - 
                     mArr[4] * mArr[1] * mArr[10] + 
                     mArr[4] * mArr[2] * mArr[9] + 
                     mArr[8] * mArr[1] * mArr[6] - 
                     mArr[8] * mArr[2] * mArr[5];

        float det = mArr[0] * invArr[0] + mArr[1] * invArr[4] + mArr[2] * invArr[8] + mArr[3] * invArr[12];
        if (det == 0.0f) return identity();

        det = 1.0f / det;
        for (int i = 0; i < 16; i++) {
            reinterpret_cast<float*>(&inv.m[0][0])[i] = invArr[i] * det;
        }
        return inv;
    }
};

} // namespace vk_math
