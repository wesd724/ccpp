#include "hud_renderer.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HUD_WIDTH 760
#define HUD_HEIGHT 96

typedef struct {
    float position[2];
    float uv[2];
} HudVertex;

static void mode_text(HudMode mode, const wchar_t **title,
                      const wchar_t **detail, const wchar_t **keys,
                      COLORREF *accent) {
    if (mode == HUD_MODE_REALTIME_FLIP) {
        *title = L"MODE  REALTIME FLIP";
        *detail = L"3D FLIP/PIC  |  126K PARTICLES  |  PARTICLE VIEW";
        *keys = L"[G] PREVIEW       [H] HIGH QUALITY       [M] MATERIAL LAB";
        *accent = RGB(38, 196, 214);
    } else if (mode == HUD_MODE_HIGH_QUALITY) {
        *title = L"MODE  HIGH QUALITY SURFACE";
        *detail = L"3D FLIP/PIC  |  834K PARTICLES  |  SURFACE OPTICS";
        *keys = L"[G] REALTIME FLIP  [H] PREVIEW            [M] MATERIAL LAB";
        *accent = RGB(245, 154, 62);
    } else if (mode == HUD_MODE_MATERIAL_LAB) {
        *title = L"MODE  MPM MATERIAL LAB";
        *detail = L"GPU MLS-MPM  |  ELASTIC + CLAY + GRANULAR  |  LMB STIR / F IMPACT";
        *keys = L"[G] REALTIME FLIP  [H] HIGH QUALITY       [M] PREVIEW";
        *accent = RGB(112, 218, 126);
    } else {
        *title = L"MODE  PREVIEW";
        *detail = L"2.5D HEIGHTFIELD  |  OBJECT PHYSICS  |  60 FPS TARGET";
        *keys = L"[G] REALTIME FLIP  [H] HIGH QUALITY       [M] MATERIAL LAB";
        *accent = RGB(238, 196, 62);
    }
}

