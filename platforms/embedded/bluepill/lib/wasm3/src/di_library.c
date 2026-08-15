#include "di_library.h"
int *log_fn_ids = NULL;
int n_log_fn_ids = 0;
int *log_fp_ids = NULL;
int n_log_fp_ids = 0;
int *debug_ids = NULL;
int n_debug_ids = 0;

int MAX_BUF_SIZE = 512;
int ids_buf[512] = { [0 ... 511] = -1 };
int s = 0;
int *cur_size = &s;

bool is_magic_number(double v) {
    return (v == 6755399441055744.0);
}

void log_value_or_special(FILE *out, double v) {
    if (is_magic_number(v)) {
        fprintf(out, "SPECIAL");
    } else if (is_magic_number(-v)) {
        fprintf(out, "-SPECIAL");
    } else {
        fprintf(out, "%.20e", v);
    }
}

// Functions for loading files with IDs
void load_debug_ids(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) { printf("No Debug IDs\n"); return; }

    int ch;
    size_t n = 0;
    while ((ch = fgetc(fp)) != EOF)
        if (ch == '\n')
            ++n;
    rewind(fp);

    debug_ids = malloc(n * sizeof(int));
    if (!debug_ids) { fclose(fp); fprintf(stderr, "Failed to allocate memory for debug IDs\n"); exit(EXIT_FAILURE); }

    for (size_t i = 0; i < n; ++i) {
        if (fscanf(fp, "%d", &debug_ids[i]) != 1) {
            fclose(fp);
            fprintf(stderr, "Failed to read integer from debug IDs file\n");
            exit(EXIT_FAILURE);
        }
    }
    n_debug_ids = n;
    fclose(fp);

    // printf("Number of debug ids: %d\n", n_debug_ids);
    // for (int i = 0; i < n_debug_ids; i++) {
    //     printf("Debug ID %d: %d\n", i, debug_ids[i]);
    // }
}

void load_log_fn_ids(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) { printf("No Log IDs (FN)\n"); return; }

    int ch;
    size_t n = 0;
    while ((ch = fgetc(fp)) != EOF)
        if (ch == '\n')
            ++n;
    rewind(fp);

    log_fn_ids = malloc(n * sizeof(int));
    if (!log_fn_ids) { fclose(fp); fprintf(stderr, "Failed to allocate memory for log IDs (FN)\n"); exit(EXIT_FAILURE); }

    for (size_t i = 0; i < n; ++i) {
        if (fscanf(fp, "%d", &log_fn_ids[i]) != 1) {
            fclose(fp);
            fprintf(stderr, "Failed to read integer from log IDs file (FN)\n");
            exit(EXIT_FAILURE);
        }
    }
    n_log_fn_ids = n;
    fclose(fp);

    // printf("Number of log ids (FN): %d\n", n_log_fn_ids);
    // for (int i = 0; i < n_log_fn_ids; i++) {
    //     printf("Log ID %d (FN): %d\n", i, log_fn_ids[i]);
    // }
}

void load_log_fp_ids(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) { printf("No Log IDs (FP)\n"); return; }

    int ch;
    size_t n = 0;
    while ((ch = fgetc(fp)) != EOF)
        if (ch == '\n')
            ++n;
    rewind(fp);

    log_fp_ids = malloc(n * sizeof(int));
    if (!log_fp_ids) { fclose(fp); fprintf(stderr, "Failed to allocate memory for log IDs (FP)\n"); exit(EXIT_FAILURE); }

    for (size_t i = 0; i < n; ++i) {
        if (fscanf(fp, "%d", &log_fp_ids[i]) != 1) {
            fclose(fp);
            fprintf(stderr, "Failed to read integer from log IDs file (FP)\n");
            exit(EXIT_FAILURE);
        }
    }
    n_log_fp_ids = n;
    fclose(fp);

    // printf("Number of log ids (FP): %d\n", n_log_fp_ids);
    // for (int i = 0; i < n_log_fp_ids; i++) {
    //     printf("Log ID %d (FP): %d\n", i, log_fp_ids[i]);
    // }
}

static void log_value_and_error(FILE *out, double v, double e) {
    log_value_or_special(out, v);
    fprintf(out, ", %.20e", e);
}

void simple_log_bin(int id, const char *op_name, double x_v, double y_v, double z_v) {
    fprintf(stderr, "(ID %d) %s: %e, %e, %e \n", id, op_name, x_v, y_v, z_v);
}

void simple_log_un(int id, const char *op_name, double x_v, double z_v) {
    fprintf(stderr, "(ID %d) %s: %e, %e \n", id, op_name, x_v, z_v);
}

