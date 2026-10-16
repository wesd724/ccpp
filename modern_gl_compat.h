#ifndef MODERN_GL_COMPAT_H
#define MODERN_GL_COMPAT_H

#include <GL/gl.h>
#include <GL/glext.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_ARRAY_BUFFER 0x8892
#define GL_DYNAMIC_DRAW 0x88E8
#endif

typedef struct {
    float x, y, z, w;
    float r, g, b, a;
} MglVertex;

static PFNGLCREATESHADERPROC mglCreateShader;
static PFNGLSHADERSOURCEPROC mglShaderSource;
static PFNGLCOMPILESHADERPROC mglCompileShader;
static PFNGLGETSHADERIVPROC mglGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC mglGetShaderInfoLog;
static PFNGLCREATEPROGRAMPROC mglCreateProgram;
static PFNGLATTACHSHADERPROC mglAttachShader;
static PFNGLLINKPROGRAMPROC mglLinkProgram;
static PFNGLGETPROGRAMIVPROC mglGetProgramiv;
static PFNGLGETPROGRAMINFOLOGPROC mglGetProgramInfoLog;
static PFNGLDELETESHADERPROC mglDeleteShader;
static PFNGLDELETEPROGRAMPROC mglDeleteProgram;
static PFNGLDELETEBUFFERSPROC mglDeleteBuffers;
static PFNGLDELETEVERTEXARRAYSPROC mglDeleteVertexArrays;
static PFNGLUSEPROGRAMPROC mglUseProgram;
static PFNGLGENBUFFERSPROC mglGenBuffers;
static PFNGLBINDBUFFERPROC mglBindBuffer;
static PFNGLBUFFERDATAPROC mglBufferData;
static PFNGLGENVERTEXARRAYSPROC mglGenVertexArrays;
static PFNGLBINDVERTEXARRAYPROC mglBindVertexArray;
static PFNGLENABLEVERTEXATTRIBARRAYPROC mglEnableVertexAttribArray;
static PFNGLVERTEXATTRIBPOINTERPROC mglVertexAttribPointer;

static GLuint mgl_program;
static GLuint mgl_vao;
static GLuint mgl_vbo;
static GLenum mgl_mode;
static MglVertex mgl_input[65536];
static MglVertex mgl_output[196608];
static int mgl_input_count;
static float mgl_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
static float mgl_projection[16];
static float mgl_modelview[16];
static float mgl_projection_stack[32][16];
static float mgl_modelview_stack[32][16];
static int mgl_projection_top;
static int mgl_modelview_top;
static GLenum mgl_matrix_mode = GL_MODELVIEW;
static bool mgl_ready;

static void mgl_identity(float *m)
{
    memset(m, 0, sizeof(float) * 16);
    m[0] = 1.0f;
    m[5] = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

static void mgl_mul(float *out, const float *a, const float *b)
{
    float r[16];
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            r[row * 4 + col] =
                a[row * 4 + 0] * b[0 * 4 + col]
                + a[row * 4 + 1] * b[1 * 4 + col]
                + a[row * 4 + 2] * b[2 * 4 + col]
                + a[row * 4 + 3] * b[3 * 4 + col];
        }
    }
    memcpy(out, r, sizeof(r));
}

static float *mgl_current_matrix(void)
{
    return mgl_matrix_mode == GL_PROJECTION ? mgl_projection : mgl_modelview;
}

static void mgl_transform(float x, float y, float z, float *out)
{
    float mv[4] = {
        mgl_modelview[0] * x + mgl_modelview[1] * y + mgl_modelview[2] * z + mgl_modelview[3],
        mgl_modelview[4] * x + mgl_modelview[5] * y + mgl_modelview[6] * z + mgl_modelview[7],
        mgl_modelview[8] * x + mgl_modelview[9] * y + mgl_modelview[10] * z + mgl_modelview[11],
        mgl_modelview[12] * x + mgl_modelview[13] * y + mgl_modelview[14] * z + mgl_modelview[15],
    };
    out[0] = mgl_projection[0] * mv[0] + mgl_projection[1] * mv[1] + mgl_projection[2] * mv[2] + mgl_projection[3] * mv[3];
    out[1] = mgl_projection[4] * mv[0] + mgl_projection[5] * mv[1] + mgl_projection[6] * mv[2] + mgl_projection[7] * mv[3];
    out[2] = mgl_projection[8] * mv[0] + mgl_projection[9] * mv[1] + mgl_projection[10] * mv[2] + mgl_projection[11] * mv[3];
    out[3] = mgl_projection[12] * mv[0] + mgl_projection[13] * mv[1] + mgl_projection[14] * mv[2] + mgl_projection[15] * mv[3];
}

