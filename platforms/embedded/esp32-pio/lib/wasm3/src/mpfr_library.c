#include "mpfr_library.h"

int PREC = 512;

mpfr_t* allocate_mpfr_d(double val) {
    mpfr_t *p = malloc(sizeof(mpfr_t));
    if (p == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }
    mpfr_init_set_d(*p, val, MPFR_RNDN);
    return p;
}

// Correct logging functions for MPFR
void log_value_and_mpfr(FILE *out, double v, mpfr_ptr e) {
    log_value_or_special(out, v);
    fprintf(out, ", ");
    mpfr_out_str(out, 10, 100, e, MPFR_RNDN);
}

void log_binary_mpfr_debug(int id, const char *op_name, double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double z_v, mpfr_ptr z_e) {
    FILE *out = stderr;

    fprintf(out, "(ID %d) %s: (", id, op_name);
    log_value_and_mpfr(out, x_v, x_e);

    fprintf(out, "), (");
    log_value_and_mpfr(out, y_v, y_e);

    fprintf(out, "), (");
    log_value_and_mpfr(out, z_v, z_e);

    fprintf(out, ")\n");
}

void log_unary_mpfr_debug(int id, const char *op_name, double x_v, mpfr_ptr x_e, double z_v, mpfr_ptr z_e) {
    FILE *out = stderr;

    fprintf(out, "(ID %d) %s: (", id, op_name);
    log_value_and_mpfr(out, x_v, x_e);

    fprintf(out, "), (");
    log_value_and_mpfr(out, z_v, z_e);

    fprintf(out, ")\n");
}

int checkErrorBits_mpfr(double fp_v, mpfr_ptr real_v, int THRESHOLD, OP op) {
    // int HRE = 0;
    // mpfr_t z_e;
    // mpfr_init_set_d(z_e, 0.0, MPFR_RNDN);
    // mpfr_sub_d(z_e, real_v, fp_v, MPFR_RNDN);
    // if ((fp_v == 0.0) && (mpfr_cmp_d(z_e, 0.0) != 0)) { HRE = 1; }
    // else {
    //     double bitsError = log2(ulp_d(fp_v, mpfr_get_d(real_v, MPFR_RNDN)) + 1);
    //     HRE = (bitsError > THRESHOLD);
    // }
    double bitsError = log2(ulp_d(fp_v, mpfr_get_d(real_v, MPFR_RNDN)) + 1);
    int HRE = (bitsError > THRESHOLD);
    if (HRE) {
        HIGH_ROUNDING_ERROR += 1;
        incErrorCount(op);
    }
    // check Inf
    if ((isinf(fp_v))) {
        INF += 1;
    }
    return HRE;
}

// type: 0 for float, 1 for double
void checkAndLogError_mpfr(bool type, double *z_f, mpfr_ptr *z_r, OP op, const char *op_name) {
    // Handle primary overflow
    if (isinf(*z_f)) {
        if ((*z_f) > 0) { mpfr_set_inf(*z_r, 1); }
        else { mpfr_set_inf(*z_r, -1); }
    }
    // Handle residue underflow
    else if (op == MUL || op == DIV || op == SQRT) {
        mpfr_t z_e;
        mpfr_init_set_d(z_e, 0.0, MPFR_RNDN);
        mpfr_sub_d(z_e, *z_r, *z_f, MPFR_RNDN);
        if (!mpfr_zero_p(z_e) && 
                ((type && (mpfr_get_d(z_e, MPFR_RNDN) == 0)) || 
                    (!type && (float)(mpfr_get_d(z_e, MPFR_RNDN) == 0)))) {
            mpfr_init_set_d(*z_r, *z_f, MPFR_RNDN);
        }
    }
    int HRE = checkErrorBits_mpfr(*z_f, *z_r, THRESHOLD, op);
    if (HRE && WASM3_EOP_LOG) {
        printf("ID: %d \n", OP_ID);
    }
}

