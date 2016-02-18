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

    memcpy(state.qt_luma, qt, 64 * sizeof(uint8_t));
    memcpy(state.qt_chroma, qt, 64 * sizeof(uint8_t));

    TJEWriteContext wc = { 0 };

    wc.context = fd;
    wc.func = tjei_stdlib_func;

    state.write_context = wc;

    tjei_huff_expand(&state);

    int result = tjei_encode_main(&state, src_data, width, height, num_components);

    result |= 0 == fclose(fd);

    return result;
}
