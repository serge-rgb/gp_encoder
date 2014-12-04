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
 * Does not support:
 *  Big endian processors.
 *
 *  TODO:
 *      Copy GIMP's quantizing & huffman matrices (aka finish header)
 *      Impl. Annex C of spec. (EHUFSI & EHUFCO) ~p57 (aka impl. huffman enc.)
 *      Impl. Annex F of spec.
 *
 *
 */

// Windows
#include <windows.h>

// C std lib
#include <assert.h>
#include <inttypes.h>

// Local includes
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"

////////////////////////////////////////////////////////////////////////////////
// Language defines
////////////////////////////////////////////////////////////////////////////////

// ............

////////////////////////////////////////////////////////////////////////////////
// Define types
////////////////////////////////////////////////////////////////////////////////

typedef char          int8;
typedef unsigned char uint8;
typedef int16_t       int16;
typedef uint16_t      uint16;
typedef int32_t       int32;
typedef uint32_t      uint32;
typedef int64_t       int64;
typedef uint64_t      uint64;
typedef int32         bool32;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

// JFIF image header:
// SOI(ffd8)
// APP0 [JFIF FRAME]
// COM | len | "blah"
// DQT <p.39>               // TODO: Why more than once?
// SOF <p.35>
// DHT <p.40>
//

// =============================================================================
//    === DQT Matrix just as with the gimp.
// =============================================================================

static uint8 dqt_1[] =
{
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
};

static uint8 dqt_2[] =
{
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,
};

// =============================================================================

static uint8 k_jfif_id[] = "JFIF";
//static uint8 k_com_str[] = "Created by Tiny JPEG Encoder";
static uint8 k_com_str[] = "Hello World";
#pragma pack(1)
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
#pragma pack(pop)

// Memory order as big endian. 0xhilo -> 0xlohi which looks as 0xhilo in memory.
static uint16 be_word(uint16 le_word)
{
    uint8 lo = (uint8)(le_word & 0x00ff);
    uint8 hi = (uint8)((le_word & 0xff00) >> 8);
    return (((uint16)lo) << 8) | hi;
}

static JPEGHeader gen_jpeg_header()
{
    JPEGHeader header;
    header.SOI = be_word(0xffd8);  // Sequential DCT

    // JFIF header.
    header.APP0 = be_word(0xffe0);

    header.jfif_len = be_word(16);
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
    assert(id < 256);
    uint8 precision_and_id = id;  // 0x0000 8 bits | 0x00id
    fwrite(&precision_and_id, sizeof(uint8), 1, fd);
    // Write matrix
    fwrite(matrix, 64*sizeof(uint8), 1, fd);
}

int CALLBACK WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow)
{

    // Make an endianness check. We only support little endian.
    {
        //    0xaabb
        // -> 0xbbaa (memory)
        //         ^--- pointer
        uint16 foo = 0xaabb;
        char* pointer = (char*)&foo;
        if (*pointer == 0xaa) {
            OutputDebugStringA("This machine is big endian. Not supported.");
            return 1;
        }
    }
    int width;
    int height;
    int num_components;

    uint8* data = stbi_load("../in.bmp", &width, &height, &num_components, 0);

    if (!data)
    {
        OutputDebugStringA("Could not load bmp file.\n");
        return 1;
    }

    assert (num_components == 3);

    // TODO: support arbitrary resolutions.
    if (((height % 8) != 0) || ((width % 8) != 0))
    {
        OutputDebugStringA(
                "Supported resolutions are width and height multiples of 8.");
        return 1;
    }

    FILE* file_out = fopen("../out.jpg", "wb");
    if (!file_out)
    {
        OutputDebugStringA(
                "Could not open file for writing.");
        return 1;
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int i = 3 * (x + (y * width));
            uint8 r = data[i + 0];
            uint8 g = data[i + 1];
            uint8 b = data[i + 2];
            uint32 pixel = 0;
            // pixel: 0xRRGGBB00
            pixel += (r << 24);
            pixel += (g << 16);
            pixel += (b <<  8);
            // TODO: Actual compression code
        }
    }

    // == Write header
    {
        JPEGHeader header = gen_jpeg_header();
        fwrite(&header, sizeof(JPEGHeader), 1, file_out);
        write_DQT(file_out, dqt_1, 0x00);
        write_DQT(file_out, dqt_2, 0x01);
    }

    // Finish the image.
    {
        uint16 EOI = be_word(0xffd9);
        fwrite(&EOI, sizeof(uint16), 1, file_out);
    }
    fclose(file_out);

    // Try to load the image with stb_image
    {
        uint8* my_data =
            stbi_load("../out.jpg", &width, &height, &num_components, 0);
        if (!my_data) {
            OutputDebugStringA("STB could not load my JPEG\n");
        }
    }

    stbi_image_free(data);

    return 0;
}
