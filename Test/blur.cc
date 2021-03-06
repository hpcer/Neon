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
    int32_t diff_num = 0;
    for (int h = 0; h < height; ++h)
    {
        for (int w = 0; w < width; ++w)
        {
            uint8_t diff = abs(dst0[h * width + w] - dst1[h * width + w]);
            if (diff != 0)
            {
                diff_num++;
                if (diff > max_diff)
                    max_diff = diff;
            }
            // std::cout << "(" << h << ", " << w << "): " << static_cast<int>(dst0[h * width + w]) << " "
            //           << static_cast<int>(dst1[h * width + w]) << " ---> " << static_cast<int>(diff) << std::endl;
        }
    }

    std::cout << "max_diff: " << static_cast<int>(max_diff) << " diff_num: " << diff_num << std::endl;
}

void BlurRef(std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int height, int width, int kernel_size)
{
    int      half_k = kernel_size / 2;
    int      count;
    uint16_t temp;

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
            dst_ptr[h * width + w] = static_cast<uint8_t>(temp * (1.f / count));
        }
    }
    return;
}

void BlurRefNew(std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int height, int width, int kernel_size)
{
    int     half_k = kernel_size / 2;
    int     count;
    int16_t temp;

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
            dst_ptr[h * width + w] = static_cast<uint8_t>(temp / 25.f + 0.5f);
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
    ptrdiff_t p   = _p + (ptrdiff_t)startMargin;
    size_t    len = _len + startMargin + endMargin;

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
    int     cn          = 1;

    float32x4_t v1_25 = vdupq_n_f32(1.0f / 25.0f);
    float32x4_t v0_5  = vdupq_n_f32(.5f);

    // border use zero
    std::vector<uint8_t> _tmp;
    uint8_t*             tmp = 0;
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
                tCurr = tNext;

                tCurr = vsetq_lane_u16(0, tCurr, 6);
                tCurr = vsetq_lane_u16(0, tCurr, 7);

                continue;
            }

            t0 = vextq_u16(tPrev, tCurr, 6);
            t1 = vextq_u16(tPrev, tCurr, 7);
            t2 = tCurr;
            t3 = vextq_u16(tCurr, tNext, 1);
            t4 = vextq_u16(tCurr, tNext, 2);

            t0 = vqaddq_u16(vqaddq_u16(vqaddq_u16(t0, t1), vqaddq_u16(t2, t3)), t4);

            uint32x4_t  tres1 = vmovl_u16(vget_low_u16(t0));
            uint32x4_t  tres2 = vmovl_u16(vget_high_u16(t0));
            float32x4_t vf1   = vmulq_f32(v1_25, vcvtq_f32_u32(tres1));
            float32x4_t vf2   = vmulq_f32(v1_25, vcvtq_f32_u32(tres2));
            tres1             = vcvtq_u32_f32(vaddq_f32(vf1, v0_5));
            tres2             = vcvtq_u32_f32(vaddq_f32(vf2, v0_5));
            t0                = vcombine_u16(vmovn_u32(tres1), vmovn_u32(tres2));
            vst1_u8(dRow + x - 8, vmovn_u16(t0));
        }

        x -= 8;

        if (x == width)
        {
            x -= cn;
        }

        int16_t   pprevx[4], prevx[4], rowx[4], nextx[4], nnextx[4];
        ptrdiff_t px = x / cn;

        for (int32_t k = 0; k < cn; k++)
        {
            ptrdiff_t ploc;
            ploc      = borderInterpolate(px - 2, width);
            pprevx[k] = ploc < 0 ? 5 * borderValue
                                 : sRow4[ploc * cn + k] + sRow3[ploc * cn + k] + sRow2[ploc * cn + k] +
                                       sRow1[ploc * cn + k] + sRow0[ploc * cn + k];

            ploc     = borderInterpolate(px - 1, width);
            prevx[k] = ploc < 0 ? 5 * borderValue
                                : sRow4[ploc * cn + k] + sRow3[ploc * cn + k] + sRow2[ploc * cn + k] +
                                      sRow1[ploc * cn + k] + sRow0[ploc * cn + k];

            rowx[k] = sRow4[ploc * cn + k] + sRow3[ploc * cn + k] + sRow2[ploc * cn + k] + sRow1[ploc * cn + k] +
                      sRow0[ploc * cn + k];

            ploc     = borderInterpolate(px + 1, width);
            nextx[k] = ploc < 0 ? 5 * borderValue
                                : sRow4[ploc * cn + k] + sRow3[ploc * cn + k] + sRow2[ploc * cn + k] +
                                      sRow1[ploc * cn + k] + sRow0[ploc * cn + k];
        }

        x = px * cn;
        for (; x < width; x += cn, px++)
        {
            for (int32_t k = 0; k < cn; ++k)
            {
                ptrdiff_t ploc = borderInterpolate(px + 2, width);
                nnextx[k]      = ploc < 0 ? 5 * borderValue
                                          : sRow4[ploc * cn + k] + sRow3[ploc * cn + k] + sRow2[ploc * cn + k] +
                                           sRow1[ploc * cn + k] + sRow0[ploc * cn + k];

                *(dRow + x + k) =
                    static_cast<uint8_t>((pprevx[k] + prevx[k] + rowx[k] + nextx[k] + nnextx[k]) * (1 / 25.));

                pprevx[k] = prevx[k];
                prevx[k]  = rowx[k];
                rowx[k]   = nextx[k];
                nextx[k]  = nnextx[k];
            }
        }
    }
}