void log_binary_op(int id, const char *op_name, double x_v, double x_e, double y_v, double y_e, double z_v, double z_e) {
    FILE *out = stderr;

    fprintf(out, "(ID %d) %s: (", id, op_name);
    log_value_and_error(out, x_v, x_e);

    fprintf(out, "), (");
    log_value_and_error(out, y_v, y_e);

    fprintf(out, "), (");
    log_value_and_error(out, z_v, z_e);

    fprintf(out, ")\n");
}

void log_unary_op(int id, const char *op_name, double x_v, double x_e, double z_v, double z_e) {
    FILE *out = stderr;

    fprintf(out, "(ID %d) %s: (", id, op_name);
    log_value_and_error(out, x_v, x_e);

    fprintf(out, "), (");
    log_value_and_error(out, z_v, z_e);

    fprintf(out, ")\n");
}

void log_binary_fn_op(int id, const char *op_name, double x_v, double x_e, double y_v, double y_e, double z_v, double z_e) {
    printf("(FN) (ID %d) %s: (%e, %e), (%e, %e), (%e, %e) \n", id, op_name, x_v, x_e, y_v, y_e, z_v, z_e);
}

void log_unary_fn_op(int id, const char *op_name, double x_v, double x_e, double z_v, double z_e) {
    printf("(FN) (ID %d) %s: (%e, %e), (%e, %e) \n", id, op_name, x_v, x_e, z_v, z_e);
}

void log_binary_fp_op(int id, const char *op_name, double x_v, double x_e, double y_v, double y_e, double z_v, double z_e) {
    printf("(FP) (ID %d) %s: (%e, %e), (%e, %e), (%e, %e) \n", id, op_name, x_v, x_e, y_v, y_e, z_v, z_e);
}

void log_unary_fp_op(int id, const char *op_name, double x_v, double x_e, double z_v, double z_e) {
    printf("(FP) (ID %d) %s: (%e, %e), (%e, %e) \n", id, op_name, x_v, x_e, z_v, z_e);
}

void log_binary_op_if_debug(int id, const char *op_name, double x_v, double y_v, double z_v) {
    if (contains_bsearch(debug_ids, n_debug_ids, id)) {
        simple_log_bin(id, op_name, x_v, y_v, z_v);
    }
}

void log_unary_op_if_debug(int id, const char *op_name, double x_v, double z_v) {
    if (contains_bsearch(debug_ids, n_debug_ids, id)) {
        simple_log_un(id, op_name, x_v, z_v);
    }
}

void log_binary_op_if_listed(int id, const char *op_name, double x_v, double x_e, double y_v, double y_e, double z_v, double z_e) {
    if (contains_bsearch(log_fn_ids, n_log_fn_ids, id)) {
        log_binary_fn_op(id, op_name, x_v, x_e, y_v, y_e, z_v, z_e);
    }
    if (contains_bsearch(log_fp_ids, n_log_fp_ids, id)) {
        log_binary_fp_op(id, op_name, x_v, x_e, y_v, y_e, z_v, z_e);
    }
}

void log_unary_op_if_listed(int id, const char *op_name, double x_v, double x_e, double z_v, double z_e) {
    if (contains_bsearch(log_fn_ids, n_log_fn_ids, id)) {
        log_unary_fn_op(id, op_name, x_v, x_e, z_v, z_e);
    }
    if (contains_bsearch(log_fp_ids, n_log_fp_ids, id)) {
        log_unary_fp_op(id, op_name, x_v, x_e, z_v, z_e);
    }
}

void print_ids_buf() {
    printf("\n");
    printf("Silent Op IDs (%d): ", *cur_size);
    if (*cur_size == 0) { printf("None\n"); }
    for (int i = 0; i < MAX_BUF_SIZE - 1; i++) {
        if (ids_buf[i] != -1) {
            if (ids_buf[i+1] != -1) { printf("%d,", ids_buf[i]); }
            else { printf("%d\n", ids_buf[i]); break; }
        }
    }
    if (ids_buf[MAX_BUF_SIZE-1] != -1) {printf("%d \n", ids_buf[MAX_BUF_SIZE-1]);}
}

void append_to_ids_buf(int id1, int id2) {
    if (*cur_size + 2 < MAX_BUF_SIZE) {
        ids_buf[(*cur_size)++] = id1;
        ids_buf[(*cur_size)++] = id2;
    }
    else {
        printf("Silent Op Buffer overflow ! \n");
        print_ids_buf();
     // reset buffer
        for (int i = 0; i < MAX_BUF_SIZE; ++i)
            ids_buf[i] = -1;
        *cur_size = 0;
        ids_buf[(*cur_size)++] = id1;
        ids_buf[(*cur_size)++] = id2;
    }
}

