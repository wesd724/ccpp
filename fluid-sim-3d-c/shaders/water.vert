#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in float a_foam;
layout(location = 3) in float a_velocity;

uniform mat4 u_view_projection;

out vec3 v_world;
out vec3 v_normal;
out float v_foam;
out float v_velocity;

void main() {
    v_world = a_position;
    v_normal = a_normal;
    v_foam = a_foam;
    v_velocity = a_velocity;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
}
