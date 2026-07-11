#version 330 core

in vec3 v_world;
in vec3 v_normal;
in float v_foam;
in float v_velocity;

uniform vec3 u_camera;
uniform vec3 u_light_direction;
uniform float u_time;

out vec4 fragment_color;

vec3 sky_radiance(vec3 direction) {
    float horizon = pow(1.0 - abs(direction.y), 4.0);
    vec3 zenith = vec3(0.16, 0.42, 0.72);
    vec3 horizon_color = vec3(0.77, 0.86, 0.94);
    vec3 sky = mix(horizon_color, zenith, smoothstep(-0.05, 0.85, direction.y));
    float sun = pow(max(dot(direction, normalize(-u_light_direction)), 0.0), 900.0);
    return sky + horizon * vec3(0.08, 0.11, 0.13) + sun * vec3(8.0, 6.5, 4.2);
}

vec2 micro_slope(vec2 position) {
    vec2 slope = vec2(0.0);
    slope += vec2(cos(dot(position, vec2(1.7, 0.45)) + u_time * 1.5),
                  sin(dot(position, vec2(0.55, 1.9)) - u_time * 1.25)) * 0.014;
    slope += vec2(sin(dot(position, vec2(3.4, -1.1)) - u_time * 2.0),
                  cos(dot(position, vec2(-1.4, 3.7)) + u_time * 1.8)) * 0.006;
    return slope;
}

vec3 aces_tonemap(vec3 color) {
    return clamp((color * (2.51 * color + 0.03)) /
                 (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec2 micro = micro_slope(v_world.xz);
    vec3 normal = normalize(v_normal + vec3(micro.x, 0.0, micro.y) * (1.0 - v_foam * 0.7));
    vec3 view = normalize(u_camera - v_world);
    vec3 light = normalize(-u_light_direction);
    vec3 half_vector = normalize(view + light);

    float facing = max(dot(normal, view), 0.0);
    float fresnel = 0.0204 + 0.9796 * pow(1.0 - facing, 5.0);
    float depth = clamp(v_world.y + 2.35, 0.05, 5.8);
    float optical_depth = depth / max(facing, 0.22);
    vec3 transmittance = exp(-vec3(0.70, 0.19, 0.085) * optical_depth);
    vec3 deep_scatter = vec3(0.004, 0.070, 0.105) * (1.0 - transmittance);
    vec3 floor_light = vec3(0.28, 0.36, 0.32) * transmittance * 0.30;

    vec3 reflected = sky_radiance(reflect(-view, normal));
    float roughness = mix(0.055, 0.24, clamp(v_foam + v_velocity * 0.025, 0.0, 1.0));
    float exponent = mix(240.0, 32.0, roughness);
    float sun_glint = pow(max(dot(normal, half_vector), 0.0), exponent) *
                      max(dot(normal, light), 0.0) * (1.0 / max(roughness, 0.04));

    float foam_grain = 0.82 + 0.18 * sin(v_world.x * 2.7 + sin(v_world.z * 3.1) + u_time * 1.3);
    float foam = smoothstep(0.30, 0.88, v_foam * foam_grain);
    vec3 water = mix(deep_scatter + floor_light, reflected * 0.74, fresnel);
    water += sun_glint * vec3(1.0, 0.82, 0.56) * 0.16;
    water = mix(water, vec3(0.88, 0.95, 0.97), foam * 0.76);
    water += vec3(0.02, 0.12, 0.14) * max(dot(normal, light), 0.0);
    fragment_color = vec4(pow(aces_tonemap(water), vec3(1.0 / 2.2)), 1.0);
}
