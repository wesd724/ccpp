#include "gl_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GlGenVertexArraysProc glGenVertexArrays;
GlBindVertexArrayProc glBindVertexArray;
GlDeleteVertexArraysProc glDeleteVertexArrays;
GlGenBuffersProc glGenBuffers;
GlBindBufferProc glBindBuffer;
GlBufferDataProc glBufferData;
GlBindBufferBaseProc glBindBufferBase;
GlDeleteBuffersProc glDeleteBuffers;
GlEnableVertexAttribArrayProc glEnableVertexAttribArray;
GlVertexAttribPointerProc glVertexAttribPointer;
GlCreateShaderProc glCreateShader;
GlShaderSourceProc glShaderSource;
GlCompileShaderProc glCompileShader;
GlGetShaderivProc glGetShaderiv;
GlGetShaderInfoLogProc glGetShaderInfoLog;
GlCreateProgramProc glCreateProgram;
GlAttachShaderProc glAttachShader;
GlLinkProgramProc glLinkProgram;
GlGetProgramivProc glGetProgramiv;
GlGetProgramInfoLogProc glGetProgramInfoLog;
GlDeleteShaderProc glDeleteShader;
GlDeleteProgramProc glDeleteProgram;
GlUseProgramProc glUseProgram;
GlGetUniformLocationProc glGetUniformLocation;
GlUniform1fProc glUniform1f;
GlUniform1iProc glUniform1i;
GlUniform3fProc glUniform3f;
GlUniform3iProc glUniform3i;
GlUniform4fProc glUniform4f;
GlUniformMatrix4fvProc glUniformMatrix4fv;
GlDispatchComputeProc glDispatchCompute;
GlMemoryBarrierProc glMemoryBarrier;
GlGenFramebuffersProc glGenFramebuffers;
GlBindFramebufferProc glBindFramebuffer;
GlFramebufferTexture2DProc glFramebufferTexture2D;
GlCheckFramebufferStatusProc glCheckFramebufferStatus;
GlDeleteFramebuffersProc glDeleteFramebuffers;
GlActiveTextureProc glActiveTexture;

static void *load_proc(const char *name) {
    void *proc = (void *)wglGetProcAddress(name);
    if (!proc || proc == (void *)1 || proc == (void *)2 || proc == (void *)3 || proc == (void *)-1) {
        static HMODULE library;
        if (!library) {
            library = LoadLibraryA("opengl32.dll");
        }
        proc = (void *)GetProcAddress(library, name);
    }
    return proc;
}

bool gl_api_load(void) {
#define LOAD(name) do { *(void **)(&(name)) = load_proc(#name); if (!(name)) { fprintf(stderr, "Missing OpenGL function: %s\n", #name); return false; } } while (0)
    LOAD(glGenVertexArrays);
    LOAD(glBindVertexArray);
    LOAD(glDeleteVertexArrays);
    LOAD(glGenBuffers);
    LOAD(glBindBuffer);
    LOAD(glBufferData);
    LOAD(glBindBufferBase);
    LOAD(glDeleteBuffers);
    LOAD(glEnableVertexAttribArray);
    LOAD(glVertexAttribPointer);
    LOAD(glCreateShader);
    LOAD(glShaderSource);
    LOAD(glCompileShader);
    LOAD(glGetShaderiv);
    LOAD(glGetShaderInfoLog);
    LOAD(glCreateProgram);
    LOAD(glAttachShader);
    LOAD(glLinkProgram);
    LOAD(glGetProgramiv);
    LOAD(glGetProgramInfoLog);
    LOAD(glDeleteShader);
    LOAD(glDeleteProgram);
    LOAD(glUseProgram);
    LOAD(glGetUniformLocation);
    LOAD(glUniform1f);
    LOAD(glUniform1i);
    LOAD(glUniform3f);
    LOAD(glUniform3i);
    LOAD(glUniform4f);
    LOAD(glUniformMatrix4fv);
    LOAD(glDispatchCompute);
    LOAD(glMemoryBarrier);
    LOAD(glGenFramebuffers);
    LOAD(glBindFramebuffer);
    LOAD(glFramebufferTexture2D);
    LOAD(glCheckFramebufferStatus);
    LOAD(glDeleteFramebuffers);
    LOAD(glActiveTexture);
#undef LOAD
    return true;
}

static char *read_file(const char *relative_path) {
    FILE *file = fopen(relative_path, "rb");
    char absolute_path[MAX_PATH * 2];
    if (!file) {
        DWORD length = GetModuleFileNameA(NULL, absolute_path, MAX_PATH);
        while (length > 0 && absolute_path[length - 1] != '\\' && absolute_path[length - 1] != '/') {
            --length;
        }
        snprintf(absolute_path + length, sizeof(absolute_path) - length, "%s", relative_path);
        file = fopen(absolute_path, "rb");
    }
    if (!file) {
        fprintf(stderr, "Cannot open shader: %s\n", relative_path);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *text = malloc((size_t)size + 1);
    if (!text || fread(text, 1, (size_t)size, file) != (size_t)size) {
        free(text);
        fclose(file);
        return NULL;
    }
    text[size] = '\0';
    fclose(file);
    return text;
}

static GLuint compile_shader(GLenum type, const char *path) {
    char *source = read_file(path);
    if (!source) {
        return 0;
    }
    GLuint shader = glCreateShader(type);
    const GLchar *sources[] = {source};
    glShaderSource(shader, 1, sources, NULL);
    glCompileShader(shader);
    free(source);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        char *log = calloc((size_t)length + 1, 1);
        glGetShaderInfoLog(shader, length, NULL, log);
        fprintf(stderr, "%s compile failed:\n%s\n", path, log);
        free(log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint gl_program_from_files(const char *vertex_path, const char *fragment_path) {
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_path);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_path);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        char *log = calloc((size_t)length + 1, 1);
        glGetProgramInfoLog(program, length, NULL, log);
        fprintf(stderr, "Shader link failed:\n%s\n", log);
        free(log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

GLuint gl_compute_program_from_file(const char *compute_path) {
    GLuint shader = compile_shader(GL_COMPUTE_SHADER, compute_path);
    if (!shader) return 0;
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        char *log = calloc((size_t)length + 1, 1);
        glGetProgramInfoLog(program, length, NULL, log);
        fprintf(stderr, "%s link failed:\n%s\n", compute_path, log);
        free(log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}
