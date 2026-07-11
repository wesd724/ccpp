#version 430 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_velocity;
layout(location = 2) in float a_material;
layout(location = 3) in float a_strain;
layout(location = 4) in vec4 a_state;

uniform mat4 u_view;
uniform mat4 u_projection;
uniform float u_point_scale;
uniform float u_particle_radius;

out vec3 v_center_view;
out float v_material;
out float v_speed;
out float v_strain;
out float v_damage;
out float v_variation;

void main() {
    v_center_view = (u_view * vec4(a_position, 1.0)).xyz;
    v_material = a_material;
    v_speed = length(a_velocity);
    v_strain = a_strain;
    v_damage = a_state.y;
    v_variation = a_state.z;
    gl_Position = u_projection * vec4(v_center_view, 1.0);
    float material_scale = a_material < 0.5 ? 1.08 : a_material < 1.5 ? 1.04 : 0.98;
    gl_PointSize = clamp(u_point_scale * u_particle_radius * material_scale /
                         max(-v_center_view.z, 0.08), 1.3, 18.0);
}
