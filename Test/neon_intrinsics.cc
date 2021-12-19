/**
 *
 * Copyright [2021] <hpcers>
 * */
#include "logging.h"

#include <arm_neon.h>
#include <random>
#include <vector>


#define ARRAYSIZE 256

typedef unsigned char uchar;

template<typename T>
void InitArrRandom(std::vector<T>& arr)
{
    std::random_device rd;
    for (int k = 0; k < ARRAYSIZE; ++k)
        arr.push_back(static_cast<T>(rd() % 1024));
}

inline void M_vaddl_u8(std::vector<unsigned char>& arr)
{
    unsigned char* ptr = reinterpret_cast<unsigned char*>(arr.data());
    uint8x8_t      a, b;

    a = vld1_u8(ptr);
    b = vld1_u8(ptr + 8);

    // print in
    std::cout << "Test vaddl_u8:\n"
              << "in 0: " << std::endl;

    unsigned char in0[8];
    unsigned char in1[8];

    vst1_u8(in0, a);
    vst1_u8(in1, b);

    for (int k = 0; k < 8; k++)
        std::cout << static_cast<int>(in0[k]) << " ";

    std::cout << std::endl;

    std::cout << "in 1: " << std::endl;

    for (int k = 0; k < 8; k++)
        std::cout << static_cast<int>(in1[k]) << " ";

    std::cout << std::endl;

    uint16x8_t c;
    c = vaddl_u8(a, b);

    unsigned short out[8];

    vst1q_u16(out, c);

    std::cout << "vaddl_u8(in0, in1): " << std::endl;

    for (int k = 0; k < 8; k++)
    {
        std::cout << static_cast<int>(out[k]) << " ";
    }
    std::cout << std::endl;
}

#define NeonPrintU16(a, n)                              \
    {                                                   \
        uint16_t x[n];                                  \
        vst1q_u16(x, a);                                \
                                                        \
        for (int i = 0; i < n; i++)                     \
            std::cout << static_cast<int>(x[i]) << " "; \
                                                        \
        std::cout << std::endl;                         \
    }

#define NeonPrintU8(a, n)                               \
    {                                                   \
        uint8_t x[n];                                   \
        vst1q_u8(x, a);                                 \
                                                        \
        for (int i = 0; i < n; ++i)                     \
            std::cout << static_cast<int>(x[i]) << " "; \
        std::cout << std::endl;                         \
    }

inline void M_vaddq_u16(std::vector<unsigned short>& arr)
{
    uchar*     ptr = reinterpret_cast<uchar*>(arr.data());
    uint16x8_t a, b;

    // load a b
    a = vld1q_u8(ptr);
    b = vld1q_u8(ptr + 16);

    std::cout << "in0: " << std::endl;
    NeonPrintU16(a, 8);
    std::cout << "in1: " << std::endl;
    NeonPrintU16(b, 8);

    uint16x8_t c = vaddq_u16(a, b);

    std::cout << "vaddq_u16(in0, in1): " << std::endl;
    NeonPrintU16(c, 8);
}

inline void M_vsetq_lane_u16(std::vector<uint16_t>& arr)
{
    uint16_t* ptr = reinterpret_cast<uint16_t*>(arr.data());
    uint16x8_t a;

    a = vld1q_u16(ptr);

    std::cout << "in0: " << std::endl;
    NeonPrintU16(a, 8);
    uint16x8_t c = vsetq_lane_u16(0, a, 6);
    std::cout << "vsetq_lane_u16 index 6: " << std::endl;
    NeonPrintU16(c, 8);
}

inline void M_vextq_u16(std::vector<uint16_t>& arr)
{
    uint16_t* ptr = reinterpret_cast<uint16_t*>(arr.data());
    uint16x8_t a, b, c;

    a = vld1q_u16(ptr);
    b = vld1q_u16(ptr + 16);

    std::cout << "in0: " << std::endl;
    NeonPrintU16(a, 8);
    std::cout << "in1: " << std::endl;
    NeonPrintU16(b, 8);

    c = vextq_u16(a, b, 6);
    std::cout << "vextq_u16 index: 6" << std::endl;
    NeonPrintU16(c, 8);

    c = vextq_u16(a, b, 4);
    std::cout << "vextq_u16 index: 4" << std::endl;
    NeonPrintU16(c, 8);
}

int main()
{
    std::vector<uchar>          arrForU8;
    std::vector<unsigned short> arrForU16;
    InitArrRandom(arrForU8);
    InitArrRandom(arrForU16);

    M_vaddl_u8(arrForU8);

    M_vaddq_u16(arrForU16);

    M_vsetq_lane_u16(arrForU16);

    M_vextq_u16(arrForU16);

    return 0;
}



// int main()
// {
//     int8_t RandomArr[ARRAYSIZE];

//     for (int i = 0; i < ARRAYSIZE; ++i)
//     {
//         RandomArr[i] = i;
//     }


//     int8x16_t vec0, vec1;
//     vec0 = vld1q_s8(RandomArr);
//     vec1 = vld1q_s8(RandomArr);

//     vec0 = vaddq_s8(vec0, vec1);

//     int8_t out[16];

//     for (int i = 0; i < 16; ++i)
//         out[i] = i;

//     vst1q_s8(out, vec0);

//     for (int i = 0; i < 16; ++i)
//         Log() << (int)out[i];

//     return 0;
// }
