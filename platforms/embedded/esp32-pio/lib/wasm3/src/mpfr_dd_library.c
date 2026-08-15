#include "mpfr_dd_library.h"

int checkErrorBits_md(double val, mpfr_ptr err, int THRESHOLD, OP op) {
    int HRE = 0;
    double real = val + mpfr_get_d(err, MPFR_RNDN);
    double bitsError = log2(ulp_d(val, real) + 1);
    // if (contains_bsearch(log_fn_ids, n_log_fn_ids, OP_ID)) {
    //     printf("ID: %d, BitsError: %f\n", OP_ID, bitsError);
    // }
    // if (contains_bsearch(log_fp_ids, n_log_fp_ids, OP_ID)) {
    //     printf("ID: %d, BitsError: %f\n", OP_ID, bitsError);
    // }
    if ((bitsError > THRESHOLD)) {
        HRE = 1;
        HIGH_ROUNDING_ERROR += 1;
        incErrorCount(op);
    }
    // check Inf
    if ((isinf(val))) {
        INF += 1;
    }
    return HRE;
}

void checkAndLogError(OP op, double z_v, mpfr_ptr z_e) {
    int HRE = checkErrorBits_md(z_v, z_e, THRESHOLD, op);
    if (HRE && WASM3_EOP_LOG) {
        printf("ID: %d \n", OP_ID);
    }
}

// Final routine in every arithmetic operation
void finalize_binary_op_md(const char* op_name, OP op_type, int op_id, double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    if (WASM3_DEBUG_LOG) {
        log_binary_op_if_debug(op_id, op_name, x_v, y_v, *z_v);
    }
    log_binary_op_if_listed(op_id, op_name, x_v, mpfr_get_d(x_e, MPFR_RNDN), y_v, mpfr_get_d(y_e, MPFR_RNDN), *z_v, mpfr_get_d(*z_e, MPFR_RNDN));
    checkAndLogError(op_type, *z_v, *z_e);
}

void finalize_unary_op_md(const char* op_name, OP op_type, int op_id, double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(op_id, op_name, x_v, *z_v);
    }
    log_unary_op_if_listed(op_id, op_name, x_v, mpfr_get_d(x_e, MPFR_RNDN), *z_v, mpfr_get_d(*z_e, MPFR_RNDN));
    checkAndLogError(op_type, *z_v, *z_e);
}

// for double type
void add_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    ADD_TT += 1;
    *z_v = x_v + y_v;

    mpfr_t tmp_xe, tmp_ye;
    mpfr_ptr xe_use = x_e;
    mpfr_ptr ye_use = y_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }
    if (y_e == NULL) {
        mpfr_init_set_d(tmp_ye, 0.0, MPFR_RNDN);
        ye_use = tmp_ye;
    }

 // *z_e = twosum_di(x_v, y_v, *z_v) + x_e + y_e;
    *z_e = *allocate_mpfr_d(twosum_di(x_v, y_v, *z_v));
    mpfr_add(*z_e, *z_e, xe_use, MPFR_RNDN);
    mpfr_add(*z_e, *z_e, ye_use, MPFR_RNDN);

    const char* OP_NAME = "ADD";
    const OP OP_TYPE = ADD;
    finalize_binary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
    if (y_e == NULL) mpfr_clear(tmp_ye);
}

void sub_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e, bool check) {
    if (check) {
        OP_ID += 1;
        SUB_TT += 1;
    }
    *z_v = x_v - y_v;

    mpfr_t tmp_xe, tmp_ye;
    mpfr_ptr xe_use = x_e;
    mpfr_ptr ye_use = y_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }
    if (y_e == NULL) {
        mpfr_init_set_d(tmp_ye, 0.0, MPFR_RNDN);
        ye_use = tmp_ye;
    }

 // *z_e = twosum_di(x_v, -y_v, *z_v) + x_e - y_e;
    *z_e = *allocate_mpfr_d(twosum_di(x_v, -y_v, *z_v));
    mpfr_add(*z_e, *z_e, xe_use, MPFR_RNDN);
    mpfr_sub(*z_e, *z_e, ye_use, MPFR_RNDN);

    const char* OP_NAME = "SUB";
    const OP OP_TYPE = SUB;
    if (check) {
        finalize_binary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);
    }

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
    if (y_e == NULL) mpfr_clear(tmp_ye);
}