// Final routine in every arithmetic operation
void finalize_binary_op_mpfr(bool type, const char* op_name, OP op_type, int op_id, double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    // Check error bits
    checkAndLogError_mpfr(type, z_f, z_r, op_type, op_name);
    
    mpfr_t x_e, y_e, z_e;
    mpfr_init_set_d(x_e, 0.0, MPFR_RNDN);
    mpfr_init_set_d(y_e, 0.0, MPFR_RNDN);
    mpfr_init_set_d(z_e, 0.0, MPFR_RNDN);
    mpfr_sub_d(x_e, x_r, x_f, MPFR_RNDN);
    mpfr_sub_d(y_e, y_r, y_f, MPFR_RNDN);
    mpfr_sub_d(z_e, *z_r, *z_f, MPFR_RNDN);

    // Logging
    if (WASM3_DEBUG_LOG) {
        log_binary_op_if_debug(op_id, op_name, x_f, y_f, *z_f);
    }
    if (WASM3_FULL_LOG && (!LOG_LIMIT || op_id <= LOG_LIMIT)) {
        log_binary_mpfr_debug(op_id, op_name, x_f, x_e , y_f, y_e , *z_f, z_e);
    }
    log_binary_op_if_listed(op_id, op_name, x_f, mpfr_get_d(x_e, MPFR_RNDN), y_f, mpfr_get_d(y_e, MPFR_RNDN), *z_f, mpfr_get_d(z_e, MPFR_RNDN));

    mpfr_clear(x_e);
    mpfr_clear(y_e);
    mpfr_clear(z_e);
}

void finalize_unary_op_mpfr(bool type, const char* op_name, OP op_type, int op_id, double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    // Check error bits
    checkAndLogError_mpfr(type, z_f, z_r, op_type, op_name);
    
    mpfr_t x_e, z_e;
    mpfr_init_set_d(x_e, 0.0, MPFR_RNDN);
    mpfr_init_set_d(z_e, 0.0, MPFR_RNDN);
    mpfr_sub_d(x_e, x_r, x_f, MPFR_RNDN);
    mpfr_sub_d(z_e, *z_r, *z_f, MPFR_RNDN);

    // Logging
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(op_id, op_name, x_f, *z_f);
    }
    if (WASM3_FULL_LOG && (!LOG_LIMIT || op_id <= LOG_LIMIT)) {
        log_unary_mpfr_debug(op_id, op_name, x_f, x_e, *z_f, z_e);
    }
    log_unary_op_if_listed(op_id, op_name, x_f, mpfr_get_d(x_e, MPFR_RNDN), *z_f, mpfr_get_d(z_e, MPFR_RNDN));

    mpfr_clear(x_e);
    mpfr_clear(z_e);
}

// for double type
void add_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    ADD_TT += 1;
    *z_f = x_f + y_f;

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = x_r;
    mpfr_ptr y_use = y_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        mpfr_init_set_d(tmp_y, y_f, MPFR_RNDN);
        y_use = tmp_y;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_add(*z_r, x_use, y_use, MPFR_RNDN);

    if ((x_f == -6755399441055744.0) || (y_f == -6755399441055744.0)) {
        mpfr_set_d(*z_r, *z_f, MPFR_RNDN);
    }

    const char* OP_NAME = "ADD";
    const OP OP_TYPE = ADD;
    finalize_binary_op_mpfr(true, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    if (y_r == NULL) mpfr_clear(tmp_y);
}

void sub_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r, bool check) {
    if (check) {
        OP_ID += 1;
        SUB_TT += 1;
    }
    *z_f = x_f - y_f;

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = x_r;
    mpfr_ptr y_use = y_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        mpfr_init_set_d(tmp_y, y_f, MPFR_RNDN);
        y_use = tmp_y;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_sub(*z_r, x_use, y_use, MPFR_RNDN);

    if (y_f == 6755399441055744.0) {
        mpfr_set_d(*z_r, *z_f, MPFR_RNDN);
    }

    const char* OP_NAME = "SUB";
    const OP OP_TYPE = SUB;
    if (check) {
        finalize_binary_op_mpfr(true, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);
    }

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    if (y_r == NULL) mpfr_clear(tmp_y);
}

