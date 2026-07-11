#version 430 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_velocity;

uniform mat4 u_view;
uniform mat4 u_projection;
uniform float u_point_scale;
uniform float u_particle_radius;

out vec3 v_center_view;
out float v_speed;

void main() {
    v_center_view = (u_view * vec4(a_position, 1.0)).xyz;
    v_speed = length(a_velocity);
    gl_Position = u_projection * vec4(v_center_view, 1.0);
    gl_PointSize = clamp(u_point_scale * u_particle_radius /
                         max(-v_center_view.z, 0.08), 1.5, 96.0);
}
