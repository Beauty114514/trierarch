#ifndef _GL4ES_DECOMPRESS_H_
#define _GL4ES_DECOMPRESS_H_

#include <stdint.h>
#include <epoxy/gl.h>

void DecompressBlockDXT1(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image);

void DecompressBlockDXT3(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image);

void DecompressBlockDXT5(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image);

GLboolean isDXTcSRGB(GLenum format);
GLboolean isDXTcAlpha(GLenum format);

GLvoid *uncompressDXTc(GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
    int transparent0, int* simpleAlpha, int* complexAlpha, const GLvoid *data);

#endif
