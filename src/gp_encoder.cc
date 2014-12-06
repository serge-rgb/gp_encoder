/**
 * gp_encoder.cc
 *
 * Tiny JPEG Encoder
 *
 * 2014 Sergio Gonzalez
 *
 * [WIP] Implements JPEG.
 *
 * Supports:
 *  Windows, MSVC
 *
 * Public domain.
 *
 */

// C std lib
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>  // FILE
#include <string.h>  // memcpy

// ============================================================
// Public interface:
// ============================================================
// In:
//  width, height. Dimensions of image.
//  src_data: Image data in RGB format.
int tje_encode(
        const unsigned char* src_data,
        const int width,
        const int height,
        const char* dest_path);

// ============================================================
// private namespace
// ============================================================
namespace tje
{
// ============================================================
// Language defines
// ============================================================

#define tje_log(msg)  // Re-defined in platform code.

// ============================================================
// Define types
// ============================================================

typedef char          int8;
typedef unsigned char uint8;
typedef int16_t       int16;
typedef uint16_t      uint16;
typedef int32_t       int32;
typedef uint32_t      uint32;
typedef int64_t       int64;
typedef uint64_t      uint64;
typedef int32         bool32;

// ============================================================
// Code
// ============================================================

// JFIF image header:
// SOI(ffd8)
// APP0 [JFIF FRAME]
// COM | len | "blah"
// DQT <p.39>               // TODO: Why more than once?
// SOF <p.35>
// DHT <p.40>

/* void *memcpy( */
/*    void *dest, */
/*    const void *src, */
/*    size_t count */
/* ); */

// ============================================================
//    === DQT Matrix just as with the gimp.
// ============================================================

// K.1 - suggested luminance QT
static uint8 qt_luma[] =
{
   16,11,10,16, 24, 40, 51, 61,
   12,12,14,19, 26, 58, 60, 55,
   14,13,16,24, 40, 57, 69, 56,
   14,17,22,29, 51, 87, 80, 62,
   18,22,37,56, 68,109,103, 77,
   24,35,55,64, 81,104,113, 92,
   49,64,78,87,103,121,120,101,
   72,92,95,98,112,100,103, 99,
};

// K.1 - suggested chrominance QT
static uint8 qt_chroma[] =
{
   17,18,24,47,99,99,99,99,
   18,21,26,66,99,99,99,99,
   24,26,56,99,99,99,99,99,
   47,66,99,99,99,99,99,99,
   99,99,99,99,99,99,99,99,
   99,99,99,99,99,99,99,99,
   99,99,99,99,99,99,99,99,
   99,99,99,99,99,99,99,99,
};
/*
 * Zig-zag order:
   0,  1,  5,  6, 14, 15, 27, 28,
   2,  4,  7, 13, 16, 26, 29, 42,
   3,  8, 12, 17, 25, 30, 41, 43,
   9, 11, 18, 24, 31, 40, 44, 53,
  10, 19, 23, 32, 39, 45, 52, 54,
  20, 22, 33, 38, 46, 51, 55, 60,
  21, 34, 37, 47, 50, 56, 59, 61,
  35, 36, 48, 49, 57, 58, 62, 63,
   */
static uint8 zig_zag_indices[64] = {
   0,  1,  5,  6, 14, 15, 27, 28, 2,  4,  7, 13, 16, 26, 29, 42, 3,  8, 12, 17,
   25, 30, 41, 43, 9, 11, 18, 24, 31, 40, 44, 53, 10, 19, 23, 32, 39, 45, 52,
   54, 20, 22, 33, 38, 46, 51, 55, 60, 21, 34, 37, 47, 50, 56, 59, 61, 35, 36,
   48, 49, 57, 58, 62, 63
};

// ============================================================

// Memory order as big endian. 0xhilo -> 0xlohi which looks as 0xhilo in memory.
static uint16 be_word(uint16 le_word)
{
    uint8 lo = (uint8)(le_word & 0x00ff);
    uint8 hi = (uint8)((le_word & 0xff00) >> 8);
    return (((uint16)lo) << 8) | hi;
}

static uint8 k_jfif_id[] = "JFIF";
static uint8 k_com_str[] = "Created by Tiny JPEG Encoder";
#pragma pack(push)
#ifdef MSVC_VER
#pragma pack(1)
#elif defined(__clang__)
#pragma pack(1)
#endif
typedef struct JPEGHeader_s
{
    uint16 SOI;
    // JFIF header.
    uint16 APP0;
    uint16 jfif_len;
    uint8  jfif_id[5];
    uint16 version;
    uint8  units;
    uint16 x_density;
    uint16 y_density;
    uint16  thumb_size;

    // Our signature
    uint16 com;
    uint16 com_len;
    char com_str[sizeof(k_com_str) - 1];
} JPEGHeader;
#ifdef MSVC_VER
#pragma pack(pop)
#elif defined(__clang__)
#pragma pack(pop)
#endif

static JPEGHeader gen_jpeg_header()
{
    JPEGHeader header;
    header.SOI = be_word(0xffd8);  // Sequential DCT

    // JFIF header.
    header.APP0 = be_word(0xffe0);

    header.jfif_len = be_word(0x0016);
    memcpy(header.jfif_id, k_jfif_id, 5);
    header.version = be_word(0x0102);
    header.units = 0x01;
    header.x_density = be_word(0x0060);  // 96 DPI
    header.y_density = be_word(0x0060);  // 96 DPI
    header.thumb_size = 0;

    // Our signature
    header.com = be_word(0xfffe);
    header.com_len = be_word(sizeof(k_com_str) - 1);
    memcpy(header.com_str, k_com_str, sizeof(k_com_str) - 1);

    return header;
}

static void write_DQT(FILE* fd, uint8* matrix, uint8 id)
{
    assert(fd);

    int16 DQT = be_word(0xffdb);
    fwrite(&DQT, sizeof(int16), 1, fd);
    int16 len = be_word(0x0043); // 2(len) + 1(id) + 64(matrix) = 67 = 0x43
    fwrite(&len, sizeof(int16), 1, fd);
    assert(id < 4);
    uint8 precision_and_id = id;  // 0x0000 8 bits | 0x00id
    fwrite(&precision_and_id, sizeof(uint8), 1, fd);
    // Write matrix
    fwrite(matrix, 64*sizeof(uint8), 1, fd);
}

static int encode(
        const unsigned char* src_data,
        const int width,
        const int height,
        const char* dest_path)
{
    int res = 0;
    // Make an endianness check. We only support little endian.
    {
        //    0xaabb
        // -> 0xbbaa (memory)
        //         ^--- pointer
        uint16 foo = 0xaabb;
        char* pointer = (char*)&foo;
        if (*pointer == (char)0xaa) {
            tje_log("This machine is big endian. Not supported.");
            return 1;
        }
    }

    // TODO: support arbitrary resolutions.
    if (((height % 8) != 0) || ((width % 8) != 0))
    {
        tje_log("Supported resolutions are width and height multiples of 8.");
        return 1;
    }

    FILE* file_out = fopen("../out.jpg", "wb");
    if (!file_out)
    {
        tje_log("Could not open file for writing.");
        return 1;
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int i = 3 * (x + (y * width));
            uint8 r = src_data[i + 0];
            uint8 g = src_data[i + 1];
            uint8 b = src_data[i + 2];
            uint32 pixel = 0;
            // pixel: 0xRRGGBB00
            pixel += (r << 24);
            pixel += (g << 16);
            pixel += (b <<  8);
            // TODO: Actual compression code
        }
    }

