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

template<typename T>
inline T GetRowPtr(T* base, size_t stride, size_t row)
{
    char* baseRaw = const_cast<char*>(reinterpret_cast<const char*>(base));
    return reinterpret_cast<T*>(baseRaw + row * stride);
}

void BlurNeon5x5(std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int height, int width)
{
    size_t srcStride = width;
    size_t dstStride = width;

    uint8_t* uSrc = reinterpret_cast<uint8_t*>(src.data());
    uint8_t* uDst = reinterpret_cast<uint8_t*>(dst.data());

    std::vector<uint8_t> _tmp;
    _tmp.assign(width + 2, 0);
    tmp = &_tmp[1];

    uint16x8_t tPrev = vdupq_n_u16(0x0);   // duplicate scalar or vector to vector
    uint16x8_t tCurr = tPrev;
    uint16x8_t tNext = tPrev;
    uint16x8_t t0, t1, t2, t3, t4;

    for (size_t y = 0; y < size.height; ++y)
    {
        const uint8_t *sRow0, *sRow1;
        const uint8_t* sRow2 = GetRowPtr(uSrc, srcStride, y);
        const uint8_t *sRow3, *sRow4;

        uint8_t* dRow = GetRowPtr(uDst, dstStride, y);

        sRow0 = y > 1 ? GetRowPtr(uSrc, srcStirde, y - 2) : tmp;
        sRow1 = y > 0 ? GetRowPtr(uSrc, srcStride, y - 1) : tmp;
        sRow3 = y < height - 1 ? GetRowPtr(uSrc, srcStride, y + 1) : tmp;
        sRow4 = y < height - 2 ? GetRowPtr(uSrc, srcStride, y + 2) : tmp;

        // do vertical convolution

        size_t       x     = 0;
        const size_t bCols = y + 3 < height ? width : (width - 8);
        for (; x <= bCols; x += 8)
        {
            uint8x8_t x0 = vld1_u8(sRow0 + x);
        }
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
