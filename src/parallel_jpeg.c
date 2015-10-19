#define LIBSERG_IMPLEMENTATION
#include <libserg/libserg.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

typedef int32_t b32;
#define true 1
#define false 0

#include "gpu.h"

int pje_encode_to_file_at_quality(const char* dest_path,
                                  const int quality,
                                  const int width,
                                  const int height,
                                  const int num_components,
                                  const unsigned char* src_data,
                                  void* memory, size_t size);
/**********************************************************
 *  Parallel JPEG encoder.
 *  Encode JPEG files with the GPU.
 *  Mostly a transformation of "TinyJPEG"
**********************************************************/

#if defined(_MSC_VER)
#define PJEI_FORCE_INLINE __forceinline
// #define PJEI_FORCE_INLINE __declspec(noinline)  // For profiling
#else
#define PJEI_FORCE_INLINE static // TODO: equivalent for gcc & clang
#endif

// Only use zero for debugging and/or inspection.
#define PJE_USE_FAST_DCT 1

// C std lib
#include <assert.h>
#include <inttypes.h>
#include <math.h>   // floorf, ceilf
#include <stdio.h>  // FILE, puts
#include <string.h> // memcpy


#ifndef pje_malloc
#if defined(_WIN32) || defined(__linux__)
#include <malloc.h>
#elif defined(__MACH__)
#include <malloc/malloc.h>
#endif
#define pje_malloc malloc
#endif

#ifndef pje_free
#if defined(_WIN32) || defined(__linux__)
#include <malloc.h>
#elif defined(__MACH__)
#include <malloc/malloc.h>
#endif
#define pje_free(x) free((x)); x = 0;
#endif


#if !defined(pje_write)

#define PJEI_BUFFER_SIZE 1024
#define pje_write pjei_fwrite

// Buffer PJE_BUFFER_SIZE in memory and flush when ready
static size_t pjei_g_output_buffer_count;
static uint8_t pjei_g_output_buffer[PJEI_BUFFER_SIZE];

#endif


#ifdef _WIN32

#include <windows.h>
#define snprintf sprintf_s
// Not quite the same but it works for us. If I am not mistaken, it differs
// only in the return value.

#endif

#ifndef NDEBUG

#ifdef _WIN32
#define pje_log(msg) OutputDebugStringA(msg)
#elif defined(__linux__) || defined(__MACH__)
#define pje_log(msg) puts(msg)
#endif

#else  // NDEBUG
#define pje_log(msg)
#endif  // NDEBUG

typedef struct PJEBlock_s {
    float d[64];
} PJEBlock;

typedef struct PJEState_s {
    uint8_t     ehuffsize[4][257];
    uint16_t    ehuffcode[4][256];

    uint8_t*    ht_bits[4];
    uint8_t*    ht_vals[4];

    uint8_t     qt_luma[64];
    uint8_t     qt_chroma[64];

    FILE*       fd;

    // Arena contents:
    //  - Stored MCUs to send to the CL kernel.
    //  - Buffers to store compressed data from GPU to CPu
    Arena*      arena;
} PJEState;

// ============================================================
// Table definitions.
//
// The spec defines pjei_default reasonably good quantization matrices and huffman
// specification tables.
//
//
// Instead of hard-coding the final huffman table, we only hard-code the table
// spec suggested by the specification, and then derive the full table from
// there.  This is only for didactic purposes but it might be useful if there
// ever is the case that we need to swap huffman tables from various sources.
// ============================================================


// K.1 - suggested luminance QT
static uint8_t pjei_default_qt_luma_from_spec[] = {
   16,11,10,16, 24, 40, 51, 61,
   12,12,14,19, 26, 58, 60, 55,
   14,13,16,24, 40, 57, 69, 56,
   14,17,22,29, 51, 87, 80, 62,
   18,22,37,56, 68,109,103, 77,
   24,35,55,64, 81,104,113, 92,
   49,64,78,87,103,121,120,101,
   72,92,95,98,112,100,103, 99,
};

static uint8_t pjei_default_qt_chroma_from_spec[] = {
    // K.1 - suggested chrominance QT
   17,18,24,47,99,99,99,99,
   18,21,26,66,99,99,99,99,
   24,26,56,99,99,99,99,99,
   47,66,99,99,99,99,99,99,
   99,99,99,99,99,99,99,99,
   99,99,99,99,99,99,99,99,
   99,99,99,99,99,99,99,99,
   99,99,99,99,99,99,99,99,
};

static uint8_t pjei_default_qt_chroma_from_paper[] = {
    // Example QT from JPEG paper
   16,  12, 14,  14, 18, 24,  49,  72,
   11,  10, 16,  24, 40, 51,  61,  12,
   13,  17, 22,  35, 64, 92,  14,  16,
   22,  37, 55,  78, 95, 19,  24,  29,
   56,  64, 87,  98, 26, 40,  51,  68,
   81, 103, 112, 58, 57, 87,  109, 104,
   121,100, 60,  69, 80, 103, 113, 120,
   103, 55, 56,  62, 77, 92,  101, 99,
};

// == Procedure to 'deflate' the huffman tree: JPEG spec, C.2

