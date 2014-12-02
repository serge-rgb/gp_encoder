/**
 *
 * gp_encoder.cc
 *
 * 2014 Sergio Gonzalez
 *
 */

#include <windows.h>
#include <inttypes.h>

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

int CALLBACK WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow)
{
    int width;
    int height;
    int numComponents;

    uint8* data = stbi_load("../in.bmp", &width, &height, &numComponents, 3);

    if (!data)
    {
        OutputDebugStringA("Could not load bmp file.\n");
        goto exit;
    }

    char buffer[256];
    sprintf(buffer, "Image size is  %dx%d\nComponents: %d", width, height, numComponents);
    OutputDebugStringA(buffer);

    stbi_image_free(data);

exit:
    return 0;
}