void sub_dmp_check_true(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    sub_dmp(x_f, x_r, y_f, y_r, z_f, z_r, true);
}
void sub_dmp_check_false(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    sub_dmp(x_f, x_r, y_f, y_r, z_f, z_r, false);
}

void mul_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    MUL_TT += 1;
    *z_f = x_f * y_f;

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = x_r;
    mpfr_ptr y_use = y_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        mpfr_init_set_d(tmp_y, y_f, MPFR_RNDN);
        y_use = tmp_y;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_mul(*z_r, x_use, y_use, MPFR_RNDN);

    const char* OP_NAME = "MUL";
    const OP OP_TYPE = MUL;
    finalize_binary_op_mpfr(true, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    if (y_r == NULL) mpfr_clear(tmp_y);
}

void div_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    DIV_TT += 1;
    *z_f = x_f / y_f;

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = x_r;
    mpfr_ptr y_use = y_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        mpfr_init_set_d(tmp_y, y_f, MPFR_RNDN);
        y_use = tmp_y;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_div(*z_r, x_use, y_use, MPFR_RNDN);

    const char* OP_NAME = "DIV";
    const OP OP_TYPE = DIV;
    finalize_binary_op_mpfr(true, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    if (y_r == NULL) mpfr_clear(tmp_y);
}

void fabs_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    FABS_TT += 1;
    *z_f = fabs(x_f);

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_abs(*z_r, x_use, MPFR_RNDN);

    const char* OP_NAME = "FABS";
    const OP OP_TYPE = FABS;
    finalize_unary_op_mpfr(true, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
}

void ceil_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    CEIL_TT += 1;
    *z_f = ceil(x_f);

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_ceil(*z_r, x_use);

    const char* OP_NAME = "CEIL";
    const OP OP_TYPE = CEIL;
    finalize_unary_op_mpfr(true, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
}

void floor_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    FLOOR_TT += 1;
    *z_f = floor(x_f);

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_floor(*z_r, x_use);

    const char* OP_NAME = "FLOOR";
    const OP OP_TYPE = FLOOR;
    finalize_unary_op_mpfr(true, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
}

void trunc_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    TRUNC_TT += 1;
    *z_f = trunc(x_f);

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_trunc(*z_r, x_use);

    const char* OP_NAME = "TRUNC";
    const OP OP_TYPE = TRUNC;
    finalize_unary_op_mpfr(true, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
}

void sqrt_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    SQRT_TT += 1;
    *z_f = sqrt(x_f);

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_sqrt(*z_r, x_use, MPFR_RNDN);

    const char* OP_NAME = "SQRT";
    const OP OP_TYPE = SQRT;
    finalize_unary_op_mpfr(true, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
}

void rint_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    RINT_TT += 1;
    *z_f = rint(x_f);

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_rint(*z_r, x_use, MPFR_RNDN);

    const char* OP_NAME = "RINT";
    const OP OP_TYPE = RINT;
    finalize_unary_op_mpfr(true, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
}

void neg_dmp(double x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    NEG_TT += 1;
    *z_f = -x_f;

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_neg(*z_r, x_use, MPFR_RNDN);

    // Compute residues
    mpfr_t x_e, z_e;
    mpfr_init_set_d(x_e, 0.0, MPFR_RNDN);
    mpfr_init_set_d(z_e, 0.0, MPFR_RNDN);
    mpfr_sub_d(x_e, x_use, x_f, MPFR_RNDN);
    mpfr_sub_d(z_e, *z_r, *z_f, MPFR_RNDN);

    const char* OP_NAME = "NEG";
    const OP OP_TYPE = NEG;
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(OP_ID, OP_NAME, x_f, *z_f);
    }
    if (WASM3_FULL_LOG && (!LOG_LIMIT || OP_ID <= LOG_LIMIT)) {
        log_unary_mpfr_debug(OP_ID, OP_NAME, x_f, x_e, *z_f, z_e);
    }

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    mpfr_clear(x_e);
    mpfr_clear(z_e);
}

