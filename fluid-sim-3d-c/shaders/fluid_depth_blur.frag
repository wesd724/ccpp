#version 430 core

in vec2 v_uv;
uniform sampler2D u_depth;
uniform int u_horizontal;
layout(location = 0) out float blurred_depth;

void main() {
    vec2 texel = 1.0 / vec2(textureSize(u_depth, 0));
    vec2 axis = u_horizontal != 0 ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);
    float center = texture(u_depth, v_uv).r;
    if (center <= 0.0001) {
        center = 1e20;
        for (int i = -4; i <= 4; ++i) {
            float candidate = texture(u_depth, v_uv + axis * float(i)).r;
            if (candidate > 0.0001) center = min(center, candidate);
        }
        if (center > 1e10) {
            blurred_depth = 0.0;
            return;
        }
    }

    float weighted_depth = 0.0;
    float total_weight = 0.0;
    for (int i = -8; i <= 8; ++i) {
        float sample_depth = texture(u_depth, v_uv + axis * float(i)).r;
        if (sample_depth <= 0.0001) continue;
        float spatial = exp(-float(i * i) / 28.0);
        float range_weight = exp(-abs(sample_depth - center) * 2.35);
        float weight = spatial * range_weight;
        weighted_depth += sample_depth * weight;
        total_weight += weight;
    }
    blurred_depth = total_weight > 0.0 ? weighted_depth / total_weight : 0.0;
}
