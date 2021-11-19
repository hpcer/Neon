/**
 *
 * Copyright [2021] <hpcers>
 * */

#include "Test/time.h"

#include <arm_neon.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#define BLOCK_SIZE 512


void matrix_multiply_c(float32_t* A, float32_t* B, float32_t* C, uint32_t n, uint32_t m, uint32_t k)
{
    for (int i_idx = 0; i_idx < n; i_idx++)
    {
        for (int j_idx = 0; j_idx < m; j_idx++)
        {
            C[n * j_idx + i_idx] = 0;
            for (int k_idx = 0; k_idx < k; k_idx++)
            {
                C[n * j_idx + i_idx] += A[n * k_idx + i_idx] * B[k * j_idx + k_idx];
            }
        }
    }
}

struct Float32x4_t
{
    float32_t s[4];
};

#define MovF32x4(r, v) \
    r.s[0] = v;        \
    r.s[1] = v;        \
    r.s[2] = v;        \
    r.s[3] = v;

#define VLoadF32x4(r, p) \
    r.s[0] = p[0];       \
    r.s[1] = p[1];       \
    r.s[2] = p[2];       \
    r.s[3] = p[4];

#define FMAF32x4Impl(a, b, c) \
    c.s[0] += a.s[0] * b;     \
    c.s[1] += a.s[1] * b;     \
    c.s[2] += a.s[2] * b;     \
    c.s[3] += a.s[3] * b;

#define FMAF32x4(a, b, c, index) FMAF32x4Impl(a, b.s[index], c)

void matrix_multiply_c_b(float32_t* A, float32_t* B, float32_t* C, uint32_t n, uint32_t m, uint32_t k)
{
    Float32x4_t a[4];
    Float32x4_t b[4];
    Float32x4_t c[4];

    for (int i_idx = 0; i_idx < n; i_idx += 4)
    {
        for (int j_idx = 0; j_idx < m; j_idx += 4)
        {

            MovF32x4(c[0], 0);
            MovF32x4(c[1], 0);
            MovF32x4(c[2], 0);
            MovF32x4(c[3], 0);

            for (int k_idx = 0; k_idx < k; k_idx += 4)
            {
                int A_idx = i_idx + n * k_idx;
                int B_idx = k * j_idx + k_idx;

                VLoadF32x4(a[0], (A + A_idx));
                VLoadF32x4(a[1], (A + A_idx + n));
                VLoadF32x4(a[2], (A + A_idx + 2 * n));
                VLoadF32x4(a[3], (A + A_idx + 3 * n));

                // B
                VLoadF32x4(b[0], (B + B_idx));

                // FMAF32x4(c[0], )


                C[n * j_idx + i_idx] += A[n * k_idx + i_idx] * B[k * j_idx + k_idx];
            }
        }
    }
}

