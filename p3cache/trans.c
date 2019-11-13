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

    int b, c, d, i, j, k, l, o;

	b = 8 / (M / 32);
	o = 8 / b; 

    for (j = 0; j < M; j+=8) {
        for (i = 0; i < N; i+=8) {
            for(c = 0; c < o; c++){
			for(d = 0; d < o; d++){
			for (k = 0; k < b; k++){
				for (l = 0; l < b; l++){
					B[j+k+(d*4)][i+l+(c*4)] = A[i+l+(c*4)][j+k+(d*4)];		
				}
			}
			}
			}
        }
	}
/*
    int i, j, k, l;

    for (j = 0; j < M; j+=8) {
        for (i = 0; i < N; i+=8) {
            for (k = 0; k < 8; k++){
				for (l = 0; l < 8; l++){
					B[j+k][i+l] = A[i+l][j+k];		
				}
			}
        }
	}
*/
}

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 */

char trans_desc[] = "Square by square trasnpose //no miss evict";
void trans(int M, int N, int A[N][M], int B[M][N])
{

//what do you want to do?
// find how many ints you can load into a block 
// b = 5, s  = 5
// 4 byte ints
// therefore 8 ints per block
// operate on matrices by 8*8 blocks
// a blocks inc column wise
// b blcoks row wise
 

    int b, i, j, k, l;

	b = 8 / (N / 32); 

    for (j = 0; j < M; j+=b) {
        for (i = 0; i < N; i+=b) {
            for (k = 0; k < b; k++){
				for (l = 0; l < b; l++){
					B[j+k][i+l] = A[i+l][j+k];		
				}
			}
        }
	}
}

char trans_desc_cont[] = "cont";
void trans_cont(int M, int N, int A[N][M], int B[M][N])
{

    int b, c, d, i, j, k, l, o;

	b = 8 / (N / 32);
	o = 8 / b; 

    for (j = 0; j < M; j+=8) {
        for (i = 0; i < N; i+=8) {
            for(c = 0; c < o; c++){
			for(d = 0; d < o; d++){
			for (k = 0; k < b; k++){
				for (l = 0; l < b; l++){
					B[j+k+(d*4)][i+l+(c*4)] = A[i+l+(c*4)][j+k+(d*4)];		
				}
			}
			}
			}
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

    registerTransFunction(trans_cont, trans_desc_cont);
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

