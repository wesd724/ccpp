#version 430 core

in float v_speed;
out vec4 fragment_color;

void main() {
    vec2 xy = gl_PointCoord * 2.0 - 1.0;
    float radius_squared = dot(xy, xy);
    if (radius_squared > 1.0) discard;
    vec3 normal = vec3(xy, sqrt(max(1.0 - radius_squared, 0.0)));
    vec3 light = normalize(vec3(-0.35, 0.72, 0.58));
    float diffuse = max(dot(normal, light), 0.0);
    float fresnel = pow(1.0 - normal.z, 5.0);
    float speed = smoothstep(0.0, 7.0, v_speed);
    vec3 deep = vec3(0.035, 0.27, 0.46);
    vec3 fast = vec3(0.04, 0.76, 0.88);
    vec3 color = mix(deep, fast, speed) * (0.56 + diffuse * 0.86);
    color = mix(color, vec3(0.82, 0.93, 1.0), fresnel * 0.72);
    fragment_color = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
