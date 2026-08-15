#include "eid_library.h"

eid g_default_eid_register = {0};

override_entry *override_table = NULL;
char *temp_override_fn = "TEMP_OVERRIDE.bin";
char *temp_override_fp = NULL;
FILE *temp_override_file = NULL;
int *silent_ids = NULL; // Need to remove duplicates
int n_silent_ids = 0;
int silent_ids_cursor = 0;
int *probe_ids = NULL; // No duplicates
int n_probe_ids = 0;
int probe_ids_cursor = 0;

double axpy(double a, double b, double x, double c, double y) { // a+b*x+c*y
    double bx = b * x;
    double cy = c * y;
    double s1 = bx + cy;
    double s2 = a + s1;
    double e1 = fma(b, x, -bx);
    double e2 = fma(c, y, -cy);
    double e3 = twosum_di(bx, cy, s1);
    double e4 = twosum_di(a, s1, s2);
    return s2 + (e1 + e2 + e3 + e4);
}

double sum3(double a, double b, double c) { // a+b+c
    double v1 = b + c;
    double e1 = twosum_di(b, c, v1);
    double v2 = a + v1;
    double e2 = twosum_di(a, v1, v2);
    return v2 + (e1 + e2);
}

int checkErrorBits_eid(double val, double err, int THRESHOLD, OP op) {
    int HRE; // High Rounding Error
    double real, bitsError; // Real value; Number of erroneous bits
    real = val + err;
    bitsError = log2(ulp_d(val, real) + 1);
    HRE = (bitsError > THRESHOLD);
    if (HRE) {
        HIGH_ROUNDING_ERROR += 1;
        incErrorCount(op);
    }
    // check Inf - this can be first tested and then compute error bits
    if (isinf(val) && isnan(err)) {
        INF += 1;
    }
    return HRE; // used for differentiate cancellations
}

int checkAndLogError_eid(double *z_v, eid_t *z_e, OP op, const char *op_name) {
    // Handle primary overflow
    if (isinf(*z_v)) {
        return 0;
    }
    // Nothing to do if residue ~ 0
    if ((*z_e)->possible_zero) {
        return 0;
    }
    // (0, nan) -> (0, 0)
    if ((*z_v) == 0.0 && isnan((*z_e)->v)) {
        (*z_e)->v = 0.0;
        (*z_e)->possible_zero = true;
        return 0;
    }

    int HRE = checkErrorBits_eid(*z_v, (*z_e)->v, THRESHOLD, op);
    if (HRE && WASM3_EOP_LOG) {
        printf("ID: %d \n", OP_ID);
    }
    return HRE;
}

void load_override_table(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) { printf("No Overridden Residues\n"); return; }

    override_pair p;
    while (fread(&p, sizeof p, 1, fp) == 1) {
        override_entry *e;
        HASH_FIND_INT(override_table, &p.id, e);

        if (e) { // Update existing entry
            e->err_v = p.err_v;
            e->err_op = p.err_op;
            e->err_id = p.err_id;
            e->err_id_aux = p.err_id_aux;
            e->err_absorb = p.err_absorb;
            e->err_possible_zero = p.err_possible_zero;
        } else {
            e = malloc(sizeof *e);
            if (!e) {
                fprintf(stderr, "Failed to allocate memory for override entry\n");
                fclose(fp);
            }
            e->id  = p.id;
            e->err_v = p.err_v;
            e->err_op = p.err_op;
            e->err_id = p.err_id;
            e->err_id_aux = p.err_id_aux;
            e->err_absorb = p.err_absorb;
            e->err_possible_zero = p.err_possible_zero;
            HASH_ADD_INT(override_table, id, e);
        }
    }
    fclose(fp);
    // override_entry *e;
    // printf("Loaded override table:\n");
    // for (e = override_table; e != NULL; e = e->hh.next) {
    //     printf("Override ID: %d, Error: [%e, %d, %d, %d, %d, %d]\n", e->id, e->err_v, e->err_op, e->err_id, e->err_id_aux, e->err_absorb, e->err_possible_zero);
    // }
    // printf("Loading end\n");
}

override_entry* find_override_entry(int id) {
    override_entry *e = NULL;
    HASH_FIND_INT(override_table, &id, e);
    return e;
}
void apply_override_entry(const override_entry *e, eid_t *z_e) {
    (*z_e)->v = e->err_v;
    (*z_e)->op = e->err_op;
    (*z_e)->id = e->err_id;
    (*z_e)->id_aux = e->err_id_aux;
    (*z_e)->absorb = e->err_absorb;
    (*z_e)->possible_zero = e->err_possible_zero;
}

static inline bool consume_sorted_id(const int *ids, int count, int *cursor, int current_id) {
    if (!ids || count <= 0) {
        return false;
    }

    while (*cursor < count && (ids[*cursor] <= 0 || ids[*cursor] < current_id)) {
        (*cursor)++;
    }

    if (*cursor < count && ids[*cursor] == current_id) {
        (*cursor)++;
        return true;
    }

    return false;
}

// Functions related to SILENT IDs
void load_silent_ids(const char *path) {
    silent_ids_cursor = 0;
    if (silent_ids) {
        free(silent_ids);
        silent_ids = NULL;
        n_silent_ids = 0;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) { printf("No Silent IDs\n"); return; }

    int ch;
    size_t n = 0;
    while ((ch = fgetc(fp)) != EOF)
        if (ch == '\n')
            ++n;
    rewind(fp);

    silent_ids = malloc(n * sizeof(int));
    if (!silent_ids) { fclose(fp); fprintf(stderr, "Failed to allocate memory for silent IDs\n"); exit(EXIT_FAILURE); }

    for (size_t i = 0; i < n; ++i) {
        if (fscanf(fp, "%d", &silent_ids[i]) != 1) {
            fclose(fp);
            fprintf(stderr, "Failed to read integer from silent IDs file\n");
            exit(EXIT_FAILURE);
        }
    }
    n_silent_ids = n;
    fclose(fp);
    
    // printf("Number of silent ids: %d\n", n_silent_ids);
    // for (int i = 0; i < n_silent_ids; i++) {
    //     printf("Silent ID %d: %d\n", i, silent_ids[i]);
    // }
}

void remove_duplicate_silent_ids() {
    if (n_silent_ids == 0) return;

    int *unique_ids = malloc(n_silent_ids * sizeof(int));
    if (!unique_ids) { fprintf(stderr, "Failed to allocate memory for unique silent IDs\n"); exit(EXIT_FAILURE); }

    int unique_count = 0;
    unique_ids[unique_count++] = silent_ids[0];

    for (int i = 1; i < n_silent_ids; i++) {
        if (silent_ids[i] != silent_ids[i - 1]) {
            unique_ids[unique_count++] = silent_ids[i];
        }
    }

    free(silent_ids);
    silent_ids = unique_ids;
    n_silent_ids = unique_count;
}

void sort_silent_ids() {
    qsort(silent_ids, n_silent_ids, sizeof(int), cmp_int);
}

// Functions related to PROBE IDs
void load_probe_ids(const char *path) {
    probe_ids_cursor = 0;
    if (probe_ids) {
        free(probe_ids);
        probe_ids = NULL;
        n_probe_ids = 0;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) { printf("No Probe IDs\n"); return; }

    int ch;
    size_t n = 0;
    while ((ch = fgetc(fp)) != EOF)
        if (ch == '\n')
            ++n;
    rewind(fp);

    probe_ids = malloc(n * sizeof(int));
    if (!probe_ids) { fclose(fp); fprintf(stderr, "Failed to allocate memory for probe IDs\n"); exit(EXIT_FAILURE); }

    for (size_t i = 0; i < n; ++i) {
        if (fscanf(fp, "%d", &probe_ids[i]) != 1) {
            fclose(fp);
            fprintf(stderr, "Failed to read integer from probe IDs file\n");
            exit(EXIT_FAILURE);
        }
    }
    n_probe_ids = n;
    fclose(fp);
    
    // printf("Number of probe ids: %d\n", n_probe_ids);
    // for (int i = 0; i < n_probe_ids; i++) {
    //     printf("Probe ID %d: %d\n", i, probe_ids[i]);
    // }
}

void sort_probe_ids() {
    qsort(probe_ids, n_probe_ids, sizeof(int), cmp_int);
}

// Logging functions
void log_eid_fields(FILE *out, double primary_v, const eid_t e) {
    fprintf(out, "[%.20e, %d, %d, %d, %d, %d, %d]",
            e->v, e->op, e->id, e->id_aux, e->absorb, e->possible_zero, isinf(primary_v)); // Change here if logging format changes
}
void log_value_and_eid(FILE *out, double v, const eid_t e) {
    log_value_or_special(out, v);
    fprintf(out, ", ");
    log_eid_fields(out, v, e);
}

