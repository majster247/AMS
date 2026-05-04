/**
 * @file GLES2/gl2.h
 * @brief OpenGL ES 2.0 interface definitions for AMS-OS Mesa3D port
 *
 * Minimal GLES2 header for wlroots GLES2 renderer.
 * Full implementation provided by Mesa3D.
 */

#ifndef __gl2_h_
#define __gl2_h_

#include <stdint.h>

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef int8_t GLbyte;
typedef int16_t GLshort;
typedef int32_t GLint;
typedef int32_t GLsizei;
typedef uint8_t GLubyte;
typedef uint16_t GLushort;
typedef uint32_t GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef int32_t GLfixed;
typedef intptr_t GLintptr;
typedef uintptr_t GLsizeiptr;
typedef void GLvoid;
typedef char GLchar;

#define GL_FALSE                    0
#define GL_TRUE                     1

#define GL_NO_ERROR                 0
#define GL_INVALID_ENUM             0x0500
#define GL_INVALID_VALUE            0x0501
#define GL_INVALID_OPERATION        0x0502
#define GL_OUT_OF_MEMORY            0x0505

#define GL_COLOR_BUFFER_BIT         0x00004000
#define GL_DEPTH_BUFFER_BIT         0x00000100
#define GL_STENCIL_BUFFER_BIT       0x00000400

#define GL_TRIANGLES                0x0004
#define GL_TRIANGLE_STRIP           0x0005
#define GL_TRIANGLE_FAN             0x0006

#define GL_TEXTURE_2D               0x0DE1
#define GL_TEXTURE_EXTERNAL_OES     0x8D65
#define GL_RGBA                     0x1908
#define GL_BGRA_EXT                 0x80E1
#define GL_UNSIGNED_BYTE            0x1401

#define GL_FRAMEBUFFER              0x8D40
#define GL_RENDERBUFFER             0x8D41
#define GL_COLOR_ATTACHMENT0        0x8CE0
#define GL_FRAMEBUFFER_COMPLETE     0x8CD5

#define GL_VERTEX_SHADER            0x8B31
#define GL_FRAGMENT_SHADER          0x8B30
#define GL_COMPILE_STATUS           0x8B81
#define GL_LINK_STATUS              0x8B82

#define GL_BLEND                    0x0BE2
#define GL_SRC_ALPHA                0x0302
#define GL_ONE_MINUS_SRC_ALPHA      0x0303

#define GL_TEXTURE_MIN_FILTER       0x2801
#define GL_TEXTURE_MAG_FILTER       0x2800
#define GL_LINEAR                   0x2601
#define GL_NEAREST                  0x2600
#define GL_CLAMP_TO_EDGE            0x812F
#define GL_TEXTURE_WRAP_S           0x2802
#define GL_TEXTURE_WRAP_T           0x2803

#define GL_FLOAT                    0x1406
#define GL_ARRAY_BUFFER             0x8892
#define GL_STATIC_DRAW              0x88E4

#define GL_EXTENSIONS               0x1F03
#define GL_RENDERER                 0x1F01
#define GL_VERSION                  0x1F02
#define GL_VENDOR                   0x1F00

#endif /* __gl2_h_ */