int equal_mp(mpfr_ptr x, mpfr_ptr y) {return mpfr_equal_p(x, y);}
int not_equal_mp(mpfr_ptr x, mpfr_ptr y) {return !(mpfr_equal_p(x, y));}
int less_than_mp(mpfr_ptr x, mpfr_ptr y) {return mpfr_less_p(x, y);}
int less_than_or_equal_mp(mpfr_ptr x, mpfr_ptr y) {return mpfr_lessequal_p(x, y);}
int greater_than_mp(mpfr_ptr x, mpfr_ptr y) {return mpfr_greater_p(x, y);}
int greater_than_or_equal_mp(mpfr_ptr x, mpfr_ptr y) {return mpfr_greaterequal_p(x, y);}

int compare_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, comp_fn_d op_d, comp_fn_mp op_mp, char *op_name) {
    int fp_comp = op_d(x_f, y_f);

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = x_r;
    mpfr_ptr y_use = y_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        mpfr_init_set_d(tmp_y, y_f, MPFR_RNDN);
        y_use = tmp_y;
    }

    int real_comp = op_mp(x_use, y_use);
    if ((fp_comp ^ real_comp)) {
        BRANCH_FLIP += 1;
    }

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    if (y_r == NULL) mpfr_clear(tmp_y);

    return fp_comp;
}

// compare operator
int eq_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r) {
    return compare_dmp(x_f, x_r, y_f, y_r, equal_d, equal_mp, "==");
}

int neq_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r) {
    return compare_dmp(x_f, x_r, y_f, y_r, not_equal_d, not_equal_mp, "!=");
}

int less_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r) {
    return compare_dmp(x_f, x_r, y_f, y_r, less_than_d, less_than_mp, "<");
}

int less_eq_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r) {
    return compare_dmp(x_f, x_r, y_f, y_r, less_than_or_equal_d, less_than_or_equal_mp, "<=");
}

int greater_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r) {
    return compare_dmp(x_f, x_r, y_f, y_r, greater_than_d, greater_than_mp, ">");
}

int greater_eq_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r) {
    return compare_dmp(x_f, x_r, y_f, y_r, greater_than_or_equal_d, greater_than_or_equal_mp, ">=");
}

void min_f64_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    int b = less_dmp(x_f, x_r, y_f, y_r);
    *z_f = b ? x_f : y_f;
    *z_r = b ? x_r : y_r;
}

void max_f64_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    int b = greater_dmp(x_f, x_r, y_f, y_r);
    *z_f = b ? x_f : y_f;
    *z_r = b ? x_r : y_r;
}

void copysign_dmp(double x_f, mpfr_ptr x_r, double y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    COPYSIGN_TT += 1;
    *z_f = copysign(x_f, y_f);

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = x_r;
    mpfr_ptr y_use = y_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        mpfr_init_set_d(tmp_y, y_f, MPFR_RNDN);
        y_use = tmp_y;
    }

    *z_r = *allocate_mpfr_d(0.0);
    if (isnan(y_f)) {
        mpfr_init_set_d(*z_r, x_f, MPFR_RNDN);
    }
    else {
        mpfr_copysign(*z_r, x_use, y_use, MPFR_RNDN);
    }

    const char* OP_NAME = "COPYSIGN";
    const OP OP_TYPE = COPYSIGN;
    finalize_binary_op_mpfr(true, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    if (y_r == NULL) mpfr_clear(tmp_y);
}


// for float type
void add_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    ADD_TT += 1;
    *z_f = x_f + y_f;

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = x_r;
    mpfr_ptr y_use = y_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        mpfr_init_set_d(tmp_y, y_f, MPFR_RNDN);
        y_use = tmp_y;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_add(*z_r, x_use, y_use, MPFR_RNDN);

    if ((x_f == -6755399441055744.0) || (y_f == -6755399441055744.0)) {
        mpfr_set_d(*z_r, *z_f, MPFR_RNDN);
    }

    const char* OP_NAME = "ADD";
    const OP OP_TYPE = ADD;
    finalize_binary_op_mpfr(false, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    if (y_r == NULL) mpfr_clear(tmp_y);
}