int cmp_int(const void *a, const void *b) {
    int ia = *(const int*)a, ib = *(const int*)b;
    return (ia < ib) ? -1 : (ia > ib);
}

bool contains_bsearch(int *arr, size_t n, int key) {
    return bsearch(&key, arr, n, sizeof(int), cmp_int) != NULL;
}

int OP_ID = 0;
int THRESHOLD = 45;
int HIGH_ROUNDING_ERROR = 0, INF = 0, BRANCH_FLIP = 0;
int ADD_TT = 0, SUB_TT = 0, MUL_TT = 0, DIV_TT = 0, FABS_TT = 0, CEIL_TT = 0, FLOOR_TT = 0, TRUNC_TT = 0, SQRT_TT = 0, RINT_TT = 0, NEG_TT = 0, COPYSIGN_TT = 0;
int ADD_ERR = 0, SUB_ERR = 0, MUL_ERR = 0, DIV_ERR = 0, FABS_ERR = 0, CEIL_ERR = 0, FLOOR_ERR = 0, TRUNC_ERR = 0, SQRT_ERR = 0, RINT_ERR = 0, COPYSIGN_ERR = 0;
static unsigned long long FP_CYCLE_TOTAL = 0;

unsigned long long read_fp_cycle_counter(void) {
    if (!WASM3_FP_TIMING) {
        return 0;
    }
#if __has_builtin(__builtin_readcyclecounter)
    return __builtin_readcyclecounter();
#else
    return 0;
#endif
}

void accumulate_fp_cycles(unsigned long long start) {
    if (!WASM3_FP_TIMING) {
        return;
    }

    FP_CYCLE_TOTAL += read_fp_cycle_counter() - start;
}

void reset_fp_cycle_counter(void) {
    FP_CYCLE_TOTAL = 0;
}

void printFpCycleInfo(void) {
    if (!WASM3_FP_TIMING) {
        return;
    }

    fprintf(stderr, "FP_CYCLES: %llu\n", FP_CYCLE_TOTAL);
}

void printErrorInfo() {
 // Print IDs needed to be silenced (Old way)
    // print_ids_buf();
 // FP Error Count
    printf("\n");
    printf("HIGH_ROUNDING_ERROR: %d\n", HIGH_ROUNDING_ERROR);
    printf("INFINITY: %d\n", INF);
    printf("BRANCH_FLIP: %d\n\n", BRANCH_FLIP);
 // Total Operation Count
    int total_ops = ADD_TT + SUB_TT + MUL_TT + DIV_TT + FABS_TT + CEIL_TT + FLOOR_TT + TRUNC_TT + SQRT_TT + RINT_TT + NEG_TT + COPYSIGN_TT;
    printf("OP_TT: %d\n", total_ops);
    printf("ADD_TT: %d\n", ADD_TT);
    printf("SUB_TT: %d\n", SUB_TT);
    printf("MUL_TT: %d\n", MUL_TT);
    printf("DIV_TT: %d\n", DIV_TT);
    printf("FABS_TT: %d\n", FABS_TT);
    printf("CEIL_TT: %d\n", CEIL_TT);
    printf("FLOOR_TT: %d\n", FLOOR_TT);
    printf("TRUNC_TT: %d\n", TRUNC_TT);
    printf("SQRT_TT: %d\n", SQRT_TT);
    printf("RINT_TT: %d\n", RINT_TT);
    printf("NEG_TT: %d\n", NEG_TT);
    printf("COPYSIGN_TT: %d\n\n", COPYSIGN_TT);
 // Operation with Error Count
    printf("ADD_ERR: %d\n", ADD_ERR);
    printf("SUB_ERR: %d\n", SUB_ERR);
    printf("MUL_ERR: %d\n", MUL_ERR);
    printf("DIV_ERR: %d\n", DIV_ERR);
    printf("FABS_ERR: %d\n", FABS_ERR);
    printf("CEIL_ERR: %d\n", CEIL_ERR);
    printf("FLOOR_ERR: %d\n", FLOOR_ERR);
    printf("TRUNC_ERR: %d\n", TRUNC_ERR);
    printf("SQRT_ERR: %d\n", SQRT_ERR);
    printf("RINT_ERR: %d\n", RINT_ERR);
    printf("COPYSIGN_ERR: %d\n\n", COPYSIGN_ERR);
}

