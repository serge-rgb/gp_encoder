/**
 * evolve.cc
 *
 * 2015 Sergio Gonzalez
 *
 */

#ifdef _WIN32

#include <windows.h>
#define platform_allocate win32_allocate

void* win32_allocate(size_t sz)
{
    static only_once = false;
    if (only_once)
    {
        OutputDebugStringA("Error: win32_allocate is not supposed to be called many times.");
    }
    void* result = VirtualAlloc(
#ifdef TJE_DEBUG
        (LPVOID)(1024LL * 1024 * 1024 * 1024), //  lpAddress,
#else
        NULL, //  lpAddress,
#endif
        sz,//  dwSize,
        MEM_COMMIT | MEM_RESERVE, //  flAllocationType,
        PAGE_READWRITE//  flProtect
        );
    return result;
}

#elif defined(__MACH__)

#define platform_allocate osx_allocate
#include <sys/mman.h>
void* osx_allocate(size_t size)
{
    void* data = mmap(
#ifdef TJE_DEBUG
            (void*)(1024LL * 1024 * 1024 * 1024), //  lpAddress,
#else
            NULL, //  lpAddress,
#endif
            size,
            PROT_READ | PROT_WRITE,
            MAP_ANON,
            -1, //fd
            0 //offset
            );
    return data;
}

#endif // _WIN32 , __MACH__

#ifndef platform_allocate
#error "platform_allocate macro needs to be defined."
#endif


#include "memory.h"

static Arena g_jpeg_arena;

void* arena_malloc_(size_t size)
{
    return arena_push_(&g_jpeg_arena, size);
}

#define tje_malloc arena_malloc_
#define tje_free(ptr)  // No freeing
#define TJE_IMPLEMENTATION
#include <tiny_jpeg.cc>

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"

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

    void* big_chunk_of_memory = platform_allocate(1024 * 1024 * 1024);  // One gigabyte

    g_jpeg_arena = arena_create(big_chunk_of_memory, 20 * 1024 * 1024); // Reserve 20 megs for jpeg code.

    Arena test_arena = arena_create(
            (void*)((uint8_t*)big_chunk_of_memory + g_jpeg_arena.size), 1024 * 1024); // One meg arena.

    int* array_of_zeroes = arena_push_array(&test_arena, 1000, int);

    int width;
    int height;
    int num_components;
    unsigned char* data = stbi_load("in.bmp", &width, &height, &num_components, 0);

    if (!data)
    {
        tje_log("Could not load bmp file.\n");
        const char* err = stbi_failure_reason();
        tje_log(err);
        return 1;
    }

    tje_assert (num_components == 3);

    int result = tje_encode_to_file(
            data,
            width,
            height,
            "out.jpg");
    return result;
}
