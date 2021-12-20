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

void InitArrRandom(std::vector<float>& arr)
{
    std::random_device rd;
    for (int k = 0; k < ARRAYSIZE; ++k)
        arr.push_back(static_cast<float>((rd() % 1024))/1024.0f);
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

inline void NeonPrintU16(uint16x4_t a)
{
    uint16_t x[4];
    vst1_u16(x, a);

    for (int k = 0; k < 4; ++k)
        std::cout << static_cast<int>(x[k]) << " ";

    std::cout << std::endl;
}

inline void NeonPrintU16(uint16x8_t a)
{
    uint16_t x[8];
    vst1q_u16(x, a);

    for (int k = 0; k < 8; ++k)
        std::cout << static_cast<int>(x[k]) << " ";

    std::cout << std::endl;
}

inline void NeonPrintU8(uint8x8_t a)
{
    uint8_t x[8];
    vst1_u8(x, a);

    for (int k = 0; k < 8; ++k)
        std::cout << static_cast<int>(x[k]) << " ";

    std::cout << std::endl;
}

inline void NeonPrintU8(uint8x16_t a)
{
    uint8_t x[16];
    vst1q_u8(x, a);

    for (int k = 0; k < 16; ++k)
        std::cout << static_cast<int>(x[k]) << " ";

    std::cout << std::endl;
}

inline void NeonPrintF32(float32x4_t a)
{
    float32_t x[4];
    vst1q_f32(x, a);

    for (int k = 0; k < 4; k++)
        std::cout << static_cast<float>(x[k]) << " ";

    std::cout << std::endl;
}

inline void NeonPrintU32(uint32x4_t a)
{
    uint32_t x[4];
    vst1q_u32(x, a);

    for (int k = 0; k < 4; k++)
        std::cout << static_cast<uint32_t>(x[k]) << " ";

    std::cout << std::endl;
}

inline void M_vaddq_u16(std::vector<unsigned short>& arr)
{
    uchar*     ptr = reinterpret_cast<uchar*>(arr.data());
    uint16x8_t a, b;

    // load a b
    a = vld1q_u8(ptr);
    b = vld1q_u8(ptr + 16);

    std::cout << "in0: " << std::endl;
    NeonPrintU16(a);
    std::cout << "in1: " << std::endl;
    NeonPrintU16(b);

    uint16x8_t c = vaddq_u16(a, b);

    std::cout << "vaddq_u16(in0, in1): " << std::endl;
    NeonPrintU16(c);
}

inline void M_vsetq_lane_u16(std::vector<uint16_t>& arr)
{
    uint16_t*  ptr = reinterpret_cast<uint16_t*>(arr.data());
    uint16x8_t a;

    a = vld1q_u16(ptr);

    std::cout << "in0: " << std::endl;
    NeonPrintU16(a);
    uint16x8_t c = vsetq_lane_u16(0, a, 6);
    std::cout << "vsetq_lane_u16 index 6: " << std::endl;
    NeonPrintU16(c);
}

inline void M_vextq_u16(std::vector<uint16_t>& arr)
{
    uint16_t*  ptr = reinterpret_cast<uint16_t*>(arr.data());
    uint16x8_t a, b, c;

    a = vld1q_u16(ptr);
    b = vld1q_u16(ptr + 16);

    std::cout << "in0: " << std::endl;
    NeonPrintU16(a);
    std::cout << "in1: " << std::endl;
    NeonPrintU16(b);

    c = vextq_u16(a, b, 6);
    std::cout << "vextq_u16 index: 6" << std::endl;
    NeonPrintU16(c);

    c = vextq_u16(a, b, 4);
    std::cout << "vextq_u16 index: 4" << std::endl;
    NeonPrintU16(c);
}

inline void M_vcgeq_u8(std::vector<uint8_t>& arr)
{
    uint8_t* ptr = reinterpret_cast<uint8_t*>(arr.data());
    uint8x16_t a, b, c;

    a = vld1q_u8(ptr);
    b = vld1q_u8(ptr);

    std::cout << "in0: " << std::endl;
    NeonPrintU8(a);
    std::cout << "in1: " << std::endl;
    NeonPrintU8(b);

    c = vcgeq_u8(a, b);

    std::cout << "vcgeq_u8(in0, in1): " << std::endl;
    NeonPrintU8(c);
}

inline void M_vcgeq_f32(std::vector<float>& arr)
{
    float32_t* ptr = reinterpret_cast<float*>(arr.data());

    float32x4_t a, b;
    uint32x4_t c;

    a = vld1q_f32(ptr);
    b = vld1q_f32(ptr + 4);

    std::cout << "in0: " << std::endl;
    NeonPrintF32(a);

    std::cout << "in1: " << std::endl;
    NeonPrintF32(b);

    c = vcgeq_f32(a, b);

    std::cout << "cgeq_f32(in0, in1): " << std::endl;
    NeonPrintU32(c);
}

inline void M_vminq_f32(std::vector<float>& arr)
{
    float32_t* ptr = reinterpret_cast<float*>(arr.data());

    float32x4_t a, b, c;
    a = vld1q_f32(ptr);
    b = vdupq_n_f32(0.5f);

    c = vminq_f32(a, b);

    std::cout << "in0: " << std::endl;
    NeonPrintF32(a);
    std::cout << "in1: " << std::endl;
    NeonPrintF32(b);

    std::cout << "vminq_f32(in0, in1): " << std::endl;
    NeonPrintF32(c);
}

inline void M_vmaxq_f32(std::vector<float>& arr)
{
    float32_t* ptr = reinterpret_cast<float*>(arr.data());

    float32x4_t a, b, c;
    a = vld1q_f32(ptr);
    b = vdupq_n_f32(0.5f);

    c = vmaxq_f32(a, b);

    std::cout << "in0: " << std::endl;
    NeonPrintF32(a);
    std::cout << "in1: " << std::endl;
    NeonPrintF32(b);

    std::cout << "vmaxq_f32(in0, in1): " << std::endl;
    NeonPrintF32(c);
}

inline void M_vfmaq_f32(std::vector<float>& arr)
{
    float32_t* ptr = reinterpret_cast<float*>(arr.data());

    float32x4_t a, b, c;

    a = vdupq_n_f32(3.0f);
    b = vdupq_n_f32(2.0f);

    c = vdupq_n_f32(5.0f);

    std::cout << "in0: " << std::endl;
    NeonPrintF32(a);

    std::cout << "in1: " << std::endl;
    NeonPrintF32(b);

    std::cout << "in2: " << std::endl;
    NeonPrintF32(c);

    c = vfmaq_f32(a, b, c);

    std::cout << "vfmaq_f32(in0, in1, in2): " << std::endl;
    NeonPrintF32(c);
}

int main()
{
    std::vector<uchar>          arrForU8;
    std::vector<unsigned short> arrForU16;

    std::vector<float> arrF32;

    InitArrRandom(arrForU8);
    InitArrRandom(arrForU16);
    InitArrRandom(arrF32);

    M_vaddl_u8(arrForU8);

    M_vaddq_u16(arrForU16);

    M_vsetq_lane_u16(arrForU16);

    M_vextq_u16(arrForU16);

    M_vcgeq_u8(arrForU8);

    M_vcgeq_f32(arrF32);

    M_vminq_f32(arrF32);

    M_vmaxq_f32(arrF32);

    M_vfmaq_f32(arrF32);

    return 0;
}


