/**
 * dummy_jpeg.h
 *  - Sergio Gonzalez
 *
 *  This is a modification of tiny_jpeg so that:
 *      - Data is transformed in a GPU-friendly way.
 *      - Nothing gets written. It does only the necessary work to calculate
 *          the size and error of the JPEG that would result.
 *
 */


#pragma once

#include "dje_common.h"

#include "gpu.h"

#define DJE_MULTITHREADED 1


typedef struct DJEProcessedQT_s {
    float chroma[64];
    float luma[64];
} DJEProcessedQT;

typedef struct DJEState_s {
    uint8_t     ehuffsize[4][257];
    uint16_t    ehuffcode[4][256];

    uint8_t*    ht_bits[4];
    uint8_t*    ht_vals[4];

    uint8_t     qt_luma[64];
    uint8_t     qt_chroma[64];

    // Stuff that persists accross multiple calls.
    Arena*          arena;
    DJEProcessedQT  pqt;
    DJEBlock*       y_blocks;
    int             num_blocks;

    // Result stuff
    uint32_t    bit_count;  // Instead of writing, we increase this value.
    uint64_t    mse;        // Mean square error.
} DJEState;


// ============================================================
// Internal
// ============================================================
#ifdef DJE_IMPLEMENTATION


#if defined(_MSC_VER)
#define DJEI_FORCE_INLINE __forceinline
// #define DJEI_FORCE_INLINE __declspec(noinline)  // For profiling
#else
#define DJEI_FORCE_INLINE static // TODO: equivalent for gcc & clang
#endif

// Only use zero for debugging and/or inspection.
#define DJE_USE_FAST_DCT 1

// C std lib
#include <assert.h>
#include <inttypes.h>
#include <math.h>   // floorf, ceilf
#include <stdio.h>  // FILE, puts
#include <string.h> // memcpy


#if !defined(dje_write)

#define DJEI_BUFFER_SIZE 1024
#define dje_write djei_write

// Buffer DJE_BUFFER_SIZE in memory and flush when ready
static size_t djei_g_output_buffer_count;
static uint8_t djei_g_output_buffer[DJEI_BUFFER_SIZE];

#endif


#ifdef _WIN32

#include <windows.h>
#define snprintf sprintf_s
// Not quite the same but it works for us. If I am not mistaken, it differs
// only in the return value.

#endif

#ifndef NDEBUG

#ifdef _WIN32
#define dje_log(msg) OutputDebugStringA(msg)
#elif defined(__linux__) || defined(__MACH__)
#define dje_log(msg) puts(msg)
#endif

#else  // NDEBUG
#define dje_log(msg)
#endif  // NDEBUG


#define DJE_UNUSED(v) (void*)&v;

// ============================================================
// Table definitions.
//
// The spec defines djei_default reasonably good quantization matrices and huffman
// specification tables.
//
//
// Instead of hard-coding the final huffman table, we only hard-code the table
// spec suggested by the specification, and then derive the full table from
// there.  This is only for didactic purposes but it might be useful if there
// ever is the case that we need to swap huffman tables from various sources.
// ============================================================


// K.1 - suggested luminance QT
static uint8_t djei_default_qt_luma_from_spec[] = {
   16,11,10,16, 24, 40, 51, 61,
   12,12,14,19, 26, 58, 60, 55,
   14,13,16,24, 40, 57, 69, 56,
   14,17,22,29, 51, 87, 80, 62,
   18,22,37,56, 68,109,103, 77,
   24,35,55,64, 81,104,113, 92,
   49,64,78,87,103,121,120,101,
   72,92,95,98,112,100,103, 99,
};