void sub_dmd_check_true(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    sub_dmd(x_v, x_e, y_v, y_e, z_v, z_e, true);
}
void sub_dmd_check_false(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    sub_dmd(x_v, x_e, y_v, y_e, z_v, z_e, false);
}

void mul_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    MUL_TT += 1;
    *z_v = x_v * y_v;

    mpfr_t tmp_xe, tmp_ye;
    mpfr_ptr xe_use = x_e;
    mpfr_ptr ye_use = y_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }
    if (y_e == NULL) {
        mpfr_init_set_d(tmp_ye, 0.0, MPFR_RNDN);
        ye_use = tmp_ye;
    }

 // *z_e = fma(x_v, y_v, -z) + x_e * y_v + x_v * y_e;
    mpfr_t tmp_xv, tmp_yv, tmp_zv_neg;
    mpfr_ptr xv_use = tmp_xv, yv_use = tmp_yv, zv_neg_use = tmp_zv_neg;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);
    mpfr_init_set_d(tmp_yv, y_v, MPFR_RNDN);
    mpfr_init_set_d(tmp_zv_neg, -(*z_v), MPFR_RNDN);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_fma(*z_e, xv_use, yv_use, zv_neg_use, MPFR_RNDN);
    mpfr_fma(*z_e, xe_use, yv_use, *z_e, MPFR_RNDN);
    mpfr_fma(*z_e, xv_use, ye_use, *z_e, MPFR_RNDN);

    mpfr_clears(tmp_xv, tmp_yv, tmp_zv_neg, (mpfr_ptr) 0);

    const char* OP_NAME = "MUL";
    const OP OP_TYPE = MUL;
    finalize_binary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
    if (y_e == NULL) mpfr_clear(tmp_ye);
}

void div_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    DIV_TT += 1;
    *z_v = x_v / y_v;

    mpfr_t tmp_xe, tmp_ye;
    mpfr_ptr xe_use = x_e;
    mpfr_ptr ye_use = y_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }
    if (y_e == NULL) {
        mpfr_init_set_d(tmp_ye, 0.0, MPFR_RNDN);
        ye_use = tmp_ye;
    }

 // *z_e = (x_e - fma(z, y_v, -x_v) - z * y_e) / (y_v + y_e);
    mpfr_t tmp_xv, tmp_yv, tmp_zv, tmp_zv_ye, tmp_den;
    mpfr_ptr xv_use = tmp_xv, yv_use = tmp_yv, zv_use = tmp_zv, zv_ye_use = tmp_zv_ye, den_use = tmp_den;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);
    mpfr_init_set_d(tmp_yv, y_v, MPFR_RNDN);
    mpfr_init_set_d(tmp_zv, *z_v, MPFR_RNDN);
    mpfr_inits(tmp_zv_ye, tmp_den, (mpfr_ptr) 0);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_neg(*z_e, xv_use, MPFR_RNDN); // *z_e = -x_v
    mpfr_fma(*z_e, zv_use, yv_use, *z_e, MPFR_RNDN); // *z_e = fma(z, y_v, -x_v)
    mpfr_sub(*z_e, xe_use, *z_e, MPFR_RNDN);
    mpfr_mul(zv_ye_use, zv_use, ye_use, MPFR_RNDN);
    mpfr_sub(*z_e, *z_e, zv_ye_use, MPFR_RNDN);

    mpfr_add(den_use, yv_use, ye_use, MPFR_RNDN);
    mpfr_div(*z_e, *z_e, den_use, MPFR_RNDN);

    mpfr_clears(tmp_xv, tmp_yv, tmp_zv, tmp_zv_ye, tmp_den, (mpfr_ptr) 0);

    const char* OP_NAME = "DIV";
    const OP OP_TYPE = DIV;
    finalize_binary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
    if (y_e == NULL) mpfr_clear(tmp_ye);
}

