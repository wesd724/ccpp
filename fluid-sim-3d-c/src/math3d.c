#include "math3d.h"

#include <math.h>

float clampf(float value, float low, float high) {
    return fminf(fmaxf(value, low), high);
}

Vec3 vec3(float x, float y, float z) {
    return (Vec3){x, y, z};
}

Vec3 vec3_add(Vec3 a, Vec3 b) {
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vec3 vec3_scale(Vec3 v, float scale) {
    return vec3(v.x * scale, v.y * scale, v.z * scale);
}

float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return vec3(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

Vec3 vec3_normalize(Vec3 v) {
    float length = sqrtf(vec3_dot(v, v));
    return length > 0.000001f ? vec3_scale(v, 1.0f / length) : vec3(0.0f, 0.0f, 0.0f);
}

Mat4 mat4_multiply(Mat4 a, Mat4 b) {
    Mat4 result = {0};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            for (int k = 0; k < 4; ++k) {
                result.m[column * 4 + row] += a.m[k * 4 + row] * b.m[column * 4 + k];
            }
        }
    }
    return result;
}

Mat4 mat4_perspective(float fov_y_radians, float aspect, float near_plane, float far_plane) {
    float focal = 1.0f / tanf(fov_y_radians * 0.5f);
    Mat4 result = {0};
    result.m[0] = focal / aspect;
    result.m[5] = focal;
    result.m[10] = (far_plane + near_plane) / (near_plane - far_plane);
    result.m[11] = -1.0f;
    result.m[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    return result;
}

Mat4 mat4_look_at(Vec3 eye, Vec3 target, Vec3 up) {
    Vec3 forward = vec3_normalize(vec3_sub(target, eye));
    Vec3 side = vec3_normalize(vec3_cross(forward, up));
    Vec3 camera_up = vec3_cross(side, forward);
    Mat4 result = {{
        side.x, camera_up.x, -forward.x, 0.0f,
        side.y, camera_up.y, -forward.y, 0.0f,
        side.z, camera_up.z, -forward.z, 0.0f,
        -vec3_dot(side, eye), -vec3_dot(camera_up, eye), vec3_dot(forward, eye), 1.0f,
    }};
    return result;
}
