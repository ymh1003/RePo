#ifndef DI_LIBRARY_H
#define DI_LIBRARY_H

#include <math.h>
#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "uthash.h"
#include <time.h>
#include "config_env.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ADD,
    SUB,
    MUL,
    DIV,
    FABS,
    CEIL,
    FLOOR,
    TRUNC,
    SQRT,
    RINT,
    NEG,
    COPYSIGN
} OP;

extern int *debug_ids; // Ops which are used to check determinism
extern int n_debug_ids; // Number of ops in debug_ids
extern int *log_fn_ids;
extern int n_log_fn_ids;
extern int *log_fp_ids;
extern int n_log_fp_ids;
extern int ids_buf[512];
extern int MAX_BUF_SIZE;
extern int *cur_size; // Position in ids_buf where the current op is stored

bool is_magic_number(double v);
void log_value_or_special(FILE *out, double v);

void print_ids_buf();
void append_to_ids_buf(int id1, int id2);
int cmp_int(const void *a, const void *b);
bool contains_bsearch(int *arr, size_t n, int key);

extern int OP_ID;
extern int THRESHOLD;
extern int HIGH_ROUNDING_ERROR, INF, BRANCH_FLIP, CC_INCLUDED, CC_NEW;
extern int ADD_TT, SUB_TT, MUL_TT, DIV_TT, FABS_TT, CEIL_TT, FLOOR_TT, TRUNC_TT, SQRT_TT, RINT_TT, NEG_TT, COPYSIGN_TT;
extern int ADD_ERR, SUB_ERR, MUL_ERR, DIV_ERR, FABS_ERR, CEIL_ERR, FLOOR_ERR, TRUNC_ERR, SQRT_ERR, RINT_ERR, COPYSIGN_ERR;
unsigned long long read_fp_cycle_counter(void);
void accumulate_fp_cycles(unsigned long long start);
void reset_fp_cycle_counter(void);
void printFpCycleInfo(void);
void load_debug_ids(const char *path);
void load_log_fn_ids(const char *path);
void load_log_fp_ids(const char *path);
void simple_log_bin(int id, const char *op_name, double x_v, double y_v, double z_v);
void simple_log_un(int id, const char *op_name, double x_v, double z_v);
void log_binary_op(int id, const char *op_name, double x_v, double x_e, double y_v, double y_e, double z_v, double z_e);
void log_unary_op(int id, const char *op_name, double x_v, double x_e, double z_v, double z_e);
void log_binary_fn_op(int id, const char *op_name, double x_v, double x_e, double y_v, double y_e, double z_v, double z_e);
void log_unary_fn_op(int id, const char *op_name, double x_v, double x_e, double z_v, double z_e);
void log_binary_fp_op(int id, const char *op_name, double x_v, double x_e, double y_v, double y_e, double z_v, double z_e);
void log_unary_fp_op(int id, const char *op_name, double x_v, double x_e, double z_v, double z_e);
void log_binary_op_if_debug(int id, const char *op_name, double x_v, double y_v, double z_v);
void log_unary_op_if_debug(int id, const char *op_name, double x_v, double z_v);
void log_binary_op_if_listed(int id, const char *op_name, double x_v, double x_e, double y_v, double y_e, double z_v, double z_e);
void log_unary_op_if_listed(int id, const char *op_name, double x_v, double x_e, double z_v, double z_e);
void printErrorInfo();

unsigned long long ulp_d(double x, double y);
void incErrorCount(OP op);
int checkErrorBits_f64(double val, double err, int THRESHOLD, OP op);
int checkAndLogError_B(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e, OP op, const char *op_name);
int checkAndLogError_U(double x_v, double x_e, double *z_v, double *z_e, OP op, const char *op_name);
void finalize_binary_op_dd(const char* op_name, OP op_type, int op_id, double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e);
void finalize_unary_op_dd(const char* op_name, OP op_type, int op_id, double x_v, double x_e, double *z_v, double *z_e);
double twosum_di(double x_v, double y_v, double z);
float twosum_fi(float x_v, float y_v, float z);