void fabs_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    FABS_TT += 1;
    *z_v = fabs(x_v);

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

 // *z_e = fabs(x_v + x_e) - z;
    mpfr_t tmp_xv;
    mpfr_ptr xv_use = tmp_xv;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_add(*z_e, xv_use, xe_use, MPFR_RNDN);
    mpfr_abs(*z_e, *z_e, MPFR_RNDN);
    mpfr_sub_d(*z_e, *z_e, *z_v, MPFR_RNDN);

    mpfr_clear(tmp_xv);

    const char* OP_NAME = "FABS";
    const OP OP_TYPE = FABS;
    finalize_unary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

void ceil_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    CEIL_TT += 1;
    *z_v = ceil(x_v);

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

 // *z_e = ceil(x_v + x_e) - z;
    mpfr_t tmp_xv;
    mpfr_ptr xv_use = tmp_xv;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_add(*z_e, xv_use, xe_use, MPFR_RNDN);
    mpfr_ceil(*z_e, *z_e);
    mpfr_sub_d(*z_e, *z_e, *z_v, MPFR_RNDN);

    mpfr_clear(tmp_xv);

    const char* OP_NAME = "CEIL";
    const OP OP_TYPE = CEIL;
    finalize_unary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

void floor_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    FLOOR_TT += 1;
    *z_v = floor(x_v);

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

 // *z_e = floor(x_v + x_e) - z;
    mpfr_t tmp_xv;
    mpfr_ptr xv_use = tmp_xv;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_add(*z_e, xv_use, xe_use, MPFR_RNDN);
    mpfr_floor(*z_e, *z_e);
    mpfr_sub_d(*z_e, *z_e, *z_v, MPFR_RNDN);

    mpfr_clear(tmp_xv);

    const char* OP_NAME = "FLOOR";
    const OP OP_TYPE = FLOOR;
    finalize_unary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

void trunc_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    TRUNC_TT += 1;
    *z_v = trunc(x_v);

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

 // *z_e = trunc(x_v + x_e) - z;
    mpfr_t tmp_xv;
    mpfr_ptr xv_use = tmp_xv;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_add(*z_e, xv_use, xe_use, MPFR_RNDN);
    mpfr_trunc(*z_e, *z_e);
    mpfr_sub_d(*z_e, *z_e, *z_v, MPFR_RNDN);

    mpfr_clear(tmp_xv);

    const char* OP_NAME = "TRUNC";
    const OP OP_TYPE = TRUNC;
    finalize_unary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

void sqrt_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    SQRT_TT += 1;
    *z_v = sqrt(x_v);

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

 // *z_e = (z == 0) ? sqrt(fabs(x_e)) : (x_e + fma(-z, z, x_v)) / (2 * z);
    *z_e = *allocate_mpfr_d(0.0);
    if (*z_v == 0.0) {
        mpfr_abs(*z_e, xe_use, MPFR_RNDN);
        mpfr_sqrt(*z_e, *z_e, MPFR_RNDN);
    } else {
        mpfr_t tmp_xv, tmp_zv, tmp_den;
        mpfr_ptr xv_use = tmp_xv, zv_use = tmp_zv, den_use = tmp_den;
        mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);
        mpfr_init_set_d(tmp_zv, *z_v, MPFR_RNDN);
        mpfr_init(tmp_den);
        
        mpfr_neg(*z_e, zv_use, MPFR_RNDN); // *z_e = -z
        mpfr_fma(*z_e, *z_e, zv_use, xv_use, MPFR_RNDN); // *z_e = fma(-z, z, x_v))
        mpfr_add(*z_e, xe_use, *z_e, MPFR_RNDN);
        mpfr_mul_d(den_use, zv_use, 2.0, MPFR_RNDN);
        mpfr_div(*z_e, *z_e, den_use, MPFR_RNDN);
        
        mpfr_clears(tmp_xv, tmp_zv, tmp_den, (mpfr_ptr) 0);
    }

    const char* OP_NAME = "SQRT";
    const OP OP_TYPE = SQRT;
    finalize_unary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

void rint_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    RINT_TT += 1;
    *z_v = rint(x_v);

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

 // *z_e = rint(x_v + x_e) - z;
    mpfr_t tmp_xv;
    mpfr_ptr xv_use = tmp_xv;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_add(*z_e, xv_use, xe_use, MPFR_RNDN);
    mpfr_rint(*z_e, *z_e, MPFR_RNDN);
    mpfr_sub_d(*z_e, *z_e, *z_v, MPFR_RNDN);

    mpfr_clear(tmp_xv);

    const char* OP_NAME = "RINT";
    const OP OP_TYPE = RINT;
    finalize_unary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

