#include "Test/time.h"

#include <arm_neon.h>
#include <cfloat>
#include <complex>
#include <iostream>
#include <random>
#include <vector>

void CheckResult(std::vector<uint8_t>& dst0, std::vector<uint8_t>& dst1, int height, int width)
{
    uint8_t max_diff = 0;
    uint8_t diff_num = 0;
    for (int h = 2; h < height - 2; ++h)
    {
        for (int w = 2; w < width - 2; ++w)
        {
            diff = abs(dst0[h * width + w] - dst[h * width + w]);
            if (diff != 0)
            {
                diff_num++;
                if (diff > max_diff)
                    max_diff = diff;
            }
        }
    }

    std::cout << "max_diff: " << max_diff << " diff_num: " << diff_num << std::endl;
}

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
inline T* GetRowPtr(T* base, size_t stride, size_t row)
{
    char* baseRaw = const_cast<char*>(reinterpret_cast<const char*>(base));
    return reinterpret_cast<T*>(baseRaw + row * stride);
}

inline ptrdiff_t borderInterpolate(ptrdiff_t _p, size_t _len, size_t startMargin = 0, size_t endMargin = 0)
{
    ptrdiff_t p = _p + (ptrdiff_t)startMargin;
    size_t len = _len + startMargin + endMargin;

    if (static_cast<size_t>(p) < len)
        return _p;

    // constant
    p = -1;

    return p;
}