void log_binary_eid_op(int id, const char *op_name,
                       double x_v, eid_t x_e,
                       double y_v, eid_t y_e,
                       double z_v, eid_t z_e) {
    FILE *out = stderr;

    fprintf(out, "(ID %d) %s: (", id, op_name);
    log_value_and_eid(out, x_v, x_e);

    fprintf(out, "), (");
    log_value_and_eid(out, y_v, y_e);

    fprintf(out, "), (");
    log_value_and_eid(out, z_v, z_e);

    fprintf(out, ")\n");
}

void log_unary_eid_op(int id, const char *op_name,
                      double x_v, eid_t x_e,
                      double z_v, eid_t z_e) {
    FILE *out = stderr;

    fprintf(out, "(ID %d) %s: (", id, op_name);
    log_value_and_eid(out, x_v, x_e);

    fprintf(out, "), (");
    log_value_and_eid(out, z_v, z_e);

    fprintf(out, ")\n");
}

void log_binary_fn_eid_op(int id, const char *op_name, double x_v, eid_t x_e, double y_v, eid_t y_e, double z_v, eid_t z_e) {
    printf("(FN) (ID %d) %s: (%e, [%e, %d, %d, %d, %d, %d]), (%e, [%e, %d, %d, %d, %d, %d]), (%e, [%e, %d, %d, %d, %d, %d]) \n", id, op_name, x_v, x_e->v, x_e->id, x_e->id_aux, x_e->absorb, x_e->possible_zero, isinf(x_v), y_v, y_e->v, y_e->id, y_e->id_aux, y_e->absorb, y_e->possible_zero, isinf(y_v), z_v, z_e->v, z_e->id, z_e->id_aux, z_e->absorb, z_e->possible_zero, isinf(z_v));
}

void log_unary_fn_eid_op(int id, const char *op_name, double x_v, eid_t x_e, double z_v, eid_t z_e) {
    printf("(FN) (ID %d) %s: (%e, [%e, %d, %d, %d, %d, %d]), (%e, [%e, %d, %d, %d, %d, %d]) \n", id, op_name, x_v, x_e->v, x_e->id, x_e->id_aux, x_e->absorb, x_e->possible_zero, isinf(x_v), z_v, z_e->v, z_e->id, z_e->id_aux, z_e->absorb, z_e->possible_zero, isinf(z_v));
}

void log_binary_fp_eid_op(int id, const char *op_name, double x_v, eid_t x_e, double y_v, eid_t y_e, double z_v, eid_t z_e) {
    printf("(FP) (ID %d) %s: (%e, [%e, %d, %d, %d, %d, %d]), (%e, [%e, %d, %d, %d, %d, %d]), (%e, [%e, %d, %d, %d, %d, %d]) \n", id, op_name, x_v, x_e->v, x_e->id, x_e->id_aux, x_e->absorb, x_e->possible_zero, isinf(x_v), y_v, y_e->v, y_e->id, y_e->id_aux, y_e->absorb, y_e->possible_zero, isinf(y_v), z_v, z_e->v, z_e->id, z_e->id_aux, z_e->absorb, z_e->possible_zero, isinf(z_v));
}

void log_unary_fp_eid_op(int id, const char *op_name, double x_v, eid_t x_e, double z_v, eid_t z_e) {
    printf("(FP) (ID %d) %s: (%e, [%e, %d, %d, %d, %d, %d]), (%e, [%e, %d, %d, %d, %d, %d]) \n", id, op_name, x_v, x_e->v, x_e->id, x_e->id_aux, x_e->absorb, x_e->possible_zero, isinf(x_v), z_v, z_e->v, z_e->id, z_e->id_aux, z_e->absorb, z_e->possible_zero, isinf(z_v));
}

void log_binary_eid_op_if_listed(int id, const char *op_name, double x_v, eid_t x_e, double y_v, eid_t y_e, double z_v, eid_t z_e) {
    if (contains_bsearch(log_fn_ids, n_log_fn_ids, id)) {
        log_binary_fn_eid_op(id, op_name, x_v, x_e, y_v, y_e, z_v, z_e);
    }
    if (contains_bsearch(log_fp_ids, n_log_fp_ids, id)) {
        log_binary_fp_eid_op(id, op_name, x_v, x_e, y_v, y_e, z_v, z_e);
    }
}

void log_unary_eid_op_if_listed(int id, const char *op_name, double x_v, eid_t x_e, double z_v, eid_t z_e) {
    if (contains_bsearch(log_fn_ids, n_log_fn_ids, id)) {
        log_unary_fn_eid_op(id, op_name, x_v, x_e, z_v, z_e);
    }
    if (contains_bsearch(log_fp_ids, n_log_fp_ids, id)) {
        log_unary_fp_eid_op(id, op_name, x_v, x_e, z_v, z_e);
    }
}

static inline void reset_output_eid(eid_t *z_e) {
    **z_e = EID_DEFAULT;
}

static inline void normalize_output_ids(eid_t *z_e) {
    if ((*z_e)->id == (*z_e)->id_aux || (*z_e)->id <= 0) {
        (*z_e)->id_aux = 0;
    }
}

// Update error terms for unary operations
int upd_un_err(double pre_sum, double real_pre_sum, double op_err, double real_op_err, double amp_err, double real_amp_err, eid_t x_e, eid_t *z_e) {
    bool need_silence = false;
    bool input_absorb = x_e->absorb, output_absorb = false;

    double  a_op = fabs(real_op_err),
            a    = fabs(real_amp_err);
    bool op_amp  = a_op >= a;

    if (consume_sorted_id(silent_ids, n_silent_ids, &silent_ids_cursor, OP_ID)) {
        need_silence = true;
    }
 // Silence the operation if it is in silent_ids
    double err_sum, abs_err_sum, real_err_sum;
    if (need_silence) {
        // printf("Silencing operation %d with errors: [%e, %e]\n", OP_ID, op_err, amp_err);
        err_sum = amp_err;
        real_err_sum = real_amp_err;
        (*z_e)->id = x_e->id;
        (*z_e)->id_aux = 0;
    } else {
        err_sum = pre_sum;
        real_err_sum = real_pre_sum;
        // Set dominant error id
        if (a_op == 0.0 && a == 0.0) { // No error from current op and input
            (*z_e)->id = 0;
            (*z_e)->id_aux = 0;
        } else {
            (*z_e)->id = op_amp ? OP_ID : x_e->id;
            (*z_e)->id_aux = op_amp ? x_e->id : OP_ID;
        }

        abs_err_sum = a_op + a;
        output_absorb = (op_amp ? ABSORBED(abs_err_sum, a_op, a) : ABSORBED(abs_err_sum, a, a_op));
    }
    normalize_output_ids(z_e);

    (*z_e)->v = err_sum;
    (*z_e)->op = OP_ID;
    (*z_e)->absorb = input_absorb || output_absorb;
    (*z_e)->possible_zero = (real_err_sum == 0.0) ? true : false;
 // Check if the operation needs to be overridden
    override_entry *e = find_override_entry(OP_ID);
    bool need_override = (e != NULL);
    if (need_override && (!need_silence)) { // only override if it's not silenced
        apply_override_entry(e, z_e);
    }
 // Probe the operation if it is in probe_ids
    if (consume_sorted_id(probe_ids, n_probe_ids, &probe_ids_cursor, OP_ID)) {
        override_pair p = { .id = OP_ID, .err_v = (*z_e)->v, .err_op = (*z_e)->op, .err_id = (*z_e)->id, .err_id_aux = (*z_e)->id_aux, .err_absorb = (*z_e)->absorb, .err_possible_zero = (*z_e)->possible_zero };
        fwrite(&p, sizeof p, 1, temp_override_file);
    }
    return need_override;
}