// Number of 16 bit values for every code length. (K.3.3.1)
static uint8_t pjei_default_ht_luma_dc_len[16] = {
    0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0
};
// values
static uint8_t pjei_default_ht_luma_dc[12] = {
    0,1,2,3,4,5,6,7,8,9,10,11
};

// Number of 16 bit values for every code length. (K.3.3.1)
static uint8_t pjei_default_ht_chroma_dc_len[16] = {
    0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0
};
// values
static uint8_t pjei_default_ht_chroma_dc[12] = {
    0,1,2,3,4,5,6,7,8,9,10,11
};

// Same as above, but AC coefficients.
static uint8_t pjei_default_ht_luma_ac_len[16] = {
    0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d
};
static uint8_t pjei_default_ht_luma_ac[] = {
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

static uint8_t pjei_default_ht_chroma_ac_len[16] = {
    0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,0x77
};
static uint8_t pjei_default_ht_chroma_ac[] = {
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
// Code
// ============================================================

// Zig-zag order:
static uint8_t pjei_zig_zag[64] = {
   0,   1,  5,  6, 14, 15, 27, 28,
   2,   4,  7, 13, 16, 26, 29, 42,
   3,   8, 12, 17, 25, 30, 41, 43,
   9,  11, 18, 24, 31, 40, 44, 53,
   10, 19, 23, 32, 39, 45, 52, 54,
   20, 22, 33, 38, 46, 51, 55, 60,
   21, 34, 37, 47, 50, 56, 59, 61,
   35, 36, 48, 49, 57, 58, 62, 63,
};

// Memory order as big endian. 0xhilo -> 0xlohi which looks as 0xhilo in memory.
static uint16_t pjei_be_word(const uint16_t le_word)
{
    uint8_t lo = (uint8_t)(le_word & 0x00ff);
    uint8_t hi = (uint8_t)((le_word & 0xff00) >> 8);
    return (((uint16_t)lo) << 8) | hi;
}

// ============================================================
// The following structs exist only for code clarity, debugability, and
// readability. They are used when writing to disk, but it is useful to have
// 1-packed-structs to document how the format works, and to inspect memory
// while developing.
// ============================================================

static const uint8_t pjeik_jfif_id[] = "JFIF";
static const uint8_t pjeik_com_str[] = "Created by Tiny JPEG Encoder";

// TODO: Get rid of packed structs!
#pragma pack(push)
#pragma pack(1)
typedef struct PJEJPEGHeader_s {
    uint16_t SOI;
    // JFIF header.
    uint16_t APP0;
    uint16_t jfif_len;
    uint8_t  jfif_id[5];
    uint16_t version;
    uint8_t  units;
    uint16_t x_density;
    uint16_t y_density;
    uint16_t thumb_size;

    // Our signature
    uint16_t com;
    uint16_t com_len;
    char     com_str[sizeof(pjeik_com_str) - 1];
} PJEJPEGHeader;

// Helper struct for PJEFrameHeader (below).
typedef struct PJEComponentSpec_s {
    uint8_t  component_id;
    uint8_t  sampling_factors;    // most significant 4 bits: horizontal. 4 LSB: vertical (A.1.1)
    uint8_t  qt;                  // Quantization table selector.
} PJEComponentSpec;

typedef struct PJEFrameHeader_s {
    uint16_t         SOF;
    uint16_t         len;                   // 8 + 3 * frame.num_components
    uint8_t          precision;             // Sample precision (bits per sample).
    uint16_t         height;
    uint16_t         width;
    uint8_t          num_components;        // For this implementation, will be equal to 3.
    PJEComponentSpec component_spec[3];
} PJEFrameHeader;

typedef struct PJEFrameComponentSpec_s {
    uint8_t component_id;                 // Just as with PJEComponentSpec
    uint8_t dc_ac;                        // (dc|ac)
} PJEFrameComponentSpec;

typedef struct PJEScanHeader_s {
    uint16_t              SOS;
    uint16_t              len;
    uint8_t               num_components;  // 3.
    PJEFrameComponentSpec component_spec[3];
    uint8_t               first;  // 0
    uint8_t               last;  // 63
    uint8_t               ah_al;  // o
} PJEScanHeader;
#pragma pack(pop)


void pjei_fwrite(PJEState* state, void* data, size_t num_bytes, size_t num_elements)
{
    size_t to_write = num_bytes * num_elements;

    // Cap to the buffer available size and copy memory.
    size_t capped_count = min(to_write, PJEI_BUFFER_SIZE - 1 - pjei_g_output_buffer_count);

    memcpy(pjei_g_output_buffer + pjei_g_output_buffer_count, data, capped_count);
    pjei_g_output_buffer_count += capped_count;

    assert (pjei_g_output_buffer_count <= PJEI_BUFFER_SIZE - 1);

    // Flush the buffer.
    if ( pjei_g_output_buffer_count == PJEI_BUFFER_SIZE - 1 ) {
        //fwrite(data, num_bytes, num_elements, state->fd);
        fwrite(pjei_g_output_buffer, pjei_g_output_buffer_count, 1, state->fd);
        pjei_g_output_buffer_count = 0;
    }

    // Recursively calling ourselves with the rest of the buffer.
    if (capped_count < to_write) {
        pjei_fwrite(state, (uint8_t*)data+capped_count, to_write - capped_count, 1);
    }
}

static void pjei_write_DQT(PJEState* state, uint8_t* matrix, uint8_t id)
{
    int16_t DQT = pjei_be_word(0xffdb);
    pje_write(state, &DQT, sizeof(int16_t), 1);
    int16_t len = pjei_be_word(0x0043); // 2(len) + 1(id) + 64(matrix) = 67 = 0x43
    pje_write(state, &len, sizeof(int16_t), 1);
    assert(id < 4);
    uint8_t precision_and_id = id;  // 0x0000 8 bits | 0x00id
    pje_write(state, &precision_and_id, sizeof(uint8_t), 1);
    // Write matrix
    pje_write(state, matrix, 64*sizeof(uint8_t), 1);
}

typedef enum {
    DC = 0,
    AC = 1
} PJEHuffmanTableClass;

static void pjei_write_DHT(PJEState* state,
                           uint8_t* matrix_len,
                           uint8_t* matrix_val,
                           PJEHuffmanTableClass ht_class,
                           uint8_t id)
{
    int num_values = 0;
    for ( int i = 0; i < 16; ++i ) {
        num_values += matrix_len[i];
    }
    assert(num_values <= 0xffff);

    int16_t DHT = pjei_be_word(0xffc4);
    // 2(len) + 1(Tc|th) + 16 (num lengths) + ?? (num values)
    uint16_t len = pjei_be_word(2 + 1 + 16 + (uint16_t)num_values);
    assert(id < 4);
    uint8_t tc_th = ((((uint8_t)ht_class) << 4) | id);

    pje_write(state, &DHT, sizeof(uint16_t), 1);
    pje_write(state, &len, sizeof(uint16_t), 1);
    pje_write(state, &tc_th, sizeof(uint8_t), 1);
    pje_write(state, matrix_len, sizeof(uint8_t), 16);
    pje_write(state, matrix_val, sizeof(uint8_t), num_values);
}
// ============================================================
//  Huffman deflation code.
// ============================================================

// Returns all code sizes from the BITS specification (JPEG C.3)
static uint8_t* pjei_huff_get_code_lengths(uint8_t huffsize[/*256*/], uint8_t* bits)
{
    int k = 0;
    for ( int i = 0; i < 16; ++i ) {
        for ( int j = 0; j < bits[i]; ++j ) {
            huffsize[k++] = (uint8_t)(i + 1);
        }
        huffsize[k] = 0;
    }
    return huffsize;
}

// Fills out the prefixes for each code.
static uint16_t* pjei_huff_get_codes(uint16_t codes[], uint8_t* huffsize, int64_t count)
{
    uint16_t code = 0;
    int k = 0;
    uint8_t sz = huffsize[0];
    for(;;) {
        do {
            assert(k < count);
            codes[k++] = code++;
        } while (huffsize[k] == sz);
        if (huffsize[k] == 0) {
            return codes;
        }
        do {
            code = code << 1;
            ++sz;
        } while( huffsize[k] != sz );
    }
}

static void pjei_huff_get_extended(uint8_t* out_ehuffsize,
                                   uint16_t* out_ehuffcode,
                                   uint8_t* huffval,
                                   uint8_t* huffsize,
                                   uint16_t* huffcode, int64_t count)
{
    int k = 0;
    do {
        uint8_t val = huffval[k];
        out_ehuffcode[val] = huffcode[k];
        out_ehuffsize[val] = huffsize[k];
        k++;
    } while ( k < count );
}
// ============================================================

// Returns:
//  out[1] : number of bits
//  out[0] : bits
PJEI_FORCE_INLINE void pjei_calculate_variable_length_int(int value, uint16_t out[2])
{
    int abs_val = value;
    if ( value < 0 ) {
        abs_val = -abs_val;
        --value;
    }
    out[1] = 1;
    while( abs_val >>= 1 ) {
        ++out[1];
    }
    out[0] = value & ((1 << out[1]) - 1);
}

// Write bits to file.
PJEI_FORCE_INLINE void pjei_write_bits(PJEState* state,
                                       uint32_t* bitbuffer, uint32_t* location,
                                       uint16_t num_bits, uint16_t bits)
{
    //   v-- location
    //  [                     ]   <-- bit buffer
    // 32                     0
    //
    // This call pushes to the bitbuffer and saves the location. Data is pushed
    // from most significant to less significant.
    // When we can write a full byte, we write a byte and shift.

    // Push the stack.
    uint32_t nloc = *location + num_bits;
    *bitbuffer |= (bits << (32 - nloc));
    *location = nloc;
    while ( *location >= 8 ) {
        // Grab the most significant byte.
        uint8_t c = (uint8_t)((*bitbuffer) >> 24);
        // Write it to file.
        pje_write(state, &c, 1, 1);
        if ( c == 0xff )  {
            // Special case: tell JPEG this is not a marker.
            char z = 0;
            pje_write(state, &z, 1, 1);
        }
        // Pop the stack.
        *bitbuffer <<= 8;
        *location -= 8;
    }
}

// DCT implementation by Thomas G. Lane.
// Obtained through NVIDIA
//  http://developer.download.nvidia.com/SDK/9.5/Samples/vidimaging_samples.html#gpgpu_dct
//
// QUOTE:
//  This implementation is based on Arai, Agui, and Nakajima's algorithm for
//  scaled DCT.  Their original paper (Trans. IEICE E-71(11):1095) is in
//  Japanese, but the algorithm is described in the Pennebaker & Mitchell
//  JPEG textbook (see REFERENCES section in file README).  The following code
//  is based directly on figure 4-8 in P&M.
//
void fdct (float * data)
{
    float tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    float tmp10, tmp11, tmp12, tmp13;
    float z1, z2, z3, z4, z5, z11, z13;
    float *dataptr;
    int ctr;

    /* Pass 1: process rows. */

    dataptr = data;
    for ( ctr = 7; ctr >= 0; ctr-- ) {
        tmp0 = dataptr[0] + dataptr[7];
        tmp7 = dataptr[0] - dataptr[7];
        tmp1 = dataptr[1] + dataptr[6];
        tmp6 = dataptr[1] - dataptr[6];
        tmp2 = dataptr[2] + dataptr[5];
        tmp5 = dataptr[2] - dataptr[5];
        tmp3 = dataptr[3] + dataptr[4];
        tmp4 = dataptr[3] - dataptr[4];

        /* Even part */

        tmp10 = tmp0 + tmp3;    /* phase 2 */
        tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2;
        tmp12 = tmp1 - tmp2;

        dataptr[0] = tmp10 + tmp11; /* phase 3 */
        dataptr[4] = tmp10 - tmp11;

        z1 = (tmp12 + tmp13) * ((float) 0.707106781); /* c4 */
        dataptr[2] = tmp13 + z1;    /* phase 5 */
        dataptr[6] = tmp13 - z1;

        /* Odd part */

        tmp10 = tmp4 + tmp5;    /* phase 2 */
        tmp11 = tmp5 + tmp6;
        tmp12 = tmp6 + tmp7;

        /* The rotator is modified from fig 4-8 to avoid extra negations. */
        z5 = (tmp10 - tmp12) * ((float) 0.382683433); /* c6 */
        z2 = ((float) 0.541196100) * tmp10 + z5; /* c2-c6 */
        z4 = ((float) 1.306562965) * tmp12 + z5; /* c2+c6 */
        z3 = tmp11 * ((float) 0.707106781); /* c4 */

        z11 = tmp7 + z3;        /* phase 5 */
        z13 = tmp7 - z3;

        dataptr[5] = z13 + z2;  /* phase 6 */
        dataptr[3] = z13 - z2;
        dataptr[1] = z11 + z4;
        dataptr[7] = z11 - z4;

        dataptr += 8;     /* advance pointer to next row */
    }

    /* Pass 2: process columns. */

    dataptr = data;
    for ( ctr = 8-1; ctr >= 0; ctr-- ) {
        tmp0 = dataptr[8*0] + dataptr[8*7];
        tmp7 = dataptr[8*0] - dataptr[8*7];
        tmp1 = dataptr[8*1] + dataptr[8*6];
        tmp6 = dataptr[8*1] - dataptr[8*6];
        tmp2 = dataptr[8*2] + dataptr[8*5];
        tmp5 = dataptr[8*2] - dataptr[8*5];
        tmp3 = dataptr[8*3] + dataptr[8*4];
        tmp4 = dataptr[8*3] - dataptr[8*4];

        /* Even part */

        tmp10 = tmp0 + tmp3;    /* phase 2 */
        tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2;
        tmp12 = tmp1 - tmp2;

        dataptr[8*0] = tmp10 + tmp11; /* phase 3 */
        dataptr[8*4] = tmp10 - tmp11;

        z1 = (tmp12 + tmp13) * ((float) 0.707106781); /* c4 */
        dataptr[8*2] = tmp13 + z1; /* phase 5 */
        dataptr[8*6] = tmp13 - z1;

        /* Odd part */

        tmp10 = tmp4 + tmp5;    /* phase 2 */
        tmp11 = tmp5 + tmp6;
        tmp12 = tmp6 + tmp7;

        /* The rotator is modified from fig 4-8 to avoid extra negations. */
        z5 = (tmp10 - tmp12) * ((float) 0.382683433); /* c6 */
        z2 = ((float) 0.541196100) * tmp10 + z5; /* c2-c6 */
        z4 = ((float) 1.306562965) * tmp12 + z5; /* c2+c6 */
        z3 = tmp11 * ((float) 0.707106781); /* c4 */

        z11 = tmp7 + z3;        /* phase 5 */
        z13 = tmp7 - z3;

        dataptr[8*5] = z13 + z2; /* phase 6 */
        dataptr[8*3] = z13 - z2;
        dataptr[8*1] = z11 + z4;
        dataptr[8*7] = z11 - z4;

        dataptr++;          /* advance pointer to next column */
    }
}

float slow_fdct(int u, int v, float* data)
{
#define kPI 3.14159265f
    float res = 0.0f;
    float cu = (u == 0) ? 0.70710678118654f : 1;
    float cv = (v == 0) ? 0.70710678118654f : 1;
    for ( int y = 0; y < 8; ++y ) {
        for ( int x = 0; x < 8; ++x ) {
            res += (data[y * 8 + x]) *
                    cosf(((2.0f * x + 1.0f) * u * kPI) / 16.0f) *
                    cosf(((2.0f * y + 1.0f) * v * kPI) / 16.0f);
        }
    }
    res *= 0.25f * cu * cv;
    return res;
#undef kPI
}

#define ABS(x) ((x) < 0 ? -(x) : (x))

static void pjei_encode_and_write_DU(PJEState* state,
                                     float* mcu,
#if PJE_USE_FAST_DCT
                                     float* qt,  // Pre-processed quantization matrix.
#else
                                     uint8_t* qt,
#endif
                                     uint8_t* huff_dc_len, uint16_t* huff_dc_code, // Huffman tables
                                     uint8_t* huff_ac_len, uint16_t* huff_ac_code,
                                     int* pred,  // Previous DC coefficient
                                     uint32_t* bitbuffer,  // Bitstack.
                                     uint32_t* location)
{
    int du[64];  // Data unit in zig-zag order

    float dct_mcu[64];
    memcpy(dct_mcu, mcu, 64 * sizeof(float));

#if PJE_USE_FAST_DCT
    fdct(dct_mcu);
    for ( int i = 0; i < 64; ++i ) {
        float fval = dct_mcu[i];
        fval *= qt[i];
#if 0
        fval = (fval > 0) ? floorf(fval + 0.5f) : ceilf(fval - 0.5f);
#else
        fval = floorf(fval + 1024 + 0.5f);
        fval -= 1024;
#endif
        int val = (int)fval;
        du[pjei_zig_zag[i]] = val;
    }
#else
    for ( int v = 0; v < 8; ++v ) {
        for ( int u = 0; u < 8; ++u ) {
            dct_mcu[v * 8 + u] = slow_fdct(u, v, mcu);
        }
    }
    for ( int i = 0; i < 64; ++i ) {
        float fval = dct_mcu[i] / (qt[i]);
        int val = (int)((fval > 0) ? floorf(fval + 0.5f) : ceilf(fval - 0.5f));
        du[pjei_zig_zag[i]] = val;
    }
#endif

    uint16_t vli[2];

    // Encode DC coefficient.
    int diff = du[0] - *pred;
    *pred = du[0];
    if ( diff != 0 ) {
        pjei_calculate_variable_length_int(diff, vli);
        // Write number of bits with Huffman coding
        pjei_write_bits(state, bitbuffer, location, huff_dc_len[vli[1]], huff_dc_code[vli[1]]);
        // Write the bits.
        pjei_write_bits(state, bitbuffer, location, vli[1], vli[0]);
    } else {
        pjei_write_bits(state, bitbuffer, location, huff_dc_len[0], huff_dc_code[0]);
    }

    // ==== Encode AC coefficients ====

    int last_non_zero_i = 0;
    // Find the last non-zero element.
    for ( int i = 63; i > 0; --i ) {
        if (du[i] != 0)
        {
            last_non_zero_i = i;
            break;
        }
    }

    for ( int i = 1; i <= last_non_zero_i; ++i ) {
        // If zero, increase count. If >=15, encode (FF,00)
        int zero_count = 0;
        while ( du[i] == 0 ) {
            ++zero_count;
            ++i;
            if (zero_count == 16) {
                // encode (ff,00) == 0xf0
                pjei_write_bits(state, bitbuffer, location, huff_ac_len[0xf0], huff_ac_code[0xf0]);
                zero_count = 0;
            }
        }
        pjei_calculate_variable_length_int(du[i], vli);

        assert(zero_count < 0x10);
        assert(vli[1] <= 10);

        uint16_t sym1 = ((uint16_t)zero_count << 4) | vli[1];

        assert(huff_ac_len[sym1] != 0);

        // Write symbol 1  --- (RUNLENGTH, SIZE)
        pjei_write_bits(state, bitbuffer, location, huff_ac_len[sym1], huff_ac_code[sym1]);
        // Write symbol 2  --- (AMPLITUDE)
        pjei_write_bits(state, bitbuffer, location, vli[1], vli[0]);
    }

    if (last_non_zero_i != 63) {
        // write EOB HUFF(00,00)
        pjei_write_bits(state, bitbuffer, location, huff_ac_len[0], huff_ac_code[0]);
    }
    return;
}

enum {
    LUMA_DC,
    LUMA_AC,
    CHROMA_DC,
    CHROMA_AC,
};

#if PJE_USE_FAST_DCT
struct PJEProcessedQT {
    float chroma[64];
    float luma[64];
};
#endif

// Set up huffman tables in state.
static void pjei_huff_expand (PJEState* state)
{
    assert(state);

    state->ht_bits[LUMA_DC]   = pjei_default_ht_luma_dc_len;
    state->ht_bits[LUMA_AC]   = pjei_default_ht_luma_ac_len;
    state->ht_bits[CHROMA_DC] = pjei_default_ht_chroma_dc_len;
    state->ht_bits[CHROMA_AC] = pjei_default_ht_chroma_ac_len;

    state->ht_vals[LUMA_DC]   = pjei_default_ht_luma_dc;
    state->ht_vals[LUMA_AC]   = pjei_default_ht_luma_ac;
    state->ht_vals[CHROMA_DC] = pjei_default_ht_chroma_dc;
    state->ht_vals[CHROMA_AC] = pjei_default_ht_chroma_ac;

    // How many codes in total for each of LUMA_(DC|AC) and CHROMA_(DC|AC)
    int32_t spec_tables_len[4] = { 0 };

    for ( int i = 0; i < 4; ++i ) {
        for ( int k = 0; k < 16; ++k ) {
            spec_tables_len[i] += state->ht_bits[i][k];
        }
    }

    // Fill out the extended tables..
    uint8_t huffsize[4][257];
    uint16_t huffcode[4][256];
    for ( int i = 0; i < 4; ++i ) {
        assert (256 >= spec_tables_len[i]);
        pjei_huff_get_code_lengths(huffsize[i], state->ht_bits[i]);
        pjei_huff_get_codes(huffcode[i], huffsize[i], spec_tables_len[i]);
    }
    for ( int i = 0; i < 4; ++i ) {
        int64_t count = spec_tables_len[i];
        pjei_huff_get_extended(state->ehuffsize[i],
                               state->ehuffcode[i],
                               state->ht_vals[i],
                               &huffsize[i][0],
                               &huffcode[i][0], count);
    }
}

static int pjei_encode_main(PJEState* state,
                            const unsigned char* src_data,
                            const int width,
                            const int height,
                            const int src_num_components)
{
    if (src_num_components != 3 && src_num_components != 4) {
        return 0;
    }

    if (width > 0xffff || height > 0xffff) {
        return 0;
    }

#if PJE_USE_FAST_DCT
    struct PJEProcessedQT pqt;
    // Again, taken from classic japanese implementation.
    //
    /* For float AA&N IDCT method, divisors are equal to quantization
     * coefficients scaled by scalefactor[row]*scalefactor[col], where
     *   scalefactor[0] = 1
     *   scalefactor[k] = cos(k*PI/16) * sqrt(2)    for k=1..7
     * We apply a further scale factor of 8.
     * What's actually stored is 1/divisor so that the inner loop can
     * use a multiplication rather than a division.
     */
    static const float aan_scales[] = {
        1.0f, 1.387039845f, 1.306562965f, 1.175875602f,
        1.0f, 0.785694958f, 0.541196100f, 0.275899379f
    };

    // build (de)quantization tables
    for(int y=0; y<8; y++) {
        for(int x=0; x<8; x++) {
            int i = y*8 + x;
            pqt.luma[y*8+x] = 1.0f / (8 * aan_scales[x] * aan_scales[y] * state->qt_luma[pjei_zig_zag[i]]);
            pqt.chroma[y*8+x] = 1.0f / (8 * aan_scales[x] * aan_scales[y] * state->qt_chroma[pjei_zig_zag[i]]);
        }
    }
#endif

    { // Write header
        PJEJPEGHeader header;
        // JFIF header.
        header.SOI = pjei_be_word(0xffd8);  // Sequential DCT
        header.APP0 = pjei_be_word(0xffe0);
        header.jfif_len = pjei_be_word(0x0016);
        memcpy(header.jfif_id, (void*)pjeik_jfif_id, 5);
        header.version = pjei_be_word(0x0102);
        //header.units = 0x01;
        //header.x_density = pjei_be_word(0x0060);  // 96 DPI
        //header.y_density = pjei_be_word(0x0060);  // 96 DPI
        header.units = 0x00;
        header.x_density = pjei_be_word(0x0000);
        header.y_density = pjei_be_word(0x0000);
        header.thumb_size = 0;
        /* // Comment */
        header.com = pjei_be_word(0xfffe);
        header.com_len = 16 + sizeof(pjeik_com_str) - 1;
        memcpy(header.com_str, (void*)pjeik_com_str, sizeof(pjeik_com_str) - 1); // Skip the 0-bit
        pje_write(state, &header, sizeof(PJEJPEGHeader), 1);
    }

    // Write quantization tables.
    pjei_write_DQT(state, state->qt_luma, 0x00);
    pjei_write_DQT(state, state->qt_chroma, 0x01);

    {  // Write the frame marker.
        PJEFrameHeader header;
        header.SOF = pjei_be_word(0xffc0);
        header.len = pjei_be_word(8 + 3 * 3);
        header.precision = 8;
        assert(width <= 0xffff);
        assert(height <= 0xffff);
        header.width = pjei_be_word((uint16_t)width);
        header.height = pjei_be_word((uint16_t)height);
        header.num_components = 3;
        uint8_t tables[3] = {
            0,  // Luma component gets luma table (see pjei_write_DQT call above.)
            1,  // Chroma component gets chroma table
            1,  // Chroma component gets chroma table
        };
        for (int i = 0; i < 3; ++i) {
            PJEComponentSpec spec;
            spec.component_id = (uint8_t)(i + 1);  // No particular reason. Just 1, 2, 3.
            spec.sampling_factors = (uint8_t)0x11;
            spec.qt = tables[i];

            header.component_spec[i] = spec;
        }
        // Write to file.
        pje_write(state, &header, sizeof(PJEFrameHeader), 1);
    }

    pjei_write_DHT(state, state->ht_bits[LUMA_DC], state->ht_vals[LUMA_DC], DC, 0);
    pjei_write_DHT(state, state->ht_bits[LUMA_AC], state->ht_vals[LUMA_AC], AC, 0);
    pjei_write_DHT(state, state->ht_bits[CHROMA_DC], state->ht_vals[CHROMA_DC], DC, 1);
    pjei_write_DHT(state, state->ht_bits[CHROMA_AC], state->ht_vals[CHROMA_AC], AC, 1);

    // Write start of scan
    {
        PJEScanHeader header;
        header.SOS = pjei_be_word(0xffda);
        header.len = pjei_be_word((uint16_t)(6 + (2 * 3)));
        header.num_components = 3;

        uint8_t tables[3] = {
            0x00,
            0x11,
            0x11,
        };
        for (int i = 0; i < 3; ++i) {
            PJEFrameComponentSpec cs;
            // Must be equal to component_id from frame header above.
            cs.component_id = (uint8_t)(i + 1);
            cs.dc_ac = (uint8_t)tables[i];

            header.component_spec[i] = cs;
        }
        header.first = 0;
        header.last  = 63;
        header.ah_al = 0;
        pje_write(state, &header, sizeof(PJEScanHeader), 1);

    }
    // Write compressed data.

    // Set diff to 0.
    int pred_y = 0;
    int pred_b = 0;
    int pred_r = 0;

    // Bit stack
    uint32_t bitbuffer = 0;
    uint32_t location = 0;


    // Number of blocks.

    int w_cap = (width % 8 == 0) ? width : (width + (8 - width % 8));
    int h_cap = (height % 8 == 0) ? height : (height + (8 - height % 8));
    int num_blocks = src_num_components * w_cap * h_cap / 64;

    PJEBlock* y_blocks = arena_alloc_array(state->arena, num_blocks/3, PJEBlock);
    PJEBlock* u_blocks = arena_alloc_array(state->arena, num_blocks/3, PJEBlock);
    PJEBlock* v_blocks = arena_alloc_array(state->arena, num_blocks/3, PJEBlock);

    uint32_t block_i = 0;
    for ( int y = 0; y < height; y += 8 ) {
        for ( int x = 0; x < width; x += 8 ) {
            // Block loop: ====
            for ( int off_y = 0; off_y < 8; ++off_y ) {
                for ( int off_x = 0; off_x < 8; ++off_x ) {
                    int block_index = (off_y * 8 + off_x);

                    int src_index = (((y + off_y) * width) + (x + off_x)) * src_num_components;

                    int col = x + off_x;
                    int row = y + off_y;

                    if(row >= height) {
                        src_index -= (width * (row - height + 1)) * src_num_components;
                    }
                    if(col >= width) {
                        src_index -= (col - width + 1) * src_num_components;
                    }
                    assert(src_index < width * height * src_num_components);

                    uint8_t r = src_data[src_index + 0];
                    uint8_t g = src_data[src_index + 1];
                    uint8_t b = src_data[src_index + 2];

                    float luma = 0.299f   * r + 0.587f    * g + 0.114f    * b - 128;
                    float cb   = -0.1687f * r - 0.3313f   * g + 0.5f      * b;
                    float cr   = 0.5f     * r - 0.4187f   * g - 0.0813f   * b;

                    // TODO: measure. This could result in many cache misses in the name of parallelization.
                    y_blocks[block_i].d[block_index] = luma;
                    u_blocks[block_i].d[block_index] = cb;
                    v_blocks[block_i].d[block_index] = cr;
                }
            }
            block_i++;
        }
    }

    for ( int bi = 0; bi < num_blocks / 3; ++bi ) {
        float* y_block = y_blocks[bi].d;
        float* u_block = u_blocks[bi].d;
        float* v_block = v_blocks[bi].d;
        pjei_encode_and_write_DU(state, y_block,
                                 pqt.luma,
                                 state->ehuffsize[LUMA_DC], state->ehuffcode[LUMA_DC],
                                 state->ehuffsize[LUMA_AC], state->ehuffcode[LUMA_AC],
                                 &pred_y, &bitbuffer, &location);
        pjei_encode_and_write_DU(state, u_block,
                                 pqt.chroma,
                                 state->ehuffsize[CHROMA_DC], state->ehuffcode[CHROMA_DC],
                                 state->ehuffsize[CHROMA_AC], state->ehuffcode[CHROMA_AC],
                                 &pred_b, &bitbuffer, &location);
        pjei_encode_and_write_DU(state, v_block,
                                 pqt.chroma,
                                 state->ehuffsize[CHROMA_DC], state->ehuffcode[CHROMA_DC],
                                 state->ehuffsize[CHROMA_AC], state->ehuffcode[CHROMA_AC],
                                 &pred_r, &bitbuffer, &location);
    }

    // Finish the image.
    { // Flush
        while (location != 0) {
            pjei_write_bits(state, &bitbuffer, &location, (uint16_t)(8 - location), 0xff);
        }
    }
    uint16_t EOI = pjei_be_word(0xffd9);
    pje_write(state, &EOI, sizeof(uint16_t), 1);

    // If compiled with buffered IO
#if defined(PJEI_BUFFER_SIZE)
    if (pjei_g_output_buffer_count) {
        fwrite(pjei_g_output_buffer, pjei_g_output_buffer_count, 1, state->fd);
    }
#endif

    return 1;
}

int pje_encode_to_file(const char* dest_path,
                       const int width,
                       const int height,
                       const int num_components,
                       const unsigned char* src_data,
                       void* memory, size_t size)
{
    int res = pje_encode_to_file_at_quality(dest_path, 3, width, height, num_components, src_data, memory, size);
    return res;
}

// Define public interface.
int pje_encode_to_file_at_quality(const char* dest_path,
                                  const int quality,
                                  const int width,
                                  const int height,
                                  const int num_components,
                                  const unsigned char* src_data,
                                  void* memory, size_t size)
{
    FILE* fd = fopen(dest_path, "wb");
    if (!fd) {
        pje_log("Could not open file for writing.");
        return 0;
    }

    if (quality < 1 || quality > 3) {
        pje_log("[ERROR] -- Valid 'quality' values are 1 (lowest), 2, or 3 (highest)\n");
        return 0;
    }

    PJEState state = { 0 };

    state.fd = fd;

    uint8_t qt_factor = 1;
    switch(quality) {
    case 3:
        for (int i = 0; i < 64; ++ i) {
            state.qt_luma[i]   = 1;
            state.qt_chroma[i] = 1;
        }
        break;
    case 2:
        qt_factor = 10;
        // don't break. fall through.
    case 1:
        for ( int i = 0; i < 64; ++i ) {
            state.qt_luma[i]   = pjei_default_qt_luma_from_spec[i] / qt_factor;
            if (state.qt_luma[i] == 0) {
                state.qt_luma[i] = 1;
            }
            state.qt_chroma[i] = pjei_default_qt_chroma_from_paper[i] / qt_factor;
            if (state.qt_chroma[i] == 0) {
                state.qt_chroma[i] = 1;
            }
        }
        break;
    default:
        assert(!"invalid code path");
        break;
    }


    pjei_huff_expand(&state);


    // Setup the arena.
    Arena encoder_arena = arena_init(memory, size);
    state.arena = &encoder_arena;


    int result = pjei_encode_main(&state, src_data, width, height, num_components);

    result |= 0 == fclose(fd);

    return result;
}

#include "gpu.c"

int main()
{
    GPUInfo gpu_info;
    if ( !gpu_init(&gpu_info)) {
        sgl_log("Could not init GPGPU.\n");
        exit(EXIT_FAILURE);
    }

    size_t memsz = 1L * 1024 * 1024 * 1024;
    Arena root_arena = arena_init(sgl_calloc(memsz, 1), memsz);

    if ( !root_arena.ptr ) {
        sgl_log("Can't allocate memory. Exiting\n");
        exit(EXIT_FAILURE);
    }


    int w, h, ncomp;
    //unsigned char* data = stbi_load("pluto.bmp", &w, &h, &ncomp, 0);

    unsigned char* data = stbi_load("in.bmp", &w, &h, &ncomp, 0);
    if ( !data ) {
        puts("Could not load file");
        return EXIT_FAILURE;
    }


    size_t size = (size_t)2 * 1024 * 1024 * 1024;
    void* memory = sgl_calloc(size, 1);
    if ( !memory ) {
        sgl_log("Could not allocate enough memory for the parallel encoder.n");
        return EXIT_FAILURE;
    }

    // Highest quality by default.
    if ( !pje_encode_to_file("parallel_out.jpg", w, h, ncomp, data, memory, size) ) {
        return EXIT_FAILURE;
    }
/*
    data = stbi_load("out.jpg", &w, &h, &ncomp, 0);
    if (!data) {
        return EXIT_FAILURE;
    }

    // Q3 -- Highest quality
    if ( 0 == tje_encode_to_file_at_quality("out_q3.jpg", 3, w, h, ncomp, data) ) {
        return EXIT_FAILURE;
    }

    // Q2 -- Almost as good as 3, about twice as much compression.
    if ( 0 == tje_encode_to_file_at_quality("out_q2.jpg", 2, w, h, ncomp, data) ) {
        return EXIT_FAILURE;
    }

    // Q1 -- About 3 times smaller than Q2. Noticeable artifacts.
    if ( 0 == tje_encode_to_file_at_quality("out_q1.jpg", 1, w, h, ncomp, data) ) {
        return EXIT_FAILURE;
    }
    */
    stbi_image_free(data);

    return EXIT_SUCCESS;
}

