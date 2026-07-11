#version 430 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_velocity;
uniform mat4 u_view_projection;
uniform float u_point_scale;
out float v_speed;

void main() {
    gl_Position = u_view_projection * vec4(a_position, 1.0);
    gl_PointSize = clamp(u_point_scale / max(gl_Position.w, 0.2), 1.2, 12.0);
    v_speed = length(a_velocity);
}
