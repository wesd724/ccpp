#ifndef GL_API_H
#define GL_API_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <stdbool.h>
#include <stddef.h>

#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPUTE_SHADER 0x91B9
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_PROGRAM_POINT_SIZE 0x8642
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#define GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT 0x00000001
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_R32F 0x822E
#define GL_RG16F 0x822F
#define GL_RGBA8 0x8058
#define GL_RED 0x1903
#define GL_RG 0x8227
#define GL_DEPTH_COMPONENT32F 0x8CAC

typedef ptrdiff_t GLsizeiptr;
typedef char GLchar;

typedef void(APIENTRY *GlGenVertexArraysProc)(GLsizei, GLuint *);
typedef void(APIENTRY *GlBindVertexArrayProc)(GLuint);
typedef void(APIENTRY *GlDeleteVertexArraysProc)(GLsizei, const GLuint *);
typedef void(APIENTRY *GlGenBuffersProc)(GLsizei, GLuint *);
typedef void(APIENTRY *GlBindBufferProc)(GLenum, GLuint);
typedef void(APIENTRY *GlBufferDataProc)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void(APIENTRY *GlBindBufferBaseProc)(GLenum, GLuint, GLuint);
typedef void(APIENTRY *GlDeleteBuffersProc)(GLsizei, const GLuint *);
typedef void(APIENTRY *GlEnableVertexAttribArrayProc)(GLuint);
typedef void(APIENTRY *GlVertexAttribPointerProc)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
typedef GLuint(APIENTRY *GlCreateShaderProc)(GLenum);
typedef void(APIENTRY *GlShaderSourceProc)(GLuint, GLsizei, const GLchar *const *, const GLint *);
typedef void(APIENTRY *GlCompileShaderProc)(GLuint);
typedef void(APIENTRY *GlGetShaderivProc)(GLuint, GLenum, GLint *);
typedef void(APIENTRY *GlGetShaderInfoLogProc)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef GLuint(APIENTRY *GlCreateProgramProc)(void);
typedef void(APIENTRY *GlAttachShaderProc)(GLuint, GLuint);
typedef void(APIENTRY *GlLinkProgramProc)(GLuint);
typedef void(APIENTRY *GlGetProgramivProc)(GLuint, GLenum, GLint *);
typedef void(APIENTRY *GlGetProgramInfoLogProc)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void(APIENTRY *GlDeleteShaderProc)(GLuint);
typedef void(APIENTRY *GlDeleteProgramProc)(GLuint);
typedef void(APIENTRY *GlUseProgramProc)(GLuint);
typedef GLint(APIENTRY *GlGetUniformLocationProc)(GLuint, const GLchar *);
typedef void(APIENTRY *GlUniform1fProc)(GLint, GLfloat);
typedef void(APIENTRY *GlUniform1iProc)(GLint, GLint);
typedef void(APIENTRY *GlUniform3fProc)(GLint, GLfloat, GLfloat, GLfloat);
typedef void(APIENTRY *GlUniform3iProc)(GLint, GLint, GLint, GLint);
typedef void(APIENTRY *GlUniform4fProc)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void(APIENTRY *GlUniformMatrix4fvProc)(GLint, GLsizei, GLboolean, const GLfloat *);
typedef void(APIENTRY *GlDispatchComputeProc)(GLuint, GLuint, GLuint);
typedef void(APIENTRY *GlMemoryBarrierProc)(GLbitfield);
typedef void(APIENTRY *GlGenFramebuffersProc)(GLsizei, GLuint *);
typedef void(APIENTRY *GlBindFramebufferProc)(GLenum, GLuint);
typedef void(APIENTRY *GlFramebufferTexture2DProc)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum(APIENTRY *GlCheckFramebufferStatusProc)(GLenum);
typedef void(APIENTRY *GlDeleteFramebuffersProc)(GLsizei, const GLuint *);
typedef void(APIENTRY *GlActiveTextureProc)(GLenum);

extern GlGenVertexArraysProc glGenVertexArrays;
extern GlBindVertexArrayProc glBindVertexArray;
extern GlDeleteVertexArraysProc glDeleteVertexArrays;
extern GlGenBuffersProc glGenBuffers;
extern GlBindBufferProc glBindBuffer;
extern GlBufferDataProc glBufferData;
extern GlBindBufferBaseProc glBindBufferBase;
extern GlDeleteBuffersProc glDeleteBuffers;
extern GlEnableVertexAttribArrayProc glEnableVertexAttribArray;
extern GlVertexAttribPointerProc glVertexAttribPointer;
extern GlCreateShaderProc glCreateShader;
extern GlShaderSourceProc glShaderSource;
extern GlCompileShaderProc glCompileShader;
extern GlGetShaderivProc glGetShaderiv;
extern GlGetShaderInfoLogProc glGetShaderInfoLog;
extern GlCreateProgramProc glCreateProgram;
extern GlAttachShaderProc glAttachShader;
extern GlLinkProgramProc glLinkProgram;
extern GlGetProgramivProc glGetProgramiv;
extern GlGetProgramInfoLogProc glGetProgramInfoLog;
extern GlDeleteShaderProc glDeleteShader;
extern GlDeleteProgramProc glDeleteProgram;
extern GlUseProgramProc glUseProgram;
extern GlGetUniformLocationProc glGetUniformLocation;
extern GlUniform1fProc glUniform1f;
extern GlUniform1iProc glUniform1i;
extern GlUniform3fProc glUniform3f;
extern GlUniform3iProc glUniform3i;
extern GlUniform4fProc glUniform4f;
extern GlUniformMatrix4fvProc glUniformMatrix4fv;
extern GlDispatchComputeProc glDispatchCompute;
extern GlMemoryBarrierProc glMemoryBarrier;
extern GlGenFramebuffersProc glGenFramebuffers;
extern GlBindFramebufferProc glBindFramebuffer;
extern GlFramebufferTexture2DProc glFramebufferTexture2D;
extern GlCheckFramebufferStatusProc glCheckFramebufferStatus;
extern GlDeleteFramebuffersProc glDeleteFramebuffers;
extern GlActiveTextureProc glActiveTexture;

bool gl_api_load(void);
GLuint gl_program_from_files(const char *vertex_path, const char *fragment_path);
GLuint gl_compute_program_from_file(const char *compute_path);

#endif