void neg_dmd(double x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    NEG_TT += 1;
    *z_v = -x_v;

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_neg(*z_e, xe_use, MPFR_RNDN);

    const char* OP_NAME = "NEG";
    const OP OP_TYPE = NEG;
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(OP_ID, OP_NAME, x_v, *z_v);
    }

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

int compare_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, comp_fn_d op_d, comp_fn_mp op_mp, char *op_name) {
    int fp_comp = op_d(x_v, y_v);

    mpfr_t tmp_xe, tmp_ye;
    mpfr_ptr xe_use = x_e;
    mpfr_ptr ye_use = y_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }
    if (y_e == NULL) {
        mpfr_init_set_d(tmp_ye, 0.0, MPFR_RNDN);
        ye_use = tmp_ye;
    }

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = tmp_x, y_use = tmp_y;
    mpfr_init_set_d(tmp_x, x_v, MPFR_RNDN);
    mpfr_init_set_d(tmp_y, y_v, MPFR_RNDN);

    mpfr_add(x_use, x_use, xe_use, MPFR_RNDN);
    mpfr_add(y_use, y_use, ye_use, MPFR_RNDN);
    int real_comp = op_mp(x_use, y_use); // compare x_v + x_e with y_v + y_e 

    mpfr_clear(tmp_x);
    mpfr_clear(tmp_y);

    if ((fp_comp ^ real_comp)) {
        BRANCH_FLIP += 1;
    }

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
    if (y_e == NULL) mpfr_clear(tmp_ye);

    return fp_comp;
}

// compare operator
int eq_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e) {
    return compare_dmd(x_v, x_e, y_v, y_e, equal_d, equal_mp, "==");
}

int neq_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e) {
    return compare_dmd(x_v, x_e, y_v, y_e, not_equal_d, not_equal_mp, "!=");
}

int less_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e) {
    return compare_dmd(x_v, x_e, y_v, y_e, less_than_d, less_than_mp, "<");
}

int less_eq_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e) {
    return compare_dmd(x_v, x_e, y_v, y_e, less_than_or_equal_d, less_than_or_equal_mp, "<=");
}

int greater_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e) {
    return compare_dmd(x_v, x_e, y_v, y_e, greater_than_d, greater_than_mp, ">");
}

int greater_eq_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e) {
    return compare_dmd(x_v, x_e, y_v, y_e, greater_than_or_equal_d, greater_than_or_equal_mp, ">=");
}

void min_f64_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    int b = less_dmd(x_v, x_e, y_v, y_e);
    *z_v = b ? x_v : y_v;
    *z_e = b ? x_e : y_e;
}

void max_f64_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    int b = greater_dmd(x_v, x_e, y_v, y_e);
    *z_v = b ? x_v : y_v;
    *z_e = b ? x_e : y_e;
}

void copysign_dmd(double x_v, mpfr_ptr x_e, double y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    COPYSIGN_TT += 1;
    *z_v = copysign(x_v, y_v);

    mpfr_t tmp_xe, tmp_ye;
    mpfr_ptr xe_use = x_e;
    mpfr_ptr ye_use = y_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }
    if (y_e == NULL) {
        mpfr_init_set_d(tmp_ye, 0.0, MPFR_RNDN);
        ye_use = tmp_ye;
    }

 // *z_e = copysign(x_v, y_v + y_e) + copysign(x_e, y_v + y_e) - z;
    mpfr_t tmp_xv, tmp_yv, tmp_y, tmp_sec_cs;
    mpfr_ptr xv_use = tmp_xv, yv_use = tmp_yv, y_use = tmp_y, sec_cs_use = tmp_sec_cs;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);
    mpfr_init_set_d(tmp_yv, y_v, MPFR_RNDN);
    mpfr_inits(tmp_y, tmp_sec_cs, (mpfr_ptr) 0);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_add(y_use, yv_use, ye_use, MPFR_RNDN); // y_use = y_v + y_e
    mpfr_copysign(*z_e, xv_use, y_use, MPFR_RNDN); // *z_e = copysign(x_v, y_v + y_e)
    mpfr_copysign(tmp_sec_cs, xe_use, y_use, MPFR_RNDN);
    mpfr_add(*z_e, *z_e, tmp_sec_cs, MPFR_RNDN);
    mpfr_sub_d(*z_e, *z_e, *z_v, MPFR_RNDN);

    mpfr_clears(tmp_xv, tmp_yv, tmp_y, tmp_sec_cs, (mpfr_ptr) 0);

    const char* OP_NAME = "COPYSIGN";
    const OP OP_TYPE = COPYSIGN;
    finalize_binary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
    if (y_e == NULL) mpfr_clear(tmp_ye);
}


