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

#define MIN(a, b) ((a) < (b) ? (a) : (b))

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

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
    int temp[8];
    int i, j, k, l;
    int block_size = N % 8 ? 16 : 8;    // 32x32 and 64x64 get the block size is 8, while 61x67 get the block size is 16
    for (i = 0; i < N; i += block_size) // rol
    {
        for (j = 0; j < M; j += block_size) // column
        {
            if (N % 8) // for 61x67 case
            {
                for (k = i; k < MIN(i + block_size, N); k++) // rol
                {
                    for (l = j; l < MIN(j + block_size, M); l++) // traverse the elem in a block(16x16), transpose it
                        B[l][k] = A[k][l];
                }
            }
            else // for 32x2, 64x64 case, divide the matrix into 8x8 block matrix
            {
                for (k = i; k < i + 4; k++) // previous 4 rol in B-block store previous 4 rol in A-block, left half is right
                {
                    for (l = 0; l < 8; l++)
                        temp[l] = A[k][j + l];
                    for (l = 0; l < 4; l++) // one rol spilit into 2 column
                    {
                        B[j + l][k] = temp[l];
                        B[j + l][k + 4] = temp[l + 4];
                    }
                }
                for (k = j + 4; k < j + 8; k++)
                {
                    for (l = 0; l < 4; l++) // temp[0 -> 4] = B[k - 4][i + 4 -> i + 8], these are the data placed in B-block left-down
                        temp[l] = B[k - 4][i + l + 4];
                    for (l = 4; l < 8; l++) // temp[4 -> 8] = A[i + 4 -> i + 8][k], these are the data placed in B-block right-down
                        temp[l] = A[i + l][k];
                    for (l = 4; l < 8; l++) // transpose the data in A-block left-down into B-block right-up
                        B[k - 4][i + l] = A[i + l][k - 4];
                    for (l = 0; l < 8; l++) // put the temp[0 -> 8] to B-block, form the B-block left-down and right-down
                        B[k][i + l] = temp[l];
                }
            }
        }
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

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; j++)
        {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
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
    registerTransFunction(trans, trans_desc);
}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; ++j)
        {
            if (A[i][j] != B[j][i])
            {
                return 0;
            }
        }
    }
    return 1;
}
