#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in float a_alpha;
layout(location = 2) in float a_size;
uniform mat4 u_view_projection;
out float v_alpha;

void main() {
    gl_Position = u_view_projection * vec4(a_position, 1.0);
    gl_PointSize = a_size / max(gl_Position.w * 0.08, 0.55);
    v_alpha = a_alpha;
}