// for float type
void add_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    ADD_TT += 1;
    *z_v = x_v + y_v;

    mpfr_t tmp_xe, tmp_ye;
    mpfr_ptr xe_use = x_e;
    mpfr_ptr ye_use = y_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }
    if (y_e == NULL) {
        mpfr_init_set_d(tmp_ye, 0.0, MPFR_RNDN);
        ye_use = tmp_ye;
    }

 // *z_e = twosum_di(x_v, y_v, *z_v) + x_e + y_e;
    *z_e = *allocate_mpfr_d(twosum_di(x_v, y_v, *z_v));
    mpfr_add(*z_e, *z_e, xe_use, MPFR_RNDN);
    mpfr_add(*z_e, *z_e, ye_use, MPFR_RNDN);

    const char* OP_NAME = "ADD";
    const OP OP_TYPE = ADD;
    finalize_binary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
    if (y_e == NULL) mpfr_clear(tmp_ye);
}

void sub_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e, bool check) {
    if (check) {
        OP_ID += 1;
        SUB_TT += 1;
    }
    *z_v = x_v - y_v;

    mpfr_t tmp_xe, tmp_ye;
    mpfr_ptr xe_use = x_e;
    mpfr_ptr ye_use = y_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }
    if (y_e == NULL) {
        mpfr_init_set_d(tmp_ye, 0.0, MPFR_RNDN);
        ye_use = tmp_ye;
    }

 // *z_e = twosum_di(x_v, -y_v, *z_v) + x_e - y_e;
    *z_e = *allocate_mpfr_d(twosum_di(x_v, -y_v, *z_v));
    mpfr_add(*z_e, *z_e, xe_use, MPFR_RNDN);
    mpfr_sub(*z_e, *z_e, ye_use, MPFR_RNDN);

    const char* OP_NAME = "SUB";
    const OP OP_TYPE = SUB;
    if (check) {
        finalize_binary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);
    }

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
    if (y_e == NULL) mpfr_clear(tmp_ye);
}

void sub_fmd_check_true(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    sub_fmd(x_v, x_e, y_v, y_e, z_v, z_e, true);
}
void sub_fmd_check_false(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    sub_fmd(x_v, x_e, y_v, y_e, z_v, z_e, false);
}

void mul_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    MUL_TT += 1;
    *z_v = x_v * y_v;

    mpfr_t tmp_xe, tmp_ye;
    mpfr_ptr xe_use = x_e;
    mpfr_ptr ye_use = y_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }
    if (y_e == NULL) {
        mpfr_init_set_d(tmp_ye, 0.0, MPFR_RNDN);
        ye_use = tmp_ye;
    }

 // *z_e = fma(x_v, y_v, -z) + x_e * y_v + x_v * y_e;
    mpfr_t tmp_xv, tmp_yv, tmp_zv_neg;
    mpfr_ptr xv_use = tmp_xv, yv_use = tmp_yv, zv_neg_use = tmp_zv_neg;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);
    mpfr_init_set_d(tmp_yv, y_v, MPFR_RNDN);
    mpfr_init_set_d(tmp_zv_neg, -(*z_v), MPFR_RNDN);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_fma(*z_e, xv_use, yv_use, zv_neg_use, MPFR_RNDN);
    mpfr_fma(*z_e, xe_use, yv_use, *z_e, MPFR_RNDN);
    mpfr_fma(*z_e, xv_use, ye_use, *z_e, MPFR_RNDN);

    mpfr_clear(tmp_xv);
    mpfr_clear(tmp_yv);
    mpfr_clear(tmp_zv_neg);

    const char* OP_NAME = "MUL";
    const OP OP_TYPE = MUL;
    finalize_binary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
    if (y_e == NULL) mpfr_clear(tmp_ye);
}