unsigned long long ulp_d(double x, double y) {
    if (x == 0) {x = 0;}
    if (y == 0) {y = 0;}
    if (x != x) {return ULLONG_MAX - 1;}
    if (y != y) {return ULLONG_MAX - 1;}

    long long xx = *((long long *)&x);
    xx = xx < 0 ? LLONG_MIN - xx : xx;
  
    long long yy = *((long long *)&y);
    yy = yy < 0 ? LLONG_MIN - yy : yy;
    return xx >= yy ? xx - yy : yy - xx;
}

void incErrorCount(OP op) {
    switch (op) {
        case ADD:
            ADD_ERR += 1; break;
        case SUB:
            SUB_ERR += 1; break;
        case MUL:
            MUL_ERR += 1; break;
        case DIV:
            DIV_ERR += 1; break;
        case FABS:
            FABS_ERR += 1; break;
        case CEIL:
            CEIL_ERR += 1; break;
        case FLOOR:
            FLOOR_ERR += 1; break;
        case TRUNC:
            TRUNC_ERR += 1; break;
        case SQRT:
            SQRT_ERR += 1; break;
        case RINT:
            RINT_ERR += 1; break;
        case COPYSIGN:
            COPYSIGN_ERR += 1; break;
        default:
            break; // do nothing
    }
}

void debug_op(OP op) {
    switch (op) {
        case ADD:
            printf("OP %d: %s\n", OP_ID, "ADD"); break;
        case SUB:
            printf("OP %d: %s\n", OP_ID, "SUB"); break;
        case MUL:
            printf("OP %d: %s\n", OP_ID, "MUL"); break;
        case DIV:
            printf("OP %d: %s\n", OP_ID, "DIV"); break;
        case FABS:
            printf("OP %d: %s\n", OP_ID, "FABS"); break;
        case CEIL:
            printf("OP %d: %s\n", OP_ID, "CEIL"); break;
        case FLOOR:
            printf("OP %d: %s\n", OP_ID, "FLOOR"); break;
        case TRUNC:
            printf("OP %d: %s\n", OP_ID, "TRUNC"); break;
        case SQRT:
            printf("OP %d: %s\n", OP_ID, "SQRT"); break;
        case RINT:
            printf("OP %d: %s\n", OP_ID, "RINT"); break;
        case COPYSIGN:
            printf("OP %d: %s\n", OP_ID, "COPYSIGN"); break;
        default:
            printf("OP %d: %s\n", OP_ID, "ERROR"); break;
    }
}

int checkErrorBits_f64(double val, double err, int THRESHOLD, OP op) {
    int HRE = 0; // High Rounding Error
    double real = val + err;
    double bitsError = log2(ulp_d(val, real) + 1);
    // Print number of erroneous bits for FP/FN
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
    // check Inf - this can be first tested and then compute error bits
    if ((isinf(val) && isnan(err))) {
        INF += 1;
    }
    return HRE; // used for differentiate cancellations
}

int checkAndLogError_B(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e, OP op, const char *op_name) {
    if (isinf(*z_v)) return 0;
    int HRE = checkErrorBits_f64(*z_v, *z_e, THRESHOLD, op);
    if (HRE && WASM3_EOP_LOG) {
        printf("ID: %d \n", OP_ID);
    }
    return HRE;
}

int checkAndLogError_U(double x_v, double x_e, double *z_v, double *z_e, OP op, const char *op_name) {
    if (isinf(*z_v)) return 0;
    int HRE = checkErrorBits_f64(*z_v, *z_e, THRESHOLD, op);
    if (HRE && WASM3_EOP_LOG) {
        printf("ID: %d \n", OP_ID);
    }
    return HRE;
}

// Final routine in every arithmetic operation
void finalize_binary_op_dd(const char* op_name, OP op_type, int op_id, double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e) {
    if (WASM3_DEBUG_LOG) {
        log_binary_op_if_debug(op_id, op_name, x_v, y_v, *z_v);
    }
    log_binary_op_if_listed(op_id, op_name, x_v, x_e, y_v, y_e, *z_v, *z_e);
    checkAndLogError_B(x_v, x_e, y_v, y_e, z_v, z_e, op_type, op_name);

    if (WASM3_FULL_LOG && (!LOG_LIMIT || op_id <= LOG_LIMIT)) {
        log_binary_op(op_id, op_name, x_v, x_e, y_v, y_e, *z_v, *z_e);
    }
}