void matrix_multiply_neon(float32_t* A, float32_t* B, float32_t* C, uint32_t n, uint32_t m, uint32_t k)
{
    /*
     * Multiply matrices A and B, store the result in C.
     * It is the user's responsibility to make sure the matrices are compatible.
     */

    int A_idx;
    int B_idx;
    int C_idx;

    // these are the columns of a 4x4 sub matrix of A
    float32x4_t A0;
    float32x4_t A1;
    float32x4_t A2;
    float32x4_t A3;

    // these are the columns of a 4x4 sub matrix of B
    float32x4_t B0;
    float32x4_t B1;
    float32x4_t B2;
    float32x4_t B3;

    // these are the columns of a 4x4 sub matrix of C
    float32x4_t C0;
    float32x4_t C1;
    float32x4_t C2;
    float32x4_t C3;

    for (int i_idx = 0; i_idx < n; i_idx += 4)
    {
        for (int j_idx = 0; j_idx < m; j_idx += 4)
        {
            // Zero accumulators before matrix op
            C0 = vmovq_n_f32(0);
            C1 = vmovq_n_f32(0);
            C2 = vmovq_n_f32(0);
            C3 = vmovq_n_f32(0);
            for (int k_idx = 0; k_idx < k; k_idx += 4)
            {
                // Compute base index to 4x4 block
                A_idx = i_idx + n * k_idx;
                B_idx = k * j_idx + k_idx;

                // Load most current A values in row
                A0 = vld1q_f32(A + A_idx);
                A1 = vld1q_f32(A + A_idx + n);
                A2 = vld1q_f32(A + A_idx + 2 * n);
                A3 = vld1q_f32(A + A_idx + 3 * n);

                // Multiply accumulate in 4x1 blocks, i.e. each column in C
                B0 = vld1q_f32(B + B_idx);
                C0 = vfmaq_laneq_f32(C0, A0, B0, 0);
                C0 = vfmaq_laneq_f32(C0, A1, B0, 1);
                C0 = vfmaq_laneq_f32(C0, A2, B0, 2);
                C0 = vfmaq_laneq_f32(C0, A3, B0, 3);

                B1 = vld1q_f32(B + B_idx + k);
                C1 = vfmaq_laneq_f32(C1, A0, B1, 0);
                C1 = vfmaq_laneq_f32(C1, A1, B1, 1);
                C1 = vfmaq_laneq_f32(C1, A2, B1, 2);
                C1 = vfmaq_laneq_f32(C1, A3, B1, 3);

                B2 = vld1q_f32(B + B_idx + 2 * k);
                C2 = vfmaq_laneq_f32(C2, A0, B2, 0);
                C2 = vfmaq_laneq_f32(C2, A1, B2, 1);
                C2 = vfmaq_laneq_f32(C2, A2, B2, 2);
                C2 = vfmaq_laneq_f32(C2, A3, B2, 3);

                B3 = vld1q_f32(B + B_idx + 3 * k);
                C3 = vfmaq_laneq_f32(C3, A0, B3, 0);
                C3 = vfmaq_laneq_f32(C3, A1, B3, 1);
                C3 = vfmaq_laneq_f32(C3, A2, B3, 2);
                C3 = vfmaq_laneq_f32(C3, A3, B3, 3);
            }
            // Compute base index for stores
            C_idx = n * j_idx + i_idx;
            vst1q_f32(C + C_idx, C0);
            vst1q_f32(C + C_idx + n, C1);
            vst1q_f32(C + C_idx + 2 * n, C2);
            vst1q_f32(C + C_idx + 3 * n, C3);
        }
    }
}

void matrix_multiply_4x4_neon(float32_t* A, float32_t* B, float32_t* C)
{
    // these are the columns A
    float32x4_t A0;
    float32x4_t A1;
    float32x4_t A2;
    float32x4_t A3;

    // these are the columns B
    float32x4_t B0;
    float32x4_t B1;
    float32x4_t B2;
    float32x4_t B3;

    // these are the columns C
    float32x4_t C0;
    float32x4_t C1;
    float32x4_t C2;
    float32x4_t C3;

    A0 = vld1q_f32(A);
    A1 = vld1q_f32(A + 4);
    A2 = vld1q_f32(A + 8);
    A3 = vld1q_f32(A + 12);

    // Zero accumulators for C values
    C0 = vmovq_n_f32(0);
    C1 = vmovq_n_f32(0);
    C2 = vmovq_n_f32(0);
    C3 = vmovq_n_f32(0);

    // Multiply accumulate in 4x1 blocks, i.e. each column in C
    B0 = vld1q_f32(B);
    C0 = vfmaq_laneq_f32(C0, A0, B0, 0);
    C0 = vfmaq_laneq_f32(C0, A1, B0, 1);
    C0 = vfmaq_laneq_f32(C0, A2, B0, 2);
    C0 = vfmaq_laneq_f32(C0, A3, B0, 3);
    vst1q_f32(C, C0);

    B1 = vld1q_f32(B + 4);
    C1 = vfmaq_laneq_f32(C1, A0, B1, 0);
    C1 = vfmaq_laneq_f32(C1, A1, B1, 1);
    C1 = vfmaq_laneq_f32(C1, A2, B1, 2);
    C1 = vfmaq_laneq_f32(C1, A3, B1, 3);
    vst1q_f32(C + 4, C1);

    B2 = vld1q_f32(B + 8);
    C2 = vfmaq_laneq_f32(C2, A0, B2, 0);
    C2 = vfmaq_laneq_f32(C2, A1, B2, 1);
    C2 = vfmaq_laneq_f32(C2, A2, B2, 2);
    C2 = vfmaq_laneq_f32(C2, A3, B2, 3);
    vst1q_f32(C + 8, C2);

    B3 = vld1q_f32(B + 12);
    C3 = vfmaq_laneq_f32(C3, A0, B3, 0);
    C3 = vfmaq_laneq_f32(C3, A1, B3, 1);
    C3 = vfmaq_laneq_f32(C3, A2, B3, 2);
    C3 = vfmaq_laneq_f32(C3, A3, B3, 3);
    vst1q_f32(C + 12, C3);
}