void div_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    DIV_TT += 1;
    *z_v = x_v / y_v;

    mpfr_t tmp_xe, tmp_ye;
    mpfr_ptr xe_use = x_e;
    mpfr_ptr ye_use = y_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }
    if (y_e == NULL) {
        mpfr_init_set_d(tmp_ye, 0.0, MPFR_RNDN);
        ye_use = tmp_ye;
    }

 // *z_e = (x_e - fma(z, y_v, -x_v) - z * y_e) / (y_v + y_e);
    mpfr_t tmp_xv, tmp_yv, tmp_zv, tmp_zv_ye, tmp_den;
    mpfr_ptr xv_use = tmp_xv, yv_use = tmp_yv, zv_use = tmp_zv, zv_ye_use = tmp_zv_ye, den_use = tmp_den;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);
    mpfr_init_set_d(tmp_yv, y_v, MPFR_RNDN);
    mpfr_init_set_d(tmp_zv, *z_v, MPFR_RNDN);
    mpfr_inits(tmp_zv_ye, tmp_den, (mpfr_ptr) 0);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_neg(*z_e, xv_use, MPFR_RNDN); // *z_e = -x_v
    mpfr_fma(*z_e, zv_use, yv_use, *z_e, MPFR_RNDN); // *z_e = fma(z, y_v, -x_v)
    mpfr_sub(*z_e, xe_use, *z_e, MPFR_RNDN);
    mpfr_mul(zv_ye_use, zv_use, ye_use, MPFR_RNDN);
    mpfr_sub(*z_e, *z_e, zv_ye_use, MPFR_RNDN);

    mpfr_add(den_use, yv_use, ye_use, MPFR_RNDN);
    mpfr_div(*z_e, *z_e, den_use, MPFR_RNDN);

    mpfr_clears(tmp_xv, tmp_yv, tmp_zv, tmp_zv_ye, tmp_den, (mpfr_ptr) 0);

    const char* OP_NAME = "DIV";
    const OP OP_TYPE = DIV;
    finalize_binary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
    if (y_e == NULL) mpfr_clear(tmp_ye);
}

void fabs_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    FABS_TT += 1;
    *z_v = fabsf(x_v);

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

 // *z_e = fabs(x_v + x_e) - z;
    mpfr_t tmp_xv;
    mpfr_ptr xv_use = tmp_xv;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_add(*z_e, xv_use, xe_use, MPFR_RNDN);
    mpfr_abs(*z_e, *z_e, MPFR_RNDN);
    mpfr_sub_d(*z_e, *z_e, *z_v, MPFR_RNDN);

    mpfr_clear(tmp_xv);

    const char* OP_NAME = "FABS";
    const OP OP_TYPE = FABS;
    finalize_unary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

void ceil_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    CEIL_TT += 1;
    *z_v = ceilf(x_v);

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

 // *z_e = ceil(x_v + x_e) - z;
    mpfr_t tmp_xv;
    mpfr_ptr xv_use = tmp_xv;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_add(*z_e, xv_use, xe_use, MPFR_RNDN);
    mpfr_ceil(*z_e, *z_e);
    mpfr_sub_d(*z_e, *z_e, *z_v, MPFR_RNDN);

    mpfr_clear(tmp_xv);

    const char* OP_NAME = "CEIL";
    const OP OP_TYPE = CEIL;
    finalize_unary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

void floor_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    FLOOR_TT += 1;
    *z_v = floorf(x_v);

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

 // *z_e = floor(x_v + x_e) - z;
    mpfr_t tmp_xv;
    mpfr_ptr xv_use = tmp_xv;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_add(*z_e, xv_use, xe_use, MPFR_RNDN);
    mpfr_floor(*z_e, *z_e);
    mpfr_sub_d(*z_e, *z_e, *z_v, MPFR_RNDN);

    mpfr_clear(tmp_xv);

    const char* OP_NAME = "FLOOR";
    const OP OP_TYPE = FLOOR;
    finalize_unary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

