#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

uniform mat4 u_view_projection;
uniform vec3 u_center;
uniform vec3 u_half_extent;
uniform vec3 u_rotation;

out vec3 v_world;
out vec3 v_normal;

mat3 rotation_xyz(vec3 r) {
    float cx = cos(r.x), sx = sin(r.x);
    float cy = cos(r.y), sy = sin(r.y);
    float cz = cos(r.z), sz = sin(r.z);
    mat3 rx = mat3(1,0,0, 0,cx,sx, 0,-sx,cx);
    mat3 ry = mat3(cy,0,-sy, 0,1,0, sy,0,cy);
    mat3 rz = mat3(cz,sz,0, -sz,cz,0, 0,0,1);
    return rz * ry * rx;
}

void main() {
    mat3 rotation = rotation_xyz(u_rotation);
    v_world = u_center + rotation * (a_position * u_half_extent);
    v_normal = normalize(rotation * a_normal);
    gl_Position = u_view_projection * vec4(v_world, 1.0);
}
