// Origin: https://github.com/alexvorxx/VirGL-Overlay-Rebuild/commit/d3052d9
// DXT1/DXT3/DXT5 texture decompression from gl4es
// Copyright (c) 2012, Matthaeus G. "Anteru" Chajdas / Benjamin Dobell (MIT)

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <epoxy/gl.h>

#include "gl4es-decompress.h"

static uint32_t PackRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return r | (g << 8) | (b << 16) | (a << 24);
}

static void DecompressBlockDXT1Internal(const uint8_t* block,
    uint32_t* output, uint32_t outputStride,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    const uint8_t* alphaValues)
{
    uint32_t temp, code;
    uint16_t color0, color1;
    uint8_t r0, g0, b0, r1, g1, b1;
    int i, j;

    color0 = *(const uint16_t*)(block);
    color1 = *(const uint16_t*)(block + 2);

    temp = (color0 >> 11) * 255 + 16;
    r0 = (uint8_t)((temp/32 + temp)/32);
    temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
    g0 = (uint8_t)((temp/64 + temp)/64);
    temp = (color0 & 0x001F) * 255 + 16;
    b0 = (uint8_t)((temp/32 + temp)/32);

    temp = (color1 >> 11) * 255 + 16;
    r1 = (uint8_t)((temp/32 + temp)/32);
    temp = ((color1 & 0x07E0) >> 5) * 255 + 32;
    g1 = (uint8_t)((temp/64 + temp)/64);
    temp = (color1 & 0x001F) * 255 + 16;
    b1 = (uint8_t)((temp/32 + temp)/32);

    code = *(const uint32_t*)(block + 4);

    if (color0 > color1) {
        for (j = 0; j < 4; ++j) {
            for (i = 0; i < 4; ++i) {
                uint32_t finalColor, positionCode;
                uint8_t alpha = alphaValues[j*4+i];
                finalColor = 0;
                positionCode = (code >> 2*(4*j+i)) & 0x03;
                switch (positionCode) {
                case 0: finalColor = PackRGBA(r0, g0, b0, alpha); break;
                case 1: finalColor = PackRGBA(r1, g1, b1, alpha); break;
                case 2: finalColor = PackRGBA((2*r0+r1)/3, (2*g0+g1)/3, (2*b0+b1)/3, alpha); break;
                case 3: finalColor = PackRGBA((r0+2*r1)/3, (g0+2*g1)/3, (b0+2*b1)/3, alpha); break;
                }
                if (transparent0 && (finalColor==0xff000000)) { alpha=0; finalColor=0; }
                if (!alpha) *simpleAlpha = 1;
                else if (alpha<0xff) *complexAlpha = 1;
                output[j*outputStride + i] = finalColor;
            }
        }
    } else {
        for (j = 0; j < 4; ++j) {
            for (i = 0; i < 4; ++i) {
                uint32_t finalColor, positionCode;
                uint8_t alpha = alphaValues[j*4+i];
                finalColor = 0;
                positionCode = (code >> 2*(4*j+i)) & 0x03;
                switch (positionCode) {
                case 0: finalColor = PackRGBA(r0, g0, b0, alpha); break;
                case 1: finalColor = PackRGBA(r1, g1, b1, alpha); break;
                case 2: finalColor = PackRGBA((r0+r1)/2, (g0+g1)/2, (b0+b1)/2, alpha); break;
                case 3: finalColor = PackRGBA(0, 0, 0, alpha); break;
                }
                if (transparent0 && (finalColor==0xff000000)) { alpha=0; finalColor=0; }
                if (!alpha) *simpleAlpha = 1;
                else if (alpha<0xff) *complexAlpha = 1;
                output[j*outputStride + i] = finalColor;
            }
        }
    }
}

void DecompressBlockDXT1(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image)
{
    static const uint8_t const_alpha[] = {
        255,255,255,255, 255,255,255,255,
        255,255,255,255, 255,255,255,255
    };
    DecompressBlockDXT1Internal(blockStorage,
        image + x + (y * width), width, transparent0, simpleAlpha, complexAlpha, const_alpha);
}

