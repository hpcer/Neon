/**
 *
 * Copyright [2021] <hpcers>
 * */
#include "logging.h"

#include <arm_neon.h>
#include <vector>


#define ARRAYSIZE 256

int main()
{
    int8_t RandomArr[ARRAYSIZE];

    for (int i = 0; i < ARRAYSIZE; ++i)
    {
        RandomArr[i] = i;
    }


    int8x16_t vec0, vec1;
    vec0 = vld1q_s8(RandomArr);
    vec1 = vld1q_s8(RandomArr);

    vec0 = vaddq_s8(vec0, vec1);

    int8_t out[16];

    for (int i = 0; i < 16; ++i)
        out[i] = i;

    vst1q_s8(out, vec0);

    for (int i = 0; i < 16; ++i)
        Log() << (int)out[i];

    return 0;
}