// Update error terms for binary operations
int upd_bin_err(double pre_sum, double real_pre_sum, double op_err, double real_op_err, double amp_err_1, double real_amp_err_1, double amp_err_2, double real_amp_err_2, eid_t x_e, eid_t y_e, eid_t *z_e) {
    bool need_silence = false;
    bool input_absorb = x_e->absorb || y_e->absorb, output_absorb = false;

    double  a_op = fabs(real_op_err),
            a1   = fabs(real_amp_err_1),
            a2   = fabs(real_amp_err_2);
    bool op_amp_1   = a_op >= a1;
    bool op_amp_2   = a_op >= a2;
    bool amp1_amp2  = a1   >= a2;

    if (consume_sorted_id(silent_ids, n_silent_ids, &silent_ids_cursor, OP_ID)) {
        need_silence = true;
    }
 // Silence the operation if it is in silent_ids
    double err_sum, abs_err_sum, real_err_sum;
    if (need_silence) {
        // printf("Silencing operation %d with errors: [%e, %e, %e]\n", OP_ID, op_err, amp_err_1, amp_err_2);
        err_sum = amp_err_1 + amp_err_2;
        real_err_sum = real_amp_err_1 + real_amp_err_2;

        if (a1 == 0.0 && a2 == 0.0) {
            (*z_e)->id = 0;
            (*z_e)->id_aux = 0;
        }
        else {
            (*z_e)->id = amp1_amp2 ? x_e->id : y_e->id;
            (*z_e)->id_aux = amp1_amp2 ? y_e->id : x_e->id;
        }

        abs_err_sum = a1 + a2;
        output_absorb = (amp1_amp2 ? ABSORBED(abs_err_sum, a1, a2) : ABSORBED(abs_err_sum, a2, a1));
    }
    else {
        err_sum = pre_sum;
        real_err_sum = real_pre_sum;
     // Set dominant error id
        if (a_op == 0.0 && a1 == 0.0 && a2 == 0.0) { // No error from current op and inputs
            (*z_e)->id = 0;
            (*z_e)->id_aux = 0;
        } else {
            if (op_amp_1 && op_amp_2) {
                (*z_e)->id = OP_ID;
                (*z_e)->id_aux = amp1_amp2 ? x_e->id : y_e->id;
            } else if (amp1_amp2) {
                (*z_e)->id = x_e->id;
                (*z_e)->id_aux = op_amp_2 ? OP_ID : y_e->id;
            } else {
                (*z_e)->id = y_e->id;
                (*z_e)->id_aux = op_amp_1 ? OP_ID : x_e->id;
            }
        }

        abs_err_sum = a_op + a1 + a2;
        if (op_amp_1 && op_amp_2) {
            output_absorb = ABSORBED(abs_err_sum, a_op, a1+a2);
        } else if (amp1_amp2) {
            output_absorb = ABSORBED(abs_err_sum, a1, a_op+a2);
        } else {
            output_absorb = ABSORBED(abs_err_sum, a2, a_op+a1);
        }
    }
    normalize_output_ids(z_e);

    (*z_e)->v = err_sum;
    (*z_e)->op = OP_ID;
    (*z_e)->absorb = input_absorb || output_absorb;
    (*z_e)->possible_zero = (real_err_sum == 0.0) ? true : false;
 // Check if the operation needs to be overridden
    override_entry *e = find_override_entry(OP_ID);
    bool need_override = (e != NULL);
    if (need_override && (!need_silence)) { // only override if it's not silenced
        apply_override_entry(e, z_e);
    }
 // Probe the operation if it is in probe_ids
    if (consume_sorted_id(probe_ids, n_probe_ids, &probe_ids_cursor, OP_ID)) {
        override_pair p = { .id = OP_ID, .err_v = (*z_e)->v, .err_op = (*z_e)->op, .err_id = (*z_e)->id, .err_id_aux = (*z_e)->id_aux, .err_absorb = (*z_e)->absorb, .err_possible_zero = (*z_e)->possible_zero };
        fwrite(&p, sizeof p, 1, temp_override_file);
    }
    return need_override;
}

int upd_mul_err(double *pre_sum, double *real_pre_sum, double op_err, double real_op_err, double amp_err_1, double real_amp_err_1, double amp_err_2, double real_amp_err_2, double sec_ord_err, double real_sec_ord_err, eid_t x_e, eid_t y_e, eid_t *z_e) {
    bool need_silence = false;
    bool input_absorb = x_e->absorb || y_e->absorb, output_absorb = false;

    double  a_op = fabs(real_op_err),
            a1   = fabs(real_amp_err_1),
            a2   = fabs(real_amp_err_2);
    bool op_amp_1   = a_op >= a1;
    bool op_amp_2   = a_op >= a2;
    bool amp1_amp2  = a1   >= a2;

    if (consume_sorted_id(silent_ids, n_silent_ids, &silent_ids_cursor, OP_ID)) {
        need_silence = true;
    }
 // Silence the operation if it is in silent_ids
    double err_sum, abs_err_sum, real_err_sum;
    if (need_silence) {
        // printf("Silencing operation %d with errors: [%e, %e, %e, %e]\n", OP_ID, op_err, amp_err_1, amp_err_2, sec_ord_err);
        err_sum = (amp_err_1 + amp_err_2) + sec_ord_err;
        real_err_sum = (real_amp_err_1 + real_amp_err_2) + real_sec_ord_err;

        if (a1 == 0.0 && a2 == 0.0) {
            (*z_e)->id = 0;
            (*z_e)->id_aux = 0;
        }
        else {
            (*z_e)->id = amp1_amp2 ? x_e->id : y_e->id;
            (*z_e)->id_aux = amp1_amp2 ? y_e->id : x_e->id;
        }

        abs_err_sum = a1 + a2;
        output_absorb = (amp1_amp2 ? ABSORBED(abs_err_sum, a1, a2) : ABSORBED(abs_err_sum, a2, a1));
    }
    else {
        if (pre_sum != NULL) {
            err_sum = *pre_sum;
            real_err_sum = *real_pre_sum;
        } else {
            err_sum = ((op_err + amp_err_1) + amp_err_2) + sec_ord_err;
            real_err_sum = ((real_op_err + real_amp_err_1) + real_amp_err_2) + real_sec_ord_err;
        }
     // Set dominant error id
        if (a_op == 0.0 && a1 == 0.0 && a2 == 0.0) { // No error from current op and inputs
            (*z_e)->id = 0;
            (*z_e)->id_aux = 0;
        } else {
            if (op_amp_1 && op_amp_2) {
                (*z_e)->id = OP_ID;
                (*z_e)->id_aux = amp1_amp2 ? x_e->id : y_e->id;
            } else if (amp1_amp2) {
                (*z_e)->id = x_e->id;
                (*z_e)->id_aux = op_amp_2 ? OP_ID : y_e->id;
            } else {
                (*z_e)->id = y_e->id;
                (*z_e)->id_aux = op_amp_1 ? OP_ID : x_e->id;
            }
        }
        abs_err_sum = a_op + a1 + a2;
        if (op_amp_1 && op_amp_2) {
            output_absorb = ABSORBED(abs_err_sum, a_op, a1+a2);
        } else if (amp1_amp2) {
            output_absorb = ABSORBED(abs_err_sum, a1, a_op+a2);
        } else {
            output_absorb = ABSORBED(abs_err_sum, a2, a_op+a1);
        }
    }
    normalize_output_ids(z_e);

    (*z_e)->v = err_sum;
    (*z_e)->op = OP_ID;
    // (*z_e)->absorb = output_absorb; 
    (*z_e)->absorb = input_absorb || output_absorb;
    (*z_e)->possible_zero = (real_err_sum == 0.0) ? true : false;
 // Check if the operation needs to be overridden
    override_entry *e = find_override_entry(OP_ID);
    bool need_override = (e != NULL);
    if (need_override && (!need_silence)) { // only override if it's not silenced
        apply_override_entry(e, z_e);
    }
 // Probe the operation if it is in probe_ids
    if (consume_sorted_id(probe_ids, n_probe_ids, &probe_ids_cursor, OP_ID)) {
        override_pair p = { .id = OP_ID, .err_v = (*z_e)->v, .err_op = (*z_e)->op, .err_id = (*z_e)->id, .err_id_aux = (*z_e)->id_aux, .err_absorb = (*z_e)->absorb, .err_possible_zero = (*z_e)->possible_zero };
        fwrite(&p, sizeof p, 1, temp_override_file);
    }
    return need_override;
}

// Final routine in every arithmetic operation
void finalize_binary_op_eid(const char* op_name, OP op_type, int op_id, double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    // Check error bits
    checkAndLogError_eid(z_v, z_e, op_type, op_name);

    // Logging
    if (WASM3_DEBUG_LOG) {
        log_binary_op_if_debug(op_id, op_name, x_v, y_v, *z_v);
    }
    if (WASM3_FULL_LOG && (!LOG_LIMIT || op_id <= LOG_LIMIT)) {
        log_binary_eid_op(op_id, op_name, x_v, x_e, y_v, y_e, *z_v, *z_e);
    }
    log_binary_eid_op_if_listed(op_id, op_name, x_v, x_e, y_v, y_e, *z_v, *z_e);
}

