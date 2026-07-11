#version 330 core

in vec3 v_world;
uniform float u_time;
out vec4 fragment_color;

void main() {
    vec2 p = v_world.xz;
    float radial = exp(-dot(p, p) * 0.012);
    float grain = 0.5 + 0.5 * sin(p.x * 4.2 + sin(p.y * 3.7));
    float grid_x = 1.0 - smoothstep(0.018, 0.045, abs(fract(p.x * 0.5) - 0.5));
    float grid_z = 1.0 - smoothstep(0.018, 0.045, abs(fract(p.y * 0.5) - 0.5));
    vec3 color = vec3(0.105, 0.115, 0.125);
    color += radial * vec3(0.18, 0.16, 0.13);
    color += grain * 0.012;
    color += max(grid_x, grid_z) * vec3(0.050, 0.047, 0.043);
    float warm_side = smoothstep(-5.0, 5.0, p.x);
    color += mix(vec3(0.012, 0.025, 0.038), vec3(0.040, 0.020, 0.008), warm_side);
    fragment_color = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
