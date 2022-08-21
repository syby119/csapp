/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

#define SWAP_A_TO_B(i, j) (B[j][i] = A[i][j])

int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void trans_block(int M, int N, int A[N][M], int B[M][N]);
void trans_32_32_block(int M, int N, int A[N][M], int B[M][N]);
void trans_64_64_block(int M, int N, int A[N][M], int B[M][N]);
void trans_61_67_block(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (M == 32 && N == 32) {
        trans_32_32_block(M, N, A, B);
    } else if (M == 64 && N == 64) {
        trans_64_64_block(M, N, A, B);
    } else {
        trans_61_67_block(M, N, A, B);
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }
}


char trans_32_32_desc[] = "32 x 32 transpose";
void trans_32_32_block(int M, int N, int A[N][M], int B[M][N]) {
    int i, ii, jj;
    int t0, t1, t2, t3, t4, t5, t6, t7;

    // 16 * (16 - 4) + 23 * 4 = 284 
    for (ii = 0; ii < N; ii += 8) {
        for (jj = 0; jj < M; jj += 8) {
            for (i = 0; i < 8; ++i) {
                t0 = A[ii + i][jj + 0];
                t1 = A[ii + i][jj + 1];
                t2 = A[ii + i][jj + 2];
                t3 = A[ii + i][jj + 3];
                t4 = A[ii + i][jj + 4];
                t5 = A[ii + i][jj + 5];
                t6 = A[ii + i][jj + 6];
                t7 = A[ii + i][jj + 7];

                B[jj + 0][ii + i] = t0;
                B[jj + 1][ii + i] = t1;
                B[jj + 2][ii + i] = t2;
                B[jj + 3][ii + i] = t3;
                B[jj + 4][ii + i] = t4;
                B[jj + 5][ii + i] = t5;
                B[jj + 6][ii + i] = t6;
                B[jj + 7][ii + i] = t7;
            }
        }
    }
}

char trans_64_64_desc[] = "64 x 64 transpose";
void trans_64_64_block(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, ii, jj;
    int t0, t1, t2, t3, t4, t5, t6, t7;

    // 18 * (64 - 8) + 32 * 8 = 1264
    for (ii = 0; ii < N; ii += 8) {
        for (jj = 0; jj < M; jj += 8) {
            if (ii != jj) {
                /* non diagnonal block: 18 misses each block */
                // 1. read the row[0] of the block in A
                // 1.1 transpose the first 4 element to B
                for (i = 0; i < 4; ++i) {
                    B[jj + i][ii] = A[ii][jj + i];
                }
                // 1.2 store the last 4 element in temp values
                t0 = A[ii][jj + 4];
                t1 = A[ii][jj + 5];
                t2 = A[ii][jj + 6];
                t3 = A[ii][jj + 7];

                // 2. read the row[1] of the block in A
                // 2.1 transpose the first 4 element to B
                for (i = 0; i < 4; ++i) {
                    B[jj + i][ii + 1] = A[ii + 1][jj + i];
                }
                // 2.2 store the last 4 element in temp values
                t4 = A[ii + 1][jj + 4];
                t5 = A[ii + 1][jj + 5];
                t6 = A[ii + 1][jj + 6];
                t7 = A[ii + 1][jj + 7];

                // 3. read the first 4 elements of row[2] ~ row[7] in the block of A
                // 3.1 transpose them to B
                for (i = 2; i < 8; ++i) {
                    for (j = 0; j < 4; ++j) {
                        B[jj + j][ii + i] = A[ii + i][jj + j];
                    }
                }

                // 4. read the last 4 elements of row[7] ~ row[2] in the block of A
                // 4.1 transpose them to B
                for (i = 7; i > 1; --i) {
                    for (j = 4; j < 8; ++j) {
                        B[jj + j][ii + i] = A[ii + i][jj + j];
                    }
                }

                // 5. store the temp values to B
                B[jj + 4][ii] = t0;
                B[jj + 5][ii] = t1;
                B[jj + 6][ii] = t2;
                B[jj + 7][ii] = t3;
                B[jj + 4][ii + 1] = t4;
                B[jj + 5][ii + 1] = t5;
                B[jj + 6][ii + 1] = t6;
                B[jj + 7][ii + 1] = t7;
            } else {
                /* diagnonal block: 32 misses each */
                for (i = 0; i < 8; i += 4) {
                    for (j = 0; j < 8; j += 4) {
                        // A: [0, 1] x [0, 3]
                        t0 = A[ii + i + 0][jj + j + 0];
                        t1 = A[ii + i + 0][jj + j + 1];
                        t4 = A[ii + i + 0][jj + j + 2];
                        t5 = A[ii + i + 0][jj + j + 3];
                        t2 = A[ii + i + 1][jj + j + 0];
                        t3 = A[ii + i + 1][jj + j + 1];
                        t6 = A[ii + i + 1][jj + j + 2];
                        t7 = A[ii + i + 1][jj + j + 3];

                        // B: [0, 1] x [0, 1]
                        B[jj + j + 0][ii + i + 0] = t0;
                        B[jj + j + 0][ii + i + 1] = t2;
                        B[jj + j + 1][ii + i + 0] = t1;
                        B[jj + j + 1][ii + i + 1] = t3;

                        // A: [2, 3] x [0, 3]
                        B[jj + j + 0][ii + i + 2] = A[ii + i + 2][jj + j + 0];
                        B[jj + j + 1][ii + i + 2] = A[ii + i + 2][jj + j + 1];
                        B[jj + j + 0][ii + i + 3] = A[ii + i + 3][jj + j + 0];
                        B[jj + j + 1][ii + i + 3] = A[ii + i + 3][jj + j + 1];
                        t0 = A[ii + i + 2][jj + j + 2];
                        t1 = A[ii + i + 2][jj + j + 3];
                        t2 = A[ii + i + 3][jj + j + 2];
                        t3 = A[ii + i + 3][jj + j + 3];

                        B[jj + j + 2][ii + i + 0] = t4;
                        B[jj + j + 2][ii + i + 1] = t6;
                        B[jj + j + 2][ii + i + 2] = t0;
                        B[jj + j + 2][ii + i + 3] = t2;

                        B[jj + j + 3][ii + i + 0] = t5;
                        B[jj + j + 3][ii + i + 1] = t7;
                        B[jj + j + 3][ii + i + 2] = t1;
                        B[jj + j + 3][ii + i + 3] = t3;
                    }
                }
            }
        }
    }
}


