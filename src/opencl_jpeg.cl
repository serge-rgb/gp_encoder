#include "src/jpeg_common.h"

// Buffer objects that we need;
//  -

// Single huffman table for luminance AC coefficients.
__constant uint8 ehuffsize[257];
__constant uint8 ehuffcode[256];

__kernel void main()
{
}
