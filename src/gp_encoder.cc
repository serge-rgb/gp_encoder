/**
 *
 * gp_encoder.cc
 *
 * 2014 Sergio Gonzalez
 *
 */

#include <windows.h>
#include <inttypes.h>

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



int CALLBACK WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow)
{
    OutputDebugStringA("Hello World\n");
}

