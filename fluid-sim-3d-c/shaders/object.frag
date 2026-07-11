#version 330 core

in vec3 v_world;
in vec3 v_normal;
uniform vec3 u_color;
uniform vec3 u_camera;
uniform vec3 u_light_direction;
uniform float u_alpha;
uniform float u_selected;
out vec4 fragment_color;

void main() {
    vec3 normal = normalize(gl_FrontFacing ? v_normal : -v_normal);
    vec3 light = normalize(-u_light_direction);
    vec3 view = normalize(u_camera - v_world);
    vec3 half_vector = normalize(light + view);
    float diffuse = max(dot(normal, light), 0.0);
    float glass = 1.0 - step(0.65, u_alpha);
    float specular = pow(max(dot(normal, half_vector), 0.0), mix(48.0, 180.0, glass));
    float fresnel = pow(1.0 - max(dot(normal, view), 0.0), 5.0);
    vec3 color = u_color * (0.28 + diffuse * 0.72);
    color += specular * mix(vec3(0.25), vec3(0.95, 1.0, 1.0), glass);
    color = mix(color, vec3(0.45, 0.88, 1.0), fresnel * glass * 0.58);
    color += u_selected * vec3(0.22, 0.16, 0.02);
    fragment_color = vec4(pow(color, vec3(1.0 / 2.2)), u_alpha);
}
