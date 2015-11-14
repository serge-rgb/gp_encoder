// Stuff used by the CPU and GPU implementations of the JPEG fitness function

#pragma once

#if defined(__OPENCL_VERSION__)
#define uint8_t uchar
#define stbi_uc uchar
#else
#define __private
#define __constant
#endif

typedef struct DJEBlock_s {
    float d[64];
} DJEBlock;

// Zig-zag order:
__constant uint8_t djei_zig_zag[64] = {
   0,   1,  5,  6, 14, 15, 27, 28,
   2,   4,  7, 13, 16, 26, 29, 42,
   3,   8, 12, 17, 25, 30, 41, 43,
   9,  11, 18, 24, 31, 40, 44, 53,
   10, 19, 23, 32, 39, 45, 52, 54,
   20, 22, 33, 38, 46, 51, 55, 60,
   21, 34, 37, 47, 50, 56, 59, 61,
   35, 36, 48, 49, 57, 58, 62, 63,
};

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

/////////////////////
//  IDCT from stb_image.h (public domain).
//  which in turn gets it from IJG who take it from:
//      C. Loeffler, A. Ligtenberg and G. Moschytz, "Practical Fast 1-D DCT
//      Algorithms with 11 Multiplications", Proc. Int'l. Conf. on Acoustics,
//      Speech, and Signal Processing 1989 (ICASSP '89), pp. 988-991.
//  /////////////////

// take a -128..127 value and stbi__clamp it and convert to 0..255

#if defined(__OPENCL_VERSION__)
stbi_uc stbi__clamp(int x)
{
   // trick to use a single test to catch both cases
   if ((unsigned int) x > 255) {
      if (x < 0) return 0;
      if (x > 255) return 255;
   }
   return (stbi_uc) x;
}
#endif

#define stbi__f2f(x)  ((int) (((x) * 4096 + 0.5)))
#define stbi__fsh(x)  ((x) << 12)

// derived from jidctint -- DCT_ISLOW
#define STBI__IDCT_1D(s0,s1,s2,s3,s4,s5,s6,s7) \
   int t0,t1,t2,t3,p1,p2,p3,p4,p5,x0,x1,x2,x3; \
   p2 = s2;                                    \
   p3 = s6;                                    \
   p1 = (p2+p3) * stbi__f2f(0.5411961f);       \
   t2 = p1 + p3*stbi__f2f(-1.847759065f);      \
   t3 = p1 + p2*stbi__f2f( 0.765366865f);      \
   p2 = s0;                                    \
   p3 = s4;                                    \
   t0 = stbi__fsh(p2+p3);                      \
   t1 = stbi__fsh(p2-p3);                      \
   x0 = t0+t3;                                 \
   x3 = t0-t3;                                 \
   x1 = t1+t2;                                 \
   x2 = t1-t2;                                 \
   t0 = s7;                                    \
   t1 = s5;                                    \
   t2 = s3;                                    \
   t3 = s1;                                    \
   p3 = t0+t2;                                 \
   p4 = t1+t3;                                 \
   p1 = t0+t3;                                 \
   p2 = t1+t2;                                 \
   p5 = (p3+p4)*stbi__f2f( 1.175875602f);      \
   t0 = t0*stbi__f2f( 0.298631336f);           \
   t1 = t1*stbi__f2f( 2.053119869f);           \
   t2 = t2*stbi__f2f( 3.072711026f);           \
   t3 = t3*stbi__f2f( 1.501321110f);           \
   p1 = p5 + p1*stbi__f2f(-0.899976223f);      \
   p2 = p5 + p2*stbi__f2f(-2.562915447f);      \
   p3 = p3*stbi__f2f(-1.961570560f);           \
   p4 = p4*stbi__f2f(-0.390180644f);           \
   t3 += p1+p4;                                \
   t2 += p2+p3;                                \
   t1 += p2+p4;                                \
   t0 += p1+p3;

#if !defined(__OPENCL_VERSION__)
static void idct_block(uint8_t *out, int out_stride, short data[64])
#else
static void idct_block(__private uint8_t* out, int out_stride, __private short* data)
#endif
{
    int i,val[64],*v=val;
    __private stbi_uc *o;
    short *d = data;

    // columns
    for (i=0; i < 8; ++i,++d, ++v) {
        // if all zeroes, shortcut -- this avoids dequantizing 0s and IDCTing
        if (d[ 8]==0 && d[16]==0 && d[24]==0 && d[32]==0
            && d[40]==0 && d[48]==0 && d[56]==0) {
            //    no shortcut                 0     seconds
            //    (1|2|3|4|5|6|7)==0          0     seconds
            //    all separate               -0.047 seconds
            //    1 && 2|3 && 4|5 && 6|7:    -0.047 seconds
            int dcterm = d[0] << 2;
            v[0] = v[8] = v[16] = v[24] = v[32] = v[40] = v[48] = v[56] = dcterm;
        } else {
            STBI__IDCT_1D(d[ 0],d[ 8],d[16],d[24],d[32],d[40],d[48],d[56])
                    // constants scaled things up by 1<<12; let's bring them back
                    // down, but keep 2 extra bits of precision
                    x0 += 512; x1 += 512; x2 += 512; x3 += 512;
            v[ 0] = (x0+t3) >> 10;
            v[56] = (x0-t3) >> 10;
            v[ 8] = (x1+t2) >> 10;
            v[48] = (x1-t2) >> 10;
            v[16] = (x2+t1) >> 10;
            v[40] = (x2-t1) >> 10;
            v[24] = (x3+t0) >> 10;
            v[32] = (x3-t0) >> 10;
        }
    }

    for (i=0, v=val, o=out;
         i < 8;
         ++i,v+=8,o+=out_stride) {
        // no fast case since the first 1D IDCT spread components out
        STBI__IDCT_1D(v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7])
                // constants scaled things up by 1<<12, plus we had 1<<2 from first
                // loop, plus horizontal and vertical each scale by sqrt(8) so together
                // we've got an extra 1<<3, so 1<<17 total we need to remove.
                // so we want to round that, which means adding 0.5 * 1<<17,
                // aka 65536. Also, we'll end up with -128 to 127 that we want
                // to encode as 0..255 by adding 128, so we'll add that before the shift
                x0 += 65536 + (128<<17);
        x1 += 65536 + (128<<17);
        x2 += 65536 + (128<<17);
        x3 += 65536 + (128<<17);
        // tried computing the shifts into temps, or'ing the temps to see
        // if any were out of range, but that was slower
        o[0] = stbi__clamp((x0+t3) >> 17);
        o[7] = stbi__clamp((x0-t3) >> 17);
        o[1] = stbi__clamp((x1+t2) >> 17);
        o[6] = stbi__clamp((x1-t2) >> 17);
        o[2] = stbi__clamp((x2+t1) >> 17);
        o[5] = stbi__clamp((x2-t1) >> 17);
        o[3] = stbi__clamp((x3+t0) >> 17);
        o[4] = stbi__clamp((x3-t0) >> 17);
    }
}
