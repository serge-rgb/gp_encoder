#define TJE_IMPLEMENTATION
#include <tiny_jpeg.h>


int tje_encode_to_file_with_qt(const char* dest_path,
                               uint8_t* qt,
                               const int width,
                               const int height,
                               const int num_components,
                               const unsigned char* src_data)
{
    FILE* fd = fopen(dest_path, "wb");
    if (!fd) {
        tje_log("Could not open file for writing.");
        return 0;
    }

    TJEState state = { 0 };

    state.fd = fd;

    memcpy(state.qt_luma, qt, 64 * sizeof(uint8_t));
    memcpy(state.qt_chroma, qt, 64 * sizeof(uint8_t));

    tjei_huff_expand(&state);

    int result = tjei_encode_main(&state, src_data, width, height, num_components);

    result |= 0 == fclose(fd);

    return result;
}