static uint8_t djei_default_qt_chroma_from_spec[] = {
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

static uint8_t djei_default_qt_chroma_from_paper[] = {
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
static uint8_t djei_default_ht_luma_dc_len[16] = {
    0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0
};
// values
static uint8_t djei_default_ht_luma_dc[12] = {
    0,1,2,3,4,5,6,7,8,9,10,11
};

// Number of 16 bit values for every code length. (K.3.3.1)
static uint8_t djei_default_ht_chroma_dc_len[16] = {
    0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0
};
// values
static uint8_t djei_default_ht_chroma_dc[12] = {
    0,1,2,3,4,5,6,7,8,9,10,11
};

// Same as above, but AC coefficients.
static uint8_t djei_default_ht_luma_ac_len[16] = {
    0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d
};
static uint8_t djei_default_ht_luma_ac[] = {
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

static uint8_t djei_default_ht_chroma_ac_len[16] = {
    0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,0x77
};
static uint8_t djei_default_ht_chroma_ac[] = {
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

// Memory order as big endian. 0xhilo -> 0xlohi which looks as 0xhilo in memory.
static uint16_t djei_be_word(const uint16_t le_word)
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

static const uint8_t djeik_jfif_id[] = "JFIF";
static const uint8_t djeik_com_str[] = "Created by Tiny JPEG Encoder";

// TODO: Get rid of packed structs!
#pragma pack(push)
#pragma pack(1)
typedef struct DJEJPEGHeader_s {
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
    char     com_str[sizeof(djeik_com_str) - 1];
} DJEJPEGHeader;

// Helper struct for DJEFrameHeader (below).
typedef struct DJEComponentSpec_s {
    uint8_t  component_id;
    uint8_t  sampling_factors;    // most significant 4 bits: horizontal. 4 LSB: vertical (A.1.1)
    uint8_t  qt;                  // Quantization table selector.
} DJEComponentSpec;

typedef struct DJEFrameHeader_s {
    uint16_t         SOF;
    uint16_t         len;                   // 8 + 3 * frame.num_components
    uint8_t          precision;             // Sample precision (bits per sample).
    uint16_t         height;
    uint16_t         width;
    uint8_t          num_components;        // For this implementation, will be equal to 3.
    DJEComponentSpec component_spec[3];
} DJEFrameHeader;

typedef struct DJEFrameComponentSpec_s {
    uint8_t component_id;                 // Just as with DJEComponentSpec
    uint8_t dc_ac;                        // (dc|ac)
} DJEFrameComponentSpec;

typedef struct DJEScanHeader_s {
    uint16_t              SOS;
    uint16_t              len;
    uint8_t               num_components;  // 3.
    DJEFrameComponentSpec component_spec[3];
    uint8_t               first;  // 0
    uint8_t               last;  // 63
    uint8_t               ah_al;  // o
} DJEScanHeader;
#pragma pack(pop)


void djei_write(DJEState* state, void* data, uint32_t num_bytes, size_t num_elements)
{
    DJE_UNUSED(data);
    DJE_UNUSED(num_elements);
    state->bit_count += num_bytes * 8;
}

static void djei_write_DQT(DJEState* state, uint8_t* matrix, uint8_t id)
{
    int16_t DQT = djei_be_word(0xffdb);
    dje_write(state, &DQT, sizeof(int16_t), 1);
    int16_t len = djei_be_word(0x0043); // 2(len) + 1(id) + 64(matrix) = 67 = 0x43
    dje_write(state, &len, sizeof(int16_t), 1);
    assert(id < 4);
    uint8_t precision_and_id = id;  // 0x0000 8 bits | 0x00id
    dje_write(state, &precision_and_id, sizeof(uint8_t), 1);
    // Write matrix
    dje_write(state, matrix, 64*sizeof(uint8_t), 1);
}

typedef enum {
    DC = 0,
    AC = 1
} DJEHuffmanTableClass;

static void djei_write_DHT(DJEState* state,
                           uint8_t* matrix_len,
                           uint8_t* matrix_val,
                           DJEHuffmanTableClass ht_class,
                           uint8_t id)
{
    int num_values = 0;
    for ( int i = 0; i < 16; ++i ) {
        num_values += matrix_len[i];
    }
    assert(num_values <= 0xffff);

    int16_t DHT = djei_be_word(0xffc4);
    // 2(len) + 1(Tc|th) + 16 (num lengths) + ?? (num values)
    uint16_t len = djei_be_word(2 + 1 + 16 + (uint16_t)num_values);
    assert(id < 4);
    uint8_t tc_th = ((((uint8_t)ht_class) << 4) | id);

    dje_write(state, &DHT, sizeof(uint16_t), 1);
    dje_write(state, &len, sizeof(uint16_t), 1);
    dje_write(state, &tc_th, sizeof(uint8_t), 1);
    dje_write(state, matrix_len, sizeof(uint8_t), 16);
    dje_write(state, matrix_val, sizeof(uint8_t), num_values);
}
// ============================================================
//  Huffman deflation code.
// ============================================================

// Returns all code sizes from the BITS specification (JPEG C.3)
static uint8_t* djei_huff_get_code_lengths(uint8_t huffsize[/*256*/], uint8_t* bits)
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
static uint16_t* djei_huff_get_codes(uint16_t codes[], uint8_t* huffsize, int64_t count)
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

static void djei_huff_get_extended(uint8_t* out_ehuffsize,
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
DJEI_FORCE_INLINE void djei_calculate_variable_length_int(int value, uint16_t out[2])
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

static void djei_encode_and_write_MCU(int block_i,
                                      DJEBlock* mcu_array,
                                      uint32_t* bitcount_array,
                                      uint64_t* out_mse,
#if DJE_USE_FAST_DCT
                                      float* qt,  // Pre-processed quantization matrix.
#else
                                      uint8_t* qt,
#endif
                                      // Huffman tables
                                      uint8_t* huff_ac_len, uint16_t* huff_ac_code)
{
    DJE_UNUSED(huff_ac_code);
    float* mcu = mcu_array[block_i].d;
    int16_t du[64];  // Data unit in zig-zag order

    float dct_mcu[64];
    memcpy(dct_mcu, mcu, 64 * sizeof(float));

#if DJE_USE_FAST_DCT
    fdct(dct_mcu);
    for ( int i = 0; i < 64; ++i ) {
        float fval = dct_mcu[i];
        fval *= qt[i];
        assert(fval >= -1024 && fval < 1024.0f);
#if 0
        fval = (fval > 0) ? floorf(fval + 0.5f) : ceilf(fval - 0.5f);
#else
        fval = floorf(fval + 1024 + 0.5f);
        fval -= 1024;
#endif
        int16_t val = (int16_t)fval;
        du[djei_zig_zag[i]] = val;
    }
#else
    for ( int v = 0; v < 8; ++v ) {
        for ( int u = 0; u < 8; ++u ) {
            dct_mcu[v * 8 + u] = slow_fdct(u, v, mcu);
        }
    }
    for ( int i = 0; i < 64; ++i ) {
        float fval = dct_mcu[i] / (qt[i]);
        int16_t val = (int16_t)((fval > 0) ? floorf(fval + 0.5f) : ceilf(fval - 0.5f));
        du[djei_zig_zag[i]] = val;
    }
#endif

    uint8_t decomp[64];
    float re[64];  // Reconstructed image =)

    // Note: stb uses w_cap as out_stride..
    idct_block(decomp, 8, du);
    for ( int id = 0; id < 64; ++id )
        re[id] = (float)decomp[id];

    uint64_t MSE = 0;

    for ( int i = 0; i < 64; ++i ) {
        int32_t re_i = (int32_t)(re[i]);
        int32_t mcu_i = (int32_t)(mcu[i] + 128);
        int32_t err = ABS(re_i - mcu_i);
        MSE += err;
    }

    out_mse[block_i] = MSE;


    uint16_t vli[2];

#if 0
    // Encode DC coefficient.
    int diff = du[0] - *pred;
    *pred = du[0];
    if ( diff != 0 ) {
        djei_calculate_variable_length_int(diff, vli);
        // Write number of bits with Huffman coding
        djei_write_bits(state, bitbuffer, location, huff_dc_len[vli[1]], huff_dc_code[vli[1]]);
        // Write the bits.
        djei_write_bits(state, bitbuffer, location, vli[1], vli[0]);
    } else {
        djei_write_bits(state, bitbuffer, location, huff_dc_len[0], huff_dc_code[0]);
    }
#else
    // By ignoring the DC coefficient, we are removing a data dependency between blocks.
    //
    // As a result, there will be an under-reporting error in the result, and
    // the evolution will be blind to this.  That said, the mean square error
    // reporting will still be accurate, so it might not be a problem.
    //
    // An alternative approach is to treat the DC coefficient as an AC
    // coefficient, but there is a problem: there might not be a valid huffman
    // code. For now, we will ignore the coefficient and hope that the
    // file-size under-reporting is not a problem, given that the Error
    // calculation is still correct.
#endif

    // ==== Encode AC coefficients ====

    int last_non_zero_i = 0;
    // Find the last non-zero element.
    for ( int i = 63; i > 0; --i ) {
        if (du[i] != 0) {
            last_non_zero_i = i;
            break;
        }
    }

    // We are starting from zero, because delta-encoding the DC coefficient
    // introduces a data dependency.
    // We would rather have an algorithm that is no longer JPEG but that is
    // data parallel and that will help us generate a good table.
    for ( int i = 1; i <= last_non_zero_i; ++i ) {
        // If zero, increase count. If >=15, encode (FF,00)
        int zero_count = 0;
        while ( du[i] == 0 ) {
            ++zero_count;
            ++i;
            if (zero_count == 16) {
                // encode (ff,00) == 0xf0
                //djei_write_bits(state, bitbuffer, location, huff_ac_len[0xf0], huff_ac_code[0xf0]);
                bitcount_array[block_i] += huff_ac_len[0xf0];
                zero_count = 0;
            }
        }


        djei_calculate_variable_length_int(du[i], vli);

        if ( i == 0 && vli[1] >= 10) {
            // There is no code for this coefficient. ignore.
            continue;
        }

        assert(zero_count < 0x10);
        assert(vli[1] <= 10);

        uint16_t sym1 = ((uint16_t)zero_count << 4) | vli[1];

        assert(huff_ac_len[sym1] != 0);

        // Write symbol 1  --- (RUNLENGTH, SIZE)
        //djei_write_bits(state, bitbuffer, location, huff_ac_len[sym1], huff_ac_code[sym1]);
        bitcount_array[block_i] += huff_ac_len[sym1];
        // Write symbol 2  --- (AMPLITUDE)
        //djei_write_bits(state, bitbuffer, location, vli[1], vli[0]);
        bitcount_array[block_i] += vli[1];
    }

    if (last_non_zero_i != 63) {
        // write EOB HUFF(00,00)
        //djei_write_bits(state, bitbuffer, location, huff_ac_len[0], huff_ac_code[0]);
        bitcount_array[block_i] += huff_ac_len[0];
    }
    return;
}

enum {
    LUMA_DC,
    LUMA_AC,
    CHROMA_DC,
    CHROMA_AC,
};

// Set up huffman tables in state.
static void djei_huff_expand (DJEState* state)
{
    assert(state);

    state->ht_bits[LUMA_DC]   = djei_default_ht_luma_dc_len;
    state->ht_bits[LUMA_AC]   = djei_default_ht_luma_ac_len;
    state->ht_bits[CHROMA_DC] = djei_default_ht_chroma_dc_len;
    state->ht_bits[CHROMA_AC] = djei_default_ht_chroma_ac_len;

    state->ht_vals[LUMA_DC]   = djei_default_ht_luma_dc;
    state->ht_vals[LUMA_AC]   = djei_default_ht_luma_ac;
    state->ht_vals[CHROMA_DC] = djei_default_ht_chroma_dc;
    state->ht_vals[CHROMA_AC] = djei_default_ht_chroma_ac;

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
        djei_huff_get_code_lengths(huffsize[i], state->ht_bits[i]);
        djei_huff_get_codes(huffcode[i], huffsize[i], spec_tables_len[i]);
    }
    for ( int i = 0; i < 4; ++i ) {
        int64_t count = spec_tables_len[i];
        djei_huff_get_extended(state->ehuffsize[i],
                               state->ehuffcode[i],
                               state->ht_vals[i],
                               &huffsize[i][0],
                               &huffcode[i][0], count);
    }
}

static int djei_encode_prelude(DJEState* state,
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

    { // Write header
        DJEJPEGHeader header;
        // JFIF header.
        header.SOI = djei_be_word(0xffd8);  // Sequential DCT
        header.APP0 = djei_be_word(0xffe0);
        header.jfif_len = djei_be_word(0x0016);
        memcpy(header.jfif_id, (void*)djeik_jfif_id, 5);
        header.version = djei_be_word(0x0102);
        //header.units = 0x01;
        //header.x_density = djei_be_word(0x0060);  // 96 DPI
        //header.y_density = djei_be_word(0x0060);  // 96 DPI
        header.units = 0x00;
        header.x_density = djei_be_word(0x0000);
        header.y_density = djei_be_word(0x0000);
        header.thumb_size = 0;
        /* // Comment */
        header.com = djei_be_word(0xfffe);
        header.com_len = 16 + sizeof(djeik_com_str) - 1;
        memcpy(header.com_str, (void*)djeik_com_str, sizeof(djeik_com_str) - 1); // Skip the 0-bit
        dje_write(state, &header, sizeof(DJEJPEGHeader), 1);
    }

    // Write quantization tables.
    //
    // NOTE: !!!
    //
    // At this point, there is garbage stored in qt_luma and qt_chroma. For
    // dummy_jpeg it doesn't matter. The reason that it is garbage is that we
    // don't want to change the data order from the real jpeg encoder but we
    // don't want to define a qt table yet. The qt tables will be defined on a
    // per-call basis by dje_encode_main.
    djei_write_DQT(state, state->qt_luma, 0x00);
    djei_write_DQT(state, state->qt_chroma, 0x01);

    {  // Write the frame marker.
        DJEFrameHeader header;
        header.SOF = djei_be_word(0xffc0);
        header.len = djei_be_word(8 + 3 * 3);
        header.precision = 8;
        assert(width <= 0xffff);
        assert(height <= 0xffff);
        header.width = djei_be_word((uint16_t)width);
        header.height = djei_be_word((uint16_t)height);
        header.num_components = 3;
        uint8_t tables[3] = {
            0,  // Luma component gets luma table (see djei_write_DQT call above.)
            1,  // Chroma component gets chroma table
            1,  // Chroma component gets chroma table
        };
        for (int i = 0; i < 3; ++i) {
            DJEComponentSpec spec;
            spec.component_id = (uint8_t)(i + 1);  // No particular reason. Just 1, 2, 3.
            spec.sampling_factors = (uint8_t)0x11;
            spec.qt = tables[i];

            header.component_spec[i] = spec;
        }
        // Write to file.
        dje_write(state, &header, sizeof(DJEFrameHeader), 1);
    }

    djei_write_DHT(state, state->ht_bits[LUMA_DC], state->ht_vals[LUMA_DC], DC, 0);
    djei_write_DHT(state, state->ht_bits[LUMA_AC], state->ht_vals[LUMA_AC], AC, 0);
    djei_write_DHT(state, state->ht_bits[CHROMA_DC], state->ht_vals[CHROMA_DC], DC, 1);
    djei_write_DHT(state, state->ht_bits[CHROMA_AC], state->ht_vals[CHROMA_AC], AC, 1);

    // Write start of scan
    {
        DJEScanHeader header;
        header.SOS = djei_be_word(0xffda);
        header.len = djei_be_word((uint16_t)(6 + (2 * 3)));
        header.num_components = 3;

        uint8_t tables[3] = {
            0x00,
            0x11,
            0x11,
        };
        for (int i = 0; i < 3; ++i) {
            DJEFrameComponentSpec cs;
            // Must be equal to component_id from frame header above.
            cs.component_id = (uint8_t)(i + 1);
            cs.dc_ac = (uint8_t)tables[i];

            header.component_spec[i] = cs;
        }
        header.first = 0;
        header.last  = 63;
        header.ah_al = 0;
        dje_write(state, &header, sizeof(DJEScanHeader), 1);

    }
    // Write compressed data.

    int w_cap = (width % 8 == 0) ? width : (width + (8 - width % 8));
    int h_cap = (height % 8 == 0) ? height : (height + (8 - height % 8));

    int num_blocks = w_cap * h_cap / 64;
    state->num_blocks = num_blocks;

    DJEBlock* y_blocks = arena_alloc_array(state->arena, num_blocks, DJEBlock);
#if 0
    DJEBlock* u_blocks = arena_alloc_array(state->arena, num_blocks, DJEBlock);
    DJEBlock* v_blocks = arena_alloc_array(state->arena, num_blocks, DJEBlock);
#endif

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

                    // TODO: measure. This could result in many cache misses in the name of parallelization.
                    y_blocks[block_i].d[block_index] = luma;
#if 0
                    u_blocks[block_i].d[block_index] = cb;
                    v_blocks[block_i].d[block_index] = cr;
#endif
                }
            }
            block_i++;
        }
    }
    state->y_blocks = y_blocks;
    return 1;
}