static bool update_texture(HudRenderer *renderer, HudMode mode) {
    BITMAPINFO bitmap_info = {0};
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = HUD_WIDTH;
    bitmap_info.bmiHeader.biHeight = -HUD_HEIGHT;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    void *pixels = NULL;
    HBITMAP bitmap = CreateDIBSection(NULL, &bitmap_info, DIB_RGB_COLORS,
                                      &pixels, NULL, 0);
    HDC dc = CreateCompatibleDC(NULL);
    if (!bitmap || !dc || !pixels) {
        if (dc) DeleteDC(dc);
        if (bitmap) DeleteObject(bitmap);
        return false;
    }
    HGDIOBJ old_bitmap = SelectObject(dc, bitmap);
    HBRUSH background = CreateSolidBrush(RGB(22, 28, 34));
    RECT panel = {0, 0, HUD_WIDTH, HUD_HEIGHT};
    FillRect(dc, &panel, background);
    DeleteObject(background);
    SetBkMode(dc, TRANSPARENT);

    const wchar_t *title;
    const wchar_t *detail;
    const wchar_t *keys;
    COLORREF accent;
    mode_text(mode, &title, &detail, &keys, &accent);

    HFONT title_font = CreateFontW(-23, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                   DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT text_font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ old_font = SelectObject(dc, title_font);
    SetTextColor(dc, RGB(246, 249, 251));
    RECT title_rect = {22, 8, HUD_WIDTH - 16, 38};
    DrawTextW(dc, title, -1, &title_rect,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, text_font);
    SetTextColor(dc, RGB(178, 190, 199));
    RECT detail_rect = {22, 40, HUD_WIDTH - 16, 63};
    DrawTextW(dc, detail, -1, &detail_rect,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    SetTextColor(dc, accent);
    RECT keys_rect = {22, 67, HUD_WIDTH - 16, 91};
    DrawTextW(dc, keys, -1, &keys_rect,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(dc, old_font);
    DeleteObject(title_font);
    DeleteObject(text_font);
    SelectObject(dc, old_bitmap);
    DeleteDC(dc);

    uint8_t accent_red = GetRValue(accent);
    uint8_t accent_green = GetGValue(accent);
    uint8_t accent_blue = GetBValue(accent);
    uint8_t *bytes = pixels;
    for (int y = 0; y < HUD_HEIGHT; ++y) {
        for (int x = 0; x < HUD_WIDTH; ++x) {
            uint8_t *pixel = bytes + ((size_t)y * HUD_WIDTH + (size_t)x) * 4;
            uint8_t blue = pixel[0];
            pixel[0] = pixel[2];
            pixel[2] = blue;
            pixel[3] = 224;
            if (x < 6) {
                pixel[0] = accent_red;
                pixel[1] = accent_green;
                pixel[2] = accent_blue;
                pixel[3] = 255;
            }
        }
    }

    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, HUD_WIDTH, HUD_HEIGHT,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    DeleteObject(bitmap);
    renderer->rendered_mode = (int)mode;
    return true;
}

bool hud_renderer_init(HudRenderer *renderer) {
    memset(renderer, 0, sizeof(*renderer));
    renderer->texture_width = HUD_WIDTH;
    renderer->texture_height = HUD_HEIGHT;
    renderer->rendered_mode = -1;
    renderer->program = gl_program_from_files("shaders/hud.vert", "shaders/hud.frag");
    if (!renderer->program) return false;

    glGenVertexArrays(1, &renderer->vao);
    glBindVertexArray(renderer->vao);
    glGenBuffers(1, &renderer->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(HudVertex) * 6, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(HudVertex),
                          (const void *)offsetof(HudVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(HudVertex),
                          (const void *)offsetof(HudVertex, uv));

    glGenTextures(1, &renderer->texture);
    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, HUD_WIDTH, HUD_HEIGHT,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindVertexArray(0);
    renderer->initialized = true;
    return update_texture(renderer, HUD_MODE_PREVIEW);
}

void hud_renderer_draw(HudRenderer *renderer, HudMode mode, int width, int height) {
    if (!renderer->initialized || width <= 0 || height <= 0) return;
    if (renderer->rendered_mode != (int)mode && !update_texture(renderer, mode)) return;

    float available_width = fmaxf((float)width - 24.0f, 1.0f);
    float scale = fminf(1.0f, available_width / (float)HUD_WIDTH);
    float panel_width = (float)HUD_WIDTH * scale;
    float panel_height = (float)HUD_HEIGHT * scale;
    float left = (float)width - panel_width - 14.0f;
    float top = 14.0f;
    float right = left + panel_width;
    float bottom = top + panel_height;
    float x0 = left * 2.0f / (float)width - 1.0f;
    float x1 = right * 2.0f / (float)width - 1.0f;
    float y0 = 1.0f - top * 2.0f / (float)height;
    float y1 = 1.0f - bottom * 2.0f / (float)height;
    const HudVertex vertices[6] = {
        {{x0, y0}, {0.0f, 0.0f}}, {{x0, y1}, {0.0f, 1.0f}},
        {{x1, y1}, {1.0f, 1.0f}}, {{x0, y0}, {0.0f, 0.0f}},
        {{x1, y1}, {1.0f, 1.0f}}, {{x1, y0}, {1.0f, 0.0f}},
    };
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(renderer->program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    glUniform1i(glGetUniformLocation(renderer->program, "u_hud"), 0);
    glBindVertexArray(renderer->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void hud_renderer_destroy(HudRenderer *renderer) {
    if (renderer->program) glDeleteProgram(renderer->program);
    if (renderer->vbo) glDeleteBuffers(1, &renderer->vbo);
    if (renderer->vao) glDeleteVertexArrays(1, &renderer->vao);
    if (renderer->texture) glDeleteTextures(1, &renderer->texture);
    memset(renderer, 0, sizeof(*renderer));
}
