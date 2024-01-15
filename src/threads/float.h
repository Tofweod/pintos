#ifndef THREAD_FLOAT_H
#define THREAD_FLOAT_H


#include <stdint.h>
typedef int32_t fp_t;

#define FP_SIZE 32
#define FP_P 17
#define FP_Q 14
#define FP_F (1<<FP_Q)

#define N_2_FP(n) ((n) * FP_F)

#define FP_2_N_0(x) ((x) / FP_F)

#define FP_2_N_I(x) (((x) > 0) ? (FP_2_N_0(x) + (FP_F / 2)) : (FP_2_N_0((x) - (FP_F /2))))

#define ADD_X_Y(x,y) ((x)+(y))

#define SUB_X_Y(x,y) ((x)-(y))

#define ADD_X_N(x,n) ((x) + N_2_FP(n))

#define SUB_X_N(x,n) ((x) - N_2_FP(n))

#define MUL_X_Y(x,y) ((int32_t)(((int64_t)(x)) * (y) / FP_F))

#define MUL_X_N(x,n) ((x) * (n))

#define DIV_X_Y(x,y) ((int32_t)(((int64_t)(x)) * FP_F / (y)))

#define DIV_X_N(x,n) ((x) / (n))

#endif
