#ifndef EID_LIBRARY_H
#define EID_LIBRARY_H

#include "m3_core.h"
#include "di_library.h"

#define ABSORBED_NAIVE(big,small)   ((small)!=0.0 && (big)+(small)==(big))
// Heuristics to be changed
#define ABSORBED(t,b,r) ((t)!=0.0 && (r)!=0.0 && (b)/(t) > (1.0 - 3*DBL_EPSILON))

typedef struct eid {
    double v;
    int op; // operation id that generated this floating-point variable; 0 means none
    int id; // primary operation id that contributed to the error; 0 means none
    int id_aux; // secondary operation id that contributed to the error; 0 means none
    bool absorb; // flag to indicate if absorption happens along the way
    bool possible_zero; // flag to indicate that residue might be zero
} eid;

typedef eid* eid_t;

#define EID_DEFAULT ((eid){0})
#define DEFAULT_EID_REGISTER (&g_default_eid_register)
#define EID_SHADOW_STACK_SLOT_SCALE ((u32)((sizeof(eid) + sizeof(m3slot_t) - 1) / sizeof(m3slot_t)))
#define EID_SHADOW_MEMORY_SCALE ((u32)((sizeof(eid) + sizeof(float) - 1) / sizeof(float)))

extern eid g_default_eid_register;
eid_t d_m3DefaultEidRegister(IM3Runtime runtime, m3stack_t ssp);

typedef struct {
    int    id;
    double err_v;
    int    err_op;
    int    err_id;
    int    err_id_aux;
    bool   err_absorb;
    bool   err_possible_zero;
} override_pair;

typedef struct {
    int    id;
    double err_v;
    int    err_op;
    int    err_id;
    int    err_id_aux;
    bool   err_absorb;
    bool   err_possible_zero;
    UT_hash_handle hh;
} override_entry;

extern char *temp_override_fn; // Filename for temporary override file
extern char *temp_override_fp; // Full path for temporary override file
extern FILE *temp_override_file; // File stream for temporary override logging
extern override_entry *override_table; // Hash table for override pairs
extern int *silent_ids; // Ops whose own round-off errors need to be silenced
extern int n_silent_ids;
extern int *probe_ids; // Ops whose errors need to be probed
extern int n_probe_ids;
extern int THRESH;

void load_override_table(const char *path);
int override_err(int id, eid_t *z_e);
void load_silent_ids(const char *path);
void sort_silent_ids();
void load_probe_ids(const char *path);
void sort_probe_ids();
void log_binary_eid_debug(int id, const char *op_name, double x_v, eid_t x_e, double y_v, eid_t y_e, double z_v, eid_t z_e);
void log_unary_eid_debug(int id, const char *op_name, double x_v, eid_t x_e, double z_v, eid_t z_e);
void log_binary_eid_op(int id, const char *op_name, double x_v, eid_t x_e, double y_v, eid_t y_e, double z_v, eid_t z_e);
void log_unary_eid_op(int id, const char *op_name, double x_v, eid_t x_e, double z_v, eid_t z_e);
void log_binary_fn_eid_op(int id, const char *op_name, double x_v, eid_t x_e, double y_v, eid_t y_e, double z_v, eid_t z_e);
void log_unary_fn_eid_op(int id, const char *op_name, double x_v, eid_t x_e, double z_v, eid_t z_e);
void log_binary_fp_eid_op(int id, const char *op_name, double x_v, eid_t x_e, double y_v, eid_t y_e, double z_v, eid_t z_e);
void log_unary_fp_eid_op(int id, const char *op_name, double x_v, eid_t x_e, double z_v, eid_t z_e);
void log_binary_eid_op_if_listed(int id, const char *op_name, double x_v, eid_t x_e, double y_v, eid_t y_e, double z_v, eid_t z_e);
void log_unary_eid_op_if_listed(int id, const char *op_name, double x_v, eid_t x_e, double z_v, eid_t z_e);

int upd_un_err(double pre_sum, double real_pre_sum, double op_err, double real_op_err, double amp_err, double real_amp_err, eid_t x_e, eid_t *z_e);
int upd_bin_err(double pre_sum, double real_pre_sum, double op_err, double real_op_err, double amp_err_1, double real_amp_err_1, double amp_err_2, double real_amp_err_2, eid_t x_e, eid_t y_e, eid_t *z_e);
int upd_mul_err(double *pre_sum, double *real_pre_sum, double op_err, double real_op_err, double amp_err_1, double real_amp_err_1, double amp_err_2, double real_amp_err_2, double sec_ord_err, double real_sec_ord_err, eid_t x_e, eid_t y_e, eid_t *z_e);

void finalize_binary_op_eid(const char* op_name, OP op_type, int op_id, double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e);
void finalize_unary_op_eid(const char* op_name, OP op_type, int op_id, double x_v, eid_t x_e, double *z_v, eid_t *z_e);
/* Arithmetic operations - double */
void add_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e);
void sub_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e, bool check);
void sub_dei_check_true(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e);
void sub_dei_check_false(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e);
void mul_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e);
void div_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e);

/* Unary functions - double */
void fabs_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e);
void ceil_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e);
void floor_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e);
void trunc_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e);
void sqrt_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e);
void rint_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e);
void neg_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e);

/* Comparison operators - double */
int compare_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, comp_fn_d op, char *op_name);
int eq_dei(double x_v, eid_t x_e, double y_v, eid_t y_e);
int neq_dei(double x_v, eid_t x_e, double y_v, eid_t y_e);
int less_dei(double x_v, eid_t x_e, double y_v, eid_t y_e);
int less_eq_dei(double x_v, eid_t x_e, double y_v, eid_t y_e);
int greater_dei(double x_v, eid_t x_e, double y_v, eid_t y_e);
int greater_eq_dei(double x_v, eid_t x_e, double y_v, eid_t y_e);

/* Utility functions - double */
void min_f64_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e);
void max_f64_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e);
void copysign_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e);

/* Float precision functions */
void add_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e);
void sub_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e, bool check);
void sub_fei_check_true(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e);
void sub_fei_check_false(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e);
void mul_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e);
void div_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e);

/* Math functions - float */
void fabsf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e);
void ceilf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e);
void floorf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e);
void truncf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e);
void sqrtf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e);
void rintf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e);
void negf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e);

/* Comparison operators - float */
int compare_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, comp_fn_f op_f, comp_fn_d op_d, char *op_name);
int eq_fei(float x_v, eid_t x_e, float y_v, eid_t y_e);
int neq_fei(float x_v, eid_t x_e, float y_v, eid_t y_e);
int less_fei(float x_v, eid_t x_e, float y_v, eid_t y_e);
int less_eq_fei(float x_v, eid_t x_e, float y_v, eid_t y_e);
int greater_fei(float x_v, eid_t x_e, float y_v, eid_t y_e);
int greater_eq_fei(float x_v, eid_t x_e, float y_v, eid_t y_e);

/* Utility functions - float */
void min_f32_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e);
void max_f32_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e);
void copysign_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e);

void free_all();
#endif /* EID_LIBRARY_H */