void finalize_unary_op_dd(const char* op_name, OP op_type, int op_id, double x_v, double x_e, double *z_v, double *z_e) {
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(op_id, op_name, x_v, *z_v);
    }
    log_unary_op_if_listed(op_id, op_name, x_v, x_e, *z_v, *z_e);
    checkAndLogError_U(x_v, x_e, z_v, z_e, op_type, op_name);

    if (WASM3_FULL_LOG && (!LOG_LIMIT || op_id <= LOG_LIMIT)) {
        log_unary_op(op_id, op_name, x_v, x_e, *z_v, *z_e);
    }
}

double twosum_di(double x_v, double y_v, double z) {  // z = x_v + y_v
    double yy = z - x_v;
    double xx = z - yy;
    double x_res = x_v - xx;
    double y_res = y_v - yy;
    return (x_res + y_res);
}

float twosum_fi(float x_v, float y_v, float z) {  // z = x_v + y_v
    float yy = z - x_v;
    float xx = z - yy;
    float x_res = x_v - xx;
    float y_res = y_v - yy;
    return (x_res + y_res);
}

// for double type
void add_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e) {
    OP_ID += 1;
    ADD_TT += 1;
    *z_v = x_v + y_v;
    *z_e = twosum_di(x_v, y_v, *z_v) + x_e + y_e;

    if ((x_v == -6755399441055744.0) || (y_v == -6755399441055744.0)) {
        *z_e = 0.0;
    }

    const char* OP_NAME = "ADD";
    const OP OP_TYPE = ADD;
    finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
}

#ifdef USE_EFTSAN
void sub_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e, bool check) {
    if (check) {
        OP_ID += 1;
        SUB_TT += 1;
    }
    *z_v = x_v - y_v;
    *z_e = twosum_di(x_v, -y_v, *z_v) + x_e + y_e; // addition
    const char* OP_NAME = "SUB";
    const OP OP_TYPE = SUB;
    if (check) {
        finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
    }
}
#else
void sub_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e, bool check) {
    if (check) {
        OP_ID += 1;
        SUB_TT += 1;
    }
    *z_v = x_v - y_v;
    *z_e = twosum_di(x_v, -y_v, *z_v) + x_e - y_e;

    if (y_v == 6755399441055744.0) {
        *z_e = 0.0;
    }

    const char* OP_NAME = "SUB";
    const OP OP_TYPE = SUB;
    if (check) {
        finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
    }
}
#endif

void sub_di_check_true(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e) {
    sub_di(x_v, x_e, y_v, y_e, z_v, z_e, true);
}
void sub_di_check_false(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e) {
    sub_di(x_v, x_e, y_v, y_e, z_v, z_e, false);
}

void mul_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e) {
    OP_ID += 1;
    MUL_TT += 1;
    double z = x_v * y_v;
    *z_v = z;
    *z_e = fma(x_v, y_v, -z) + x_e * y_v + x_v * y_e;
    const char* OP_NAME = "MUL";
    const OP OP_TYPE = MUL;
    finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
}

#ifdef USE_EFTSAN
void div_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e) {
    OP_ID += 1;    
    DIV_TT += 1;
    double z = x_v / y_v;
    *z_v = z;
    *z_e = (x_e + fma(z, y_v, -x_v) - z * y_e) / (y_v + y_e);  // change - fma to + fma as EFTSan did
    const char* OP_NAME = "DIV";
    const OP OP_TYPE = DIV;
    finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
}
#else
void div_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e) {
    OP_ID += 1;    
    DIV_TT += 1;
    double z = x_v / y_v;
    *z_v = z;
    *z_e = (x_e - fma(z, y_v, -x_v) - z * y_e) / (y_v + y_e);
    const char* OP_NAME = "DIV";
    const OP OP_TYPE = DIV;
    finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
}
#endif

void fabs_di(double x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    FABS_TT += 1;
    double z = fabs(x_v);
    *z_v = z;
    *z_e = fabs(x_v + x_e) - z;  // not safe if x_v >> x_e
    const char* OP_NAME = "FABS";
    const OP OP_TYPE = FABS;
    finalize_unary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, z_v, z_e);
}

void ceil_di(double x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    CEIL_TT += 1;
    double z = ceil(x_v);
    *z_v = z;
    *z_e = ceil(x_v + x_e) - z;  // not safe if x_v >> x_e
    const char* OP_NAME = "CEIL";
    const OP OP_TYPE = CEIL;
    finalize_unary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, z_v, z_e);
}

void floor_di(double x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    FLOOR_TT += 1;
    double z = floor(x_v);
    *z_v = z;
    *z_e = floor(x_v + x_e) - z;  // not safe if x_v >> x_e
    const char* OP_NAME = "FLOOR";
    const OP OP_TYPE = FLOOR;
    finalize_unary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, z_v, z_e);
}

