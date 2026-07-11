#version 430 core

layout(location = 0) in vec3 a_position;
uniform mat4 u_view_projection;
uniform vec3 u_light_direction;
uniform float u_floor;
uniform float u_point_scale;

void main() {
    float distance = (u_floor - a_position.y) / min(u_light_direction.y, -0.001);
    vec3 shadow_position = a_position + u_light_direction * max(distance, 0.0);
    shadow_position.y = u_floor + 0.012;
    gl_Position = u_view_projection * vec4(shadow_position, 1.0);
    gl_PointSize = clamp(u_point_scale / max(gl_Position.w, 0.3), 1.0, 8.0);
}
