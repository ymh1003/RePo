#ifndef MPFR_DD_LIBRARY_H
#define MPFR_DD_LIBRARY_H

#include <math.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpfr.h>
#include "di_library.h"
#include "mpfr_library.h"

int checkErrorBits_md(double val, mpfr_ptr err, int THRESHOLD, OP op);
void checkAndLogError(OP op, double z_v, mpfr_ptr z_e);

void finalize_binary_op_md(const char* op_name, OP op_type, int op_id, double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);
void finalize_unary_op_md(const char* op_name, OP op_type, int op_id, double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);

/* Arithmetic operations - double */
void add_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);
void sub_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e, bool check);
void sub_dmd_check_true(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);
void sub_dmd_check_false(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);
void mul_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);
void div_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);

/* Unary functions - double */
void fabs_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);
void ceil_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);
void floor_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);
void trunc_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);
void sqrt_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);
void rint_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);
void neg_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);

/* Comparison operators - double */
int compare_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, comp_fn_d op_d, comp_fn_mp op_mp, char *op_name);
int eq_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e);
int neq_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e);
int less_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e);
int less_eq_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e);
int greater_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e);
int greater_eq_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e);

/* Utility functions - double */
void min_f64_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);
void max_f64_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);
void copysign_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);

/* Float precision functions */
void add_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);
void sub_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e, bool check);
void sub_fmd_check_true(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);
void sub_fmd_check_false(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);
void mul_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);
void div_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);

/* Unary functions - float */
void fabs_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);
void ceil_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);
void floor_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);
void trunc_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);
void sqrt_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);
void rint_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);
void neg_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e);

/* Comparison operators - float */
int compare_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, comp_fn_f op_f, comp_fn_mp op_mp, char *op_name);
int eq_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e);
int neq_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e);
int less_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e);
int less_eq_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e);
int greater_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e);
int greater_eq_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e);

/* Utility functions - float */
void min_f32_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);
void max_f32_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);
void copysign_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e);

#endif /* MPFR_DD_LIBRARY_H */