#version 330 core

in vec2 v_uv;
uniform float u_aspect;
uniform vec3 u_view_direction;
uniform vec3 u_sun_direction;
out vec4 fragment_color;

vec3 aces_tonemap(vec3 color) {
    return clamp((color * (2.51 * color + 0.03)) /
                 (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec2 screen = v_uv * 2.0 - 1.0;
    screen.x *= u_aspect;
    vec3 forward = normalize(u_view_direction);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = normalize(cross(right, forward));
    vec3 ray = normalize(forward + right * screen.x * 0.62 + up * screen.y * 0.62);
    float elevation = smoothstep(-0.18, 0.82, ray.y);
    vec3 horizon = vec3(0.78, 0.72, 0.66);
    vec3 zenith = vec3(0.30, 0.40, 0.56);
    vec3 color = mix(horizon, zenith, elevation);
    color += exp(-abs(ray.y) * 7.0) * vec3(0.13, 0.09, 0.055);
    float sun = pow(max(dot(ray, normalize(u_sun_direction)), 0.0), 900.0);
    float glow = pow(max(dot(ray, normalize(u_sun_direction)), 0.0), 24.0);
    color += sun * vec3(9.0, 7.1, 4.5) + glow * vec3(0.32, 0.20, 0.09);
    fragment_color = vec4(pow(aces_tonemap(color * 1.35), vec3(1.0 / 2.2)), 1.0);
}