char trans_61_67_desc[] = "61 x 67 transpose";
void trans_61_67_block(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, ii = 0, jj = 0;
    int t0, t1, t2, t3, t4, t5, t6, t7;

    // 8 x 8 blocking
    for (jj = 0; jj < M - 7; jj += 8) {
        for (ii = 0; ii < N - 7; ii += 8) {
            for (i = 0; i < 8; ++i) {
                t0 = A[ii + i][jj + 0];
                t1 = A[ii + i][jj + 1];
                t2 = A[ii + i][jj + 2];
                t3 = A[ii + i][jj + 3];
                t4 = A[ii + i][jj + 4];
                t5 = A[ii + i][jj + 5];
                t6 = A[ii + i][jj + 6];
                t7 = A[ii + i][jj + 7];

                B[jj + 0][ii + i] = t0;
                B[jj + 1][ii + i] = t1;
                B[jj + 2][ii + i] = t2;
                B[jj + 3][ii + i] = t3;
                B[jj + 4][ii + i] = t4;
                B[jj + 5][ii + i] = t5;
                B[jj + 6][ii + i] = t6;
                B[jj + 7][ii + i] = t7;
            }
        }
    }

    // the bottom remaining part
    for (i = ii; i < N; ++i) {
        for (j = 0; j < jj; j += 8) {
            t0 = A[i][j + 0];
            t1 = A[i][j + 1];
            t2 = A[i][j + 2];
            t3 = A[i][j + 3];
            t4 = A[i][j + 4];
            t5 = A[i][j + 5];
            t6 = A[i][j + 6];
            t7 = A[i][j + 7];
            B[j + 0][i] = t0;
            B[j + 1][i] = t1;
            B[j + 2][i] = t2;
            B[j + 3][i] = t3;
            B[j + 4][i] = t4;
            B[j + 5][i] = t5;
            B[j + 6][i] = t6;
            B[j + 7][i] = t7;
        }
    }

    // the right remaining part 
    for (i = 0; i < N; ++i) {
        t0 = A[i][jj + 0];
        t1 = A[i][jj + 1];
        t2 = A[i][jj + 2];
        t3 = A[i][jj + 3];
        t4 = A[i][jj + 4];
        B[jj + 0][i] = t0;
        B[jj + 1][i] = t1;
        B[jj + 2][i] = t2;
        B[jj + 3][i] = t3;
        B[jj + 4][i] = t4;
    }
}


/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    // registerTransFunction(trans, trans_desc); 
}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}