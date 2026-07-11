#ifndef MATH3D_H
#define MATH3D_H

typedef struct {
    float x;
    float y;
    float z;
} Vec3;

typedef struct {
    float m[16];
} Mat4;

float clampf(float value, float low, float high);
Vec3 vec3(float x, float y, float z);
Vec3 vec3_add(Vec3 a, Vec3 b);
Vec3 vec3_sub(Vec3 a, Vec3 b);
Vec3 vec3_scale(Vec3 v, float scale);
float vec3_dot(Vec3 a, Vec3 b);
Vec3 vec3_cross(Vec3 a, Vec3 b);
Vec3 vec3_normalize(Vec3 v);
Mat4 mat4_multiply(Mat4 a, Mat4 b);
Mat4 mat4_perspective(float fov_y_radians, float aspect, float near_plane, float far_plane);
Mat4 mat4_look_at(Vec3 eye, Vec3 target, Vec3 up);

#endif