void trunc_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    TRUNC_TT += 1;
    *z_v = truncf(x_v);

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

 // *z_e = trunc(x_v + x_e) - z;
    mpfr_t tmp_xv;
    mpfr_ptr xv_use = tmp_xv;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_add(*z_e, xv_use, xe_use, MPFR_RNDN);
    mpfr_trunc(*z_e, *z_e);
    mpfr_sub_d(*z_e, *z_e, *z_v, MPFR_RNDN);

    mpfr_clear(tmp_xv);

    const char* OP_NAME = "TRUNC";
    const OP OP_TYPE = TRUNC;
    finalize_unary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

void sqrt_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    SQRT_TT += 1;
    *z_v = sqrtf(x_v);

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

 // *z_e = (z == 0) ? sqrt(fabs(x_e)) : (x_e + fma(-z, z, x_v)) / (2 * z);
    *z_e = *allocate_mpfr_d(0.0);
    if (*z_v == 0.0) {
        mpfr_abs(*z_e, xe_use, MPFR_RNDN);
        mpfr_sqrt(*z_e, *z_e, MPFR_RNDN);
    } else {
        mpfr_t tmp_xv, tmp_zv, tmp_den;
        mpfr_ptr xv_use = tmp_xv, zv_use = tmp_zv, den_use = tmp_den;
        mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);
        mpfr_init_set_d(tmp_zv, *z_v, MPFR_RNDN);
        mpfr_init(tmp_den);
        
        mpfr_neg(*z_e, zv_use, MPFR_RNDN); // *z_e = -z
        mpfr_fma(*z_e, *z_e, zv_use, xv_use, MPFR_RNDN); // *z_e = fma(-z, z, x_v))
        mpfr_add(*z_e, xe_use, *z_e, MPFR_RNDN);
        mpfr_mul_d(den_use, zv_use, 2.0, MPFR_RNDN);
        mpfr_div(*z_e, *z_e, den_use, MPFR_RNDN);
        
        mpfr_clears(tmp_xv, tmp_zv, tmp_den, (mpfr_ptr) 0);
    }

    const char* OP_NAME = "SQRT";
    const OP OP_TYPE = SQRT;
    finalize_unary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

void rint_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    RINT_TT += 1;
    *z_v = rintf(x_v);

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

 // *z_e = rint(x_v + x_e) - z;
    mpfr_t tmp_xv;
    mpfr_ptr xv_use = tmp_xv;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_add(*z_e, xv_use, xe_use, MPFR_RNDN);
    mpfr_rint(*z_e, *z_e, MPFR_RNDN);
    mpfr_sub_d(*z_e, *z_e, *z_v, MPFR_RNDN);

    mpfr_clear(tmp_xv);

    const char* OP_NAME = "RINT";
    const OP OP_TYPE = RINT;
    finalize_unary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

void neg_fmd(float x_v, mpfr_ptr x_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    NEG_TT += 1;
    *z_v = -x_v;

    mpfr_t tmp_xe;
    mpfr_ptr xe_use = x_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_neg(*z_e, xe_use, MPFR_RNDN);

    const char* OP_NAME = "NEG";
    const OP OP_TYPE = NEG;
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(OP_ID, OP_NAME, x_v, *z_v);
    }

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
}

int compare_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, comp_fn_f op_f, comp_fn_mp op_mp, char *op_name) {
    int fp_comp = op_f(x_v, y_v);

    mpfr_t tmp_xe, tmp_ye;
    mpfr_ptr xe_use = x_e;
    mpfr_ptr ye_use = y_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }
    if (y_e == NULL) {
        mpfr_init_set_d(tmp_ye, 0.0, MPFR_RNDN);
        ye_use = tmp_ye;
    }

    mpfr_t tmp_x, tmp_y;
    mpfr_ptr x_use = tmp_x, y_use = tmp_y;
    mpfr_init_set_d(tmp_x, x_v, MPFR_RNDN);
    mpfr_init_set_d(tmp_y, y_v, MPFR_RNDN);

    mpfr_add(x_use, x_use, xe_use, MPFR_RNDN);
    mpfr_add(y_use, y_use, ye_use, MPFR_RNDN);
    int real_comp = op_mp(x_use, y_use); // compare x_v + x_e with y_v + y_e 

    mpfr_clear(tmp_x);
    mpfr_clear(tmp_y);

    if ((fp_comp ^ real_comp)) {
        BRANCH_FLIP += 1;
    }

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
    if (y_e == NULL) mpfr_clear(tmp_ye);

    return fp_comp;
}