void finalize_unary_op_eid(const char* op_name, OP op_type, int op_id, double x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    // Check error bits
    checkAndLogError_eid(z_v, z_e, op_type, op_name);

    // Logging
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(op_id, op_name, x_v, *z_v);
    }
    if (WASM3_FULL_LOG && (!LOG_LIMIT || op_id <= LOG_LIMIT)) {
        log_unary_eid_op(op_id, op_name, x_v, x_e, *z_v, *z_e);
    }
    log_unary_eid_op_if_listed(op_id, op_name, x_v, x_e, *z_v, *z_e);
}

bool no_error_source(eid_t e) {
    return (e->id <= 0 && e->id_aux <= 0);
}

static void detect_absorption(OP op_type, int op_id, const char* op_name, double pre_sum, double op_err, double amp_err_1, double amp_err_2, double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    bool is_fp_residue_zero = (pre_sum == 0.0); // Complete cancellation
    bool is_real_residue_zero = is_fp_residue_zero;

    if (!is_fp_residue_zero) { // High condition number check
        double condition_number = (fabs(op_err)+fabs(amp_err_1)+fabs(amp_err_2))/fabs(pre_sum);
        // fprintf(stderr, "OP_ID: %d, Condition Number: %f\n", op_id, log2(condition_number));
        if (condition_number > ldexp(1.0, COND_THRESH)) {
            is_real_residue_zero = true;
            (*z_e)->possible_zero = true;
        }
    }

    if (is_real_residue_zero) {
        bool xe_clear = no_error_source(x_e);
        bool ye_clear = no_error_source(y_e);
        if ((*z_e)->absorb) {
            if (op_type == MUL && (((x_v == 0.0) && xe_clear) || ((y_v == 0.0) && ye_clear))) {} // pass
            else if (xe_clear && ye_clear) {} // pass
            else {
                printf("Absorption: (%d,%d), (%d,%d), %d\n", x_e->id, x_e->id_aux, y_e->id, y_e->id_aux, op_id);
            }
        }
    }
    // if (is_absorb) {
    //     log_binary_eid_op(op_id, op_name, x_v, x_e, y_v, y_e, *z_v, *z_e);
    // }
}

// for double type
void add_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    ADD_TT += 1;

    *z_v = x_v + y_v;

// resue
    eid xe_tmp, ye_tmp;
    eid_t xe_use = x_e, ye_use = y_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }
    if (!ye_use) {
        ye_tmp = EID_DEFAULT;
        ye_use = &ye_tmp;
    }

    reset_output_eid(z_e);

    double pre_sum = twosum_di(x_v, y_v, *z_v) + xe_use->v + ye_use->v;
    double op_err = twosum_di(x_v, y_v, *z_v);
    double amp_err_1 = xe_use->v;
    double amp_err_2 = ye_use->v;

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v, real_y_err = ye_use->possible_zero ? 0.0 : ye_use->v;
    double real_pre_sum = twosum_di(x_v, y_v, *z_v) + real_x_err + real_y_err;
    double real_op_err = op_err;
    double real_amp_err_1 = real_x_err;
    double real_amp_err_2 = real_y_err;

    bool overridden = upd_bin_err(pre_sum, real_pre_sum, op_err, real_op_err, amp_err_1, real_amp_err_1, amp_err_2, real_amp_err_2, xe_use, ye_use, z_e);
// reuse
    if ((x_v == -6755399441055744.0 && xe_use->op <= 0) || (y_v == -6755399441055744.0 && ye_use->op <= 0)) {
        (*z_e)->v = 0.0;
        (*z_e)->id = 0;
        (*z_e)->id_aux = 0;
        (*z_e)->possible_zero = true;
    }

    const char* OP_NAME = "ADD";
    const OP OP_TYPE = ADD;
    if (!overridden) {
        detect_absorption(OP_TYPE, OP_ID, OP_NAME, pre_sum, op_err, amp_err_1, amp_err_2, x_v, xe_use, y_v, ye_use, z_v, z_e);
    }
    finalize_binary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);
}

//continue here

void sub_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e, bool check) {
    if (check) {
        OP_ID += 1;
        SUB_TT += 1;
    }
    *z_v = x_v - y_v;

    eid xe_tmp, ye_tmp;
    eid_t xe_use = x_e, ye_use = y_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }
    if (!ye_use) {
        ye_tmp = EID_DEFAULT;
        ye_use = &ye_tmp;
    }

    reset_output_eid(z_e);

    double pre_sum = twosum_di(x_v, -y_v, *z_v) + xe_use->v - ye_use->v;
    double op_err = twosum_di(x_v, -y_v, *z_v);
    double amp_err_1 = xe_use->v;
    double amp_err_2 = -ye_use->v;

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v, real_y_err = ye_use->possible_zero ? 0.0 : ye_use->v;
    double real_pre_sum = twosum_di(x_v, -y_v, *z_v) + real_x_err - real_y_err;
    double real_op_err = op_err;
    double real_amp_err_1 = real_x_err;
    double real_amp_err_2 = -real_y_err;

    bool overridden = upd_bin_err(pre_sum, real_pre_sum, op_err, real_op_err, amp_err_1, real_amp_err_1, amp_err_2, real_amp_err_2, xe_use, ye_use, z_e);

    if (y_v == 6755399441055744.0 && ye_use->op <= 0) {
        (*z_e)->v = 0.0;
        (*z_e)->id = 0;
        (*z_e)->id_aux = 0;
        (*z_e)->possible_zero = true;
    }

    const char* OP_NAME = "SUB";
    const OP OP_TYPE = SUB;
    if (check) {
     // no silence feature for branch
        if (!overridden) {
            detect_absorption(OP_TYPE, OP_ID, OP_NAME, pre_sum, op_err, amp_err_1, amp_err_2, x_v, xe_use, y_v, ye_use, z_v, z_e);
        }
        finalize_binary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);
    }
}

void sub_dei_check_true(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    sub_dei(x_v, x_e, y_v, y_e, z_v, z_e, true);
}
void sub_dei_check_false(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    sub_dei(x_v, x_e, y_v, y_e, z_v, z_e, false);
}

void mul_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    MUL_TT += 1;

    double z = x_v * y_v;
    *z_v = z;

    eid xe_tmp, ye_tmp;
    eid_t xe_use = x_e, ye_use = y_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }
    if (!ye_use) {
        ye_tmp = EID_DEFAULT;
        ye_use = &ye_tmp;
    }

    reset_output_eid(z_e);

    double op_err = fma(x_v, y_v, -z);
    double amp_err_1 = xe_use->v * (y_v + ye_use->v / 2);
    double amp_err_2 = ye_use->v * (x_v + xe_use->v / 2);
    double sec_ord_err = xe_use->v * ye_use->v;
    // avoid nan contamination
    if (isnan(sec_ord_err)) {
        amp_err_1 = xe_use->v * y_v;
        amp_err_2 = x_v * ye_use->v;
        sec_ord_err = 0.0;
    }

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v, real_y_err = ye_use->possible_zero ? 0.0 : ye_use->v;
    double real_op_err = op_err;
    double real_amp_err_1 = real_x_err * (y_v + real_y_err / 2);
    double real_amp_err_2 = real_y_err * (x_v + real_x_err / 2);
    double real_sec_ord_err = real_x_err * real_y_err;
    // avoid nan contamination
    if (isnan(real_sec_ord_err)) {
        real_amp_err_1 = real_x_err * y_v;
        real_amp_err_2 = x_v * real_y_err;
        real_sec_ord_err = 0.0;
    }

    double pre_sum = fma(x_v, y_v, -z) + xe_use->v * y_v + x_v * ye_use->v + sec_ord_err;
    double real_pre_sum = fma(x_v, y_v, -z) + real_x_err * y_v + x_v * real_y_err + real_sec_ord_err;

    if (isinf(sec_ord_err)) {
        if (isinf(xe_use->v) && isinf(ye_use->v)) {
            pre_sum = sec_ord_err;
        }
        else if (isinf(xe_use->v)) {
            pre_sum = copysign(xe_use->v, y_v + ye_use->v);
        }
        else if (isinf(ye_use->v)) {
            pre_sum = copysign(ye_use->v, x_v + xe_use->v);
        }
    }
    if (isinf(real_sec_ord_err)) {
        if (isinf(xe_use->v) && isinf(ye_use->v)) {
            real_pre_sum = real_sec_ord_err;
        }
        else if (isinf(xe_use->v)) {
            real_pre_sum = copysign(xe_use->v, y_v + ye_use->v);
        }
        else if (isinf(ye_use->v)) {
            real_pre_sum = copysign(ye_use->v, x_v + xe_use->v);
        }
    }

    bool overridden = upd_bin_err(pre_sum, real_pre_sum, op_err, real_op_err, amp_err_1, real_amp_err_1, amp_err_2, real_amp_err_2, xe_use, ye_use, z_e);

    const char* OP_NAME = "MUL";
    const OP OP_TYPE = MUL;
    if (!overridden) {
        // Need a different version for SECOND_ORDER_ERR?
        detect_absorption(OP_TYPE, OP_ID, OP_NAME, pre_sum, op_err, amp_err_1, amp_err_2, x_v, xe_use, y_v, ye_use, z_v, z_e);
    }
    finalize_binary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);
}

