#version 330 core

layout(location = 0) in vec3 a_position;
uniform mat4 u_view_projection;
out vec3 v_world;

void main() {
    v_world = a_position;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
}