static PROC mgl_get_proc(const char *name)
{
    PROC proc = wglGetProcAddress(name);
    if (!proc || proc == (PROC)1 || proc == (PROC)2 || proc == (PROC)3 || proc == (PROC)-1) {
        HMODULE module = GetModuleHandleA("opengl32.dll");
        proc = module ? GetProcAddress(module, name) : NULL;
    }
    return proc;
}

static GLuint mgl_compile(GLenum type, const char *source)
{
    GLuint shader = mglCreateShader(type);
    mglShaderSource(shader, 1, &source, NULL);
    mglCompileShader(shader);
    GLint ok = 0;
    mglGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        mglGetShaderInfoLog(shader, sizeof(log), NULL, log);
        MessageBoxA(NULL, log, "Shader compile error", MB_ICONERROR);
        mglDeleteShader(shader);
        return 0;
    }
    return shader;
}

static void mgl_shutdown(void)
{
    if (mgl_program) mglUseProgram(0);
    if (mgl_vbo) mglDeleteBuffers(1, &mgl_vbo);
    if (mgl_vao) mglDeleteVertexArrays(1, &mgl_vao);
    if (mgl_program) mglDeleteProgram(mgl_program);
    mgl_program = mgl_vao = mgl_vbo = 0;
    mgl_projection_top = mgl_modelview_top = 0;
    mgl_ready = false;
}

