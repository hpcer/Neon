#include <arm_neon.h>
#include <iostream>

typedef unsigned char uint8_t;

void BlurRef(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int height, int width, int kernel_size)
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

template <typename T, typename D>
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

int main()
{
    std::uniform_int_distribution<int32_t>(low, high);

    std::vector<uint8_t> input(H * W);
    std::vector<uint8_t> output(H * W);

    CreateRandom(input, std::uniform_int_distribution<int8_t>(0, 255));
    CreateRandom(output, std::uniform_int_distribution<int8_t>(0, 255));

    BlurRef(input, output, H, W, 5);

    return 0;
}
