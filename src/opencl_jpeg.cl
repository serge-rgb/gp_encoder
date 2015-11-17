#include "src/dje_common.h"

// Buffer objects that we need;
//  -

// Single huffman table for luminance AC coefficients.
/* __constant uchar ehuffsize[257]; */
/* __constant uchar ehuffcode[256]; */

void djei_calculate_variable_length_int(int value, ushort out[2])
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

__kernel void cl_encode_and_write_MCU(/*0*/__global DJEBlock* mcu_array,
                                      /*1*/__global uint* bitcount_array,
                                      /*2*/__global ulong* out_mse,
                                      /*3*/__global float* qt,  // Pre-processed quantization matrix.
                                      /*4*/__constant uchar* huff_ac_len,
                                      /*5*/__constant ushort* huff_ac_code)
{
    int block_i = (int)get_global_id(0);
    __global float* mcu = mcu_array[block_i].d;
    short du[64];  // Data unit in zig-zag order

    float dct_mcu[64];
    for (int i = 0; i < 64; ++i) {
        dct_mcu[i] = mcu_array[block_i].d[i];
    }
    fdct(dct_mcu);

    for ( int i = 0; i < 64; ++i ) {
        float fval = dct_mcu[i];
        fval *= qt[i];
        fval = floor(fval + 1024 + 0.5f);
        fval -= 1024;
        short val = (short)fval;
        du[djei_zig_zag[i]] = val;
    }

    uchar decomp[64];
    float re[64];  // Reconstructed image =)

    // Note: stb uses w_cap as out_stride..
    idct_block(decomp, 8, du);
    for ( int id = 0; id < 64; ++id )
        re[id] = (float)decomp[id];

    ulong MSE = 0;

    for ( int i = 0; i < 64; ++i ) {
        int precision = 10;  // Decimal-points of precision.
        int re_i = (int)(re[i]);
        int mcu_i = (int)(mcu[i] + 128);
        int err = abs(re_i - mcu_i);
        MSE += err;
    }

    out_mse[block_i] = MSE;

    ushort vli[2];

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
    for (int i = 1; i <= last_non_zero_i; ++i) {
        // If zero, increase count. If >=15, encode (FF,00)
        int zero_count = 0;
        while (du[i] == 0) {
            ++zero_count;
            ++i;
            if (zero_count == 16) {
                // encode (ff,00) == 0xf0
                bitcount_array[block_i] += huff_ac_len[0xf0];
                zero_count = 0;
            }
        }

        djei_calculate_variable_length_int(du[i], vli);

        if ( i == 0 && vli[1] >= 10) {
            // There is no code for this coefficient. ignore.
            continue;
        }

        ushort sym1 = ((ushort)zero_count << 4) | vli[1];

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
}