void BlurNeon5x5(std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int height, int width)
{
    size_t srcStride = width;
    size_t dstStride = width;

    uint8_t* uSrc = reinterpret_cast<uint8_t*>(src.data());
    uint8_t* uDst = reinterpret_cast<uint8_t*>(dst.data());

    uint8_t borderValue = 0;
    int cn = 1;

    float32x4_t v1_25 = vdupq_n_f32(1.0f/25.0f);
    float32x4_t v0_5  = vdupq_n_f32(.5f);

    // border use zero
    std::vector<uint8_t> _tmp;
    uint8_t* tmp = 0;
    // constant
    {
        _tmp.assign(width + 2, borderValue);
        tmp = &_tmp[cn];
    }

    uint16x8_t tPrev = vdupq_n_u16(0x0);   // duplicate scalar or vector to vector
    uint16x8_t tCurr = tPrev;
    uint16x8_t tNext = tPrev;
    uint16x8_t t0, t1, t2, t3, t4;

    for (size_t y = 0; y < height; ++y)
    {
        const uint8_t *sRow0, *sRow1;
        const uint8_t* sRow2 = GetRowPtr(uSrc, srcStride, y);
        const uint8_t *sRow3, *sRow4;

        uint8_t* dRow = GetRowPtr(uDst, dstStride, y);

        sRow0 = y > 1 ? GetRowPtr(uSrc, srcStride, y - 2) : tmp;
        sRow1 = y > 0 ? GetRowPtr(uSrc, srcStride, y - 1) : tmp;
        sRow3 = y < height - 1 ? GetRowPtr(uSrc, srcStride, y + 1) : tmp;
        sRow4 = y < height - 2 ? GetRowPtr(uSrc, srcStride, y + 2) : tmp;

        // do vertical convolution
        size_t       x     = 0;
        const size_t bCols = y + 3 < height ? width : (width - 8);
        for (; x <= bCols; x += 8)
        {
            uint8x8_t x0 = vld1_u8(sRow0 + x);
            uint8x8_t x1 = vld1_u8(sRow1 + x);
            uint8x8_t x2 = vld1_u8(sRow2 + x);
            uint8x8_t x3 = vld1_u8(sRow3 + x);
            uint8x8_t x4 = vld1_u8(sRow4 + x);

            tPrev = tCurr;
            tCurr = tNext;
            tNext = vaddw_u8(vaddq_u16(vaddl_u8(x0, x1), vaddl_u8(x2, x3)), x4);

            if (!x)
            {
                tCurr = vsetq_lane_u16(0, tCurr, 6);
                tCurr = vsetq_lane_u16(0, tCurr, 7);
            }

            t0 = vextq_u16(tPrev, tCurr, 6);
            t1 = vextq_u16(tPrev, tCurr, 7);
            t2 = tCurr;
            t3 = vextq_u16(tCurr, tNext, 1);
            t4 = vextq_u16(tCurr, tNext, 2);

            t0 = vqaddq_u16(vqaddq_u16(vqaddq_u16(t0, t1), vqaddq_u16(t2, t3)), t4);

            uint32x4_t tres1 = vmovl_u16(vget_low_u16(t0));
            uint32x4_t tres2 = vmovl_u16(vget_high_u16(t0));
            float32x4_t vf1 = vmulq_f32(v1_25, vcvtq_f32_u32(tres1));
            float32x4_t vf2 = vmulq_f32(v1_25, vcvtq_f32_u32(tres2));
            tres1 = vcvtq_u32_f32(vaddq_f32(vf1, v0_5));
            tres1 = vcvtq_u32_f32(vaddq_f32(vf2, v0_5));
            t0 = vcombine_u16(vmovn_u32(tres1), vmovn_u32(tres2));
            vst1_u8(dRow + x - 8, vmovn_u16(t0));
        }

        x -= 8;

        if (x == width)
        {
            x -= cn;
        }

        int16_t pprevx[4], prevx[4], rowx[4], nextx[4], nnextx[4];
        ptrdiff_t px = x / cn;

        for (int32_t k = 0; k < cn; k++)
        {
            ptrdiff_t ploc;
            ploc = borderInterpolate(px-2, width);
            pprevx[k] = ploc < 0 ? 5 * borderValue :
                                   sRow4[ploc*cn+k] + sRow3[ploc*cn+k] +
                                   sRow2[ploc*cn+k] + sRow1[ploc*cn+k] +
                                   sRow0[ploc*cn+k];

            ploc = borderInterpolate(px-1, width);
            prevx[k] = ploc < 0 ? 5 * borderValue :
                                  sRow4[ploc*cn+k] + sRow3[ploc*cn+k] +
                                  sRow2[ploc*cn+k] + sRow1[ploc*cn+k] +
                                  sRow0[ploc*cn+k];

            rowx[k] = sRow4[ploc*cn+k] + sRow3[ploc*cn+k] + sRow2[ploc*cn+k]
                        + sRow1[ploc*cn+k] + sRow0[ploc*cn+k];

            ploc = borderInterpolate(px+1, width);
            nextx[k] = ploc < 0 ? 5 * borderValue :
                                  sRow4[ploc*cn+k] + sRow3[ploc*cn+k] +
                                  sRow2[ploc*cn+k] + sRow1[ploc*cn+k] +
                                  sRow0[ploc*cn+k];
        }

        x = px * cn;
        for (; x < width; x+=cn, px++)
        {
            for (int32_t k = 0; k < cn; ++k)
            {
                ptrdiff_t ploc = borderInterpolate(px+2, width);
                nnextx[k] = ploc < 0 ? 5 * borderValue :
                                       sRow4[ploc*cn+k] + sRow3[ploc*cn+k] +
                                       sRow2[ploc*cn+k] + sRow1[ploc*cn+k] +
                                       sRow0[ploc*cn+k];

                *(dRow+x+k) = static_cast<uint8_t>((pprevx[k] + prevx[k] +
                    rowx[k] + nextx[k] +nnextx[k])*(1/25.));

                pprevx[k] = prevx[k];
                prevx[k]  = rowx[k];
                rowx[k]   = nextx[k];
                nextx[k]  = nnextx[k];
            }
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

    std::cout << "RefBlur cost avg: " << total_time / LOOP << " min: " << min_time << std::endl;

    min_time   = DBL_MAX;
    total_time = 0;

    for (int i = 0; i < LOOP; ++i)
    {
        Tic();
        BlurNeon5x5(input, output, H, W);
        double time = Toc();

        if (time < min_time)
            min_time = time;

        total_time += time;
    }
    std::cout << "NeonBlur cost avg: " << total_time / LOOP << "min: " << min_time << std::endl;

    // check result;

    return 0;
}
