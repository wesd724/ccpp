#version 330 core

in vec2 v_uv;
uniform vec3 u_color;
uniform float u_selected;
uniform float u_icon;
out vec4 fragment_color;

void main() {
    float edge = min(min(v_uv.x, 1.0 - v_uv.x), min(v_uv.y, 1.0 - v_uv.y));
    float border = 1.0 - smoothstep(0.035, 0.085, edge);
    vec3 inner = mix(u_color * 0.48, u_color, smoothstep(0.12, 0.28, edge));
    int icon = int(u_icon + 0.5);
    float ink = 0.0;
    if (icon == 0) {
        ink = 1.0 - smoothstep(0.025, 0.065, min(abs(v_uv.x - v_uv.y), abs(v_uv.x + v_uv.y - 1.0)));
    } else if (icon == 1) {
        float tower = step(0.18, v_uv.x) * step(v_uv.x, 0.82) * step(0.24, v_uv.y);
        float notch = step(0.64, v_uv.y) * step(0.14, fract(v_uv.x * 4.0));
        float gate = step(0.42, v_uv.x) * step(v_uv.x, 0.58) * step(0.58, v_uv.y);
        ink = tower * notch * (1.0 - gate);
    } else if (icon == 2) {
        ink = 1.0 - smoothstep(0.025, 0.075, abs(v_uv.y - 0.5 - sin(v_uv.x * 12.0) * 0.11));
    } else if (icon == 3) {
        ink = 1.0 - smoothstep(0.025, 0.07, abs(v_uv.x + v_uv.y * 0.55 - 0.72));
    } else if (icon == 4) {
        vec2 cell = fract(v_uv * vec2(3.0, 2.0)) - 0.5;
        ink = 1.0 - smoothstep(0.18, 0.29, length(cell));
    } else {
        ink = 1.0 - smoothstep(0.025, 0.065, abs(fract(v_uv.x * 5.0) - 0.5));
    }
    inner = mix(inner, icon == 3 ? vec3(0.88, 1.0, 1.0) : inner * 0.32, ink * 0.72);
    vec3 border_color = mix(vec3(0.07, 0.09, 0.10), vec3(1.0, 0.82, 0.16), u_selected);
    fragment_color = vec4(mix(inner, border_color, border), 0.94);
}
