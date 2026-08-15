#include "qd_library.h"

int comp_res = 0;
int *res = &comp_res;

int checkErrorBits_qd(double fp_v, qd_t real_v, int THRESHOLD, OP op) {
    double bitsError = log2(ulp_d(fp_v, *real_v) + 1);
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

// for double type
void checkAndLogError_QD(bool type, double *z_f, qd_t *z_r, OP op, const char *op_name) {
    // Handle primary overflow
    if (isinf(*z_f)) {
        if ((*z_f) > 0) { c_qd_copy_d(INFINITY, *z_r); }
        else { c_qd_copy_d(-INFINITY, *z_r); }
    }
    // Handle residue underflow
    else if (op == MUL || op == DIV || op == SQRT) {
        qd_t z_e = (qd_t) malloc(4 * sizeof(double));
        c_qd_copy_d(0.0, z_e);
        c_qd_sub_qd_d(*z_r, *z_f, z_e);

        int cmp;
        qd_t zero_p = (qd_t) malloc(4 * sizeof(double));
        c_qd_copy_d(0.0, zero_p);
        c_qd_comp(z_e, zero_p, &cmp);
        if ((cmp != 0) && ((type && (*z_e) == 0) || (!type && (float)(*z_e) == 0))) {
            c_qd_copy_d(*z_f, *z_r);
        }
    }
    int HRE = checkErrorBits_qd(*z_f, *z_r, THRESHOLD, op);
    if (HRE && WASM3_EOP_LOG) {
        printf("ID: %d \n", OP_ID);
    }
}

void log_binary_qd_debug(int id, const char *op_name, double x_v, qd_t x_q, double y_v, qd_t y_q, double z_v, qd_t z_q) {
    FILE *out = stdout;

    fprintf(out, "(ID %d) %s:\n", id, op_name);
    log_value_or_special(out, x_v);
    fprintf(out, ", ");
    if (x_q) c_qd_write(x_q); else fprintf(out, "NULL\n");

    log_value_or_special(out, y_v);
    fprintf(out, ", ");
    if (y_q) c_qd_write(y_q); else fprintf(out, "NULL\n");

    log_value_or_special(out, z_v);
    fprintf(out, ", ");
    if (z_q) c_qd_write(z_q); else fprintf(out, "NULL\n");
}

void log_unary_qd_debug(int id, const char *op_name, double x_v, qd_t x_q, double z_v, qd_t z_q) {
    FILE *out = stdout;

    fprintf(out, "(ID %d) %s:\n", id, op_name);
    log_value_or_special(out, x_v);
    fprintf(out, ", ");
    if (x_q) c_qd_write(x_q); else fprintf(out, "NULL\n");

    log_value_or_special(out, z_v);
    fprintf(out, ", ");
    if (z_q) c_qd_write(z_q); else fprintf(out, "NULL\n");
}

static qd_t compute_qd_error(qd_t real_v, double fp_v, double err_buf[4]) {
    c_qd_copy_d(0.0, err_buf);
    if (real_v) {
        c_qd_sub_qd_d(real_v, fp_v, err_buf);
    }
    return err_buf;
}

void finalize_binary_op_qd(bool type, const char *op_name, OP op_type, int op_id,
                           double x_f, qd_t x_r, double y_f, qd_t y_r,
                           double *z_f, qd_t *z_r) {
    checkAndLogError_QD(type, z_f, z_r, op_type, op_name);

    double x_e_buf[4];
    double y_e_buf[4];
    double z_e_buf[4];
    qd_t x_e = compute_qd_error(x_r, x_f, x_e_buf);
    qd_t y_e = compute_qd_error(y_r, y_f, y_e_buf);
    qd_t z_e = compute_qd_error(z_r ? *z_r : NULL, *z_f, z_e_buf);

    // Logging
    if (WASM3_DEBUG_LOG) {
        log_binary_op_if_debug(op_id, op_name, x_f, y_f, *z_f);
    }
    if (WASM3_FULL_LOG && (!LOG_LIMIT || op_id <= LOG_LIMIT)) {
        log_binary_qd_debug(op_id, op_name, x_f, x_e, y_f, y_e, *z_f, z_e);
    }
    log_binary_op_if_listed(op_id, op_name, x_f, *x_e, y_f, *y_e, *z_f, *z_e);
}

void finalize_unary_op_qd(bool type, const char *op_name, OP op_type, int op_id,
                          double x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    checkAndLogError_QD(type, z_f, z_r, op_type, op_name);

    double x_e_buf[4];
    double z_e_buf[4];
    qd_t x_e = compute_qd_error(x_r, x_f, x_e_buf);
    qd_t z_e = compute_qd_error(z_r ? *z_r : NULL, *z_f, z_e_buf);

    // Logging
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(op_id, op_name, x_f, *z_f);
    }
    if (WASM3_FULL_LOG && (!LOG_LIMIT || op_id <= LOG_LIMIT)) {
        log_unary_qd_debug(op_id, op_name, x_f, x_e, *z_f, z_e);
    }
    log_unary_op_if_listed(op_id, op_name, x_f, *x_e, *z_f, *z_e);
}

