/**
 * evolve.cc
 *
 * 2015 Sergio Gonzalez
 *
 */

#ifdef _WIN32
#include <windows.h>
#endif
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

    int result = tje_encode(
            data,
            width,
            height,
            "out.jpg");
    return result;
}