static SglMutex* work_queue_mutex;
static volatile int32_t work_done;

struct global_work_data {
    uint64_t* mse;
    DJEBlock* y_blocks;
    uint32_t* bitcount_array;
    DJEState* state;
    uint32_t  num_blocks;
};
static struct global_work_data* gwd;

void dje_worker_thread(void* data)
{
    DJE_UNUSED(data);
    for(;;) {
        sgl_mutex_lock(work_queue_mutex);
        volatile uint32_t bi =  work_done++;
        sgl_mutex_unlock(work_queue_mutex);
        if (bi < gwd->num_blocks) {
            djei_encode_and_write_MCU(bi, gwd->y_blocks, gwd->bitcount_array, gwd->mse,
#if DJE_USE_FAST_DCT
                                      gwd->state->pqt.luma,
#else
                                      gwd->state->qt_luma,
#endif
                                      // AC was removed from call since we are not "dummy encodeing" dc values
                                      gwd->state->ehuffsize[LUMA_AC], gwd->state->ehuffcode[LUMA_AC]);
        }
    }
}

static int dje_encode_main(DJEState* state, GPUInfo* gpu_info, uint8_t* qt)
{
    memcpy(state->qt_luma, qt, 64);
    memcpy(state->qt_chroma, qt, 64);

#if DJE_USE_FAST_DCT
    DJEProcessedQT pqt;
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
            pqt.luma[y*8+x] = 1.0f / (8 * aan_scales[x] * aan_scales[y] * state->qt_luma[djei_zig_zag[i]]);
            pqt.chroma[y*8+x] = 1.0f / (8 * aan_scales[x] * aan_scales[y] * state->qt_chroma[djei_zig_zag[i]]);
        }
    }

    state->pqt = pqt;