void div_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;    
    DIV_TT += 1;

    double z = x_v / y_v;
    *z_v = z;

    eid xe_tmp, ye_tmp;
    eid_t xe_use = x_e, ye_use = y_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }
    if (!ye_use) {
        ye_tmp = EID_DEFAULT;
        ye_use = &ye_tmp;
    }

    reset_output_eid(z_e);

    double pre_sum = (xe_use->v - fma(z, y_v, -x_v) - z * ye_use->v) / (y_v + ye_use->v);
    double op_err = (- fma(z, y_v, -x_v)) / (y_v + ye_use->v);
    double amp_err_1 = xe_use->v / (y_v + ye_use->v);
    double amp_err_2 = (- z * ye_use->v) / (y_v + ye_use->v);
    double sec_op_err = (- fma(op_err, y_v, -fma(z, y_v, -x_v))) / (y_v + ye_use->v);

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v, real_y_err = ye_use->possible_zero ? 0.0 : ye_use->v;
    double real_pre_sum = (real_x_err - fma(z, y_v, -x_v) - z * real_y_err) / (y_v + real_y_err);
    double real_op_err = (- fma(z, y_v, -x_v)) / (y_v + real_y_err);
    double real_amp_err_1 = real_x_err / (y_v + real_y_err);
    double real_amp_err_2 = (- z * real_y_err) / (y_v + real_y_err);
    double real_sec_op_err = (- fma(op_err, y_v, -fma(z, y_v, -x_v))) / (y_v + real_y_err);

    bool overridden = upd_bin_err(pre_sum, real_pre_sum, op_err, real_op_err, amp_err_1, real_amp_err_1, amp_err_2, real_amp_err_2, xe_use, ye_use, z_e);

    const char* OP_NAME = "DIV";
    const OP OP_TYPE = DIV;
    if (!overridden) {
        detect_absorption(OP_TYPE, OP_ID, OP_NAME, pre_sum, op_err, amp_err_1, amp_err_2, x_v, xe_use, y_v, ye_use, z_v, z_e);
    }
    finalize_binary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);
}

void fabs_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    FABS_TT += 1;

    double z = fabs(x_v);
    *z_v = z;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);

    double op_err = 0.0;
    double x_err = xe_use->v;
    double amp_err_1;
    if (x_v > 0 && x_err > 0) { // both positive
        amp_err_1 = x_err;
    } else if (x_v < 0 && x_err < 0) { // both negative
        amp_err_1 = -x_err;
    } else { // different signs
        if (fabs(x_v) > fabs(x_err)) { // error smaller than value
            if (x_v > 0) {
                amp_err_1 = x_err;
            } else {
                amp_err_1 = -x_err;
            }
        }
        else { // naive handling
            amp_err_1 = fabs(x_v + x_err) - z;
        }
    }

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v;
    double real_op_err = op_err;
    double real_amp_err_1;
    if (x_v > 0 && real_x_err > 0) {
        real_amp_err_1 = real_x_err;
    } else if (x_v < 0 && real_x_err < 0) {
        real_amp_err_1 = -real_x_err;
    } else {
        if (fabs(x_v) > fabs(real_x_err)) {
            if (x_v > 0) {
                real_amp_err_1 = real_x_err;
            } else {
                real_amp_err_1 = -real_x_err;
            }
        }
        else {
            real_amp_err_1 = fabs(x_v + real_x_err) - z;
        }
    }
    upd_un_err(op_err + amp_err_1, real_op_err + real_amp_err_1, op_err, real_op_err, amp_err_1, real_amp_err_1, xe_use, z_e);
    (*z_e)->id_aux = xe_use->id_aux;

    const char* OP_NAME = "FABS";
    const OP OP_TYPE = FABS;
    finalize_unary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);
}

void ceil_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    CEIL_TT += 1;

    double z = ceil(x_v);
    *z_v = z;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);

    double op_err = 0.0;
    double amp_err_1 = ceil(x_v + xe_use->v) - z;

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v;
    double real_op_err = op_err;
    double real_amp_err_1 = ceil(x_v + real_x_err) - z;
    upd_un_err(op_err + amp_err_1, real_op_err + real_amp_err_1, op_err, real_op_err, amp_err_1, real_amp_err_1, xe_use, z_e);

    const char* OP_NAME = "CEIL";
    const OP OP_TYPE = CEIL;
    finalize_unary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);
}

void floor_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    FLOOR_TT += 1;

    double z = floor(x_v);
    *z_v = z;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);

    double op_err = 0.0;
    double amp_err_1 = floor(x_v + xe_use->v) - z;

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v;
    double real_op_err = op_err;
    double real_amp_err_1 = floor(x_v + real_x_err) - z;
    upd_un_err(op_err + amp_err_1, real_op_err + real_amp_err_1, op_err, real_op_err, amp_err_1, real_amp_err_1, xe_use, z_e);

    const char* OP_NAME = "FLOOR";
    const OP OP_TYPE = FLOOR;
    finalize_unary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);
}

void trunc_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    TRUNC_TT += 1;

    double z = trunc(x_v);
    *z_v = z;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);

    double op_err = 0.0;
    double amp_err_1 = trunc(x_v + xe_use->v) - z;

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v;
    double real_op_err = op_err;
    double real_amp_err_1 = trunc(x_v + real_x_err) - z;
    upd_un_err(op_err + amp_err_1, real_op_err + real_amp_err_1, op_err, real_op_err, amp_err_1, real_amp_err_1, xe_use, z_e);

    const char* OP_NAME = "TRUNC";
    const OP OP_TYPE = TRUNC;
    finalize_unary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);
}

void sqrt_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    SQRT_TT += 1;

    double z = sqrt(x_v);
    *z_v = z;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);

    double pre_sum, real_pre_sum, op_err, real_op_err, amp_err_1, real_amp_err_1;
    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v;
    if (z == 0.0) {
        pre_sum = sqrt(fabs(xe_use->v));
        op_err = 0.0;
        amp_err_1 = sqrt(fabs(xe_use->v));

        real_pre_sum = sqrt(fabs(real_x_err));
        real_op_err = op_err;
        real_amp_err_1 = sqrt(fabs(real_x_err));
    } else {
        double tmp = sqrt(x_v + xe_use->v);
        pre_sum = (fma(-z, z, x_v) + xe_use->v) / (z + tmp);
        op_err = fma(-z, z, x_v) / (z + tmp);
        amp_err_1 = xe_use->v / (z + tmp);

        double real_tmp = sqrt(x_v + real_x_err);
        real_pre_sum = (fma(-z, z, x_v) + real_x_err) / (z + real_tmp);
        real_op_err = fma(-z, z, x_v) / (z + real_tmp);
        real_amp_err_1 = real_x_err / (z + real_tmp);
    }
    bool overridden = upd_un_err(pre_sum, real_pre_sum, op_err, real_op_err, amp_err_1, real_amp_err_1, xe_use, z_e);

    const char* OP_NAME = "SQRT";
    const OP OP_TYPE = SQRT;
    finalize_unary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);
}

void rint_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    RINT_TT += 1;

    double z = rint(x_v);
    *z_v = z;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);

    double op_err = 0.0;
    double amp_err_1 = rint(x_v + xe_use->v) - z;

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v;
    double real_op_err = op_err;
    double real_amp_err_1 = rint(x_v + real_x_err) - z;
    upd_un_err(op_err + amp_err_1, real_op_err + real_amp_err_1, op_err, real_op_err, amp_err_1, real_amp_err_1, xe_use, z_e);

    const char* OP_NAME = "RINT";
    const OP OP_TYPE = RINT;
    finalize_unary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);
}