void BlurNeon5x5New(std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int height, int width)
{
    size_t srcStride = width;
    size_t dstStride = width;

    uint8_t* uSrc = reinterpret_cast<uint8_t*>(src.data());
    uint8_t* uDst = reinterpret_cast<uint8_t*>(dst.data());

    uint8_t borderValue = 0;
    int     cn          = 1;

    float32x4_t v1_25 = vdupq_n_f32(1.0f / 25.0f);
    float32x4_t v0_5  = vdupq_n_f32(.5f);

    // border use zero
    std::vector<uint8_t> _tmp;
    uint8_t*             tmp = 0;
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
        const size_t bCols = width;

        float32x4_t vScale = v1_25;

        // only 0 1 2 ( 3x5 )
        if (y == 0 || y == height - 1)
            vScale = vdupq_n_f32(1.0f / 15.0f);

        // only 0 1 2 3 (4x5)
        if (y == 1 || y == height - 2)
            vScale = vdupq_n_f32(1.0f / 20.f);

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
                tCurr = tNext;

                tCurr = vsetq_lane_u16(0, tCurr, 6);
                tCurr = vsetq_lane_u16(0, tCurr, 7);

                continue;
            }

            t0 = vextq_u16(tPrev, tCurr, 6);
            t1 = vextq_u16(tPrev, tCurr, 7);
            t2 = tCurr;
            t3 = vextq_u16(tCurr, tNext, 1);
            t4 = vextq_u16(tCurr, tNext, 2);

            t0 = vqaddq_u16(vqaddq_u16(vqaddq_u16(t0, t1), vqaddq_u16(t2, t3)), t4);

            uint32x4_t tres1 = vmovl_u16(vget_low_u16(t0));
            uint32x4_t tres2 = vmovl_u16(vget_high_u16(t0));

            float32x4_t scale0 = vScale;
            float32x4_t scale1 = vScale;
            if (x == 8)
            {
                scale0 = vsetq_lane_f32(1.0f / 15.0f, scale0, 0);
                scale0 = vsetq_lane_f32(1.0f / 20.0f, scale0, 1);

                if (y == 0 || y == height - 1)
                {
                    scale0 = vsetq_lane_f32(1.0f / 9.0f, scale0, 0);
                    scale0 = vsetq_lane_f32(1.0f / 12.0f, scale0, 1);
                }

                if (y == 1 || y == height - 2)
                {
                    scale0 = vsetq_lane_f32(1.0f / 12.0f, scale0, 0);
                    scale0 = vsetq_lane_f32(1.0f / 16.0f, scale0, 1);
                }
            }

            float32x4_t vf1 = vmulq_f32(scale0, vcvtq_f32_u32(tres1));
            float32x4_t vf2 = vmulq_f32(scale1, vcvtq_f32_u32(tres2));
            tres1           = vcvtq_u32_f32(vaddq_f32(vf1, v0_5));
            tres2           = vcvtq_u32_f32(vaddq_f32(vf2, v0_5));
            t0              = vcombine_u16(vmovn_u32(tres1), vmovn_u32(tres2));
            vst1_u8(dRow + x - 8, vmovn_u16(t0));
        }

        x -= 8;

        // if (x == width)
        // {
        x -= 2;
        // }

        int16_t   pprevx, prevx, rowx, nextx, nnextx;
        ptrdiff_t px = x;

        ptrdiff_t ploc;
        ploc   = borderInterpolate(px - 2, width);
        pprevx = ploc < 0 ? 5 * borderValue : sRow4[ploc] + sRow3[ploc] + sRow2[ploc] + sRow1[ploc] + sRow0[ploc];

        ploc  = borderInterpolate(px - 1, width);
        prevx = ploc < 0 ? 5 * borderValue : sRow4[ploc] + sRow3[ploc] + sRow2[ploc] + sRow1[ploc] + sRow0[ploc];

        rowx = sRow4[px] + sRow3[px] + sRow2[px] + sRow1[px] + sRow0[px];

        ploc  = borderInterpolate(px + 1, width);
        nextx = ploc < 0 ? 5 * borderValue : sRow4[ploc] + sRow3[ploc] + sRow2[ploc] + sRow1[ploc] + sRow0[ploc];

        x = px;

        uint16_t count;
        for (; x < width; x++, px++)
        {
            ptrdiff_t ploc = borderInterpolate(px + 2, width);
            nnextx = ploc < 0 ? 5 * borderValue : sRow4[ploc] + sRow3[ploc] + sRow2[ploc] + sRow1[ploc] + sRow0[ploc];

            count = (width - x) + (5 / 2);

            if (y == 0 || y == height - 1)
                count *= 3;
            else if (y == 1 || y == height - 2)
                count *= 4;
            else
                count *= 5;

            *(dRow + x) = static_cast<uint8_t>(
                static_cast<float>(pprevx + prevx + rowx + nextx + nnextx) * (1.0f / count) + 0.5f);

            pprevx = prevx;
            prevx  = rowx;
            rowx   = nextx;
            nextx  = nnextx;
        }
    }
}