#endif

    // These will be the kernel parameters

    int num_blocks           = state->num_blocks;
    DJEBlock* y_blocks       = state->y_blocks;
    uint64_t* mse            = arena_alloc_array(state->arena, num_blocks, uint64_t);
    uint32_t* bitcount_array = arena_alloc_array(state->arena, num_blocks, uint32_t);

    if (gpu_info) {
        cl_int err;

#define ERR_CHECK if ( err != CL_SUCCESS ) { gpu_handle_cl_error(err); assert(!"kernel argument fail"); }
#define CHECK_WRAPPER(expr) err=expr; ERR_CHECK;

        // Write input parameters
        //  - Huffman data is passed in gpu_setup_buffers
        //  - pqt is per-kernel call. Pass it now.
        cl_event write_events[3];
        CHECK_WRAPPER (clEnqueueWriteBuffer(gpu_info->queue, gpu_info->qt_mem, /*blocking=*/CL_TRUE,
                                            /*offset=*/0,
                                            /*cb=*/64*sizeof(float),
                                            /*ptr=*/pqt.luma,
                                            /*num_in_wait_list=*/0,
                                            /*wait_list*/NULL,
                                            /*event*/&write_events[0]));
        // Zeroed-out.
        CHECK_WRAPPER (clEnqueueWriteBuffer(gpu_info->queue, gpu_info->mse_mem, /*blocking=*/CL_TRUE,
                                            /*offset=*/0,
                                            /*cb=*/num_blocks*sizeof(uint64_t),
                                            /*ptr=*/mse,
                                            /*num_in_wait_list=*/0,
                                            /*wait_list*/NULL,
                                            /*event*/&write_events[1]));
        // Zeroed-out.
        CHECK_WRAPPER (clEnqueueWriteBuffer(gpu_info->queue, gpu_info->bitcount_array_mem, /*blocking=*/CL_TRUE,
                                            /*offset=*/0,
                                            /*cb=*/num_blocks*sizeof(uint32_t),
                                            /*ptr=*/mse,
                                            /*num_in_wait_list=*/0,
                                            /*wait_list*/NULL,
                                            /*event*/&write_events[2]));
        clWaitForEvents(sgl_array_count(write_events), write_events);

        // Reset buffers.

        CHECK_WRAPPER(clSetKernelArg(gpu_info->kernel,0,sizeof(cl_mem),&gpu_info->mcu_array_mem));
        CHECK_WRAPPER(clSetKernelArg(gpu_info->kernel,1,sizeof(cl_mem),&gpu_info->bitcount_array_mem));
        CHECK_WRAPPER(clSetKernelArg(gpu_info->kernel,2,sizeof(cl_mem),&gpu_info->mse_mem));
        CHECK_WRAPPER(clSetKernelArg(gpu_info->kernel,3,sizeof(cl_mem),&gpu_info->qt_mem));
        CHECK_WRAPPER(clSetKernelArg(gpu_info->kernel,4,sizeof(cl_mem),&gpu_info->huffman_len_mem));

        assert(err == CL_SUCCESS);

        size_t global_work_size[1] = { num_blocks };
        size_t local_work_size[1] = { 32 };
        cl_event event = {0};
        CHECK_WRAPPER( clEnqueueNDRangeKernel (gpu_info->queue,
                                               gpu_info->kernel,
                                               /*cl_uint work_dim = */1,
                                               /* const size_t *global_work_offset = */ NULL,
                                               /* const size_t *global_work_size = */ global_work_size,
                                               /* const size_t *local_work_size = */ local_work_size,
                                               /* cl_uint num_events_in_wait_list = */ 0,
                                               /* const cl_event *event_wait_list = */ NULL,
                                               /* cl_event *event = */ &event));

        // Wait until the GPU processed the image.
        clWaitForEvents(1, &event);

        cl_event read_events[2];
        // Read from GPU memory for the 'reduce' step.
        clEnqueueReadBuffer(gpu_info->queue, gpu_info->bitcount_array_mem, /*blocking=*/CL_TRUE,
                            /*offset=*/0, /*size=*/num_blocks*sizeof(uint32_t), bitcount_array, /*event crap*/0,NULL,
                            &read_events[0]);
        clEnqueueReadBuffer(gpu_info->queue, gpu_info->mse_mem, /*blocking=*/CL_TRUE,
                            /*offset=*/0, /*size=*/num_blocks*sizeof(uint64_t), mse, /*event crap*/0,NULL,
                            &read_events[1]);

        clWaitForEvents(2, read_events);


#undef CHECK_WRAPPER
#undef ERR_CHECK
    } else {
#if DJE_MULTITHREADED
        // Fill work to do and unlock queue
        gwd->mse = mse;
        gwd->y_blocks = y_blocks;
        gwd->state = state;
        gwd->bitcount_array = bitcount_array;
        gwd->num_blocks = num_blocks;
        work_done = 0;

        sgl_memory_barrier();

        sgl_mutex_unlock(work_queue_mutex);

        for (;;) {
            sgl_mutex_lock(work_queue_mutex);
            if (work_done >= num_blocks) {
                break;
            } else {
                sgl_mutex_unlock(work_queue_mutex);
            }
        }
        sgl_usleep(1000);
        // Janky... but time constrainded =( i just want to leave enough time
        // for the trailing work to finish. I would use a semaphore but I am
        // running into a weird thread ownership bug which I honestly don't
        // have time to fix right now.

        gwd->num_blocks = 0;

#else
        // This loop is ready to be substituted by a single OpenCL kernel call
        for ( int bi = 0; bi < num_blocks; ++bi ) {
            djei_encode_and_write_MCU(bi, y_blocks, bitcount_array, mse,
#if DJE_USE_FAST_DCT
                                      state->pqt.luma,
#else
                                      state->qt_luma,
#endif
                                      state->ehuffsize[LUMA_AC], state->ehuffcode[LUMA_AC]);
        }
#endif
    }
    // "Reduce" step
    uint64_t mse_accum = 0;
    uint64_t mse_sum  = 0;
    for ( int bi = 0; bi < num_blocks; ++bi ) {
        mse_accum += mse[bi];

        if ( mse_accum >= (uint64_t)num_blocks ) {
            mse_accum -= (uint64_t)num_blocks;
            mse_sum += num_blocks;
        }

        state->bit_count += bitcount_array[bi];
    }
    mse_sum += mse_accum;

    state->mse = mse_sum;

    uint16_t EOI = djei_be_word(0xffd9);
    dje_write(state, &EOI, sizeof(uint16_t), 1);

    // If compiled with buffered IO
    djei_g_output_buffer_count = 0;

    return 1;
}