void trunc_di(double x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    TRUNC_TT += 1;
    double z = trunc(x_v);
    *z_v = z;
    *z_e = trunc(x_v + x_e) - z;  // not safe if x_v >> x_e
    const char* OP_NAME = "TRUNC";
    const OP OP_TYPE = TRUNC;
    finalize_unary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, z_v, z_e);
}

void sqrt_di(double x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    SQRT_TT += 1;
    double z = sqrt(x_v);
    *z_v = z;
    *z_e = (z == 0) ? sqrt(fabs(x_e)) : (x_e + fma(-z, z, x_v)) / (2 * z);
    const char* OP_NAME = "SQRT";
    const OP OP_TYPE = SQRT;
    finalize_unary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, z_v, z_e);
}

void rint_di(double x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    RINT_TT += 1;
    double z = rint(x_v);
    *z_v = z;
    *z_e = rint(x_v + x_e) - z;  // not safe if x_v >> x_e
    const char* OP_NAME = "RINT";
    const OP OP_TYPE = RINT;
    finalize_unary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, z_v, z_e);
}

void neg_di(double x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    NEG_TT += 1;
    *z_v = -x_v;
    *z_e = -x_e;
    const char* OP_NAME = "NEG";
    const OP OP_TYPE = NEG;
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(OP_ID, OP_NAME, x_v, *z_v);
    }

    if (WASM3_FULL_LOG && (!LOG_LIMIT || OP_ID <= LOG_LIMIT)) {
        log_unary_op(OP_ID, OP_NAME, x_v, x_e, *z_v, *z_e);
    }
}

int equal_d(double x, double y) {return (x == y);}
int not_equal_d(double x, double y) {return !(x == y);}
int less_than_d(double x, double y) {return (x < y);}
int less_than_or_equal_d(double x, double y) {return (x <= y);}
int greater_than_d(double x, double y) {return (x > y);}
int greater_than_or_equal_d(double x, double y) {return (x >= y);}

int compare_d(double x_v, double x_e, double y_v, double y_e, comp_fn_d op, char *op_name) {
    int fp_comp = op(x_v, y_v);
    double z_v = 0.0;
    double z_e = 0.0;
    double *z_v_ptr = &z_v;
    double *z_e_ptr = &z_e;
    sub_di_check_false(x_v, x_e, y_v, y_e, z_v_ptr, z_e_ptr);
    int real_comp = op(z_v, -z_e);
    if ((fp_comp ^ real_comp)) {
        BRANCH_FLIP += 1;
    }
    return fp_comp;
}

// compare operator
int eq_di(double x_v, double x_e, double y_v, double y_e) {
    return compare_d(x_v, x_e, y_v, y_e, equal_d, "==");
}

int neq_di(double x_v, double x_e, double y_v, double y_e) {
    return compare_d(x_v, x_e, y_v, y_e, not_equal_d, "!=");
}

int less_di(double x_v, double x_e, double y_v, double y_e) {
    return compare_d(x_v, x_e, y_v, y_e, less_than_d, "<");
}

int less_eq_di(double x_v, double x_e, double y_v, double y_e) {
    return compare_d(x_v, x_e, y_v, y_e, less_than_or_equal_d, "<=");
}

int greater_di(double x_v, double x_e, double y_v, double y_e) {
    return compare_d(x_v, x_e, y_v, y_e, greater_than_d, ">");
}

int greater_eq_di(double x_v, double x_e, double y_v, double y_e) {
    return compare_d(x_v, x_e, y_v, y_e, greater_than_or_equal_d, ">=");
}

void min_f64_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e) {
    int b = less_di(x_v, x_e, y_v, y_e);
    *z_v = b ? x_v : y_v;
    *z_e = b ? x_e : y_e;
}

void max_f64_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e) {
    int b = greater_di(x_v, x_e, y_v, y_e);
    *z_v = b ? x_v : y_v;
    *z_e = b ? x_e : y_e;
}

void copysign_di(double x_v, double x_e, double y_v, double y_e, double *z_v, double *z_e) {
    OP_ID += 1;
    COPYSIGN_TT += 1;
    double z = copysign(x_v, y_v);
    *z_v = z;
    // check sign of y_v and y_v + y_e are the same
    if (copysign(1.0, y_v) == copysign(1.0, y_v + y_e)) {
        *z_e = copysign(x_e, y_v);
    }
    else {
        *z_e = copysign(x_v, y_v + y_e) + copysign(x_e, y_v + y_e) - z;
    }
    if (isnan(y_v)) { *z_e = 0.0; }
    const char* OP_NAME = "COPYSIGN";
    const OP OP_TYPE = COPYSIGN;
    finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
}


