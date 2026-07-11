#version 430 core

in vec3 v_center_view;
in float v_speed;
uniform float u_particle_radius;
layout(location = 0) out vec2 fragment_thickness;

void main() {
    vec2 point = gl_PointCoord * 2.0 - 1.0;
    float radius_squared = dot(point, point);
    if (radius_squared > 1.0) discard;
    float thickness = 2.0 * u_particle_radius *
                      sqrt(max(1.0 - radius_squared, 0.0));
    fragment_thickness = vec2(thickness, thickness * min(v_speed, 16.0));
}