void neg_dei(double x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    NEG_TT += 1;

    *z_v = -x_v;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);
    (*z_e)->v = -xe_use->v;
    (*z_e)->op = xe_use->op;
    (*z_e)->id = xe_use->id;
    (*z_e)->id_aux = xe_use->id_aux;
    (*z_e)->absorb = xe_use->absorb;
    (*z_e)->possible_zero = xe_use->possible_zero;

    const char* OP_NAME = "NEG";
    const OP OP_TYPE = NEG;
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(OP_ID, OP_NAME, x_v, *z_v);
    }
    if (WASM3_FULL_LOG && (!LOG_LIMIT || OP_ID <= LOG_LIMIT)) {
        log_unary_eid_op(OP_ID, OP_NAME, x_v, xe_use, *z_v, *z_e);
    }
}

int compare_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, comp_fn_d op, char *op_name) {
    int fp_comp = op(x_v, y_v);

    double z_v = 0.0;
    eid z_e = EID_DEFAULT;
    double *z_v_ptr = &z_v;
    eid_t z_e_ptr = &z_e;
    eid_t *z_e_out = &z_e_ptr;
    sub_dei_check_false(x_v, x_e, y_v, y_e, z_v_ptr, z_e_out);

    int real_comp = op(z_v, -(z_e.v));
    if ((fp_comp ^ real_comp)) {
        BRANCH_FLIP += 1;
    }
    return fp_comp;
}

int eq_dei(double x_v, eid_t x_e, double y_v, eid_t y_e) {
    return compare_dei(x_v, x_e, y_v, y_e, equal_d, "==");
}

int neq_dei(double x_v, eid_t x_e, double y_v, eid_t y_e) {
    return compare_dei(x_v, x_e, y_v, y_e, not_equal_d, "!=");
}

int less_dei(double x_v, eid_t x_e, double y_v, eid_t y_e) {
    return compare_dei(x_v, x_e, y_v, y_e, less_than_d, "<");
}

int less_eq_dei(double x_v, eid_t x_e, double y_v, eid_t y_e) {
    return compare_dei(x_v, x_e, y_v, y_e, less_than_or_equal_d, "<=");
}

int greater_dei(double x_v, eid_t x_e, double y_v, eid_t y_e) {
    return compare_dei(x_v, x_e, y_v, y_e, greater_than_d, ">");
}

int greater_eq_dei(double x_v, eid_t x_e, double y_v, eid_t y_e) {
    return compare_dei(x_v, x_e, y_v, y_e, greater_than_or_equal_d, ">=");
}

void min_f64_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    eid xe_tmp = EID_DEFAULT;
    eid ye_tmp = EID_DEFAULT;
    eid_t xe_use = x_e ? x_e : &xe_tmp;
    eid_t ye_use = y_e ? y_e : &ye_tmp;
    int b = less_dei(x_v, x_e, y_v, y_e);
    *z_v = b ? x_v : y_v;
    **z_e = b ? *xe_use : *ye_use;
}

void max_f64_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    eid xe_tmp = EID_DEFAULT;
    eid ye_tmp = EID_DEFAULT;
    eid_t xe_use = x_e ? x_e : &xe_tmp;
    eid_t ye_use = y_e ? y_e : &ye_tmp;
    int b = greater_dei(x_v, x_e, y_v, y_e);
    *z_v = b ? x_v : y_v;
    **z_e = b ? *xe_use : *ye_use;
}

void copysign_dei(double x_v, eid_t x_e, double y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    COPYSIGN_TT += 1;

    double z = copysign(x_v, y_v);
    *z_v = z;

    eid xe_tmp, ye_tmp;
    eid_t xe_use = x_e, ye_use = y_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }
    if (!ye_use) {
        ye_tmp = EID_DEFAULT;
        ye_use = &ye_tmp;
    }

    reset_output_eid(z_e);

    if (isinf(y_v) || isnan(y_v)) {
        upd_bin_err(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, xe_use, ye_use, z_e);
    }
    else {
        double op_err = copysign(x_v, y_v + ye_use->v) + copysign(xe_use->v, y_v + ye_use->v) - z;

        double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v, real_y_err = ye_use->possible_zero ? 0.0 : ye_use->v;
        double real_op_err = copysign(x_v, y_v + real_y_err) + copysign(real_x_err, y_v + real_y_err) - z;

        upd_bin_err(op_err, real_op_err, op_err, real_op_err, 0.0, 0.0, 0.0, 0.0, xe_use, ye_use, z_e);
    }

    const char* OP_NAME = "COPYSIGN";
    const OP OP_TYPE = COPYSIGN;
    finalize_binary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);
}

// for float type
void add_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    ADD_TT += 1;

    *z_v = x_v + y_v;

// resue
    eid xe_tmp, ye_tmp;
    eid_t xe_use = x_e, ye_use = y_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }
    if (!ye_use) {
        ye_tmp = EID_DEFAULT;
        ye_use = &ye_tmp;
    }

    reset_output_eid(z_e);

    double pre_sum = twosum_fi(x_v, y_v, *z_v) + xe_use->v + ye_use->v;
    double op_err = twosum_fi(x_v, y_v, *z_v);
    double amp_err_1 = xe_use->v;
    double amp_err_2 = ye_use->v;

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v, real_y_err = ye_use->possible_zero ? 0.0 : ye_use->v;
    double real_pre_sum = twosum_fi(x_v, y_v, *z_v) + real_x_err + real_y_err;
    double real_op_err = op_err;
    double real_amp_err_1 = real_x_err;
    double real_amp_err_2 = real_y_err;

    bool overridden = upd_bin_err(pre_sum, real_pre_sum, op_err, real_op_err, amp_err_1, real_amp_err_1, amp_err_2, real_amp_err_2, xe_use, ye_use, z_e);
// reuse
    if ((x_v == -6755399441055744.0 && xe_use->op <= 0) || (y_v == -6755399441055744.0 && ye_use->op <= 0)) {
        (*z_e)->v = 0.0;
        (*z_e)->id = 0;
        (*z_e)->id_aux = 0;
        (*z_e)->possible_zero = true;
    }

    const char* OP_NAME = "ADD";
    const OP OP_TYPE = ADD;
    if (!overridden) {
        detect_absorption(OP_TYPE, OP_ID, OP_NAME, pre_sum, op_err, amp_err_1, amp_err_2, x_v, xe_use, y_v, ye_use, z_v, z_e);
    }
    finalize_binary_op_eid("ADD", ADD, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);
}

void sub_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e, bool check) {
    if (check) {
        OP_ID += 1;
        SUB_TT += 1;
    }
    *z_v = x_v - y_v;

    eid xe_tmp, ye_tmp;
    eid_t xe_use = x_e, ye_use = y_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }
    if (!ye_use) {
        ye_tmp = EID_DEFAULT;
        ye_use = &ye_tmp;
    }

    reset_output_eid(z_e);

    double pre_sum = twosum_fi(x_v, -y_v, *z_v) + xe_use->v - ye_use->v;
    double op_err = twosum_fi(x_v, -y_v, *z_v);
    double amp_err_1 = xe_use->v;
    double amp_err_2 = -ye_use->v;

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v, real_y_err = ye_use->possible_zero ? 0.0 : ye_use->v;
    double real_pre_sum = twosum_fi(x_v, -y_v, *z_v) + real_x_err - real_y_err;
    double real_op_err = op_err;
    double real_amp_err_1 = real_x_err;
    double real_amp_err_2 = -real_y_err;

    bool overridden = upd_bin_err(pre_sum, real_pre_sum, op_err, real_op_err, amp_err_1, real_amp_err_1, amp_err_2, real_amp_err_2, xe_use, ye_use, z_e);

    if (y_v == 6755399441055744.0 && ye_use->op <= 0) {
        (*z_e)->v = 0.0;
        (*z_e)->id = 0;
        (*z_e)->id_aux = 0;
        (*z_e)->possible_zero = true;
    }

    const char* OP_NAME = "SUB";
    const OP OP_TYPE = SUB;
    if (check) {
     // no silence feature for branch
        if (!overridden) {
            detect_absorption(OP_TYPE, OP_ID, OP_NAME, pre_sum, op_err, amp_err_1, amp_err_2, x_v, xe_use, y_v, ye_use, z_v, z_e);
        }
        finalize_binary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);
    }
}

void sub_fei_check_true(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    sub_fei(x_v, x_e, y_v, y_e, z_v, z_e, true);
}
void sub_fei_check_false(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    sub_fei(x_v, x_e, y_v, y_e, z_v, z_e, false);
}