// Define public interface.

DJEState dje_init(Arena* arena,
                  GPUInfo* gpu_info,
                  int width,
                  int height,
                  int num_components,
                  unsigned char* src_data)
{
    static int called_once = true;
    if (!called_once) {
        assert (!"Failing because dje_init creates persistent buffers. Preventing leaks.");
    }
    called_once = false;

#if DJE_MULTITHREADED
    if (!gpu_info) {
        gwd = sgl_calloc(sizeof(struct global_work_data), 1);
        work_queue_mutex = sgl_create_mutex();
        sgl_mutex_lock(work_queue_mutex);
        for (int i = 0 ; i < 8; ++i)
            sgl_create_thread(dje_worker_thread, NULL);
    }
#endif

    int res = 1;
    DJEState state = { 0 };

    state.arena = arena;

    djei_huff_expand(&state);

    if (res) {
        res = djei_encode_prelude(&state, src_data, width, height, num_components);

        if (res && gpu_info) {
            // Assuming that we have already called gpu_init()
            gwd = sgl_calloc(sizeof(struct global_work_data), 1);

            res = gpu_setup_buffers(gpu_info,
                                    state.ehuffsize[LUMA_AC], state.num_blocks,
                                    state.y_blocks);
        }

    }
    if ( !res ) {
        assert (!"prelude failed");
    }
    return state;
}
// ============================================================
#endif // DJE_IMPLEMENTATION
// ============================================================

