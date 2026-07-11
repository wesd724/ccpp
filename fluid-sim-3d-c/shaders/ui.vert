#version 330 core

uniform vec4 u_rect;
out vec2 v_uv;

void main() {
    const vec2 corners[6] = vec2[6](
        vec2(0,0), vec2(1,0), vec2(1,1),
        vec2(0,0), vec2(1,1), vec2(0,1)
    );
    v_uv = corners[gl_VertexID];
    vec2 position = mix(u_rect.xy, u_rect.zw, v_uv);
    gl_Position = vec4(position, 0.0, 1.0);
}
