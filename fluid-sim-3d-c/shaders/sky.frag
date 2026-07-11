#version 330 core

in vec2 v_uv;
uniform float u_aspect;
uniform vec3 u_view_direction;
uniform vec3 u_sun_direction;
uniform float u_exposure;
out vec4 fragment_color;

void main() {
    vec2 screen = v_uv * 2.0 - 1.0;
    screen.x *= u_aspect;
    vec3 forward = normalize(u_view_direction);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = normalize(cross(right, forward));
    vec3 ray = normalize(forward + right * screen.x * 0.62 + up * screen.y * 0.62);
    float elevation = smoothstep(-0.18, 0.82, ray.y);
    vec3 color = mix(vec3(0.78, 0.86, 0.91), vec3(0.12, 0.39, 0.72), elevation);
    float horizon = exp(-abs(ray.y) * 8.0);
    color += horizon * vec3(0.20, 0.19, 0.16);
    float sun = pow(max(dot(ray, normalize(u_sun_direction)), 0.0), 1300.0);
    float glow = pow(max(dot(ray, normalize(u_sun_direction)), 0.0), 32.0);
    color += sun * vec3(8.0, 6.2, 3.8) + glow * vec3(0.32, 0.20, 0.08);
    color *= u_exposure;
    fragment_color = vec4(pow(color / (color + vec3(1.0)), vec3(1.0 / 2.2)), 1.0);
}