void BlurNeon5x5Final(std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int height, int width)
{
    size_t srcStride = width;
    size_t dstStride = width;

    uint8_t* uSrc = reinterpret_cast<uint8_t*>(src.data());
    uint8_t* uDst = reinterpret_cast<uint8_t*>(dst.data());

    uint8_t borderValue = 0;
    int     cn          = 1;

    // float32x4_t v1_25 = vdupq_n_f32(1.0f / 25.0f);
    float32x4_t v0_5 = vdupq_n_f32(.5f);

    // float32_t fScale[] = {1.0f/9.0f, 1.0f / 12.0f, 1.0f / 15.0f, 1.0f / 16.0f, 1.0f / 20.0f, 1.0f / 25.0f };

    float32_t fScale[] = {0.0f, 1.0f, 1.0f / 2.0f, 1.0f / 3.0f, 1.0f / 4.0f, 1.0f / 5.0f,
                          1.0f / 6.0f, 1.0f / 7.0f, 1.0f / 8.0f, 1.0f / 9.0f, 1.0f / 10.0f,
                          1.0f / 11.0f, 1.0f / 12.0f, 1.0f / 13.0f, 1.0f / 14.0f, 1.0f / 15.0f,
                          1.0f / 16.0f, 1.0f / 17.0f, 1.0f / 18.0f, 1.0f / 19.0f, 1.0f / 20.0f,
                          1.0f / 21.0f, 1.0f / 22.0f, 1.0f / 23.0f, 1.0f / 24.0f, 1.0f / 25.0f};

    // border use zero
    std::vector<uint8_t> _tmp;
    uint8_t*             tmp = 0;
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
        const size_t bCols = width;

        float32x4_t vScale = vdupq_n_f32(fScale[25]);

        // only 0 1 2 ( 3x5 )
        if (y == 0 || y == height - 1)
            vScale = vdupq_n_f32(fScale[15]);

        // only 0 1 2 3 (4x5)
        if (y == 1 || y == height - 2)
            vScale = vdupq_n_f32(fScale[20]);

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
                tCurr = tNext;

                tCurr = vsetq_lane_u16(0, tCurr, 6);
                tCurr = vsetq_lane_u16(0, tCurr, 7);

                continue;
            }

            t0 = vextq_u16(tPrev, tCurr, 6);
            t1 = vextq_u16(tPrev, tCurr, 7);
            t2 = tCurr;
            t3 = vextq_u16(tCurr, tNext, 1);
            t4 = vextq_u16(tCurr, tNext, 2);

            t0 = vqaddq_u16(vqaddq_u16(vqaddq_u16(t0, t1), vqaddq_u16(t2, t3)), t4);

            uint32x4_t tres1 = vmovl_u16(vget_low_u16(t0));
            uint32x4_t tres2 = vmovl_u16(vget_high_u16(t0));

            float32x4_t scale0 = vScale;
            float32x4_t scale1 = vScale;
            if (x == 8)
            {
                scale0 = vsetq_lane_f32(fScale[15], scale0, 0);
                scale0 = vsetq_lane_f32(fScale[20], scale0, 1);

                if (y == 0 || y == height - 1)
                {
                    scale0 = vsetq_lane_f32(fScale[9], scale0, 0);
                    scale0 = vsetq_lane_f32(fScale[12], scale0, 1);
                }

                if (y == 1 || y == height - 2)
                {
                    scale0 = vsetq_lane_f32(fScale[12], scale0, 0);
                    scale0 = vsetq_lane_f32(fScale[16], scale0, 1);
                }
            }

            float32x4_t vf1 = vmulq_f32(scale0, vcvtq_f32_u32(tres1));
            float32x4_t vf2 = vmulq_f32(scale1, vcvtq_f32_u32(tres2));
            tres1           = vcvtq_u32_f32(vf1);
            tres2           = vcvtq_u32_f32(vf2);
            t0              = vcombine_u16(vmovn_u32(tres1), vmovn_u32(tres2));
            vst1_u8(dRow + x - 8, vmovn_u16(t0));
        }

        x -= 8;
        x -= 2;

        int16_t   pprevx, prevx, rowx, nextx, nnextx;
        ptrdiff_t px = x;

        ptrdiff_t ploc;
        ploc   = borderInterpolate(px - 2, width);
        pprevx = ploc < 0 ? 5 * borderValue : sRow4[ploc] + sRow3[ploc] + sRow2[ploc] + sRow1[ploc] + sRow0[ploc];

        ploc  = borderInterpolate(px - 1, width);
        prevx = ploc < 0 ? 5 * borderValue : sRow4[ploc] + sRow3[ploc] + sRow2[ploc] + sRow1[ploc] + sRow0[ploc];

        rowx = sRow4[px] + sRow3[px] + sRow2[px] + sRow1[px] + sRow0[px];

        ploc  = borderInterpolate(px + 1, width);
        nextx = ploc < 0 ? 5 * borderValue : sRow4[ploc] + sRow3[ploc] + sRow2[ploc] + sRow1[ploc] + sRow0[ploc];

        x = px;

        uint16_t count;
        for (; x < width; x++, px++)
        {
            ptrdiff_t ploc = borderInterpolate(px + 2, width);
            nnextx = ploc < 0 ? 5 * borderValue : sRow4[ploc] + sRow3[ploc] + sRow2[ploc] + sRow1[ploc] + sRow0[ploc];

            count = (width - x) + (5 / 2);

            if (y == 0 || y == height - 1)
                count *= 3;
            else if (y == 1 || y == height - 2)
                count *= 4;
            else
                count *= 5;

            *(dRow + x) = static_cast<uint8_t>(
                static_cast<float>(pprevx + prevx + rowx + nextx + nnextx) * fScale[count]);

            pprevx = prevx;
            prevx  = rowx;
            rowx   = nextx;
            nextx  = nnextx;
        }
    }
}

