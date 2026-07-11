#version 330 core

in vec2 v_uv;
uniform sampler2D u_hud;
out vec4 fragment_color;

void main() {
    fragment_color = texture(u_hud, v_uv);
}