void add_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    ADD_TT += 1;
    *z_f = x_f + y_f;

    double tmp_x[4];
    double tmp_y[4];
    qd_t x_use = x_r;
    qd_t y_use = y_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        c_qd_copy_d(y_f, tmp_y);
        y_use = tmp_y;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_add(x_use, y_use, *z_r);

    if ((x_f == -6755399441055744.0) || (y_f == -6755399441055744.0)) {
        c_qd_copy_d(*z_f, *z_r);
    }

    finalize_binary_op_qd(true, "ADD", ADD, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);
}

void sub_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r, bool check) {
    if (check) {
        OP_ID += 1;
        SUB_TT += 1;
    }
    *z_f = x_f - y_f;

    double tmp_x[4];
    double tmp_y[4];
    qd_t x_use = x_r;
    qd_t y_use = y_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        c_qd_copy_d(y_f, tmp_y);
        y_use = tmp_y;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_sub(x_use, y_use, *z_r);

    if (y_f == 6755399441055744.0) {
        c_qd_copy_d(*z_f, *z_r);
    }

    if (check) {
        finalize_binary_op_qd(true, "SUB", SUB, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);
    }
}

void sub_dqd_check_true(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    sub_dqd(x_f, x_r, y_f, y_r, z_f, z_r, true);
}
void sub_dqd_check_false(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    sub_dqd(x_f, x_r, y_f, y_r, z_f, z_r, false);
}

void mul_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    MUL_TT += 1;
    *z_f = x_f * y_f;

    double tmp_x[4];
    double tmp_y[4];
    qd_t x_use = x_r;
    qd_t y_use = y_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        c_qd_copy_d(y_f, tmp_y);
        y_use = tmp_y;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_mul(x_use, y_use, *z_r);

    finalize_binary_op_qd(true, "MUL", MUL, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);
}

void div_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    DIV_TT += 1;
    *z_f = x_f / y_f;

    double tmp_x[4];
    double tmp_y[4];
    qd_t x_use = x_r;
    qd_t y_use = y_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        c_qd_copy_d(y_f, tmp_y);
        y_use = tmp_y;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_div(x_use, y_use, *z_r);

    finalize_binary_op_qd(true, "DIV", DIV, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);
}

void fabs_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    FABS_TT += 1;
    *z_f = fabs(x_f);

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_abs(x_use, *z_r);

    finalize_unary_op_qd(true, "FABS", FABS, OP_ID, x_f, x_use, z_f, z_r);
}

void ceil_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    CEIL_TT += 1;
    *z_f = ceil(x_f);

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_ceil(x_use, *z_r);

    finalize_unary_op_qd(true, "CEIL", CEIL, OP_ID, x_f, x_use, z_f, z_r);
}

void floor_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    FLOOR_TT += 1;
    *z_f = floor(x_f);

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_floor(x_use, *z_r);

    finalize_unary_op_qd(true, "FLOOR", FLOOR, OP_ID, x_f, x_use, z_f, z_r);
}