static bool mgl_init(void)
{
    mglCreateShader = (PFNGLCREATESHADERPROC)mgl_get_proc("glCreateShader");
    mglShaderSource = (PFNGLSHADERSOURCEPROC)mgl_get_proc("glShaderSource");
    mglCompileShader = (PFNGLCOMPILESHADERPROC)mgl_get_proc("glCompileShader");
    mglGetShaderiv = (PFNGLGETSHADERIVPROC)mgl_get_proc("glGetShaderiv");
    mglGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)mgl_get_proc("glGetShaderInfoLog");
    mglCreateProgram = (PFNGLCREATEPROGRAMPROC)mgl_get_proc("glCreateProgram");
    mglAttachShader = (PFNGLATTACHSHADERPROC)mgl_get_proc("glAttachShader");
    mglLinkProgram = (PFNGLLINKPROGRAMPROC)mgl_get_proc("glLinkProgram");
    mglGetProgramiv = (PFNGLGETPROGRAMIVPROC)mgl_get_proc("glGetProgramiv");
    mglGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)mgl_get_proc("glGetProgramInfoLog");
    mglDeleteShader = (PFNGLDELETESHADERPROC)mgl_get_proc("glDeleteShader");
    mglDeleteProgram = (PFNGLDELETEPROGRAMPROC)mgl_get_proc("glDeleteProgram");
    mglDeleteBuffers = (PFNGLDELETEBUFFERSPROC)mgl_get_proc("glDeleteBuffers");
    mglDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)mgl_get_proc("glDeleteVertexArrays");
    mglUseProgram = (PFNGLUSEPROGRAMPROC)mgl_get_proc("glUseProgram");
    mglGenBuffers = (PFNGLGENBUFFERSPROC)mgl_get_proc("glGenBuffers");
    mglBindBuffer = (PFNGLBINDBUFFERPROC)mgl_get_proc("glBindBuffer");
    mglBufferData = (PFNGLBUFFERDATAPROC)mgl_get_proc("glBufferData");
    mglGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)mgl_get_proc("glGenVertexArrays");
    mglBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)mgl_get_proc("glBindVertexArray");
    mglEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)mgl_get_proc("glEnableVertexAttribArray");
    mglVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)mgl_get_proc("glVertexAttribPointer");

    if (!mglCreateShader || !mglShaderSource || !mglCompileShader || !mglGetShaderiv ||
        !mglGetShaderInfoLog || !mglCreateProgram || !mglAttachShader || !mglLinkProgram ||
        !mglGetProgramiv || !mglGetProgramInfoLog || !mglDeleteShader || !mglDeleteProgram ||
        !mglDeleteBuffers || !mglDeleteVertexArrays || !mglUseProgram || !mglGenBuffers ||
        !mglBindBuffer || !mglBufferData || !mglGenVertexArrays || !mglBindVertexArray ||
        !mglEnableVertexAttribArray || !mglVertexAttribPointer) {
        MessageBoxA(NULL, "Modern OpenGL functions could not be loaded.", "OpenGL", MB_ICONERROR);
        return false;
    }

    const char *vs =
        "#version 330 core\n"
        "layout(location=0) in vec4 a_pos;\n"
        "layout(location=1) in vec4 a_color;\n"
        "out vec4 v_color;\n"
        "void main(){ gl_Position = a_pos; gl_PointSize = 5.5; v_color = a_color; }\n";
    const char *fs =
        "#version 330 core\n"
        "in vec4 v_color;\n"
        "out vec4 frag_color;\n"
        "void main(){ frag_color = v_color; }\n";

    GLuint vert = mgl_compile(GL_VERTEX_SHADER, vs);
    GLuint frag = mgl_compile(GL_FRAGMENT_SHADER, fs);
    if (!vert || !frag) {
        if (vert) mglDeleteShader(vert);
        if (frag) mglDeleteShader(frag);
        return false;
    }
    mgl_program = mglCreateProgram();
    mglAttachShader(mgl_program, vert);
    mglAttachShader(mgl_program, frag);
    mglLinkProgram(mgl_program);
    GLint ok = 0;
    mglGetProgramiv(mgl_program, GL_LINK_STATUS, &ok);
    mglDeleteShader(vert);
    mglDeleteShader(frag);
    if (!ok) {
        char log[2048];
        mglGetProgramInfoLog(mgl_program, sizeof(log), NULL, log);
        MessageBoxA(NULL, log, "Program link error", MB_ICONERROR);
        mgl_shutdown();
        return false;
    }

    mglGenVertexArrays(1, &mgl_vao);
    mglGenBuffers(1, &mgl_vbo);
    if (!mgl_vao || !mgl_vbo) {
        mgl_shutdown();
        return false;
    }
    mglBindVertexArray(mgl_vao);
    mglBindBuffer(GL_ARRAY_BUFFER, mgl_vbo);
    mglBufferData(GL_ARRAY_BUFFER, sizeof(mgl_output), NULL, GL_DYNAMIC_DRAW);
    mglEnableVertexAttribArray(0);
    mglVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(MglVertex), (void *)offsetof(MglVertex, x));
    mglEnableVertexAttribArray(1);
    mglVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(MglVertex), (void *)offsetof(MglVertex, r));

    mgl_identity(mgl_projection);
    mgl_identity(mgl_modelview);
    mgl_ready = true;
    return true;
}

static void mglMatrixMode(GLenum mode) { mgl_matrix_mode = mode; }
static void mglLoadIdentity(void) { mgl_identity(mgl_current_matrix()); }

static void mglPushMatrix(void)
{
    if (mgl_matrix_mode == GL_PROJECTION && mgl_projection_top < 32) {
        memcpy(mgl_projection_stack[mgl_projection_top++], mgl_projection, sizeof(mgl_projection));
    } else if (mgl_matrix_mode == GL_MODELVIEW && mgl_modelview_top < 32) {
        memcpy(mgl_modelview_stack[mgl_modelview_top++], mgl_modelview, sizeof(mgl_modelview));
    }
}

static void mglPopMatrix(void)
{
    if (mgl_matrix_mode == GL_PROJECTION && mgl_projection_top > 0) {
        memcpy(mgl_projection, mgl_projection_stack[--mgl_projection_top], sizeof(mgl_projection));
    } else if (mgl_matrix_mode == GL_MODELVIEW && mgl_modelview_top > 0) {
        memcpy(mgl_modelview, mgl_modelview_stack[--mgl_modelview_top], sizeof(mgl_modelview));
    }
}

static void mglMultMatrix(const float *m)
{
    float *cur = mgl_current_matrix();
    mgl_mul(cur, cur, m);
}

