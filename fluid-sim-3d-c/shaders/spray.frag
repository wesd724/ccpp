#version 330 core

in float v_alpha;
out vec4 fragment_color;

void main() {
    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    float radius = dot(centered, centered);
    if (radius > 1.0) discard;
    float soft_edge = smoothstep(1.0, 0.18, radius);
    fragment_color = vec4(0.88, 0.96, 1.0, v_alpha * soft_edge * 0.72);
}
