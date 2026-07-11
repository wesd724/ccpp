#version 430 core

in vec2 v_uv;
uniform sampler2D u_depth;
uniform sampler2D u_thickness;
uniform sampler2D u_scene;
uniform mat4 u_view_projection;
uniform vec3 u_camera;
uniform vec3 u_forward;
uniform vec3 u_right;
uniform vec3 u_up;
uniform vec3 u_light_direction;
uniform float u_aspect;
uniform float u_tan_half_fov;
uniform float u_time;
out vec4 fragment_color;

vec3 screen_ray(vec2 uv) {
    vec2 screen = uv * 2.0 - 1.0;
    return normalize(u_forward + u_right * screen.x * u_aspect * u_tan_half_fov +
                     u_up * screen.y * u_tan_half_fov);
}

vec3 reconstruct_position(vec2 uv, float linear_depth) {
    vec3 ray = screen_ray(uv);
    return u_camera + ray * (linear_depth / max(dot(ray, u_forward), 0.08));
}

vec3 sky_radiance(vec3 direction) {
    float elevation = smoothstep(-0.15, 0.88, direction.y);
    vec3 sky = mix(vec3(0.79, 0.88, 0.95), vec3(0.10, 0.38, 0.76), elevation);
    float horizon = exp(-abs(direction.y) * 7.0);
    vec3 to_sun = normalize(-u_light_direction);
    float sun = pow(max(dot(direction, to_sun), 0.0), 1100.0);
    float glow = pow(max(dot(direction, to_sun), 0.0), 28.0);
    return sky + horizon * vec3(0.15, 0.17, 0.16) +
           sun * vec3(10.0, 7.8, 4.6) + glow * vec3(0.38, 0.24, 0.10);
}

float pillar_shadow(vec3 position, vec3 to_light) {
    float shadow = 0.0;
    float horizontal_length_squared = dot(to_light.xz, to_light.xz);
    if (horizontal_length_squared < 0.0001) return shadow;
    for (int i = 0; i < 5; ++i) {
        vec2 center = vec2(-2.2 + float(i) * 1.1, sin(float(i) * 1.35) * 0.48);
        float top = -0.1 + float(i) * 0.52;
        vec2 toward = center - position.xz;
        float distance_along = dot(toward, to_light.xz) / horizontal_length_squared;
        if (distance_along <= 0.0) continue;
        vec2 closest = position.xz + to_light.xz * distance_along;
        float radial = length(closest - center);
        float ray_height = position.y + to_light.y * distance_along;
        float cylinder = 1.0 - smoothstep(0.35, 0.55, radial);
        float within_height = step(-2.32, ray_height) * step(ray_height, top);
        shadow = max(shadow, cylinder * within_height);
    }
    return shadow;
}

vec3 aces_tonemap(vec3 color) {
    return clamp((color * (2.51 * color + 0.03)) /
                 (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    float depth = texture(u_depth, v_uv).r;
    if (depth <= 0.0001) discard;
    vec2 texel = 1.0 / vec2(textureSize(u_depth, 0));
    float depth_left = texture(u_depth, v_uv - vec2(texel.x, 0.0)).r;
    float depth_right = texture(u_depth, v_uv + vec2(texel.x, 0.0)).r;
    float depth_down = texture(u_depth, v_uv - vec2(0.0, texel.y)).r;
    float depth_up = texture(u_depth, v_uv + vec2(0.0, texel.y)).r;
    depth_left = depth_left > 0.0 ? depth_left : depth;
    depth_right = depth_right > 0.0 ? depth_right : depth;
    depth_down = depth_down > 0.0 ? depth_down : depth;
    depth_up = depth_up > 0.0 ? depth_up : depth;

    vec3 position = reconstruct_position(v_uv, depth);
    vec3 tangent_x = reconstruct_position(v_uv + vec2(texel.x, 0.0), depth_right) -
                     reconstruct_position(v_uv - vec2(texel.x, 0.0), depth_left);
    vec3 tangent_y = reconstruct_position(v_uv + vec2(0.0, texel.y), depth_up) -
                     reconstruct_position(v_uv - vec2(0.0, texel.y), depth_down);
    vec3 normal = normalize(cross(tangent_x, tangent_y));
    vec3 view = normalize(u_camera - position);
    if (dot(normal, view) < 0.0) normal = -normal;

    vec2 thickness_data = texture(u_thickness, v_uv).rg;
    float thickness = clamp(thickness_data.x, 0.0, 8.0);
    float speed = thickness > 0.001 ? thickness_data.y / thickness : 0.0;
    float curvature = abs(depth_left + depth_right + depth_down + depth_up - 4.0 * depth);
    float foam = clamp(smoothstep(4.2, 10.5, speed) * 0.68 +
                       smoothstep(0.10, 0.62, curvature) * 0.32, 0.0, 1.0);

    vec3 to_light = normalize(-u_light_direction);
    vec3 half_vector = normalize(view + to_light);
    float facing = max(dot(normal, view), 0.0);
    float fresnel = 0.0204 + 0.9796 * pow(1.0 - facing, 5.0);
    float shadow = pillar_shadow(position, to_light);
    float direct = max(dot(normal, to_light), 0.0) * (1.0 - shadow * 0.78);
    float roughness = mix(0.045, 0.22, foam);
    float specular = pow(max(dot(normal, half_vector), 0.0),
                         mix(320.0, 42.0, roughness)) * direct /
                     max(roughness, 0.045);

    float optical_thickness = max(thickness * 0.19, 0.025);
    vec3 absorption = vec3(0.78, 0.20, 0.075);
    vec3 transmittance = exp(-absorption * optical_thickness);
    vec2 refract_direction = vec2(dot(normal, u_right), dot(normal, u_up));
    vec2 refracted_uv = clamp(v_uv + refract_direction *
                              (0.006 + 0.012 * clamp(optical_thickness, 0.0, 1.0)),
                              vec2(0.002), vec2(0.998));
    vec3 refracted_scene = pow(texture(u_scene, refracted_uv).rgb, vec3(2.2));
    vec3 deep_scatter = vec3(0.004, 0.075, 0.105) *
                        (0.38 + 0.62 * direct);
    vec3 refracted = refracted_scene * transmittance +
                     deep_scatter * (vec3(1.0) - transmittance);
    vec3 reflected = sky_radiance(reflect(-view, normal));
    vec3 color = mix(refracted, reflected, fresnel);
    color += specular * vec3(1.0, 0.84, 0.58) * 0.18;
    color = mix(color, vec3(0.86, 0.94, 0.97), foam * 0.66);
    color += vec3(0.012, 0.055, 0.075) * direct;

    vec4 clip = u_view_projection * vec4(position, 1.0);
    gl_FragDepth = clip.z / clip.w * 0.5 + 0.5;
    fragment_color = vec4(pow(aces_tonemap(color), vec3(1.0 / 2.2)), 1.0);
}