void print_matrix(float32_t* M, uint32_t cols, uint32_t rows)
{
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            printf("%f ", M[i * rows + j]);
        }
        printf("\n");
    }
    printf("\n");
}

void matrix_init_rand(float32_t* M, uint32_t numvals)
{
    for (int i = 0; i < numvals; i++)
    {
        M[i] = (float)rand() / (float)(RAND_MAX);
    }
}

void matrix_init(float32_t* M, uint32_t cols, uint32_t rows, float32_t val)
{
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            M[j * rows + i] = val;
        }
    }
}

bool f32comp_noteq1(float32_t a, float32_t b)
{
    if (fabs(a - b) < 0.000001)
    {
        return false;
    }
    return true;
}

float32_t f32comp_noteq(float32_t a, float32_t b)
{
    return fabs(a - b);
}

bool matrix_comp(float32_t* A, float32_t* B, uint32_t rows, uint32_t cols)
{
    float32_t a;
    float32_t b;

    float32_t max_diff = 0.0f;
    float32_t avg_diff = 0.0f;

    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            a = A[rows * j + i];
            b = B[rows * j + i];

            // if (f32comp_noteq(a, b))
            // {
            //     printf("i=%d, j=%d, A=%f, B=%f\n", i, j, a, b);
            //     return false;
            // }
            float32_t diff = f32comp_noteq(a, b);
            if (diff > max_diff)
                max_diff = diff;

            avg_diff += diff;
        }
    }

    printf("max_diff: %f, avg_diff: %f\n", max_diff, avg_diff / rows / cols);
    return true;
}

int main()
{
    uint32_t n = 2 * BLOCK_SIZE;   // rows in A
    uint32_t m = 2 * BLOCK_SIZE;   // cols in B
    uint32_t k = 2 * BLOCK_SIZE;   // cols in a and rows in b

    // float32_t A[n * k];
    // float32_t B[k * m];
    // float32_t C[n * m];
    // float32_t D[n * m];
    // float32_t E[n * m];

    std::vector<float32_t> vA(n * k);
    std::vector<float32_t> vB(n * k);
    std::vector<float32_t> vC(n * k);
    std::vector<float32_t> vD(n * k);
    std::vector<float32_t> vE(n * k);

    float32_t* A = vA.data();
    float32_t* B = vB.data();
    float32_t* C = vC.data();
    float32_t* D = vD.data();
    float32_t* E = vE.data();

    bool c_eq_asm;
    bool c_eq_neon;

    matrix_init_rand(A, n * k);
    matrix_init_rand(B, k * m);
    matrix_init(C, n, m, 0);

    // print_matrix(A, k, n);
    // print_matrix(B, m, k);
    // print_matrix(C, n, m);

    Tic();

    matrix_multiply_c(A, B, E, n, m, k);

    printf("c cost: %lf ms\n", Toc() / 1000.0f);
    // printf("C\n");
    // print_matrix(E, n, m);
    printf("===============================\n");

    Tic();
    matrix_multiply_neon(A, B, D, n, m, k);
    printf("neon cost: %lf ms\n", Toc() / 1000.0f);

    // printf("Neon\n");
    // print_matrix(D, n, m);
    c_eq_neon = matrix_comp(E, D, n, m);
    printf("Neon equal to C? %d\n", c_eq_neon);
    printf("===============================\n");
}
