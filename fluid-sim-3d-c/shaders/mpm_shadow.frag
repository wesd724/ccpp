#version 430 core

out vec4 fragment_color;

void main() {
    vec2 point = gl_PointCoord * 2.0 - 1.0;
    float radius_squared = dot(point, point);
    if (radius_squared > 1.0) discard;
    float alpha = (1.0 - smoothstep(0.12, 1.0, radius_squared)) * 0.022;
    fragment_color = vec4(0.025, 0.030, 0.034, alpha);
}
