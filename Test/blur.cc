#include "Test/time.h"

#include <arm_neon.h>
#include <cfloat>
#include <complex>
#include <iostream>
#include <random>
#include <vector>

// typedef unsigned char uint8_t;

void BlurRef(std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int height, int width, int kernel_size)
{
    int   half_k = kernel_size / 2;
    int   count;
    float temp;

    if (dst.size() < height * width)
        dst.resize(height * width);

    uint8_t* src_ptr = reinterpret_cast<uint8_t*>(src.data());
    uint8_t* dst_ptr = reinterpret_cast<uint8_t*>(dst.data());

    for (int h = 0; h < height; ++h)
    {
        for (int w = 0; w < width; ++w)
        {
            temp  = 0;
            count = 0;
            for (int kh = -half_k; kh < kernel_size - half_k; ++kh)
            {
                for (int kw = -half_k; kw < kernel_size - half_k; ++kw)
                {
                    if (h + kh < 0 || h + kh >= height || w + kw < 0 || w + kw >= width)
                    {
                        continue;
                    }
                    temp = temp + src_ptr[((h + kh) * width + (w + kw))];
                    ++count;
                }
            }
            dst_ptr[h * width + w] = static_cast<uint8_t>(temp / count);
        }
    }
    return;
}

void BlurNeon5x5(std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int height, int width)
{
    uint8_t* uSrc = reinterpret_cast<uint8_t*>(src.data());
    uint8_t* uDst = reinterpret_cast<uint8_t*>(dst.data());

    std::vector<float> row(width);
    float32_t* rowPtr = reinterpret_cast<float32_t*>(row.data());

    float32_t   t0 = 0.0f;
    float32x4_t tx;


    for (int h0 = 0; h0 < 3; ++h0)
    {
        int w0 = 0;
        for (w0 = 0; w0 < width; w0 += 16)
        {
            uint8x16_t reg = vld1q_u8(uSrc);

            for (uint8_t k = 0; k < 16; ++k)
            {
                t0 += reg[k];
                tx[k & 3] = t0;

                if (((k & 3) == 0) && (k != 0))
                {
                    float32x4_t tmp = vld1q_f32(rowPtr);
                    tx              = vaddq_f32(tmp, tx);
                    vst1q_f32(rowPtr, tx);
                    rowPtr += 4;
                }
            }
        }
    }

    rowPtr = reinterpret_cast<float32_t*>(row.data());

    float32x4_t scale = vmovq_n_f32(12.0f);

    for(int w0 = 0; w0 < width; ++w0)
    {
        float32x4_t tmp = vld1q_f32(rowPtr);
        tmp = vdivq_f32(tmp, scale);
        vst1q_f32()

    }
}

template<typename T, typename D>
void CreateRandom(std::vector<T> in, D distribution)
{
    std::mt19937 random_seed;

    std::generate_n(in.data(),
                    in.size(),
                    [&]()
                    {
                        if (std::is_same<T, std::complex<float>>::value)
                        {
                            return static_cast<T>(distribution(random_seed), distribution(random_seed));
                        }
                        else
                        {
                            return static_cast<T>(distribution(random_seed));
                        }
                    });
}

#define H 288
#define W 160

#define LOOP 20

int main()
{
    std::vector<uint8_t> input(H * W);
    std::vector<uint8_t> output(H * W);

    CreateRandom(input, std::uniform_int_distribution<int32_t>(0, 255));
    CreateRandom(output, std::uniform_int_distribution<int32_t>(0, 255));

    double min_time   = DBL_MAX;
    double total_time = 0;

    // warm up
    for (int i = 0; i < LOOP; ++i)
        BlurRef(input, output, H, W, 5);

    for (int i = 0; i < LOOP; ++i)
    {
        Tic() BlurRef(input, output, H, W, 5);
        double time = Toc();

        if (time < min_time)
            min_time = time;

        total_time += time;
    }

    std::cout << "blur cost avg: " << total_time / LOOP << " min: " << min_time << std::endl;

    return 0;
}