void sub_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r, bool check) {
    if (check) {
        OP_ID += 1;
        SUB_TT += 1;
    }
    *z_f = x_f - y_f;

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = x_r;
    mpfr_ptr y_use = y_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        mpfr_init_set_d(tmp_y, y_f, MPFR_RNDN);
        y_use = tmp_y;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_sub(*z_r, x_use, y_use, MPFR_RNDN);

    if (y_f == 6755399441055744.0) {
        mpfr_set_d(*z_r, *z_f, MPFR_RNDN);
    }

    const char* OP_NAME = "SUB";
    const OP OP_TYPE = SUB;
    if (check) {
        finalize_binary_op_mpfr(false, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);
    }

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    if (y_r == NULL) mpfr_clear(tmp_y);
}

void sub_fmp_check_true(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    sub_fmp(x_f, x_r, y_f, y_r, z_f, z_r, true);
}
void sub_fmp_check_false(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    sub_fmp(x_f, x_r, y_f, y_r, z_f, z_r, false);
}

void mul_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    MUL_TT += 1;
    *z_f = x_f * y_f;

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = x_r;
    mpfr_ptr y_use = y_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        mpfr_init_set_d(tmp_y, y_f, MPFR_RNDN);
        y_use = tmp_y;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_mul(*z_r, x_use, y_use, MPFR_RNDN);

    const char* OP_NAME = "MUL";
    const OP OP_TYPE = MUL;
    finalize_binary_op_mpfr(false, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    if (y_r == NULL) mpfr_clear(tmp_y);
}

void div_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    DIV_TT += 1;
    *z_f = x_f / y_f;

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = x_r;
    mpfr_ptr y_use = y_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        mpfr_init_set_d(tmp_y, y_f, MPFR_RNDN);
        y_use = tmp_y;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_div(*z_r, x_use, y_use, MPFR_RNDN);

    const char* OP_NAME = "DIV";
    const OP OP_TYPE = DIV;
    finalize_binary_op_mpfr(false, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    if (y_r == NULL) mpfr_clear(tmp_y);
}

void fabs_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    FABS_TT += 1;
    *z_f = fabsf(x_f);

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_abs(*z_r, x_use, MPFR_RNDN);

    const char* OP_NAME = "FABS";
    const OP OP_TYPE = FABS;
    finalize_unary_op_mpfr(false, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
}

void ceil_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    CEIL_TT += 1;
    *z_f = ceilf(x_f);

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_ceil(*z_r, x_use);

    const char* OP_NAME = "CEIL";
    const OP OP_TYPE = CEIL;
    finalize_unary_op_mpfr(false, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
}

void floor_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    FLOOR_TT += 1;
    *z_f = floorf(x_f);

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_floor(*z_r, x_use);

    const char* OP_NAME = "FLOOR";
    const OP OP_TYPE = FLOOR;
    finalize_unary_op_mpfr(false, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
}

void trunc_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    TRUNC_TT += 1;
    *z_f = truncf(x_f);

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_trunc(*z_r, x_use);

    const char* OP_NAME = "TRUNC";
    const OP OP_TYPE = TRUNC;
    finalize_unary_op_mpfr(false, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
}

void sqrt_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    SQRT_TT += 1;
    *z_f = sqrtf(x_f);

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_sqrt(*z_r, x_use, MPFR_RNDN);

    const char* OP_NAME = "SQRT";
    const OP OP_TYPE = SQRT;
    finalize_unary_op_mpfr(false, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
}

void rint_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    RINT_TT += 1;
    *z_f = rintf(x_f);

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_rint(*z_r, x_use, MPFR_RNDN);

    const char* OP_NAME = "RINT";
    const OP OP_TYPE = RINT;
    finalize_unary_op_mpfr(false, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
}

