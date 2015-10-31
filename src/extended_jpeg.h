#pragma once

int tje_encode_to_file_with_qt(const char* dest_path,
                               uint8_t* qt,
                               const int width,
                               const int height,
                               const int num_components,
                               const unsigned char* src_data);
