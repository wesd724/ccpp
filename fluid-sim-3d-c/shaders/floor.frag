#version 330 core

in vec3 v_world;
uniform float u_time;
out vec4 fragment_color;

float caustic_layer(vec2 p, float time) {
    float a = sin(p.x * 1.7 + sin(p.y * 1.3 + time));
    float b = sin(p.y * 1.9 - sin(p.x * 1.1 - time * 0.8));
    float c = sin((p.x + p.y) * 1.25 + time * 0.65);
    return pow(clamp(0.5 + 0.16 * (a + b + c), 0.0, 1.0), 8.0);
}

void main() {
    vec2 p = v_world.xz;
    float grain = sin(p.x * 0.31 + sin(p.y * 0.23)) * sin(p.y * 0.37 - p.x * 0.11);
    vec3 stone = mix(vec3(0.29, 0.32, 0.31), vec3(0.39, 0.42, 0.39), grain * 0.5 + 0.5);
    float underwater = 1.0 - smoothstep(23.5, 25.5, max(abs(p.x), abs(p.y)));
    float caustics = caustic_layer(p * 0.72, u_time * 1.3) * underwater;
    vec3 color = stone * mix(0.78, 0.48, underwater) + caustics * vec3(0.08, 0.26, 0.22);
    float distance_fog = smoothstep(35.0, 76.0, length(p));
    color = mix(color, vec3(0.42, 0.57, 0.66), distance_fog);
    fragment_color = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