void trunc_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    TRUNC_TT += 1;
    *z_f = trunc(x_f);

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(trunc(*x_use), tmp_z); // no trunc provided by QD
    *z_r = tmp_z;

    finalize_unary_op_qd(true, "TRUNC", TRUNC, OP_ID, x_f, x_use, z_f, z_r);
}

void sqrt_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    SQRT_TT += 1;
    *z_f = sqrt(x_f);

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_sqrt(x_use, *z_r);

    finalize_unary_op_qd(true, "SQRT", SQRT, OP_ID, x_f, x_use, z_f, z_r);
}

void rint_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    RINT_TT += 1;
    *z_f = rint(x_f);

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_nint(x_use, *z_r);

    finalize_unary_op_qd(true, "RINT", RINT, OP_ID, x_f, x_use, z_f, z_r);
}

void neg_dqd(double x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    NEG_TT += 1;
    *z_f = -x_f;

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_neg(x_use, *z_r);

    double x_e_buf[4];
    double z_e_buf[4];
    qd_t x_e = compute_qd_error(x_use, x_f, x_e_buf);
    qd_t z_e = compute_qd_error(*z_r, *z_f, z_e_buf);

    const char *op_name = "NEG";
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(OP_ID, op_name, x_f, *z_f);
    }
    if (WASM3_FULL_LOG && (!LOG_LIMIT || OP_ID <= LOG_LIMIT)) {
        log_unary_qd_debug(OP_ID, op_name, x_f, x_e, *z_f, z_e);
    }
}

int equal_qd(qd_t x, qd_t y) {c_qd_comp(x, y, res); return (*res == 0);}
int not_equal_qd(qd_t x, qd_t y) {c_qd_comp(x, y, res); return (*res != 0);}
int less_than_qd(qd_t x, qd_t y) {c_qd_comp(x, y, res); return (*res < 0);}
int less_than_or_equal_qd(qd_t x, qd_t y) {c_qd_comp(x, y, res); return (*res <= 0);}
int greater_than_qd(qd_t x, qd_t y) {c_qd_comp(x, y, res); return (*res > 0);}
int greater_than_or_equal_qd(qd_t x, qd_t y) {c_qd_comp(x, y, res); return (*res >= 0);}

int compare_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, comp_fn_d op_d, comp_fn_qd op_qd, const char *op_name) {
    int fp_comp = op_d(x_f, y_f);

    double tmp_x[4];
    double tmp_y[4];
    qd_t x_use = x_r;
    qd_t y_use = y_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        c_qd_copy_d(y_f, tmp_y);
        y_use = tmp_y;
    }

    int real_comp = op_qd(x_use, y_use);
    if ((fp_comp ^ real_comp)) {
        BRANCH_FLIP += 1;
    }
    return fp_comp;
}

// compare operator
int eq_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r) {
    return compare_dqd(x_f, x_r, y_f, y_r, equal_d, equal_qd, "==");
}

int neq_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r) {
    return compare_dqd(x_f, x_r, y_f, y_r, not_equal_d, not_equal_qd, "!=");
}

int less_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r) {
    return compare_dqd(x_f, x_r, y_f, y_r, less_than_d, less_than_qd, "<");
}

int less_eq_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r) {
    return compare_dqd(x_f, x_r, y_f, y_r, less_than_or_equal_d, less_than_or_equal_qd, "<=");
}

int greater_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r) {
    return compare_dqd(x_f, x_r, y_f, y_r, greater_than_d, greater_than_qd, ">");
}

int greater_eq_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r) {
    return compare_dqd(x_f, x_r, y_f, y_r, greater_than_or_equal_d, greater_than_or_equal_qd, ">=");
}

void min_f64_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    int b = less_dqd(x_f, x_r, y_f, y_r);
    *z_f = b ? x_f : y_f;
    *z_r = b ? x_r : y_r;
}

void max_f64_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    int b = greater_dqd(x_f, x_r, y_f, y_r);
    *z_f = b ? x_f : y_f;
    *z_r = b ? x_r : y_r;
}

