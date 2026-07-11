#version 430 core

in vec3 v_center_view;
in float v_material;
in float v_speed;
in float v_strain;
in float v_damage;
in float v_variation;

uniform mat4 u_projection;
uniform vec3 u_forward;
uniform vec3 u_right;
uniform vec3 u_up;
uniform vec3 u_light_direction;
uniform float u_particle_radius;
out vec4 fragment_color;

vec3 aces_tonemap(vec3 color) {
    return clamp((color * (2.51 * color + 0.03)) /
                 (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec2 point = gl_PointCoord * 2.0 - 1.0;
    float radius_squared = dot(point, point);
    if (radius_squared > 1.0) discard;
    float sphere_z = sqrt(max(1.0 - radius_squared, 0.0));
    vec3 view_position = v_center_view +
        vec3(point * u_particle_radius, sphere_z * u_particle_radius);
    vec4 clip = u_projection * vec4(view_position, 1.0);
    gl_FragDepth = clip.z / clip.w * 0.5 + 0.5;

    vec3 normal = normalize(u_right * point.x + u_up * point.y - u_forward * sphere_z);
    vec3 light = normalize(-u_light_direction);
    vec3 half_vector = normalize(light + normalize(-u_forward));
    float diffuse = max(dot(normal, light), 0.0);
    float facing = max(dot(normal, normalize(-u_forward)), 0.0);
    int material = int(v_material + 0.5);
    vec3 base;
    float roughness;
    if (material == 0) {
        base = vec3(0.92, 0.075, 0.045);
        roughness = 0.24;
    } else if (material == 1) {
        base = vec3(0.94, 0.46, 0.055);
        roughness = 0.48;
    } else {
        base = vec3(0.075, 0.48, 0.82);
        roughness = 0.68;
    }
    base *= mix(0.82, 1.14, v_variation);
    float exponent = mix(190.0, 22.0, roughness);
    float specular = pow(max(dot(normal, half_vector), 0.0), exponent) *
                     (1.0 - roughness * 0.62);
    float fresnel = pow(1.0 - facing, 5.0);
    float strain_glow = smoothstep(0.10, 0.82, v_strain);
    vec3 color = base * (0.28 + diffuse * 0.82);
    color += specular * vec3(1.0, 0.88, 0.68) * 0.72;
    color = mix(color, vec3(1.0, 0.24, 0.055), strain_glow * 0.42);
    color = mix(color, vec3(0.16, 0.12, 0.10), v_damage * 0.22);
    color += fresnel * mix(vec3(0.08), vec3(0.36, 0.48, 0.62), float(material == 0));
    if (material == 0) {
        float backlight = pow(max(dot(-normal, light), 0.0), 2.0);
        color += base * backlight * 0.20;
    }
    fragment_color = vec4(pow(aces_tonemap(color), vec3(1.0 / 2.2)), 1.0);
}
