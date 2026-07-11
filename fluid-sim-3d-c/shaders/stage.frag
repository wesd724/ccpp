#version 330 core

in vec3 v_world;
uniform float u_time;
uniform vec3 u_light_direction;
out vec4 fragment_color;

float pillar_shadow(vec3 position) {
    vec3 to_light = normalize(-u_light_direction);
    float horizontal_length_squared = dot(to_light.xz, to_light.xz);
    float shadow = 0.0;
    for (int i = 0; i < 5; ++i) {
        vec2 center = vec2(-2.2 + float(i) * 1.1, sin(float(i) * 1.35) * 0.48);
        float top = -0.1 + float(i) * 0.52;
        float along = dot(center - position.xz, to_light.xz) /
                      max(horizontal_length_squared, 0.0001);
        if (along <= 0.0) continue;
        vec2 closest = position.xz + to_light.xz * along;
        float radial = length(closest - center);
        float ray_height = position.y + to_light.y * along;
        float cylinder = 1.0 - smoothstep(0.34, 0.62, radial);
        shadow = max(shadow, cylinder * step(-2.32, ray_height) * step(ray_height, top));
    }
    return shadow;
}

void main() {
    vec2 p = v_world.xz;
    float fine = sin(p.x * 1.7) * sin(p.y * 1.9) * 0.5 + 0.5;
    float spotlight = exp(-dot(p, p) * 0.0045);
    float line_x = 1.0 - smoothstep(0.012, 0.035, abs(fract(p.x * 0.25) - 0.5));
    float line_z = 1.0 - smoothstep(0.012, 0.035, abs(fract(p.y * 0.25) - 0.5));
    vec3 color = vec3(0.105, 0.125, 0.145);
    color += spotlight * vec3(0.14, 0.17, 0.19);
    color += fine * 0.012;
    color += max(line_x, line_z) * vec3(0.035, 0.042, 0.048);
    color *= 1.0 - pillar_shadow(v_world) * 0.48;
    fragment_color = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