void copysign_dqd(double x_f, qd_t x_r, double y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    COPYSIGN_TT += 1;
    *z_f = copysign(x_f, y_f);

    double tmp_x[4];
    double tmp_y[4];
    qd_t x_use = x_r;
    qd_t y_use = y_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        c_qd_copy_d(y_f, tmp_y);
        y_use = tmp_y;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;

    if (isnan(y_f)) {
        c_qd_copy_d(*x_use, *z_r);
    }
    else {
        c_qd_copy_d(copysign(*x_use, *y_use), *z_r); // no copysign provided by QD
    }

    finalize_binary_op_qd(true, "COPYSIGN", COPYSIGN, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);
}


// for float type
void add_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    ADD_TT += 1;
    *z_f = x_f + y_f;

    double tmp_x[4];
    double tmp_y[4];
    qd_t x_use = x_r;
    qd_t y_use = y_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        c_qd_copy_d(y_f, tmp_y);
        y_use = tmp_y;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_add(x_use, y_use, *z_r);

    if ((x_f == -6755399441055744.0) || (y_f == -6755399441055744.0)) {
        c_qd_copy_d(*z_f, *z_r);
    }

    finalize_binary_op_qd(false, "ADD", ADD, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);
}

void sub_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r, bool check) {
    if (check) {
        OP_ID += 1;
        SUB_TT += 1;
    }
    *z_f = x_f - y_f;

    double tmp_x[4];
    double tmp_y[4];
    qd_t x_use = x_r;
    qd_t y_use = y_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        c_qd_copy_d(y_f, tmp_y);
        y_use = tmp_y;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_sub(x_use, y_use, *z_r);

    if (y_f == 6755399441055744.0) {
        c_qd_copy_d(*z_f, *z_r);
    }

    if (check) {
        finalize_binary_op_qd(false, "SUB", SUB, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);
    }
}

void sub_fqd_check_true(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    sub_fqd(x_f, x_r, y_f, y_r, z_f, z_r, true);
}
void sub_fqd_check_false(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    sub_fqd(x_f, x_r, y_f, y_r, z_f, z_r, false);
}

void mul_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    MUL_TT += 1;
    *z_f = x_f * y_f;

    double tmp_x[4];
    double tmp_y[4];
    qd_t x_use = x_r;
    qd_t y_use = y_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        c_qd_copy_d(y_f, tmp_y);
        y_use = tmp_y;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_mul(x_use, y_use, *z_r);

    finalize_binary_op_qd(false, "MUL", MUL, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);
}

void div_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    DIV_TT += 1;
    *z_f = x_f / y_f;

    double tmp_x[4];
    double tmp_y[4];
    qd_t x_use = x_r;
    qd_t y_use = y_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        c_qd_copy_d(y_f, tmp_y);
        y_use = tmp_y;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_div(x_use, y_use, *z_r);

    finalize_binary_op_qd(false, "DIV", DIV, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);
}

void fabs_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    FABS_TT += 1;
    *z_f = fabsf(x_f);

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_abs(x_use, *z_r);

    finalize_unary_op_qd(false, "FABS", FABS, OP_ID, x_f, x_use, z_f, z_r);
}

void ceil_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    CEIL_TT += 1;
    *z_f = ceilf(x_f);

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_ceil(x_use, *z_r);

    finalize_unary_op_qd(false, "CEIL", CEIL, OP_ID, x_f, x_use, z_f, z_r);
}

void floor_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    FLOOR_TT += 1;
    *z_f = floorf(x_f);

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_floor(x_use, *z_r);

    finalize_unary_op_qd(false, "FLOOR", FLOOR, OP_ID, x_f, x_use, z_f, z_r);
}

void trunc_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    TRUNC_TT += 1;
    *z_f = truncf(x_f);

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(trunc(*x_use), tmp_z); // no trunc provided by QD
    *z_r = tmp_z;

    finalize_unary_op_qd(false, "TRUNC", TRUNC, OP_ID, x_f, x_use, z_f, z_r);
}

void sqrt_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    SQRT_TT += 1;
    *z_f = sqrtf(x_f);

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_sqrt(x_use, *z_r);

    finalize_unary_op_qd(false, "SQRT", SQRT, OP_ID, x_f, x_use, z_f, z_r);
}

