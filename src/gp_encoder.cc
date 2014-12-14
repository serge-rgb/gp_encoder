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
#include <inttypes.h>
#include <stdio.h>  // FILE
#include <string.h>  // memcpy
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#define tje_log(msg) OutputDebugStringA(msg)  // Re-defined in platform code.
#endif
#ifdef TJE_DEBUG
#define tje_assert(expr) if (!(expr)) {*((int*)0) = 0; }
#else
#define tje_assert(expr)
#endif

#define tje_array_count(array) (sizeof(array) / sizeof(array[0]))


// ============================================================
// Public interface:
// ============================================================
// In:
//  width, height. Dimensions of image.
//  src_data: Image data in RGB format.
//  dest_path: Name of file to open and write to.
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
// DQT <p.39>
// SOF <p.35>
// DHT <p.40>

// ============================================================
//    === Define various default tables.
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
static uint8 zig_zag_indices[64] =
{
   0,  1,  5,  6, 14, 15, 27, 28, 2,  4,  7, 13, 16, 26, 29, 42, 3,  8, 12, 17,
   25, 30, 41, 43, 9, 11, 18, 24, 31, 40, 44, 53, 10, 19, 23, 32, 39, 45, 52,
   54, 20, 22, 33, 38, 46, 51, 55, 60, 21, 34, 37, 47, 50, 56, 59, 61, 35, 36,
   48, 49, 57, 58, 62, 63
};

// Huffman table as suggested by the spec.
// We can derive the complete Huffman tree from this at runtime, and save ourselves
// extra complexity / code.
//  (Procedure described in jpeg spec: C.2)


// Number of 16 bit values for every code length. (K.3.3.1)
static uint8 ht_luma_dc_len[16] =
{
    0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0
};
// values
static uint8 ht_luma_dc[12] =
{
    0,1,2,3,4,5,6,7,8,9,10,11
};

// Number of 16 bit values for every code length. (K.3.3.1)
static uint8 ht_chroma_dc_len[16] =
{
    0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0
};
// values
static uint8 ht_chroma_dc[12] =
{
    0,1,2,3,4,5,6,7,8,9,10,11
};

// Same as above, but AC coefficients.
static uint8 ht_luma_ac_len[16] =
{
    0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d
};
static uint8 ht_luma_ac[] =
{
0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
0xF9, 0xFA
};

static uint8 ht_chroma_ac_len[16] =
{
    0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,0x77
};
static uint8 ht_chroma_ac[] =
{
0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
0xF9, 0xFA
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

    memcpy(header.com_str, k_com_str, sizeof(k_com_str) - 1);

    return header;
}

static void write_DQT(FILE* fd, uint8* matrix, uint8 id)
{
    tje_assert(fd);

    int16 DQT = be_word(0xffdb);
    fwrite(&DQT, sizeof(int16), 1, fd);
    int16 len = be_word(0x0043); // 2(len) + 1(id) + 64(matrix) = 67 = 0x43
    fwrite(&len, sizeof(int16), 1, fd);
    tje_assert(id < 4);
    uint8 precision_and_id = id;  // 0x0000 8 bits | 0x00id
    fwrite(&precision_and_id, sizeof(uint8), 1, fd);
    // Write matrix
    fwrite(matrix, 64*sizeof(uint8), 1, fd);
}

enum HuffmanTableClass
{
    DC = 0,
    AC = 1
};

static void write_DHT(
        FILE *fd,
        uint8* matrix_len,
        uint8* matrix_val,
        HuffmanTableClass ht_class,
        uint8 id)
{
    tje_assert(fd);

    int num_values = 0;
    for (int i = 0; i < 16; ++i)
    {
        num_values += matrix_len[i];
    }
    tje_assert(num_values <= 0xffff);

    int16 DHT = be_word(0xffc4);
    // 2(len) + 1(Tc|th) + 16 (num lengths) + ?? (num values)
    uint16 len = be_word(2 + 1 + 16 + (uint16)num_values);
    tje_assert(id < 4);
    uint8 tc_th = ((((uint8)ht_class) << 4) | id);

    fwrite(&DHT, sizeof(uint16), 1, fd);
    fwrite(&len, sizeof(uint16), 1, fd);
    fwrite(&tc_th, sizeof(uint8), 1, fd);
    fwrite(matrix_len, sizeof(uint8), 16, fd);
    fwrite(matrix_val, sizeof(uint8), num_values, fd);
}

// NOTE:
//  Once this function correctly implements a jpeg encoder, we need to
//  start morphing it into something that  can do this:
//
//  1) Receives quantization tables from caller.
//  2) Can constrain itself to a limited number of MCUs
//
//  It would be nice to add this functionality and keep the jpeg encoder
//  orthogonal so that it can be of more general use outside of my genetic
//  algorithm adventure.

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
        if (*pointer != (char)(0xbb))
        {
            tje_log("This machine is big endian. Not supported.");
            return 1;
        }
    }
#ifdef TJE_DEBUG   // Test our hard-coded huffman specification tables.
    {
        int luma_dc_c = 0;
        int luma_ac_c = 0;
        int chroma_dc_c = 0;
        int chroma_ac_c = 0;
        for (int i = 0; i < 16; ++i)
        {
            luma_dc_c += ht_luma_dc_len[i];
            luma_ac_c += ht_luma_ac_len[i];
            chroma_dc_c += ht_chroma_dc_len[i];
            chroma_ac_c += ht_chroma_ac_len[i];
        }
        tje_assert(tje_array_count(ht_chroma_ac) == chroma_ac_c);
        tje_assert(tje_array_count(ht_chroma_dc) == chroma_dc_c);
        tje_assert(tje_array_count(ht_luma_ac) == luma_ac_c);
        tje_assert(tje_array_count(ht_luma_dc) == luma_dc_c);
    }
#endif

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

    int bytes_per_pixel = 3;  // Only supporting RGB right now..

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int i = bytes_per_pixel * (x + (y * width));
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

    // TODO: write Start of Frame marker:
    // <----- SOF  // ffc0

    // Write huffman table specifications
    HuffmanTableClass ac = AC;
    HuffmanTableClass dc = DC;
    write_DHT(file_out, ht_luma_dc_len  , ht_luma_dc  , dc, 0);
    write_DHT(file_out, ht_luma_ac_len  , ht_luma_ac  , ac, 0);
    write_DHT(file_out, ht_chroma_dc_len, ht_chroma_dc, dc, 1);
    write_DHT(file_out, ht_chroma_ac_len, ht_chroma_ac, ac, 1);

    // Write start of scan!
    uint16 SOS = be_word(0xffda);
    fwrite(&SOS, sizeof(int16), 1, file_out);

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

#ifdef TJE_STANDALONE

// Local includes
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"

#ifdef _WIN32
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

    tje_assert (num_components == 3);
    int result = tje_encode(
            data,
            width,
            height,
            "../out.bmp");

    // Try to load the image with stb_image
    {
        // test ref image'
        stbi_load("../ref.jpg", NULL, NULL, NULL, 0);
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