void thMaskNeon(std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int height, int width, float bg, float fg)
{
    uint8_t* src_ptr = static_cast<uint8_t*>(src.data());
    uint8_t* dst_ptr = static_cast<uint8_t*>(dst.data());

    int size = height * width;

    int k = 0;

    // fg < bg
    if (fg <= bg)
    {
        uint8_t scale = fg * 255 + 1;
        uint8x16_t vscale = vdupq_n_u8(scale);

        uint8x16_t res;

        for (k = 0; k < size; k += 16)
        {
            uint8x16_t val;
            val = vld1q_u8(src_ptr + k);

            // >= 
            res = vcgeq_u8(val, vscale);

            vst1q_u8(dst_ptr + k, res);
        }

        k -= 16;

        for (; k < size; k++)
        {
            uint8_t val = src_ptr[k];
            if (val > scale)
            {
                dst_ptr[k] = 255;
            }
            else
            {
                dst_ptr[k] = 0;
            }
        }
    }
    else
    {
        float32_t v1_fg_bg = 1 / (fg - bg);
        float32_t v1_255 = 1.0f / 255.0f;
        float32_t v_bg = -bg;

        for (k = 0; k < size; k += 16)
        {
            uint8x16_t val;
            val = vld1q_u8(src_ptr + k);

            uint16x8_t v0 = vmovl_u8(vget_low_u8(val));
            uint16x8_t v1 = vmovl_u8(vget_high_u8(val));

            uint32x4_t t0 = vmovl_u16(vget_low_u16(v0));
            uint32x4_t t1 = vmovl_u16(vget_high_u16(v0));
            uint32x4_t t2 = vmovl_u16(vget_low_u16(v1));
            uint32x4_t t3 = vmovl_u16(vget_high_u16(v1));

            // div 255.0
            float32x4_t r0 = vmulq_f32(vdupq_n_f32(v1_255), vcvtq_f32_u32(t0));
            float32x4_t r1 = vmulq_f32(vdupq_n_f32(v1_255), vcvtq_f32_u32(t1));
            float32x4_t r2 = vmulq_f32(vdupq_n_f32(v1_255), vcvtq_f32_u32(t2));
            float32x4_t r3 = vmulq_f32(vdupq_n_f32(v1_255), vcvtq_f32_u32(t3));

            // sub bg
            r0 = vaddq_f32(vdupq_n_f32(v_bg), r0);
            r1 = vaddq_f32(vdupq_n_f32(v_bg), r1);
            r2 = vaddq_f32(vdupq_n_f32(v_bg), r2);
            r3 = vaddq_f32(vdupq_n_f32(v_bg), r3);

            // div (fg - bg)
            r0 = vmulq_f32(vdupq_n_f32(v1_fg_bg), r0);
            r1 = vmulq_f32(vdupq_n_f32(v1_fg_bg), r1);
            r2 = vmulq_f32(vdupq_n_f32(v1_fg_bg), r2);
            r3 = vmulq_f32(vdupq_n_f32(v1_fg_bg), r3);

            r0 = vminq_f32(r0, vdupq_n_f32(1.0f));
            r1 = vminq_f32(r1, vdupq_n_f32(1.0f));
            r2 = vminq_f32(r2, vdupq_n_f32(1.0f));
            r3 = vminq_f32(r3, vdupq_n_f32(1.0f));

            r0 = vmaxq_f32(r0, vdupq_n_f32(0.0f));
            r1 = vmaxq_f32(r1, vdupq_n_f32(0.0f));
            r2 = vmaxq_f32(r2, vdupq_n_f32(0.0f));
            r3 = vmaxq_f32(r3, vdupq_n_f32(0.0f));

            r0 = vfmaq_f32(vdupq_n_f32(0.5f), vdupq_n_f32(255.0f), r0);
            r1 = vfmaq_f32(vdupq_n_f32(0.5f), vdupq_n_f32(255.0f), r1);
            r2 = vfmaq_f32(vdupq_n_f32(0.5f), vdupq_n_f32(255.0f), r2);
            r3 = vfmaq_f32(vdupq_n_f32(0.5f), vdupq_n_f32(255.0f), r3);

            // cvt to u32
            t0 = vcvtq_u32_f32(r0);
            t1 = vcvtq_u32_f32(r1);
            t2 = vcvtq_u32_f32(r2);
            t3 = vcvtq_u32_f32(r3);

            // cvt to uint16
            v0 = vcombine_u16(vqmovn_u32(t0), vqmovn_u32(t1));
            v1 = vcombine_u16(vqmovn_u32(t2), vqmovn_u32(t3));

            // cvt to u8
            val = vcombine_u8(vqmovn_u16(v0), vqmovn_u16(v1));

            vst1q_u8(dst_ptr + k, val);
        }

        k -= 16;

        for (; k < size; ++k)
        {
            float temp = (src_ptr[k] * v1_255 - bg) * v1_fg_bg;
            if (temp > 1.0)
            {
                temp = 255.0;
            }
            else if (temp < 0.0)
            {
                temp = 0.0;
            }
            else
            {
                temp = temp * 255;
            }

            dst_ptr[k] = static_cast<unsigned char>(temp + 0.5);
        }
    }
}

