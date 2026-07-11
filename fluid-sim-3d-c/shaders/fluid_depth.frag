#version 430 core

in vec3 v_center_view;
in float v_speed;
uniform mat4 u_projection;
uniform float u_particle_radius;
layout(location = 0) out float fragment_depth;

void main() {
    vec2 point = gl_PointCoord * 2.0 - 1.0;
    float radius_squared = dot(point, point);
    if (radius_squared > 1.0) discard;
    float sphere_z = sqrt(max(1.0 - radius_squared, 0.0));
    vec3 view_position = v_center_view +
        vec3(point * u_particle_radius, sphere_z * u_particle_radius);
    vec4 clip = u_projection * vec4(view_position, 1.0);
    gl_FragDepth = clip.z / clip.w * 0.5 + 0.5;
    fragment_depth = -view_position.z;
}
