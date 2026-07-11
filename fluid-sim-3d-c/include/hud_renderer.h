#ifndef HUD_RENDERER_H
#define HUD_RENDERER_H

#include "gl_api.h"

#include <stdbool.h>

typedef enum {
    HUD_MODE_PREVIEW,
    HUD_MODE_REALTIME_FLIP,
    HUD_MODE_HIGH_QUALITY,
    HUD_MODE_MATERIAL_LAB,
} HudMode;

typedef struct {
    GLuint program;
    GLuint vao;
    GLuint vbo;
    GLuint texture;
    int texture_width;
    int texture_height;
    int rendered_mode;
    bool initialized;
} HudRenderer;

bool hud_renderer_init(HudRenderer *renderer);
void hud_renderer_draw(HudRenderer *renderer, HudMode mode, int width, int height);
void hud_renderer_destroy(HudRenderer *renderer);

#endif