static void mglTranslatef(float x, float y, float z)
{
    float m[16];
    mgl_identity(m);
    m[3] = x;
    m[7] = y;
    m[11] = z;
    mglMultMatrix(m);
}

static void mglRotatef(float angle, float x, float y, float z)
{
    float len = sqrtf(x * x + y * y + z * z);
    if (len < 0.00001f) return;
    x /= len; y /= len; z /= len;
    float a = angle * 3.14159265358979323846f / 180.0f;
    float c = cosf(a), s = sinf(a), t = 1.0f - c;
    float m[16] = {
        t*x*x + c,     t*x*y - s*z,   t*x*z + s*y,   0.0f,
        t*x*y + s*z,   t*y*y + c,     t*y*z - s*x,   0.0f,
        t*x*z - s*y,   t*y*z + s*x,   t*z*z + c,     0.0f,
        0.0f,          0.0f,          0.0f,          1.0f,
    };
    mglMultMatrix(m);
}

static void mglOrtho(double left, double right, double bottom, double top, double nearv, double farv)
{
    float m[16];
    mgl_identity(m);
    m[0] = (float)(2.0 / (right - left));
    m[5] = (float)(2.0 / (top - bottom));
    m[10] = (float)(-2.0 / (farv - nearv));
    m[3] = (float)(-(right + left) / (right - left));
    m[7] = (float)(-(top + bottom) / (top - bottom));
    m[11] = (float)(-(farv + nearv) / (farv - nearv));
    mglMultMatrix(m);
}

static void mglFrustum(double left, double right, double bottom, double top, double nearv, double farv)
{
    float m[16] = {0};
    m[0] = (float)(2.0 * nearv / (right - left));
    m[2] = (float)((right + left) / (right - left));
    m[5] = (float)(2.0 * nearv / (top - bottom));
    m[6] = (float)((top + bottom) / (top - bottom));
    m[10] = (float)(-(farv + nearv) / (farv - nearv));
    m[11] = (float)(-(2.0 * farv * nearv) / (farv - nearv));
    m[14] = -1.0f;
    mglMultMatrix(m);
}

static void mglColor3f(float r, float g, float b) { mgl_color[0]=r; mgl_color[1]=g; mgl_color[2]=b; mgl_color[3]=1.0f; }
static void mglColor4f(float r, float g, float b, float a) { mgl_color[0]=r; mgl_color[1]=g; mgl_color[2]=b; mgl_color[3]=a; }
static void mglNormal3f(float x, float y, float z) { (void)x; (void)y; (void)z; }
static void mglLightfv(GLenum light, GLenum pname, const GLfloat *params) { (void)light; (void)pname; (void)params; }
static void mglLightModelfv(GLenum pname, const GLfloat *params) { (void)pname; (void)params; }
static void mglMaterialfv(GLenum face, GLenum pname, const GLfloat *params) { (void)face; (void)pname; (void)params; }
static void mglMaterialf(GLenum face, GLenum pname, GLfloat param) { (void)face; (void)pname; (void)param; }
static void mglColorMaterial(GLenum face, GLenum mode) { (void)face; (void)mode; }
static void mglShadeModel(GLenum mode) { (void)mode; }
static void mglFogi(GLenum pname, GLint param) { (void)pname; (void)param; }
static void mglFogf(GLenum pname, GLfloat param) { (void)pname; (void)param; }
static void mglFogfv(GLenum pname, const GLfloat *params) { (void)pname; (void)params; }

static void mglBegin(GLenum mode)
{
    mgl_mode = mode;
    mgl_input_count = 0;
}

static void mglVertex4(float x, float y, float z)
{
    if (mgl_input_count >= (int)(sizeof(mgl_input) / sizeof(mgl_input[0]))) return;
    MglVertex *v = &mgl_input[mgl_input_count++];
    mgl_transform(x, y, z, &v->x);
    v->r = mgl_color[0]; v->g = mgl_color[1]; v->b = mgl_color[2]; v->a = mgl_color[3];
}

static void mglVertex3f(float x, float y, float z) { mglVertex4(x, y, z); }
static void mglVertex2f(float x, float y) { mglVertex4(x, y, 0.0f); }