/* Arithmetic operations - double */
void add_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e);
void sub_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e, bool check);
void sub_di_check_true(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e);
void sub_di_check_false(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e);
void mul_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e);
void div_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e);

/* Unary functions - double */
void fabs_di(double x_v, double x_e, double *z_v, double *z_e);
void ceil_di(double x_v, double x_e, double *z_v, double *z_e);
void floor_di(double x_v, double x_e, double *z_v, double *z_e);
void trunc_di(double x_v, double x_e, double *z_v, double *z_e);
void sqrt_di(double x_v, double x_e, double *z_v, double *z_e);
void rint_di(double x_v, double x_e, double *z_v, double *z_e);
void exp_di(double x_v, double x_e, double *z_v, double *z_e);
double wasm_exp(double x);
void neg_di(double x_v, double x_e, double *z_v, double *z_e);

/* Comparison operators - double */
typedef int (*comp_fn_d)(double, double);
int equal_d(double x, double y);
int not_equal_d(double x, double y);
int less_than_d(double x, double y);
int less_than_or_equal_d(double x, double y);
int greater_than_d(double x, double y);
int greater_than_or_equal_d(double x, double y);

int compare_d(double x_v, double x_e, double y_v, double y_e, comp_fn_d op, char *op_name);
int eq_di(double x_v, double x_e, double y_v, double y_e);
int neq_di(double x_v, double x_e, double y_v, double y_e);
int less_di(double x_v, double x_e, double y_v, double y_e);
int less_eq_di(double x_v, double x_e, double y_v, double y_e);
int greater_di(double x_v, double x_e, double y_v, double y_e);
int greater_eq_di(double x_v, double x_e, double y_v, double y_e);

/* Utility functions - double */
void min_f64_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e);
void max_f64_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e);
void copysign_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e);

/* Float precision functions */
void add_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e);
void sub_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e, bool check);
void sub_fi_check_true(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e);
void sub_fi_check_false(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e);
void mul_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e);
void div_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e);

/* Math functions - float */
void fabsf_fi(float x_v, double x_e, double *z_v, double *z_e);
void ceilf_fi(float x_v, double x_e, double *z_v, double *z_e);
void floorf_fi(float x_v, double x_e, double *z_v, double *z_e);
void truncf_fi(float x_v, double x_e, double *z_v, double *z_e);
void sqrtf_fi(float x_v, double x_e, double *z_v, double *z_e);
void rintf_fi(float x_v, double x_e, double *z_v, double *z_e);
void expf_fi(float x_v, double x_e, double *z_v, double *z_e);
float wasm_expf(float x);
void negf_fi(float x_v, double x_e, double *z_v, double *z_e);

/* Comparison operators - float */
typedef int (*comp_fn_f)(float, float);
int equal_f(float x, float y);
int not_equal_f(float x, float y);
int less_than_f(float x, float y);
int less_than_or_equal_f(float x, float y);
int greater_than_f(float x, float y);
int greater_than_or_equal_f(float x, float y);

int compare_f(float x_v, double x_e, float y_v, double y_e, comp_fn_f op_f, comp_fn_d op_d, char *op_name);
int eq_fi(float x_v, double x_e, float y_v, double y_e);
int neq_fi(float x_v, double x_e, float y_v, double y_e);
int less_fi(float x_v, double x_e, float y_v, double y_e);
int less_eq_fi(float x_v, double x_e, float y_v, double y_e);
int greater_fi(float x_v, double x_e, float y_v, double y_e);
int greater_eq_fi(float x_v, double x_e, float y_v, double y_e);

/* Utility functions - float */
void min_f32_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e);
void max_f32_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e);
void copysign_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e);

#ifdef __cplusplus
}
#endif

#endif /* DI_LIBRARY_H */