void neg_fmp(float x_f, mpfr_ptr x_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    NEG_TT += 1;
    *z_f = -x_f;

    mpfr_t tmp_x;
    mpfr_ptr x_use = x_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }

    *z_r = *allocate_mpfr_d(0.0);
    mpfr_neg(*z_r, x_use, MPFR_RNDN);

    // Compute residues
    mpfr_t x_e, z_e;
    mpfr_init_set_d(x_e, 0.0, MPFR_RNDN);
    mpfr_init_set_d(z_e, 0.0, MPFR_RNDN);
    mpfr_sub_d(x_e, x_use, x_f, MPFR_RNDN);
    mpfr_sub_d(z_e, *z_r, *z_f, MPFR_RNDN);

    const char* OP_NAME = "NEG";
    const OP OP_TYPE = NEG;
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(OP_ID, OP_NAME, x_f, *z_f);
    }
    if (WASM3_FULL_LOG && (!LOG_LIMIT || OP_ID <= LOG_LIMIT)) {
        log_unary_mpfr_debug(OP_ID, OP_NAME, x_f, x_e, *z_f, z_e);
    }

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    mpfr_clear(x_e);
    mpfr_clear(z_e);
}

int compare_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, comp_fn_f op_f, comp_fn_mp op_mp, char *op_name) {
    int fp_comp = op_f(x_f, y_f);

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = x_r;
    mpfr_ptr y_use = y_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        mpfr_init_set_d(tmp_y, y_f, MPFR_RNDN);
        y_use = tmp_y;
    }

    int real_comp = op_mp(x_use, y_use);
    if ((fp_comp ^ real_comp)) {
        BRANCH_FLIP += 1;
    }

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    if (y_r == NULL) mpfr_clear(tmp_y);

    return fp_comp;
}

// compare operator
int eq_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r) {
    return compare_fmp(x_f, x_r, y_f, y_r, equal_f, equal_mp, "==");
}

int neq_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r) {
    return compare_fmp(x_f, x_r, y_f, y_r, not_equal_f, not_equal_mp, "!=");
}

int less_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r) {
    return compare_fmp(x_f, x_r, y_f, y_r, less_than_f, less_than_mp, "<");
}

int less_eq_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r) {
    return compare_fmp(x_f, x_r, y_f, y_r, less_than_or_equal_f, less_than_or_equal_mp, "<=");
}

int greater_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r) {
    return compare_fmp(x_f, x_r, y_f, y_r, greater_than_f, greater_than_mp, ">");
}

int greater_eq_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r) {
    return compare_fmp(x_f, x_r, y_f, y_r, greater_than_or_equal_f, greater_than_or_equal_mp, ">=");
}

void min_f32_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    int b = less_fmp(x_f, x_r, y_f, y_r);
    *z_f = b ? x_f : y_f;
    *z_r = b ? x_r : y_r;
}

void max_f32_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    int b = greater_fmp(x_f, x_r, y_f, y_r);
    *z_f = b ? x_f : y_f;
    *z_r = b ? x_r : y_r;
}

void copysign_fmp(float x_f, mpfr_ptr x_r, float y_f, mpfr_ptr y_r, double *z_f, mpfr_ptr *z_r) {
    OP_ID += 1;
    COPYSIGN_TT += 1;
    *z_f = copysignf(x_f, y_f);

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = x_r;
    mpfr_ptr y_use = y_r;
    if (x_r == NULL) {
        mpfr_init_set_d(tmp_x, x_f, MPFR_RNDN);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        mpfr_init_set_d(tmp_y, y_f, MPFR_RNDN);
        y_use = tmp_y;
    }

    *z_r = *allocate_mpfr_d(0.0);
    if (isnan(y_f)) {
        mpfr_init_set_d(*z_r, x_f, MPFR_RNDN);
    }
    else {
        mpfr_copysign(*z_r, x_use, y_use, MPFR_RNDN);
    }

    const char* OP_NAME = "COPYSIGN";
    const OP OP_TYPE = COPYSIGN;
    finalize_binary_op_mpfr(false, OP_NAME, OP_TYPE, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);

    // clear temporary mpfr variables
    if (x_r == NULL) mpfr_clear(tmp_x);
    if (y_r == NULL) mpfr_clear(tmp_y);
}