void DecompressBlockDXT5(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image)
{
    uint8_t alpha0, alpha1;
    const uint8_t* bits;
    uint32_t alphaCode1;
    uint16_t alphaCode2;
    int i, j;

    alpha0 = *(blockStorage);
    alpha1 = *(blockStorage + 1);
    bits = blockStorage + 2;
    alphaCode1 = bits[2] | (bits[3] << 8) | (bits[4] << 16) | (bits[5] << 24);
    alphaCode2 = bits[0] | (bits[1] << 8);

    uint16_t color0 = *(const uint16_t*)(blockStorage + 8);
    uint16_t color1 = *(const uint16_t*)(blockStorage + 10);
    uint32_t temp, code;
    uint8_t r0, g0, b0, r1, g1, b1;

    temp = (color0 >> 11) * 255 + 16; r0 = (uint8_t)((temp/32 + temp)/32);
    temp = ((color0 & 0x07E0) >> 5) * 255 + 32; g0 = (uint8_t)((temp/64 + temp)/64);
    temp = (color0 & 0x001F) * 255 + 16; b0 = (uint8_t)((temp/32 + temp)/32);
    temp = (color1 >> 11) * 255 + 16; r1 = (uint8_t)((temp/32 + temp)/32);
    temp = ((color1 & 0x07E0) >> 5) * 255 + 32; g1 = (uint8_t)((temp/64 + temp)/64);
    temp = (color1 & 0x001F) * 255 + 16; b1 = (uint8_t)((temp/32 + temp)/32);

    code = *(const uint32_t*)(blockStorage + 12);

    for (j = 0; j < 4; j++) {
        for (i = 0; i < 4; i++) {
            uint8_t finalAlpha;
            int alphaCode, alphaCodeIndex = 3*(4*j+i);
            if (alphaCodeIndex <= 12)
                alphaCode = (alphaCode2 >> alphaCodeIndex) & 0x07;
            else if (alphaCodeIndex == 15)
                alphaCode = (alphaCode2 >> 15) | ((alphaCode1 << 1) & 0x06);
            else
                alphaCode = (alphaCode1 >> (alphaCodeIndex - 16)) & 0x07;
            if (alphaCode == 0) finalAlpha = alpha0;
            else if (alphaCode == 1) finalAlpha = alpha1;
            else {
                if (alpha0 > alpha1)
                    finalAlpha = (uint8_t)(((8-alphaCode)*alpha0 + (alphaCode-1)*alpha1)/7);
                else {
                    if (alphaCode == 6) finalAlpha = 0;
                    else if (alphaCode == 7) finalAlpha = 255;
                    else finalAlpha = (uint8_t)(((6-alphaCode)*alpha0 + (alphaCode-1)*alpha1)/5);
                }
            }
            uint8_t colorCode = (code >> 2*(4*j+i)) & 0x03;
            uint32_t finalColor = 0;
            switch (colorCode) {
            case 0: finalColor = PackRGBA(r0, g0, b0, finalAlpha); break;
            case 1: finalColor = PackRGBA(r1, g1, b1, finalAlpha); break;
            case 2: finalColor = PackRGBA((2*r0+r1)/3, (2*g0+g1)/3, (2*b0+b1)/3, finalAlpha); break;
            case 3: finalColor = PackRGBA((r0+2*r1)/3, (g0+2*g1)/3, (b0+2*b1)/3, finalAlpha); break;
            }
            if (finalAlpha==0) *simpleAlpha = 1;
            else if (finalAlpha<0xff) *complexAlpha = 1;
            image[i + x + (width * (y+j))] = finalColor;
        }
    }
}

void DecompressBlockDXT3(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image)
{
    int i;
    uint8_t alphaValues[16] = { 0 };
    for (i = 0; i < 4; ++i) {
        const uint16_t* alphaData = (const uint16_t*)(blockStorage);
        alphaValues[i*4 + 0] = (((*alphaData) >> 0) & 0xF) * 17;
        alphaValues[i*4 + 1] = (((*alphaData) >> 4) & 0xF) * 17;
        alphaValues[i*4 + 2] = (((*alphaData) >> 8) & 0xF) * 17;
        alphaValues[i*4 + 3] = (((*alphaData) >> 12) & 0xF) * 17;
        blockStorage += 2;
    }
    DecompressBlockDXT1Internal(blockStorage,
        image + x + (y * width), width, transparent0, simpleAlpha, complexAlpha, alphaValues);
}

GLboolean isDXTcSRGB(GLenum format) {
    switch (format) {
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        return 1;
    }
    return 0;
}

GLboolean isDXTcAlpha(GLenum format) {
    switch (format) {
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        return 1;
    }
    return 0;
}

GLvoid *uncompressDXTc(GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
    int transparent0, int* simpleAlpha, int* complexAlpha, const GLvoid *data)
{
    int pixelsize = 4;
    if (imageSize == width*height*pixelsize || data==NULL)
        return (GLvoid*)data;
    GLvoid *pixels = malloc(((width+3)&~3)*((height+3)&~3)*pixelsize);
    int blocksize;
    switch (format) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        blocksize = 8; break;
    default:
        blocksize = 16; break;
    }
    uintptr_t src = (uintptr_t)data;
    for (int y=0; y<height; y+=4) {
        for (int x=0; x<width; x+=4) {
            switch(format) {
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
                DecompressBlockDXT1(x, y, width, (const uint8_t*)src, transparent0, simpleAlpha, complexAlpha, (uint32_t*)pixels);
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
                DecompressBlockDXT3(x, y, width, (const uint8_t*)src, transparent0, simpleAlpha, complexAlpha, (uint32_t*)pixels);
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
                DecompressBlockDXT5(x, y, width, (const uint8_t*)src, transparent0, simpleAlpha, complexAlpha, (uint32_t*)pixels);
                break;
            }
            src += blocksize;
        }
    }
    return pixels;
}
