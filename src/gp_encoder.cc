/**
 *
 * gp_encoder.cc
 *
 * 2014 Sergio Gonzalez
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

///////////////////////////////////////////////////////////////////////////////
// Language macros
///////////////////////////////////////////////////////////////////////////////

#define internal_variable   static
#define internal_constant   static const
#define internal            static
#define local_persist       static

///////////////////////////////////////////////////////////////////////////////
// Define types
///////////////////////////////////////////////////////////////////////////////

typedef char          int8;
typedef unsigned char uint8;
typedef int16_t       int16;
typedef uint16_t      uint16;
typedef int32_t       int32;
typedef uint32_t      uint32;
typedef int64_t       int64;
typedef uint64_t      uint64;
typedef int32         bool32;

///////////////////////////////////////////////////////////////////////////////
// Code
///////////////////////////////////////////////////////////////////////////////

// SOI      ffd8
// APP0     ffe0
//

int CALLBACK WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow)
{
    int width;
    int height;
    int num_components;

    uint8* data = stbi_load("../in.bmp", &width, &height, &num_components, 3);

    if (!data)
    {
        OutputDebugStringA("Could not load bmp file.\n");
        goto exit;
    }

    assert (num_components == 3);

    // TODO: support arbitrary resolutions.
    if (((height % 8) != 0) || ((width % 8) != 0))
    {
        OutputDebugStringA("Supported resolutions are width and height multiples of 8.");
        goto exit;
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

    char buffer[256];
    sprintf(buffer, "Image size is  %dx%d\nComponents: %d\n", width, height, num_components);
    OutputDebugStringA(buffer);

    stbi_image_free(data);

exit:
    return 0;
}