// Need to use fma instead of fmaf to compute the error (gromacs-bondfree-580)
void mul_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    MUL_TT += 1;

    float z = x_v * y_v;
    *z_v = z;

    eid xe_tmp, ye_tmp;
    eid_t xe_use = x_e, ye_use = y_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }
    if (!ye_use) {
        ye_tmp = EID_DEFAULT;
        ye_use = &ye_tmp;
    }

    reset_output_eid(z_e);

    // double op_err = fmaf(x_v, y_v, -z);
    double op_err = fma(x_v, y_v, -z);
    double amp_err_1 = xe_use->v * (y_v + ye_use->v / 2);
    double amp_err_2 = ye_use->v * (x_v + xe_use->v / 2);
    double sec_ord_err = xe_use->v * ye_use->v;
    if (isnan(sec_ord_err)) {
        amp_err_1 = xe_use->v * y_v;
        amp_err_2 = x_v * ye_use->v;
        sec_ord_err = 0.0;
    }

    // double pre_sum = axpy(op_err, xe_use->v, y_v, x_v, ye_use->v);
    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v, real_y_err = ye_use->possible_zero ? 0.0 : ye_use->v;
    double real_op_err = op_err;
    double real_amp_err_1 = real_x_err * (y_v + real_y_err / 2);
    double real_amp_err_2 = real_y_err * (x_v + real_x_err / 2);
    double real_sec_ord_err = real_x_err * real_y_err;
    if (isnan(real_sec_ord_err)) {
        real_amp_err_1 = real_x_err * y_v;
        real_amp_err_2 = x_v * real_y_err;
        real_sec_ord_err = 0.0;
    }

    double pre_sum = fma(x_v, y_v, -z) + xe_use->v * y_v + x_v * ye_use->v + sec_ord_err;
    double real_pre_sum = fma(x_v, y_v, -z) + real_x_err * y_v + x_v * real_y_err + real_sec_ord_err;

    if (isinf(sec_ord_err)) {
        if (isinf(xe_use->v) && isinf(ye_use->v)) {
            pre_sum = sec_ord_err;
        }
        else if (isinf(xe_use->v)) {
            pre_sum = copysign(xe_use->v, y_v + ye_use->v);
        }
        else if (isinf(ye_use->v)) {
            pre_sum = copysign(ye_use->v, x_v + xe_use->v);
        }
    }
    if (isinf(real_sec_ord_err)) {
        if (isinf(xe_use->v) && isinf(ye_use->v)) {
            real_pre_sum = real_sec_ord_err;
        }
        else if (isinf(xe_use->v)) {
            real_pre_sum = copysign(xe_use->v, y_v + ye_use->v);
        }
        else if (isinf(ye_use->v)) {
            real_pre_sum = copysign(ye_use->v, x_v + xe_use->v);
        }
    }

    bool overridden = upd_bin_err(pre_sum, real_pre_sum, op_err, real_op_err, amp_err_1, real_amp_err_1, amp_err_2, real_amp_err_2, xe_use, ye_use, z_e);

    const char* OP_NAME = "MUL";
    const OP OP_TYPE = MUL;
    if (!overridden) {
        // Need a different version for SECOND_ORDER_ERR?
        detect_absorption(OP_TYPE, OP_ID, OP_NAME, pre_sum, op_err, amp_err_1, amp_err_2, x_v, xe_use, y_v, ye_use, z_v, z_e);
    }
    finalize_binary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);
}

void div_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;    
    DIV_TT += 1;

    float z = x_v / y_v;
    *z_v = z;

    eid xe_tmp = EID_DEFAULT;
    eid ye_tmp = EID_DEFAULT;

    eid_t xe_use = x_e ? x_e : &xe_tmp;
    eid_t ye_use = y_e ? y_e : &ye_tmp;

    reset_output_eid(z_e);

    double pre_sum = (xe_use->v - fmaf(z, y_v, -x_v) - z * ye_use->v) / (y_v + ye_use->v);
    double op_err = (- fmaf(z, y_v, -x_v)) / (y_v + ye_use->v);
    double amp_err_1 = xe_use->v / (y_v + ye_use->v);
    double amp_err_2 = (- z * ye_use->v) / (y_v + ye_use->v);
    double sec_op_err = (- fmaf(op_err, y_v, -fmaf(z, y_v, -x_v))) / (y_v + ye_use->v);

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v, real_y_err = ye_use->possible_zero ? 0.0 : ye_use->v;
    double real_pre_sum = (real_x_err - fmaf(z, y_v, -x_v) - z * real_y_err) / (y_v + real_y_err);
    double real_op_err = (- fmaf(z, y_v, -x_v)) / (y_v + real_y_err);
    double real_amp_err_1 = real_x_err / (y_v + real_y_err);
    double real_amp_err_2 = (- z * real_y_err) / (y_v + real_y_err);
    double real_sec_op_err = (- fmaf(op_err, y_v, -fmaf(z, y_v, -x_v))) / (y_v + real_y_err);

    bool overridden = upd_bin_err(pre_sum, real_pre_sum, op_err, real_op_err, amp_err_1, real_amp_err_1, amp_err_2, real_amp_err_2, xe_use, ye_use, z_e);

    const char* OP_NAME = "DIV";
    const OP OP_TYPE = DIV;
    if (!overridden) {
        detect_absorption(OP_TYPE, OP_ID, OP_NAME, pre_sum, op_err, amp_err_1, amp_err_2, x_v, xe_use, y_v, ye_use, z_v, z_e);
    }
    finalize_binary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);
}

void fabsf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    FABS_TT += 1;

    float z = fabsf(x_v);
    *z_v = z;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);

    double op_err = 0.0;
    double x_err = xe_use->v;
    double amp_err_1;
    if (x_v > 0 && x_err > 0) {
        amp_err_1 = x_err;
    } else if (x_v < 0 && x_err < 0) {
        amp_err_1 = -x_err;
    } else {
        if (fabs(x_v) > fabs(x_err)) { // error smaller than value
            if (x_v > 0) {
                amp_err_1 = x_err;
            } else {
                amp_err_1 = -x_err;
            }
        }
        else { // naive handling
            amp_err_1 = fabs(x_v + x_err) - z;
        }
    }

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v;
    double real_op_err = op_err;
    double real_amp_err_1;
    if (x_v > 0 && real_x_err > 0) {
        real_amp_err_1 = real_x_err;
    } else if (x_v < 0 && real_x_err < 0) {
        real_amp_err_1 = -real_x_err;
    } else {
        if (fabs(x_v) > fabs(real_x_err)) {
            if (x_v > 0) {
                real_amp_err_1 = real_x_err;
            } else {
                real_amp_err_1 = -real_x_err;
            }
        }
        else {
            real_amp_err_1 = fabs(x_v + real_x_err) - z;
        }
    }
    upd_un_err(op_err + amp_err_1, real_op_err + real_amp_err_1, op_err, real_op_err, amp_err_1, real_amp_err_1, xe_use, z_e);
    (*z_e)->id_aux = xe_use->id_aux;

    const char* OP_NAME = "FABS";
    const OP OP_TYPE = FABS;
    finalize_unary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);
}

void ceilf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    CEIL_TT += 1;

    float z = ceilf(x_v);
    *z_v = z;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);

    double op_err = 0.0;
    double amp_err_1 = ceil(x_v + xe_use->v) - z;

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v;
    double real_op_err = op_err;
    double real_amp_err_1 = ceil(x_v + real_x_err) - z;
    upd_un_err(op_err + amp_err_1, real_op_err + real_amp_err_1, op_err, real_op_err, amp_err_1, real_amp_err_1, xe_use, z_e);

    const char* OP_NAME = "CEIL";
    const OP OP_TYPE = CEIL;
    finalize_unary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);
}

void floorf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    FLOOR_TT += 1;

    float z = floorf(x_v);
    *z_v = z;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);

    double op_err = 0.0;
    double amp_err_1 = floor(x_v + xe_use->v) - z;

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v;
    double real_op_err = op_err;
    double real_amp_err_1 = floor(x_v + real_x_err) - z;
    upd_un_err(op_err + amp_err_1, real_op_err + real_amp_err_1, op_err, real_op_err, amp_err_1, real_amp_err_1, xe_use, z_e);

    const char* OP_NAME = "FLOOR";
    const OP OP_TYPE = FLOOR;
    finalize_unary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);
}

void truncf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    TRUNC_TT += 1;

    float z = truncf(x_v);
    *z_v = z;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);

    double op_err = 0.0;
    double amp_err_1 = trunc(x_v + xe_use->v) - z;

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v;
    double real_op_err = op_err;
    double real_amp_err_1 = trunc(x_v + real_x_err) - z;
    upd_un_err(op_err + amp_err_1, real_op_err + real_amp_err_1, op_err, real_op_err, amp_err_1, real_amp_err_1, xe_use, z_e);

    const char* OP_NAME = "TRUNC";
    const OP OP_TYPE = TRUNC;
    finalize_unary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);
}

