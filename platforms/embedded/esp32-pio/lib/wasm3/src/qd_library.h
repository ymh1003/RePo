#ifndef QD_LIBRARY_H
#define QD_LIBRARY_H

#include <math.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <qd/c_qd.h>
#include "di_library.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int comp_res;
extern int *res;
typedef double* qd_t;

int checkErrorBits_qd(double fp_v, qd_t real_v, int THRESHOLD, OP op);
void checkAndLogError_QD(bool type, double *z_f, qd_t *z_r, OP op, const char *op_name);
/* Arithmetic operations - double */
void add_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r);
void sub_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r, bool check);
void sub_dqd_check_true(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r);
void sub_dqd_check_false(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r);
void mul_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r);
void div_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r);

/* Unary functions - double */
void fabs_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r);
void ceil_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r);
void floor_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r);
void trunc_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r);
void sqrt_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r);
void rint_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r);
void neg_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r);

/* Comparison operators - double */
typedef int (*comp_fn_qd)(qd_t, qd_t);
int equal_qd(qd_t x, qd_t y);
int not_equal_qd(qd_t x, qd_t y);
int less_than_qd(qd_t x, qd_t y);
int less_than_or_equal_qd(qd_t x, qd_t y);
int greater_than_qd(qd_t x, qd_t y);
int greater_than_or_equal_qd(qd_t x, qd_t y);

int compare_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, comp_fn_d op_d, comp_fn_qd op_qd, const char *op_name);
int eq_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r);
int neq_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r);
int less_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r);
int less_eq_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r);
int greater_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r);
int greater_eq_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r);

/* Utility functions - double */
void min_f64_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r);
void max_f64_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r);
void copysign_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r);

/* Float precision functions */
void add_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r);
void sub_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r, bool check);
void sub_fqd_check_true(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r);
void sub_fqd_check_false(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r);
void mul_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r);
void div_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r);

/* Unary functions - float */
void fabs_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r);
void ceil_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r);
void floor_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r);
void trunc_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r);
void sqrt_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r);
void rint_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r);
void neg_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r);

/* Comparison operators - float */
int compare_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, comp_fn_f op_f, comp_fn_qd op_qd, const char *op_name);
int eq_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r);
int neq_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r);
int less_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r);
int less_eq_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r);
int greater_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r);
int greater_eq_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r);

/* Utility functions - float */
void min_f32_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r);
void max_f32_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r);
void copysign_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r);

#ifdef __cplusplus
}
#endif

#endif /* QD_LIBRARY_H */