void rint_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    RINT_TT += 1;
    *z_f = rintf(x_f);

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_nint(x_use, *z_r);

    finalize_unary_op_qd(false, "RINT", RINT, OP_ID, x_f, x_use, z_f, z_r);
}

void neg_fqd(float x_f, qd_t x_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    NEG_TT += 1;
    *z_f = -x_f;

    double tmp_x[4];
    qd_t x_use = x_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;
    c_qd_neg(x_use, *z_r);

    double x_e_buf[4];
    double z_e_buf[4];
    qd_t x_e = compute_qd_error(x_use, x_f, x_e_buf);
    qd_t z_e = compute_qd_error(*z_r, *z_f, z_e_buf);

    const char *op_name = "NEG";
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(OP_ID, op_name, x_f, *z_f);
    }
    if (WASM3_FULL_LOG && (!LOG_LIMIT || OP_ID <= LOG_LIMIT)) {
        log_unary_qd_debug(OP_ID, op_name, x_f, x_e, *z_f, z_e);
    }
}

int compare_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, comp_fn_f op_f, comp_fn_qd op_qd, const char *op_name) {
    int fp_comp = op_f(x_f, y_f);

    double tmp_x[4];
    double tmp_y[4];
    qd_t x_use = x_r;
    qd_t y_use = y_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        c_qd_copy_d(y_f, tmp_y);
        y_use = tmp_y;
    }

    int real_comp = op_qd(x_use, y_use);
    if ((fp_comp ^ real_comp)) {
        BRANCH_FLIP += 1;
    }
    return fp_comp;
}

// compare operator
int eq_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r) {
    return compare_fqd(x_f, x_r, y_f, y_r, equal_f, equal_qd, "==");
}

int neq_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r) {
    return compare_fqd(x_f, x_r, y_f, y_r, not_equal_f, not_equal_qd, "!=");
}

int less_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r) {
    return compare_fqd(x_f, x_r, y_f, y_r, less_than_f, less_than_qd, "<");
}

int less_eq_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r) {
    return compare_fqd(x_f, x_r, y_f, y_r, less_than_or_equal_f, less_than_or_equal_qd, "<=");
}

int greater_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r) {
    return compare_fqd(x_f, x_r, y_f, y_r, greater_than_f, greater_than_qd, ">");
}

int greater_eq_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r) {
    return compare_fqd(x_f, x_r, y_f, y_r, greater_than_or_equal_f, greater_than_or_equal_qd, ">=");
}

void min_f32_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    int b = less_fqd(x_f, x_r, y_f, y_r);
    *z_f = b ? x_f : y_f;
    *z_r = b ? x_r : y_r;
}

void max_f32_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    int b = greater_fqd(x_f, x_r, y_f, y_r);
    *z_f = b ? x_f : y_f;
    *z_r = b ? x_r : y_r;
}

void copysign_fqd(float x_f, qd_t x_r, float y_f, qd_t y_r, double *z_f, qd_t *z_r) {
    OP_ID += 1;
    COPYSIGN_TT += 1;
    *z_f = copysignf(x_f, y_f);

    double tmp_x[4];
    double tmp_y[4];
    qd_t x_use = x_r;
    qd_t y_use = y_r;

    if (x_r == NULL) {
        c_qd_copy_d(x_f, tmp_x);
        x_use = tmp_x;
    }
    if (y_r == NULL) {
        c_qd_copy_d(y_f, tmp_y);
        y_use = tmp_y;
    }

    qd_t tmp_z = (qd_t) malloc(4 * sizeof(double));
    c_qd_copy_d(0.0, tmp_z);
    *z_r = tmp_z;

    if (isnan(y_f)) {
        c_qd_copy_d(*x_use, *z_r);
    }
    else {
        c_qd_copy_d(copysign(*x_use, *y_use), *z_r); // no copysign provided by QD
    }

    finalize_binary_op_qd(false, "COPYSIGN", COPYSIGN, OP_ID, x_f, x_use, y_f, y_use, z_f, z_r);
}