    { // Write header
        JPEGHeader header = gen_jpeg_header();
        fwrite(&header, sizeof(JPEGHeader), 1, file_out);
    }

    // Write quantization tables.
    write_DQT(file_out, qt_luma, 0x00);
    write_DQT(file_out, qt_chroma, 0x01);

    // Finish the image.
    {
        uint16 EOI = be_word(0xffd9);
        fwrite(&EOI, sizeof(uint16), 1, file_out);
    }
    res |= fclose(file_out);
    return res;
}
}  // namespace tje

// Define public interface.
int tje_encode(
        const unsigned char* src_data,
        const int width,
        const int height,
        const char* dest_path)
{
    return tje::encode(src_data, width, height, dest_path);
}

// ============================================================
// Standalone Windows app.
// ============================================================

#ifdef tje_log
#undef tje_log
#endif

#ifdef TJE_STANDALONE

#ifdef _WIN64
// Windows includes
#include <windows.h>
#define tje_log OutputDebugStringA
#elif defined(__linux__) || defined(__MACH__)
#define tje_log puts
#endif

// Local includes
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"

#ifdef _WIN64
int CALLBACK WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow)
#elif defined(__linux__) || defined(__MACH__)
int main()
#endif
{
    int width;
    int height;
    int num_components;

    unsigned char* data = stbi_load("../in.bmp", &width, &height, &num_components, 0);

    if (!data)
    {
        tje_log("Could not load bmp file.\n");
        return 1;
    }

    assert (num_components == 3);
    int result = tje_encode(
            data,
            width,
            height,
            "../out.bmp");

    // Try to load the image with stb_image
    {
        unsigned char* my_data =
            stbi_load("../out.jpg", &width, &height, &num_components, 0);
        if (!my_data) {
            tje_log("STB could not load my JPEG\n");
        }
    }

    return result;
    // stbi_image_free(data);
}

#endif  // TJE_STANDALONE