// for float type
void add_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e) {
    OP_ID += 1;
    ADD_TT += 1;
    *z_v = x_v + y_v;
    *z_e = twosum_di(x_v, y_v, *z_v) + x_e + y_e;

    if ((x_v == -6755399441055744.0) || (y_v == -6755399441055744.0)) {
        *z_e = 0.0;
    }

    const char* OP_NAME = "ADD";
    const OP OP_TYPE = ADD;
    finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
}

#ifdef USE_EFTSAN
void sub_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e, bool check) {
    if (check) {
        OP_ID += 1;
        SUB_TT += 1;
    }
    *z_v = x_v - y_v;
    *z_e = twosum_di(x_v, -y_v, *z_v) + x_e + y_e; // addition
    const char* OP_NAME = "SUB";
    const OP OP_TYPE = SUB;
    if (check) {
        finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
    }
}
#else
void sub_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e, bool check) {
    if (check) {
        OP_ID += 1;
        SUB_TT += 1;
    }
    *z_v = x_v - y_v;
    *z_e = twosum_di(x_v, -y_v, *z_v) + x_e - y_e;

    if (y_v == 6755399441055744.0) {
        *z_e = 0.0;
    }

    const char* OP_NAME = "SUB";
    const OP OP_TYPE = SUB;
    if (check) {
        finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
    }
}
#endif

void sub_fi_check_true(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e) {
    sub_fi(x_v, x_e, y_v, y_e, z_v, z_e, true);
}
void sub_fi_check_false(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e) {
    sub_fi(x_v, x_e, y_v, y_e, z_v, z_e, false);
}

void mul_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e) {
    OP_ID += 1;
    MUL_TT += 1;
    float z = x_v * y_v;
    *z_v = z;
    *z_e = fma(x_v, y_v, -z) + x_e * y_v + x_v * y_e;
    const char* OP_NAME = "MUL";
    const OP OP_TYPE = MUL;
    finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
}

#ifdef USE_EFTSAN
void div_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e) {
    OP_ID += 1;
    DIV_TT += 1;
    float z = x_v / y_v;
    *z_v = z;
    *z_e = (x_e + fma(z, y_v, -x_v) - z * y_e) / (y_v + y_e);  // change - fma to + fma as EFTSan did
    const char* OP_NAME = "DIV";
    const OP OP_TYPE = DIV;
    finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
}
#else
void div_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e) {
    OP_ID += 1;
    DIV_TT += 1;
    float z = x_v / y_v;
    *z_v = z;
    *z_e = (x_e - fma(z, y_v, -x_v) - z * y_e) / (y_v + y_e);
    const char* OP_NAME = "DIV";
    const OP OP_TYPE = DIV;
    finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
}
#endif

void fabsf_fi(float x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    FABS_TT += 1;
    float z = fabsf(x_v);
    *z_v = z;
    *z_e = fabs(x_v + x_e) - z;  // not safe if x_v >> x_e
    const char *OP_NAME = "FABS";
    const OP OP_TYPE = FABS;
    finalize_unary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, z_v, z_e);
}

void ceilf_fi(float x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    CEIL_TT += 1;
    float z = ceilf(x_v);
    *z_v = z;
    *z_e = ceil(x_v + x_e) - z;  // not safe if x_v >> x_e
    const char *OP_NAME = "CEIL";
    const OP OP_TYPE = CEIL;
    finalize_unary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, z_v, z_e);
}

void floorf_fi(float x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    FLOOR_TT += 1;
    float z = floorf(x_v);
    *z_v = z;
    *z_e = floor(x_v + x_e) - z;  // not safe if x_v >> x_e
    const char *OP_NAME = "FLOOR";
    const OP OP_TYPE = FLOOR;
    finalize_unary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, z_v, z_e);
}

void truncf_fi(float x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    TRUNC_TT += 1;
    float z = truncf(x_v);
    *z_v = z;
    *z_e = trunc(x_v + x_e) - z;  // not safe if x_v >> x_e
    const char *OP_NAME = "TRUNC";
    const OP OP_TYPE = TRUNC;
    finalize_unary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, z_v, z_e);
}

void sqrtf_fi(float x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    SQRT_TT += 1;
    float z = sqrtf(x_v);
    *z_v = z;
    *z_e = (z == 0) ? sqrt(fabs(x_e)) : (x_e + fma(-z, z, x_v)) / (2 * z);  // change fmaf to fma here
    const char *OP_NAME = "SQRT";
    const OP OP_TYPE = SQRT;
    finalize_unary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, z_v, z_e);
}

