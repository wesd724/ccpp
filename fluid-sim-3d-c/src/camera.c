#include "camera.h"

#include <math.h>

#define PI 3.14159265358979323846f
void camera_reset(Camera *camera) {
    camera->position = vec3(17.5f, 7.2f, 23.5f);
    camera->yaw = -127.0f;
    camera->pitch = -13.5f;
    camera->fov_degrees = 62.0f;
}

void camera_reset_cinematic(Camera *camera) {
    camera->position = vec3(11.5f, 5.8f, 14.5f);
    camera->yaw = -128.0f;
    camera->pitch = -16.0f;
    camera->fov_degrees = 55.0f;
}

void camera_reset_material_lab(Camera *camera) {
    camera->position = vec3(2.8f, 4.7f, 12.2f);
    camera->yaw = -102.9f;
    camera->pitch = -14.8f;
    camera->fov_degrees = 47.0f;
}

Vec3 camera_forward(const Camera *camera) {
    float yaw = camera->yaw * PI / 180.0f;
    float pitch = camera->pitch * PI / 180.0f;
    return vec3_normalize(vec3(cosf(yaw) * cosf(pitch), sinf(pitch), sinf(yaw) * cosf(pitch)));
}

void camera_update(Camera *camera, const InputState *input, float delta_time) {
    camera->yaw += ((input->keys[VK_RIGHT] ? 1.0f : 0.0f) -
                    (input->keys[VK_LEFT] ? 1.0f : 0.0f)) * 82.0f * delta_time;
    camera->pitch += ((input->keys[VK_UP] ? 1.0f : 0.0f) -
                      (input->keys[VK_DOWN] ? 1.0f : 0.0f)) * 65.0f * delta_time;
    camera->yaw += (float)input->mouse_delta_x * 0.14f;
    camera->pitch -= (float)input->mouse_delta_y * 0.14f;
    camera->pitch = clampf(camera->pitch, -82.0f, 82.0f);
    camera->fov_degrees = clampf(camera->fov_degrees - input->wheel_delta * 4.0f, 30.0f, 82.0f);

    Vec3 forward = camera_forward(camera);
    Vec3 flat_forward = vec3_normalize(vec3(forward.x, 0.0f, forward.z));
    Vec3 right = vec3_normalize(vec3_cross(flat_forward, vec3(0.0f, 1.0f, 0.0f)));
    float distance = (input->keys[VK_SHIFT] ? 15.0f : 7.5f) * delta_time;
    camera->position = vec3_add(camera->position,
        vec3_scale(flat_forward, ((input->keys['W'] ? 1.0f : 0.0f) -
                                  (input->keys['S'] ? 1.0f : 0.0f)) * distance));
    camera->position = vec3_add(camera->position,
        vec3_scale(right, ((input->keys['D'] ? 1.0f : 0.0f) -
                           (input->keys['A'] ? 1.0f : 0.0f)) * distance));
    camera->position.y += ((input->keys[VK_SPACE] ? 1.0f : 0.0f) -
                           (input->keys['C'] ? 1.0f : 0.0f)) * distance;
    camera->position.y = fmaxf(camera->position.y, 0.35f);
}

Mat4 camera_view_matrix(const Camera *camera) {
    Vec3 forward = camera_forward(camera);
    return mat4_look_at(camera->position, vec3_add(camera->position, forward),
                        vec3(0.0f, 1.0f, 0.0f));
}

Mat4 camera_projection_matrix(const Camera *camera, float aspect) {
    float fov_radians = camera->fov_degrees * PI / 180.0f;
    return mat4_perspective(fov_radians, aspect, 0.08f, 240.0f);
}

Mat4 camera_view_projection(const Camera *camera, float aspect) {
    return mat4_multiply(camera_projection_matrix(camera, aspect),
                         camera_view_matrix(camera));
}

bool camera_screen_ray(const Camera *camera, const InputState *input, Vec3 *origin, Vec3 *direction) {
    if (input->width <= 0 || input->height <= 0) return false;
    Vec3 forward = camera_forward(camera);
    Vec3 right = vec3_normalize(vec3_cross(forward, vec3(0.0f, 1.0f, 0.0f)));
    Vec3 up = vec3_normalize(vec3_cross(right, forward));
    float aspect = (float)input->width / (float)input->height;
    float tangent = tanf(camera->fov_degrees * PI / 360.0f);
    float screen_x = (2.0f * (float)input->mouse_x / (float)input->width - 1.0f) * aspect * tangent;
    float screen_y = (1.0f - 2.0f * (float)input->mouse_y / (float)input->height) * tangent;
    *origin = camera->position;
    *direction = vec3_normalize(vec3_add(forward,
        vec3_add(vec3_scale(right, screen_x), vec3_scale(up, screen_y))));
    return true;
}

bool camera_ray_to_horizontal_plane(const Camera *camera, const InputState *input,
                                    float plane_y, float half_extent, Vec3 *hit) {
    Vec3 origin;
    Vec3 ray;
    if (!camera_screen_ray(camera, input, &origin, &ray) || fabsf(ray.y) < 0.0001f) return false;
    float distance = (plane_y - origin.y) / ray.y;
    if (distance <= 0.0f) return false;
    *hit = vec3_add(origin, vec3_scale(ray, distance));
    return fabsf(hit->x) <= half_extent && fabsf(hit->z) <= half_extent;
}

bool camera_ray_to_water(const Camera *camera, const InputState *input, float water_size,
                         float *world_x, float *world_z) {
    Vec3 hit;
    if (!camera_ray_to_horizontal_plane(camera, input, 0.0f, water_size * 0.5f, &hit)) return false;
    *world_x = hit.x;
    *world_z = hit.z;
    return true;
}