void sqrtf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    SQRT_TT += 1;

    float z = sqrtf(x_v);
    *z_v = z;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);

    double pre_sum, real_pre_sum, op_err, real_op_err, amp_err_1, real_amp_err_1;
    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v;
    if (z == 0.0) {
        pre_sum = sqrt(fabs(xe_use->v));
        op_err = 0.0;
        amp_err_1 = sqrt(fabs(xe_use->v));

        real_pre_sum = sqrt(fabs(real_x_err));
        real_op_err = op_err;
        real_amp_err_1 = sqrt(fabs(real_x_err));
    } else {
        double tmp = sqrt(x_v + xe_use->v);
        pre_sum = (fma(-z, z, x_v) + xe_use->v) / (z + tmp);
        op_err = fma(-z, z, x_v) / (z + tmp);
        amp_err_1 = xe_use->v / (z + tmp);

        double real_tmp = sqrt(x_v + real_x_err);
        real_pre_sum = (fma(-z, z, x_v) + real_x_err) / (z + real_tmp);
        real_op_err = fma(-z, z, x_v) / (z + real_tmp);
        real_amp_err_1 = real_x_err / (z + real_tmp);
    }
    bool overridden = upd_un_err(pre_sum, real_pre_sum, op_err, real_op_err, amp_err_1, real_amp_err_1, xe_use, z_e);

    const char* OP_NAME = "SQRT";
    const OP OP_TYPE = SQRT;
    finalize_unary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);
}

void rintf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    RINT_TT += 1;

    float z = rintf(x_v);
    *z_v = z;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);

    double op_err = 0.0;
    double amp_err_1 = rint(x_v + xe_use->v) - z;

    double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v;
    double real_op_err = op_err;
    double real_amp_err_1 = rint(x_v + real_x_err) - z;
    upd_un_err(op_err + amp_err_1, real_op_err + real_amp_err_1, op_err, real_op_err, amp_err_1, real_amp_err_1, xe_use, z_e);

    const char* OP_NAME = "RINT";
    const OP OP_TYPE = RINT;
    finalize_unary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, z_v, z_e);
}

void negf_fei(float x_v, eid_t x_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    NEG_TT += 1;

    *z_v = -x_v;

    eid xe_tmp;
    eid_t xe_use = x_e;
    if (!xe_use) {
        xe_tmp = EID_DEFAULT;
        xe_use = &xe_tmp;
    }

    reset_output_eid(z_e);
    (*z_e)->v = -xe_use->v;
    (*z_e)->op = xe_use->op;
    (*z_e)->id = xe_use->id;
    (*z_e)->id_aux = xe_use->id_aux;
    (*z_e)->absorb = xe_use->absorb;
    (*z_e)->possible_zero = xe_use->possible_zero;

    const char* OP_NAME = "NEG";
    const OP OP_TYPE = NEG;
    if (WASM3_DEBUG_LOG) {
        log_unary_op_if_debug(OP_ID, OP_NAME, x_v, *z_v);
    }
    if (WASM3_FULL_LOG && (!LOG_LIMIT || OP_ID <= LOG_LIMIT)) {
        log_unary_eid_op(OP_ID, OP_NAME, x_v, xe_use, *z_v, *z_e);
    }
}

int compare_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, comp_fn_f op_f, comp_fn_d op_d, char *op_name) {
    int fp_comp = op_f(x_v, y_v);

    double z_v = 0.0;
    eid z_e = EID_DEFAULT;
    double *z_v_ptr = &z_v;
    eid_t z_e_ptr = &z_e;
    eid_t *z_e_out = &z_e_ptr;
    sub_fei_check_false(x_v, x_e, y_v, y_e, z_v_ptr, z_e_out);
    
    int real_comp = op_d(z_v, -(z_e.v));
    if ((fp_comp ^ real_comp)) {
        BRANCH_FLIP += 1;
    }
    return fp_comp;
}

int eq_fei(float x_v, eid_t x_e, float y_v, eid_t y_e) {
    return compare_fei(x_v, x_e, y_v, y_e, equal_f, equal_d, "==");
}

int neq_fei(float x_v, eid_t x_e, float y_v, eid_t y_e) {
    return compare_fei(x_v, x_e, y_v, y_e, not_equal_f, not_equal_d, "!=");
}

int less_fei(float x_v, eid_t x_e, float y_v, eid_t y_e) {
    return compare_fei(x_v, x_e, y_v, y_e, less_than_f, less_than_d, "<");
}

int less_eq_fei(float x_v, eid_t x_e, float y_v, eid_t y_e) {
    return compare_fei(x_v, x_e, y_v, y_e, less_than_or_equal_f, less_than_or_equal_d, "<=");
}

int greater_fei(float x_v, eid_t x_e, float y_v, eid_t y_e) {
    return compare_fei(x_v, x_e, y_v, y_e, greater_than_f, greater_than_d, ">");
}

int greater_eq_fei(float x_v, eid_t x_e, float y_v, eid_t y_e) {
    return compare_fei(x_v, x_e, y_v, y_e, greater_than_or_equal_f, greater_than_or_equal_d, ">=");
}

void min_f32_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    eid xe_tmp = EID_DEFAULT;
    eid ye_tmp = EID_DEFAULT;
    eid_t xe_use = x_e ? x_e : &xe_tmp;
    eid_t ye_use = y_e ? y_e : &ye_tmp;
    int b = less_fei(x_v, x_e, y_v, y_e);
    *z_v = b ? x_v : y_v;
    **z_e = b ? *xe_use : *ye_use;
}

void max_f32_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    eid xe_tmp = EID_DEFAULT;
    eid ye_tmp = EID_DEFAULT;
    eid_t xe_use = x_e ? x_e : &xe_tmp;
    eid_t ye_use = y_e ? y_e : &ye_tmp;
    int b = greater_fei(x_v, x_e, y_v, y_e);
    *z_v = b ? x_v : y_v;
    **z_e = b ? *xe_use : *ye_use;
}

void copysign_fei(float x_v, eid_t x_e, float y_v, eid_t y_e, double *z_v, eid_t *z_e) {
    OP_ID += 1;
    COPYSIGN_TT += 1;

    float z = copysignf(x_v, y_v);
    *z_v = z;

    eid xe_tmp = EID_DEFAULT;
    eid ye_tmp = EID_DEFAULT;

    eid_t xe_use = x_e ? x_e : &xe_tmp;
    eid_t ye_use = y_e ? y_e : &ye_tmp;

    reset_output_eid(z_e);

    if (isinf(y_v) || isnan(y_v)) {
        upd_bin_err(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, xe_use, ye_use, z_e);
    }
    else {
        double op_err = copysign(x_v, y_v + ye_use->v) + copysign(xe_use->v, y_v + ye_use->v) - z;

        double real_x_err = xe_use->possible_zero ? 0.0 : xe_use->v, real_y_err = ye_use->possible_zero ? 0.0 : ye_use->v;
        double real_op_err = copysign(x_v, y_v + real_y_err) + copysign(real_x_err, y_v + real_y_err) - z;

        upd_bin_err(op_err, real_op_err, op_err, real_op_err, 0.0, 0.0, 0.0, 0.0, xe_use, ye_use, z_e);
    }

    const char* OP_NAME = "COPYSIGN";
    const OP OP_TYPE = COPYSIGN;
    finalize_binary_op_eid(OP_NAME, OP_TYPE, OP_ID, x_v, xe_use, y_v, ye_use, z_v, z_e);
}

void free_all() {
 // Close opened file
    if (temp_override_file) {
        fclose(temp_override_file);
        temp_override_file = NULL;
    }
 // Free override table
    override_entry *cur, *tmp;
    HASH_ITER(hh, override_table, cur, tmp) {
        HASH_DEL(override_table, cur);
        free(cur);
    }
 // Free silent IDs
    if (silent_ids) {
        free(silent_ids);
        silent_ids = NULL;
        n_silent_ids = 0;
        silent_ids_cursor = 0;
    }
 // Free probe IDs
    if (probe_ids) {    
        free(probe_ids);
        probe_ids = NULL;
        n_probe_ids = 0;
        probe_ids_cursor = 0;
    }
 // Free log IDs (FN)
    if (log_fn_ids) {
        free(log_fn_ids);
        log_fn_ids = NULL;
        n_log_fn_ids = 0;
    }
 // Free log IDs (FP)
    if (log_fp_ids) {
        free(log_fp_ids);
        log_fp_ids = NULL;
        n_log_fp_ids = 0;
    }
}