void rintf_fi(float x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    RINT_TT += 1;
    float z = rintf(x_v);
    *z_v = z;
    *z_e = rint(x_v + x_e) - z;  // not safe if x_v >> x_e
    const char *OP_NAME = "RINT";
    const OP OP_TYPE = RINT;
    finalize_unary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, z_v, z_e);
}

void negf_fi(float x_v, double x_e, double *z_v, double *z_e) {
    OP_ID += 1;
    NEG_TT += 1;
    *z_v = -x_v;
    *z_e = -x_e;
    const char* OP_NAME = "NEG";
    const OP OP_TYPE = NEG;
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(OP_ID, "NEG", x_v, *z_v);
    }

    if (WASM3_FULL_LOG && (!LOG_LIMIT || OP_ID <= LOG_LIMIT)) {
        log_unary_op(OP_ID, OP_NAME, x_v, x_e, *z_v, *z_e);
    }
}

int equal_f(float x, float y) {return (x == y);}
int not_equal_f(float x, float y) {return !(x == y);}
int less_than_f(float x, float y) {return (x < y);}
int less_than_or_equal_f(float x, float y) {return (x <= y);}
int greater_than_f(float x, float y) {return (x > y);}
int greater_than_or_equal_f(float x, float y) {return (x >= y);}

int compare_f(float x_v, double x_e, float y_v, double y_e, comp_fn_f op_f, comp_fn_d op_d, char *op_name) {
    int fp_comp = op_f(x_v, y_v);
    double z_v = 0.0;
    double z_e = 0.0;
    double *z_v_ptr = &z_v;
    double *z_e_ptr = &z_e;
    sub_fi_check_false(x_v, x_e, y_v, y_e, z_v_ptr, z_e_ptr);
    int real_comp = op_d(z_v, -z_e);  // real comparison done in double precision
    if ((fp_comp ^ real_comp)) {
        BRANCH_FLIP += 1;
    }
    return fp_comp;
}

// compare operator

int eq_fi(float x_v, double x_e, float y_v, double y_e) {
    return compare_f(x_v, x_e, y_v, y_e, equal_f, equal_d, "==");
}

int neq_fi(float x_v, double x_e, float y_v, double y_e) {
    return compare_f(x_v, x_e, y_v, y_e, not_equal_f, not_equal_d, "!=");
}

int less_fi(float x_v, double x_e, float y_v, double y_e) {
    return compare_f(x_v, x_e, y_v, y_e, less_than_f, less_than_d, "<");
}

int less_eq_fi(float x_v, double x_e, float y_v, double y_e) {
    return compare_f(x_v, x_e, y_v, y_e, less_than_or_equal_f, less_than_or_equal_d, "<=");
}

int greater_fi(float x_v, double x_e, float y_v, double y_e) {
    return compare_f(x_v, x_e, y_v, y_e, greater_than_f, greater_than_d, ">");
}

int greater_eq_fi(float x_v, double x_e, float y_v, double y_e) {
    return compare_f(x_v, x_e, y_v, y_e, greater_than_or_equal_f, greater_than_or_equal_d, ">=");
}

void min_f32_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e) {
    int b = less_fi(x_v, x_e, y_v, y_e);
    *z_v = b ? x_v : y_v;
    *z_e = b ? x_e : y_e;
}

void max_f32_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e) {
    int b = greater_fi(x_v, x_e, y_v, y_e);
    *z_v = b ? x_v : y_v;
    *z_e = b ? x_e : y_e;
}

void copysign_fi(float x_v, double x_e, float y_v, double y_e, double *z_v, double *z_e) {
    OP_ID += 1;
    COPYSIGN_TT += 1;
    float z = copysignf(x_v, y_v);
    *z_v = z;
    // check sign of y_v and y_v + y_e are the same
    if (copysign(1.0, y_v) == copysign(1.0, y_v + y_e)) {
        *z_e = copysign(x_e, y_v);
    }
    else {
        *z_e = copysign(x_v, y_v + y_e) + copysign(x_e, y_v + y_e) - z;
    }
    if (isnan(y_v)) { *z_e = 0.0; }
    const char *OP_NAME = "COPYSIGN";
    const OP OP_TYPE = COPYSIGN;
    finalize_binary_op_dd(OP_NAME, OP_TYPE, OP_ID, x_v, x_e, y_v, y_e, z_v, z_e);
}