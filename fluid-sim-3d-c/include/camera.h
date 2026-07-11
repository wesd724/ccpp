#ifndef CAMERA_H
#define CAMERA_H

#include "math3d.h"
#include "platform.h"

typedef struct {
    Vec3 position;
    float yaw;
    float pitch;
    float fov_degrees;
} Camera;

void camera_reset(Camera *camera);
void camera_reset_cinematic(Camera *camera);
void camera_reset_material_lab(Camera *camera);
void camera_update(Camera *camera, const InputState *input, float delta_time);
Vec3 camera_forward(const Camera *camera);
Mat4 camera_view_matrix(const Camera *camera);
Mat4 camera_projection_matrix(const Camera *camera, float aspect);
Mat4 camera_view_projection(const Camera *camera, float aspect);
bool camera_screen_ray(const Camera *camera, const InputState *input, Vec3 *origin, Vec3 *direction);
bool camera_ray_to_horizontal_plane(const Camera *camera, const InputState *input,
                                    float plane_y, float half_extent, Vec3 *hit);
bool camera_ray_to_water(const Camera *camera, const InputState *input, float water_size,
                         float *world_x, float *world_z);

#endif