// compare operator
int eq_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e) {
    return compare_fmd(x_v, x_e, y_v, y_e, equal_f, equal_mp, "==");
}

int neq_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e) {
    return compare_fmd(x_v, x_e, y_v, y_e, not_equal_f, not_equal_mp, "!=");
}

int less_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e) {
    return compare_fmd(x_v, x_e, y_v, y_e, less_than_f, less_than_mp, "<");
}

int less_eq_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e) {
    return compare_fmd(x_v, x_e, y_v, y_e, less_than_or_equal_f, less_than_or_equal_mp, "<=");
}

int greater_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e) {
    return compare_fmd(x_v, x_e, y_v, y_e, greater_than_f, greater_than_mp, ">");
}

int greater_eq_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e) {
    return compare_fmd(x_v, x_e, y_v, y_e, greater_than_or_equal_f, greater_than_or_equal_mp, ">=");
}

void min_f32_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    int b = less_fmd(x_v, x_e, y_v, y_e);
    *z_v = b ? x_v : y_v;
    *z_e = b ? x_e : y_e;
}

void max_f32_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    int b = greater_fmd(x_v, x_e, y_v, y_e);
    *z_v = b ? x_v : y_v;
    *z_e = b ? x_e : y_e;
}

void copysign_fmd(float x_v, mpfr_ptr x_e, float y_v, mpfr_ptr y_e, double *z_v, mpfr_ptr *z_e) {
    OP_ID += 1;
    COPYSIGN_TT += 1;
    *z_v = copysignf(x_v, y_v);

    mpfr_t tmp_xe, tmp_ye;
    mpfr_ptr xe_use = x_e;
    mpfr_ptr ye_use = y_e;
    if (x_e == NULL) {
        mpfr_init_set_d(tmp_xe, 0.0, MPFR_RNDN);
        xe_use = tmp_xe;
    }
    if (y_e == NULL) {
        mpfr_init_set_d(tmp_ye, 0.0, MPFR_RNDN);
        ye_use = tmp_ye;
    }

 // *z_e = copysign(x_v, y_v + y_e) + copysign(x_e, y_v + y_e) - z;
    mpfr_t tmp_xv, tmp_yv, tmp_y, tmp_sec_cs;
    mpfr_ptr xv_use = tmp_xv, yv_use = tmp_yv, y_use = tmp_y, sec_cs_use = tmp_sec_cs;
    mpfr_init_set_d(tmp_xv, x_v, MPFR_RNDN);
    mpfr_init_set_d(tmp_yv, y_v, MPFR_RNDN);
    mpfr_inits(tmp_y, tmp_sec_cs, (mpfr_ptr) 0);

    *z_e = *allocate_mpfr_d(0.0);
    mpfr_add(y_use, yv_use, ye_use, MPFR_RNDN); // y_use = y_v + y_e
    mpfr_copysign(*z_e, xv_use, y_use, MPFR_RNDN); // *z_e = copysign(x_v, y_v + y_e)
    mpfr_copysign(tmp_sec_cs, xe_use, y_use, MPFR_RNDN);
    mpfr_add(*z_e, *z_e, tmp_sec_cs, MPFR_RNDN);
    mpfr_sub_d(*z_e, *z_e, *z_v, MPFR_RNDN);

    mpfr_clears(tmp_xv, tmp_yv, tmp_y, tmp_sec_cs, (mpfr_ptr) 0);

    const char* OP_NAME = "COPYSIGN";
    const OP OP_TYPE = COPYSIGN;
    finalize_binary_op_md(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);

    // clear temporary mpfr variables
    if (x_e == NULL) mpfr_clear(tmp_xe);
    if (y_e == NULL) mpfr_clear(tmp_ye);
}