static void mgl_emit(MglVertex *out, int *count, MglVertex a, MglVertex b, MglVertex c)
{
    out[(*count)++] = a;
    out[(*count)++] = b;
    out[(*count)++] = c;
}

static GLenum mgl_build_output(int *count)
{
    *count = 0;
    if (mgl_mode == GL_TRIANGLES) {
        memcpy(mgl_output, mgl_input, sizeof(MglVertex) * mgl_input_count);
        *count = mgl_input_count;
        return GL_TRIANGLES;
    }
    if (mgl_mode == GL_LINES) {
        memcpy(mgl_output, mgl_input, sizeof(MglVertex) * mgl_input_count);
        *count = mgl_input_count;
        return GL_LINES;
    }
    if (mgl_mode == GL_QUADS) {
        for (int i = 0; i + 3 < mgl_input_count; i += 4) {
            mgl_emit(mgl_output, count, mgl_input[i], mgl_input[i + 1], mgl_input[i + 2]);
            mgl_emit(mgl_output, count, mgl_input[i], mgl_input[i + 2], mgl_input[i + 3]);
        }
        return GL_TRIANGLES;
    }
    if (mgl_mode == GL_TRIANGLE_FAN) {
        for (int i = 1; i + 1 < mgl_input_count; ++i) {
            mgl_emit(mgl_output, count, mgl_input[0], mgl_input[i], mgl_input[i + 1]);
        }
        return GL_TRIANGLES;
    }
    if (mgl_mode == GL_TRIANGLE_STRIP) {
        for (int i = 0; i + 2 < mgl_input_count; ++i) {
            if ((i & 1) == 0) mgl_emit(mgl_output, count, mgl_input[i], mgl_input[i + 1], mgl_input[i + 2]);
            else mgl_emit(mgl_output, count, mgl_input[i + 1], mgl_input[i], mgl_input[i + 2]);
        }
        return GL_TRIANGLES;
    }
    if (mgl_mode == GL_QUAD_STRIP) {
        for (int i = 0; i + 3 < mgl_input_count; i += 2) {
            mgl_emit(mgl_output, count, mgl_input[i], mgl_input[i + 1], mgl_input[i + 3]);
            mgl_emit(mgl_output, count, mgl_input[i], mgl_input[i + 3], mgl_input[i + 2]);
        }
        return GL_TRIANGLES;
    }
    if (mgl_mode == GL_LINE_LOOP) {
        for (int i = 0; i < mgl_input_count; ++i) {
            mgl_output[(*count)++] = mgl_input[i];
            mgl_output[(*count)++] = mgl_input[(i + 1) % mgl_input_count];
        }
        return GL_LINES;
    }
    return GL_TRIANGLES;
}

static void mglEnd(void)
{
    if (!mgl_ready || mgl_input_count <= 0) return;
    int count = 0;
    GLenum draw_mode = mgl_build_output(&count);
    if (count <= 0) return;
    mglUseProgram(mgl_program);
    mglBindVertexArray(mgl_vao);
    mglBindBuffer(GL_ARRAY_BUFFER, mgl_vbo);
    mglBufferData(GL_ARRAY_BUFFER, sizeof(MglVertex) * (size_t)count, mgl_output, GL_DYNAMIC_DRAW);
    glDrawArrays(draw_mode, 0, count);
}

#define glBegin mglBegin
#define glEnd mglEnd
#define glVertex3f mglVertex3f
#define glVertex2f mglVertex2f
#define glColor3f mglColor3f
#define glColor4f mglColor4f
#define glNormal3f mglNormal3f
#define glMatrixMode mglMatrixMode
#define glLoadIdentity mglLoadIdentity
#define glPushMatrix mglPushMatrix
#define glPopMatrix mglPopMatrix
#define glTranslatef mglTranslatef
#define glRotatef mglRotatef
#define glOrtho mglOrtho
#define glFrustum mglFrustum
#define glLightfv mglLightfv
#define glLightModelfv mglLightModelfv
#define glMaterialfv mglMaterialfv
#define glMaterialf mglMaterialf
#define glColorMaterial mglColorMaterial
#define glShadeModel mglShadeModel
#define glFogi mglFogi
#define glFogf mglFogf
#define glFogfv mglFogfv

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