void thMaskRef(std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int height, int width, float bg, float fg){
    uint8_t* src_ptr = static_cast<uint8_t*>(src.data());
    uint8_t* dst_ptr = static_cast<uint8_t*>(dst.data());
    float temp;

    float scale_255 = 1.0f / 255.0f;
    float s_fg_bg = 1.0f / (fg - bg);

    for (int r = 0; r < height * width; r++)
    {
        if (fg <= bg)
        {
            if (src_ptr[r] > fg * 255.0f)
            {
                temp = 255;
            }
            else
            {
                temp = 0;
            }
        }
        else
        {  // fg > bg
            temp = (src_ptr[r] * scale_255 - bg) * s_fg_bg;
            if (temp > 1.0f)
            {
                temp = 255.0f;
            }
            else if (temp < 0.0f)
            {
                temp = 0.0f;
            }
            else
            {
                temp = temp * 255;
            }
        }
        dst_ptr[r] = static_cast<unsigned char>(temp + 0.5);
    }
}

template<typename T, typename D>
void CreateRandom(std::vector<T>& in, D distribution)
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

#define H 280
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

    std::vector<uint8_t> in(H * W);
    std::vector<uint8_t> ot(H * W);

    std::memcpy(in.data(), input.data(), H * W * sizeof(uint8_t));
    for (int i = 0; i < LOOP; ++i)
    {
        Tic();
        BlurNeon5x5Final(in, ot, H, W);
        double time = Toc();

        if (time < min_time)
            min_time = time;

        total_time += time;
    }
    std::cout << "NeonBlur cost avg: " << total_time / LOOP << " min: " << min_time << std::endl;

    // check result;
    // std::cout << "check input:" << std::endl;
    // CheckResult(input, in, H, W);

    // std::cout << "check output:" << std::endl;
    // CheckResult(output, ot, H, W);

    // compute th_mask
    std::vector<uint8_t>thRefOut(H * W);
    std::vector<uint8_t>thNeonOt(H * W);

    Tic();
    thMaskRef(output, thRefOut, H, W, 0.8, 0.4);
    double itime = Toc();
    std::cout << "thRefMask cost: " << itime << " us" << std::endl;

    Tic();
    thMaskNeon(output, thNeonOt, H, W, 0.8, 0.4);
    double otime = Toc();
    std::cout << "NeonMask cost: " << otime << " us" << std::endl;

    // checkout result;
    CheckResult(thRefOut, thNeonOt, H, W);
    return 0;
}
