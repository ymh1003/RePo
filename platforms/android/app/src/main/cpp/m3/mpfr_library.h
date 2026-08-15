#ifndef MPFR_LIBRARY_H
#define MPFR_LIBRARY_H

#include <math.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpfr.h>
#include "di_library.h"

extern int PREC;

mpfr_t* allocate_mpfr_d(double val);
void log_binary_mpfr_debug(int id, const char *op_name, double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double z_v, mpfr_ptr z_e);
void log_unary_mpfr_debug(int id, const char *op_name, double x_v, mpfr_ptr x_e, double z_v, mpfr_ptr z_e);
int checkErrorBits_mpfr(double fp_v, mpfr_ptr real_v, int THRESHOLD, OP op);
void checkAndLogError_mpfr(bool type, double *z_f, mpfr_ptr *z_r, OP op, const char *op_name);

void finalize_binary_op_mpfr(bool type, const char* op_name, OP op_type, int op_id, double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);
void finalize_unary_op_mpfr(bool type, const char* op_name, OP op_type, int op_id, double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);
/* Arithmetic operations - double */
void add_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);
void sub_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r, bool check);
void sub_dmp_check_true(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);
void sub_dmp_check_false(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);
void mul_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);
void div_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);

/* Unary functions - double */
void fabs_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);
void ceil_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);
void floor_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);
void trunc_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);
void sqrt_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);
void rint_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);
void neg_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);

/* Comparison operators - double */
typedef int (*comp_fn_mp)(mpfr_ptr, mpfr_ptr);
int equal_mp(mpfr_ptr x, mpfr_ptr y);
int not_equal_mp(mpfr_ptr x, mpfr_ptr y);
int less_than_mp(mpfr_ptr x, mpfr_ptr y);
int less_than_or_equal_mp(mpfr_ptr x, mpfr_ptr y);
int greater_than_mp(mpfr_ptr x, mpfr_ptr y);
int greater_than_or_equal_mp(mpfr_ptr x, mpfr_ptr y);

int compare_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, comp_fn_d op_d, comp_fn_mp op_mp, char *op_name);
int eq_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r);
int neq_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r);
int less_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r);
int less_eq_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r);
int greater_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r);
int greater_eq_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r);

/* Utility functions - double */
void min_f64_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);
void max_f64_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);
void copysign_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);

/* Float precision functions */
void add_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);
void sub_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r, bool check);
void sub_fmp_check_true(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);
void sub_fmp_check_false(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);
void mul_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);
void div_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);

/* Unary functions - float */
void fabs_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);
void ceil_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);
void floor_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);
void trunc_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);
void sqrt_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);
void rint_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);
void neg_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r);

/* Comparison operators - float */
int compare_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, comp_fn_f op_f, comp_fn_mp op_mp, char *op_name);
int eq_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r);
int neq_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r);
int less_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r);
int less_eq_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r);
int greater_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r);
int greater_eq_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r);

/* Utility functions - float */
void min_f32_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);
void max_f32_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);
void copysign_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r);

#endif /* MPFR_LIBRARY_H */