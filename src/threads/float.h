#ifndef THREAD_FLOAT_H
#define THREAD_FLOAT_Ho


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

// #define F_SHIFT_AMOUNT 16
// #define F_CONST(a) ((float_t)((a)<<F_SHIFT_AMOUNT))
// #define F_ADD(a,b) ((a)+(b))
// #define F_ADD_MIX(a,b) ((a)+((b)<<F_SHIFT_AMOUNT))
// #define F_SUB(a,b) ((a)-(b))
// #define F_SUB_MIX(a,b) ((a)-((b)<<F_SHIFT_AMOUNT))
// #define F_MULT_MIX(a,b) ((a)*(b))
// #define F_DIV_MIX(a,b)  ((a)/(b))
// #define F_MULT(a,b)     ((float_t)(((int64_t)(a))*(b)>>F_SHIFT_AMOUNT))
// #define F_DIV(a,b)      ((float_t)((((int64_t)(a))<<F_SHIFT_AMOUNT)/(b)))
// #define F_INT_PART(a)   ((a)>>F_SHIFT_AMOUNT)
// #define F_ROUND(a)      ((a) >= 0 ? (((a)+(1<<(F_SHIFT_AMOUNT-1)))>>F_SHIFT_AMOUNT) \
//                           : (((a)-(1<<(F_SHIFT_AMOUNT-1)))>>F_SHIFT_AMOUNT))

#endif
