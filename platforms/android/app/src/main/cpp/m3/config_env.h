#ifndef CONFIG_ENV_H
#define CONFIG_ENV_H

extern bool WASM3_EOP_LOG;
extern bool WASM3_DEBUG_LOG;
extern bool WASM3_FULL_LOG;
extern bool WASM3_FP_TIMING;
extern bool SECOND_ORDER_ERR;

extern int COND_THRESH; // Threshold for condition number check (2^x)
extern int LOG_LIMIT; // Nonzero value to limit the number of logged operations

#endif /* CONFIG_ENV_H */