//
//  m3_exec.h
//
//  Created by Steven Massey on 4/17/19.
//  Copyright © 2019 Steven Massey. All rights reserved.


#ifndef m3_exec_h
#define m3_exec_h

// TODO: all these functions could move over to the .c at some point. normally, I'd say screw it,
// but it might prove useful to be able to compile m3_exec alone w/ optimizations while the remaining
// code is at debug O0


// About the naming convention of these operations/macros (_rs, _sr_, _ss, _srs, etc.)
//------------------------------------------------------------------------------------------------------
//   - 'r' means register and 's' means slot
//   - the first letter is the top of the stack
//
//  so, for example, _rs means the first operand (the first thing pushed to the stack) is in a slot
//  and the second operand (the top of the stack) is in a register
//------------------------------------------------------------------------------------------------------

#ifndef M3_COMPILE_OPCODES
#  error "Opcodes should only be included in one compilation unit"
#endif

#include "m3_math_utils.h"
#include "m3_compile.h"
#include "m3_env.h"
#include "m3_info.h"
#include "m3_exec_defs.h"

#include <limits.h>

#include "di_library.h"
#include "mpfr_library.h"
#include "qd_library.h"
#include "eid_library.h"
#include "mpfr_dd_library.h"

d_m3BeginExternC

# define rewrite_op(OP)             * ((void **) (_pc-1)) = (void*)(OP)

# define immediate(TYPE)            * ((TYPE *) _pc++)
# define skip_immediate(TYPE)       (_pc++)

#if defined(USE_EID)
# define M3_SHADOW_STACK_SLOT_SCALE EID_SHADOW_STACK_SLOT_SCALE
# define M3_SHADOW_MEMORY_SCALE     EID_SHADOW_MEMORY_SCALE
static inline eid_t d_m3CurrentEidRegister(IM3Runtime runtime, m3stack_t ssp)
{
    if (!runtime || !runtime->eid_registers || !runtime->s_stack) {
        return &g_default_eid_register;
    }

    size_t shadow_slot_offset = (size_t) (ssp - (m3stack_t) runtime->s_stack);
    size_t register_index = shadow_slot_offset / M3_SHADOW_STACK_SLOT_SCALE;
    d_m3Assert (register_index < (runtime->numStackSlots + 4));

    return &((eid *) runtime->eid_registers)[register_index];
}
# undef DEFAULT_EID_REGISTER
# define DEFAULT_EID_REGISTER       d_m3CurrentEidRegister(((_mem) ? (_mem)->runtime : NULL), _ssp)
# define slot_eid()                 ((eid_t) (_ssp + ((* ((i32 *) (_pc-1))) * M3_SHADOW_STACK_SLOT_SCALE)))
#else
# define M3_SHADOW_STACK_SLOT_SCALE 2
# define M3_SHADOW_MEMORY_SCALE     2
#endif

# define slot(TYPE)                 * (TYPE *) (_sp + immediate (i32))
# define slot_new(TYPE)             * (TYPE *) (_ssp + ((* ((i32 *) (_pc-1))) * M3_SHADOW_STACK_SLOT_SCALE ))
# define slot_ptr(TYPE)             (TYPE *) (_sp + immediate (i32))
# define slot_ptr_new(TYPE)         (TYPE *) (_ssp + ((* ((i32 *) (_pc-1))) * M3_SHADOW_STACK_SLOT_SCALE ))


# if d_m3EnableOpProfiling
                                    d_m3RetSig  profileOp   (d_m3OpSig, cstr_t i_operationName);
#   define nextOp()                 M3_MUSTTAIL return profileOp (d_m3OpAllArgs, __FUNCTION__)
# elif d_m3EnableOpTracing
                                    d_m3RetSig  debugOp     (d_m3OpSig, cstr_t i_operationName);
#   define nextOp()                 M3_MUSTTAIL return debugOp (d_m3OpAllArgs, __FUNCTION__)
# else
#   define nextOp()                 nextOpDirect()
# endif

#define jumpOp(PC)                  jumpOpDirect(PC)

#if d_m3RecordBacktraces
    #define pushBacktraceFrame()            (PushBacktraceFrame (_mem->runtime, _pc - 1))
    #define fillBacktraceFrame(FUNCTION)    (FillBacktraceFunctionInfo (_mem->runtime, function))

    #define newTrap(err)                    return (pushBacktraceFrame (), err)
    #define forwardTrap(err)                return err
#else
    #define pushBacktraceFrame()            do {} while (0)
    #define fillBacktraceFrame(FUNCTION)    do {} while (0)

    #define newTrap(err)                    return err
    #define forwardTrap(err)                return err
#endif


#if d_m3EnableStrace == 1
    // Flat trace
    #define d_m3TracePrepare
    #define d_m3TracePrint(fmt, ...)            fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#elif d_m3EnableStrace >= 2
    // Structured trace
    #define d_m3TracePrepare                    const IM3Runtime trace_rt = m3MemRuntime(_mem);
    #define d_m3TracePrint(fmt, ...)            fprintf(stderr, "%*s" fmt "\n", (trace_rt->callDepth)*2, "", ##__VA_ARGS__)
#else
    #define d_m3TracePrepare
    #define d_m3TracePrint(fmt, ...)
#endif

#if d_m3EnableStrace >= 3
    #define d_m3TraceLoad(TYPE,offset,val)      d_m3TracePrint("load." #TYPE "  0x%x = %" PRI##TYPE, offset, val)
    #define d_m3TraceStore(TYPE,offset,val)     d_m3TracePrint("store." #TYPE " 0x%x , %" PRI##TYPE, offset, val)
#else
    #define d_m3TraceLoad(TYPE,offset,val)
    #define d_m3TraceStore(TYPE,offset,val)
#endif

#ifdef DEBUG
  #define d_outOfBounds newTrap (ErrorRuntime (m3Err_trapOutOfBoundsMemoryAccess,   \
                        _mem->runtime, "memory size: %zu; access offset: %zu",      \
                        _mem->length, operand))

#   define d_outOfBoundsMemOp(OFFSET, SIZE) newTrap (ErrorRuntime (m3Err_trapOutOfBoundsMemoryAccess,   \
                      _mem->runtime, "memory size: %zu; access offset: %zu; size: %u",     \
                      _mem->length, OFFSET, SIZE))
#else
  #define d_outOfBounds newTrap (m3Err_trapOutOfBoundsMemoryAccess)

#   define d_outOfBoundsMemOp(OFFSET, SIZE) newTrap (m3Err_trapOutOfBoundsMemoryAccess)

#endif

# if (d_m3EnableOpProfiling || d_m3EnableOpTracing)
d_m3RetSig  Call  (d_m3OpSig, cstr_t i_operationName)
# else
d_m3RetSig  Call  (d_m3OpSig)
# endif
{
    m3ret_t possible_trap = m3_Yield ();
    if (M3_UNLIKELY(possible_trap)) return possible_trap;

    nextOpDirect();
}

// TODO: OK, this needs some explanation here ;0

#define d_m3CommutativeOpMacro(RES, REG, TYPE, NAME, OP, ...) \
d_m3Op(TYPE##_##NAME##_rs)                              \
{                                                       \
    TYPE operand = slot (TYPE);                         \
    OP((RES), operand, ((TYPE) REG), ##__VA_ARGS__);    \
    nextOp ();                                          \
}                                                       \
d_m3Op(TYPE##_##NAME##_ss)                              \
{                                                       \
    TYPE operand2 = slot (TYPE);                        \
    TYPE operand1 = slot (TYPE);                        \
    OP((RES), operand1, operand2, ##__VA_ARGS__);       \
    nextOp ();                                          \
}

// d_m3CommutativeOpMacro variants start
#if defined(USE_MPFR) || defined(USE_MPFR_DD)

#define d_m3CommutativeOpMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ...) \
d_m3Op(TYPE##_##NAME##_rs)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    mpfr_ptr operand_e = slot_new (mpfr_ptr);           \
    OP((&(RES_V)), (&(RES_E)), operand_v, operand_e,    \
    ((TYPE) REG_V), ((mpfr_ptr) REG_E), ##__VA_ARGS__); \
    nextOp ();                                          \
}                                                       \
d_m3Op(TYPE##_##NAME##_ss)                              \
{                                                       \
    TYPE operand2_v = slot (TYPE);                      \
    mpfr_ptr operand2_e = slot_new (mpfr_ptr);          \
    TYPE operand1_v = slot (TYPE);                      \
    mpfr_ptr operand1_e = slot_new (mpfr_ptr);          \
    OP((&(RES_V)), (&(RES_E)), operand1_v, operand1_e,  \
        operand2_v, operand2_e, ##__VA_ARGS__);         \
    nextOp ();                                          \
}

#define d_m3CommutativeOpMacro_comp(RES, REG_V, REG_E, TYPE, NAME, OP, ...)        \
d_m3Op(TYPE##_##NAME##_rs)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    mpfr_ptr operand_e = slot_new (mpfr_ptr);           \
    OP((RES), operand_v, operand_e,                     \
    ((TYPE) REG_V), ((mpfr_ptr) REG_E), ##__VA_ARGS__); \
    nextOp ();                                          \
}                                                       \
d_m3Op(TYPE##_##NAME##_ss)                              \
{                                                       \
    TYPE operand2_v = slot (TYPE);                      \
    mpfr_ptr operand2_e = slot_new (mpfr_ptr);          \
    TYPE operand1_v = slot (TYPE);                      \
    mpfr_ptr operand1_e = slot_new (mpfr_ptr);          \
    OP((RES), operand1_v, operand1_e,                   \
       operand2_v, operand2_e, ##__VA_ARGS__);          \
    nextOp ();                                          \
}

#elif defined(USE_QD)

#define d_m3CommutativeOpMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ...) \
d_m3Op(TYPE##_##NAME##_rs)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    f64* operand_e = slot_new (f64*);                   \
    OP((&(RES_V)), (&(RES_E)), operand_v, operand_e,    \
    ((TYPE) REG_V), ((f64*) REG_E), ##__VA_ARGS__);     \
    nextOp ();                                          \
}                                                       \
d_m3Op(TYPE##_##NAME##_ss)                              \
{                                                       \
    TYPE operand2_v = slot (TYPE);                      \
    f64* operand2_e = slot_new (f64*);                  \
    TYPE operand1_v = slot (TYPE);                      \
    f64* operand1_e = slot_new (f64*);                  \
    OP((&(RES_V)), (&(RES_E)), operand1_v, operand1_e,  \
        operand2_v, operand2_e, ##__VA_ARGS__);         \
    nextOp ();                                          \
}

#define d_m3CommutativeOpMacro_comp(RES, REG_V, REG_E, TYPE, NAME, OP, ...)        \
d_m3Op(TYPE##_##NAME##_rs)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    f64* operand_e = slot_new (f64*);                   \
    OP((RES), operand_v, operand_e,                     \
    ((TYPE) REG_V), ((f64*) REG_E), ##__VA_ARGS__);     \
    nextOp ();                                          \
}                                                       \
d_m3Op(TYPE##_##NAME##_ss)                              \
{                                                       \
    TYPE operand2_v = slot (TYPE);                      \
    f64* operand2_e = slot_new (f64*);                  \
    TYPE operand1_v = slot (TYPE);                      \
    f64* operand1_e = slot_new (f64*);                  \
    OP((RES), operand1_v, operand1_e,                   \
       operand2_v, operand2_e, ##__VA_ARGS__);          \
    nextOp ();                                          \
}

#elif defined(USE_EID)

#define d_m3CommutativeOpMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ...) \
d_m3Op(TYPE##_##NAME##_rs)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    eid_t operand_e = slot_eid();                       \
    eid reg_e = *(REG_E);                               \
    RES_E = DEFAULT_EID_REGISTER;                       \
    OP((&(RES_V)), &(RES_E), operand_v, operand_e,      \
    ((TYPE) REG_V), &reg_e, ##__VA_ARGS__);             \
    nextOp ();                                          \
}                                                       \
d_m3Op(TYPE##_##NAME##_ss)                              \
{                                                       \
    RES_E = DEFAULT_EID_REGISTER;                       \
    TYPE operand2_v = slot (TYPE);                      \
    eid_t operand2_e = slot_eid();                      \
    TYPE operand1_v = slot (TYPE);                      \
    eid_t operand1_e = slot_eid();                      \
    OP((&(RES_V)), &(RES_E), operand1_v, operand1_e,    \
        operand2_v, operand2_e, ##__VA_ARGS__);         \
    nextOp ();                                          \
}

#define d_m3CommutativeOpMacro_comp(RES, REG_V, REG_E, TYPE, NAME, OP, ...)        \
d_m3Op(TYPE##_##NAME##_rs)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    eid_t operand_e = slot_eid();                       \
    eid reg_e = *(REG_E);                               \
    OP((RES), operand_v, operand_e,                     \
    ((TYPE) REG_V), &reg_e, ##__VA_ARGS__);             \
    nextOp ();                                          \
}                                                       \
d_m3Op(TYPE##_##NAME##_ss)                              \
{                                                       \
    TYPE operand2_v = slot (TYPE);                      \
    eid_t operand2_e = slot_eid();                      \
    TYPE operand1_v = slot (TYPE);                      \
    eid_t operand1_e = slot_eid();                      \
    OP((RES), operand1_v, operand1_e,                   \
       operand2_v, operand2_e, ##__VA_ARGS__);          \
    nextOp ();                                          \
}

#elif defined(USE_DD) || defined(USE_EFTSAN)

#define d_m3CommutativeOpMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ...) \
d_m3Op(TYPE##_##NAME##_rs)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    f64 operand_e = slot_new (f64);                     \
    OP((&(RES_V)), (&(RES_E)), operand_v, operand_e,    \
       ((TYPE) REG_V), ((f64) REG_E), ##__VA_ARGS__);   \
    nextOp ();                                          \
}                                                       \
d_m3Op(TYPE##_##NAME##_ss)                              \
{                                                       \
    TYPE operand2_v = slot (TYPE);                      \
    f64 operand2_e = slot_new (f64);                    \
    TYPE operand1_v = slot (TYPE);                      \
    f64 operand1_e = slot_new (f64);                    \
    OP((&(RES_V)), (&(RES_E)), operand1_v, operand1_e,  \
        operand2_v, operand2_e, ##__VA_ARGS__);         \
    nextOp ();                                          \
}

#define d_m3CommutativeOpMacro_comp(RES, REG_V, REG_E, TYPE, NAME, OP, ...)        \
d_m3Op(TYPE##_##NAME##_rs)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    f64 operand_e = slot_new (f64);                     \
    OP((RES), operand_v, operand_e,                     \
       ((TYPE) REG_V), ((f64) REG_E), ##__VA_ARGS__);   \
    nextOp ();                                          \
}                                                       \
d_m3Op(TYPE##_##NAME##_ss)                              \
{                                                       \
    TYPE operand2_v = slot (TYPE);                      \
    f64 operand2_e = slot_new (f64);                    \
    TYPE operand1_v = slot (TYPE);                      \
    f64 operand1_e = slot_new (f64);                    \
    OP((RES), operand1_v, operand1_e,                   \
       operand2_v, operand2_e, ##__VA_ARGS__);          \
    nextOp ();                                          \
}

#else
#endif
// d_m3CommutativeOpMacro variants end

#define d_m3OpMacro(RES, REG, TYPE, NAME, OP, ...)      \
d_m3Op(TYPE##_##NAME##_sr)                              \
{                                                       \
    TYPE operand = slot (TYPE);                         \
    OP((RES), ((TYPE) REG), operand, ##__VA_ARGS__);    \
    nextOp ();                                          \
}                                                       \
d_m3CommutativeOpMacro(RES, REG, TYPE, NAME, OP, ##__VA_ARGS__)

// d_m3OpMacro variants start
#if defined(USE_MPFR) || defined(USE_MPFR_DD)

#define d_m3OpMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ...)      \
d_m3Op(TYPE##_##NAME##_sr)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    mpfr_ptr operand_e = slot_new (mpfr_ptr);           \
    OP((&(RES_V)), (&(RES_E)),                          \
       ((TYPE) REG_V), ((mpfr_ptr) REG_E),              \
       operand_v, operand_e, ##__VA_ARGS__);            \
    nextOp ();                                          \
}                                                       \
d_m3CommutativeOpMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ##__VA_ARGS__)

#define d_m3OpMacro_comp(RES, REG_V, REG_E, TYPE, NAME, OP, ...)              \
d_m3Op(TYPE##_##NAME##_sr)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    mpfr_ptr operand_e = slot_new (mpfr_ptr);           \
    OP((RES), ((TYPE) REG_V), ((mpfr_ptr) REG_E),       \
       operand_v, operand_e, ##__VA_ARGS__);            \
    nextOp ();                                          \
}                                                       \
d_m3CommutativeOpMacro_comp(RES, REG_V, REG_E, TYPE, NAME, OP, ##__VA_ARGS__)

#elif defined(USE_QD)

#define d_m3OpMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ...)      \
d_m3Op(TYPE##_##NAME##_sr)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    f64* operand_e = slot_new (f64*);                   \
    OP((&(RES_V)), (&(RES_E)),                          \
       ((TYPE) REG_V), ((f64*) REG_E),                  \
       operand_v, operand_e, ##__VA_ARGS__);            \
    nextOp ();                                          \
}                                                       \
d_m3CommutativeOpMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ##__VA_ARGS__)

#define d_m3OpMacro_comp(RES, REG_V, REG_E, TYPE, NAME, OP, ...)              \
d_m3Op(TYPE##_##NAME##_sr)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    f64* operand_e = slot_new (f64*);                   \
    OP((RES), ((TYPE) REG_V), ((f64*) REG_E),           \
       operand_v, operand_e, ##__VA_ARGS__);            \
    nextOp ();                                          \
}                                                       \
d_m3CommutativeOpMacro_comp(RES, REG_V, REG_E, TYPE, NAME, OP, ##__VA_ARGS__)

#elif defined(USE_EID)

#define d_m3OpMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ...)      \
d_m3Op(TYPE##_##NAME##_sr)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    eid_t operand_e = slot_eid();                       \
    eid reg_e = *(REG_E);                               \
    RES_E = DEFAULT_EID_REGISTER;                       \
    OP((&(RES_V)), &(RES_E),                            \
       ((TYPE) REG_V), &reg_e,                          \
       operand_v, operand_e, ##__VA_ARGS__);            \
    nextOp ();                                          \
}                                                       \
d_m3CommutativeOpMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ##__VA_ARGS__)

#define d_m3OpMacro_comp(RES, REG_V, REG_E, TYPE, NAME, OP, ...)              \
d_m3Op(TYPE##_##NAME##_sr)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    eid_t operand_e = slot_eid();                       \
    eid reg_e = *(REG_E);                               \
    OP((RES), ((TYPE) REG_V), &reg_e,                   \
       operand_v, operand_e, ##__VA_ARGS__);            \
    nextOp ();                                          \
}                                                       \
d_m3CommutativeOpMacro_comp(RES, REG_V, REG_E, TYPE, NAME, OP, ##__VA_ARGS__)

#elif defined(USE_DD) || defined(USE_EFTSAN)

#define d_m3OpMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ...)      \
d_m3Op(TYPE##_##NAME##_sr)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    f64 operand_e = slot_new (f64);                     \
    OP((&(RES_V)), (&(RES_E)),                          \
       ((TYPE) REG_V), ((f64) REG_E),                   \
       operand_v, operand_e, ##__VA_ARGS__);            \
    nextOp ();                                          \
}                                                       \
d_m3CommutativeOpMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ##__VA_ARGS__)

#define d_m3OpMacro_comp(RES, REG_V, REG_E, TYPE, NAME, OP, ...)              \
d_m3Op(TYPE##_##NAME##_sr)                              \
{                                                       \
    TYPE operand_v = slot (TYPE);                       \
    f64 operand_e = slot_new (f64);                     \
    OP((RES), ((TYPE) REG_V), ((f64) REG_E),            \
       operand_v, operand_e, ##__VA_ARGS__);            \
    nextOp ();                                          \
}                                                       \
d_m3CommutativeOpMacro_comp(RES, REG_V, REG_E, TYPE, NAME, OP, ##__VA_ARGS__)

#else
#endif
// d_m3OpMacro variants end
// Accept macros
#define d_m3CommutativeOpMacro_i(TYPE, NAME, MACRO, ...)    d_m3CommutativeOpMacro  ( _r0,  _r0, TYPE, NAME, MACRO, ##__VA_ARGS__)
#define d_m3OpMacro_i(TYPE, NAME, MACRO, ...)               d_m3OpMacro             ( _r0,  _r0, TYPE, NAME, MACRO, ##__VA_ARGS__)

#if d_m3HasErrorSlot
#define d_m3CommutativeOpMacro_f(TYPE, NAME, MACRO, ...)    d_m3CommutativeOpMacro_new  (_fp0, _fp1, _fp0, _fp1, TYPE, NAME, MACRO, ##__VA_ARGS__)
#define d_m3OpMacro_f(TYPE, NAME, MACRO, ...)               d_m3OpMacro_new             (_fp0, _fp1, _fp0, _fp1, TYPE, NAME, MACRO, ##__VA_ARGS__)
#else
#define d_m3CommutativeOpMacro_f(TYPE, NAME, MACRO, ...)    d_m3CommutativeOpMacro  (_fp0, _fp0, TYPE, NAME, MACRO, ##__VA_ARGS__)
#define d_m3OpMacro_f(TYPE, NAME, MACRO, ...)               d_m3OpMacro             (_fp0, _fp0, TYPE, NAME, MACRO, ##__VA_ARGS__)
#endif

#define M3_FUNC(RES, A, B, OP)  (RES) = OP((A), (B))        // Accept functions: res = OP(a,b)
#define M3_OPER(RES, A, B, OP)  (RES) = ((A) OP (B))        // Accept operators: res = a OP b
#define M3_FUNC_TIMED(RES, A, B, OP)  do { \
    unsigned long long __m3_fp_cycle_start = read_fp_cycle_counter(); \
    (RES) = OP((A), (B)); \
    accumulate_fp_cycles(__m3_fp_cycle_start); \
} while (0)
#define M3_OPER_TIMED(RES, A, B, OP)  do { \
    unsigned long long __m3_fp_cycle_start = read_fp_cycle_counter(); \
    (RES) = ((A) OP (B)); \
    accumulate_fp_cycles(__m3_fp_cycle_start); \
} while (0)
#define M3_FUNC_FP(RES_V, RES_E, A_v, A_e, B_v, B_e, OP)  do { \
    unsigned long long __m3_fp_cycle_start = read_fp_cycle_counter(); \
    OP((A_v), (A_e), (B_v), (B_e), RES_V, RES_E); \
    accumulate_fp_cycles(__m3_fp_cycle_start); \
} while (0)
#define M3_COMP_FP(RES, A_v, A_e, B_v, B_e, OP)  do { \
    unsigned long long __m3_fp_cycle_start = read_fp_cycle_counter(); \
    (RES) = OP((A_v), (A_e), (B_v), (B_e)); \
    accumulate_fp_cycles(__m3_fp_cycle_start); \
} while (0)

#define d_m3CommutativeOpFunc_i(TYPE, NAME, OP)     d_m3CommutativeOpMacro_i    (TYPE, NAME, M3_FUNC, OP)
#define d_m3OpFunc_i(TYPE, NAME, OP)                d_m3OpMacro_i               (TYPE, NAME, M3_FUNC, OP)

#if d_m3HasErrorSlot
#define d_m3CommutativeOpFunc_f(TYPE, NAME, OP)     d_m3CommutativeOpMacro_f    (TYPE, NAME, M3_FUNC_FP, OP)
#define d_m3OpFunc_f(TYPE, NAME, OP)                d_m3OpMacro_f               (TYPE, NAME, M3_FUNC_FP, OP)
#else
#define d_m3CommutativeOpFunc_f(TYPE, NAME, OP)     d_m3CommutativeOpMacro_f    (TYPE, NAME, M3_FUNC_TIMED, OP)
#define d_m3OpFunc_f(TYPE, NAME, OP)                d_m3OpMacro_f               (TYPE, NAME, M3_FUNC_TIMED, OP)
#endif

#define d_m3CommutativeOp_i(TYPE, NAME, OP)         d_m3CommutativeOpMacro_i    (TYPE, NAME, M3_OPER, OP)
#define d_m3Op_i(TYPE, NAME, OP)                    d_m3OpMacro_i               (TYPE, NAME, M3_OPER, OP)

#if d_m3HasErrorSlot
#define d_m3CommutativeOp_f(TYPE, NAME, OP)         d_m3CommutativeOpMacro_f    (TYPE, NAME, M3_FUNC_FP, OP)
#define d_m3Op_f(TYPE, NAME, OP)                    d_m3OpMacro_f               (TYPE, NAME, M3_FUNC_FP, OP)
#else
#define d_m3CommutativeOp_f(TYPE, NAME, OP)         d_m3CommutativeOpMacro_f    (TYPE, NAME, M3_OPER_TIMED, OP)
#define d_m3Op_f(TYPE, NAME, OP)                    d_m3OpMacro_f               (TYPE, NAME, M3_OPER_TIMED, OP)
#endif

// compare needs to be distinct for fp 'cause the result must be _r0
#if d_m3HasErrorSlot
#define d_m3CompareOp_f(TYPE, NAME, OP)             d_m3OpMacro_comp                 (_r0, _fp0, _fp1, TYPE, NAME, M3_COMP_FP, OP)
#define d_m3CommutativeCmpOp_f(TYPE, NAME, OP)      d_m3CommutativeOpMacro_comp      (_r0, _fp0, _fp1, TYPE, NAME, M3_COMP_FP, OP)
#else
#define d_m3CompareOp_f(TYPE, NAME, OP)             d_m3OpMacro                 (_r0, _fp0, TYPE, NAME, M3_OPER_TIMED, OP)
#define d_m3CommutativeCmpOp_f(TYPE, NAME, OP)      d_m3CommutativeOpMacro      (_r0, _fp0, TYPE, NAME, M3_OPER_TIMED, OP)
#endif

//-----------------------

// signed
d_m3CommutativeOp_i (i32, Equal,            ==)     d_m3CommutativeOp_i (i64, Equal,            ==)
d_m3CommutativeOp_i (i32, NotEqual,         !=)     d_m3CommutativeOp_i (i64, NotEqual,         !=)

d_m3Op_i (i32, LessThan,                    < )     d_m3Op_i (i64, LessThan,                    < )
d_m3Op_i (i32, GreaterThan,                 > )     d_m3Op_i (i64, GreaterThan,                 > )
d_m3Op_i (i32, LessThanOrEqual,             <=)     d_m3Op_i (i64, LessThanOrEqual,             <=)
d_m3Op_i (i32, GreaterThanOrEqual,          >=)     d_m3Op_i (i64, GreaterThanOrEqual,          >=)

// unsigned
d_m3Op_i (u32, LessThan,                    < )     d_m3Op_i (u64, LessThan,                    < )
d_m3Op_i (u32, GreaterThan,                 > )     d_m3Op_i (u64, GreaterThan,                 > )
d_m3Op_i (u32, LessThanOrEqual,             <=)     d_m3Op_i (u64, LessThanOrEqual,             <=)
d_m3Op_i (u32, GreaterThanOrEqual,          >=)     d_m3Op_i (u64, GreaterThanOrEqual,          >=)

#if d_m3HasFloat

#ifdef USE_MPFR
d_m3CommutativeCmpOp_f (f32, Equal,         eq_fmp)     d_m3CommutativeCmpOp_f (f64, Equal,         eq_dmp)
d_m3CommutativeCmpOp_f (f32, NotEqual,      neq_fmp)     d_m3CommutativeCmpOp_f (f64, NotEqual,      neq_dmp)
d_m3CompareOp_f (f32, LessThan,             less_fmp)     d_m3CompareOp_f (f64, LessThan,             less_dmp)
d_m3CompareOp_f (f32, GreaterThan,          greater_fmp)     d_m3CompareOp_f (f64, GreaterThan,          greater_dmp)
d_m3CompareOp_f (f32, LessThanOrEqual,      less_eq_fmp)     d_m3CompareOp_f (f64, LessThanOrEqual,      less_eq_dmp)
d_m3CompareOp_f (f32, GreaterThanOrEqual,   greater_eq_fmp)     d_m3CompareOp_f (f64, GreaterThanOrEqual,   greater_eq_dmp)
#elif defined(USE_MPFR_DD)
d_m3CommutativeCmpOp_f (f32, Equal,         eq_fmd)     d_m3CommutativeCmpOp_f (f64, Equal,         eq_dmd)
d_m3CommutativeCmpOp_f (f32, NotEqual,      neq_fmd)     d_m3CommutativeCmpOp_f (f64, NotEqual,      neq_dmd)
d_m3CompareOp_f (f32, LessThan,             less_fmd)     d_m3CompareOp_f (f64, LessThan,             less_dmd)
d_m3CompareOp_f (f32, GreaterThan,          greater_fmd)     d_m3CompareOp_f (f64, GreaterThan,          greater_dmd)
d_m3CompareOp_f (f32, LessThanOrEqual,      less_eq_fmd)     d_m3CompareOp_f (f64, LessThanOrEqual,      less_eq_dmd)
d_m3CompareOp_f (f32, GreaterThanOrEqual,   greater_eq_fmd)     d_m3CompareOp_f (f64, GreaterThanOrEqual,   greater_eq_dmd)
#elif defined(USE_QD)
d_m3CommutativeCmpOp_f (f32, Equal,         eq_fqd)     d_m3CommutativeCmpOp_f (f64, Equal,         eq_dqd)
d_m3CommutativeCmpOp_f (f32, NotEqual,      neq_fqd)     d_m3CommutativeCmpOp_f (f64, NotEqual,      neq_dqd)
d_m3CompareOp_f (f32, LessThan,             less_fqd)     d_m3CompareOp_f (f64, LessThan,             less_dqd)
d_m3CompareOp_f (f32, GreaterThan,          greater_fqd)     d_m3CompareOp_f (f64, GreaterThan,          greater_dqd)
d_m3CompareOp_f (f32, LessThanOrEqual,      less_eq_fqd)     d_m3CompareOp_f (f64, LessThanOrEqual,      less_eq_dqd)
d_m3CompareOp_f (f32, GreaterThanOrEqual,   greater_eq_fqd)     d_m3CompareOp_f (f64, GreaterThanOrEqual,   greater_eq_dqd)
#elif defined(USE_EID)
d_m3CommutativeCmpOp_f (f32, Equal,         eq_fei)     d_m3CommutativeCmpOp_f (f64, Equal,         eq_dei)
d_m3CommutativeCmpOp_f (f32, NotEqual,      neq_fei)     d_m3CommutativeCmpOp_f (f64, NotEqual,      neq_dei)
d_m3CompareOp_f (f32, LessThan,             less_fei)     d_m3CompareOp_f (f64, LessThan,             less_dei)
d_m3CompareOp_f (f32, GreaterThan,          greater_fei)     d_m3CompareOp_f (f64, GreaterThan,          greater_dei)
d_m3CompareOp_f (f32, LessThanOrEqual,      less_eq_fei)     d_m3CompareOp_f (f64, LessThanOrEqual,      less_eq_dei)
d_m3CompareOp_f (f32, GreaterThanOrEqual,   greater_eq_fei)     d_m3CompareOp_f (f64, GreaterThanOrEqual,   greater_eq_dei)
#elif defined(USE_DD) || defined(USE_EFTSAN)
d_m3CommutativeCmpOp_f (f32, Equal,         eq_fi)     d_m3CommutativeCmpOp_f (f64, Equal,         eq_di)
d_m3CommutativeCmpOp_f (f32, NotEqual,      neq_fi)     d_m3CommutativeCmpOp_f (f64, NotEqual,      neq_di)
d_m3CompareOp_f (f32, LessThan,             less_fi)     d_m3CompareOp_f (f64, LessThan,             less_di)
d_m3CompareOp_f (f32, GreaterThan,          greater_fi)     d_m3CompareOp_f (f64, GreaterThan,          greater_di)
d_m3CompareOp_f (f32, LessThanOrEqual,      less_eq_fi)     d_m3CompareOp_f (f64, LessThanOrEqual,      less_eq_di)
d_m3CompareOp_f (f32, GreaterThanOrEqual,   greater_eq_fi)     d_m3CompareOp_f (f64, GreaterThanOrEqual,   greater_eq_di)
#else
d_m3CommutativeCmpOp_f (f32, Equal,         ==)     d_m3CommutativeCmpOp_f (f64, Equal,         ==)
d_m3CommutativeCmpOp_f (f32, NotEqual,      !=)     d_m3CommutativeCmpOp_f (f64, NotEqual,      !=)
d_m3CompareOp_f (f32, LessThan,             < )     d_m3CompareOp_f (f64, LessThan,             < )
d_m3CompareOp_f (f32, GreaterThan,          > )     d_m3CompareOp_f (f64, GreaterThan,          > )
d_m3CompareOp_f (f32, LessThanOrEqual,      <=)     d_m3CompareOp_f (f64, LessThanOrEqual,      <=)
d_m3CompareOp_f (f32, GreaterThanOrEqual,   >=)     d_m3CompareOp_f (f64, GreaterThanOrEqual,   >=)
#endif

#endif

#define OP_ADD_32(A,B) (i32)((u32)(A) + (u32)(B))
#define OP_ADD_64(A,B) (i64)((u64)(A) + (u64)(B))
#define OP_SUB_32(A,B) (i32)((u32)(A) - (u32)(B))
#define OP_SUB_64(A,B) (i64)((u64)(A) - (u64)(B))
#define OP_MUL_32(A,B) (i32)((u32)(A) * (u32)(B))
#define OP_MUL_64(A,B) (i64)((u64)(A) * (u64)(B))

d_m3CommutativeOpFunc_i (i32, Add,      OP_ADD_32)  d_m3CommutativeOpFunc_i (i64, Add,      OP_ADD_64)
d_m3CommutativeOpFunc_i (i32, Multiply, OP_MUL_32)  d_m3CommutativeOpFunc_i (i64, Multiply, OP_MUL_64)

d_m3OpFunc_i (i32, Subtract,            OP_SUB_32)  d_m3OpFunc_i (i64, Subtract,            OP_SUB_64)

#define OP_SHL_32(X,N) ((X) << ((u32)(N) % 32))
#define OP_SHL_64(X,N) ((X) << ((u64)(N) % 64))
#define OP_SHR_32(X,N) ((X) >> ((u32)(N) % 32))
#define OP_SHR_64(X,N) ((X) >> ((u64)(N) % 64))

d_m3OpFunc_i (u32, ShiftLeft,       OP_SHL_32)      d_m3OpFunc_i (u64, ShiftLeft,       OP_SHL_64)
d_m3OpFunc_i (i32, ShiftRight,      OP_SHR_32)      d_m3OpFunc_i (i64, ShiftRight,      OP_SHR_64)
d_m3OpFunc_i (u32, ShiftRight,      OP_SHR_32)      d_m3OpFunc_i (u64, ShiftRight,      OP_SHR_64)

d_m3CommutativeOp_i (u32, And,              &)
d_m3CommutativeOp_i (u32, Or,               |)
d_m3CommutativeOp_i (u32, Xor,              ^)

d_m3CommutativeOp_i (u64, And,              &)
d_m3CommutativeOp_i (u64, Or,               |)
d_m3CommutativeOp_i (u64, Xor,              ^)

#if d_m3HasFloat

#ifdef USE_MPFR
d_m3CommutativeOp_f (f32, Add,              add_fmp)      d_m3CommutativeOp_f (f64, Add,              add_dmp)
d_m3CommutativeOp_f (f32, Multiply,         mul_fmp)      d_m3CommutativeOp_f (f64, Multiply,         mul_dmp)
d_m3Op_f (f32, Subtract,         sub_fmp_check_true)      d_m3Op_f (f64, Subtract,         sub_dmp_check_true)
d_m3Op_f (f32, Divide,                      div_fmp)      d_m3Op_f (f64, Divide,                      div_dmp)
#elif defined(USE_MPFR_DD)
d_m3CommutativeOp_f (f32, Add,              add_fmd)      d_m3CommutativeOp_f (f64, Add,              add_dmd)
d_m3CommutativeOp_f (f32, Multiply,         mul_fmd)      d_m3CommutativeOp_f (f64, Multiply,         mul_dmd)
d_m3Op_f (f32, Subtract,         sub_fmd_check_true)      d_m3Op_f (f64, Subtract,         sub_dmd_check_true)
d_m3Op_f (f32, Divide,                      div_fmd)      d_m3Op_f (f64, Divide,                      div_dmd)
#elif defined(USE_QD)
d_m3CommutativeOp_f (f32, Add,              add_fqd)      d_m3CommutativeOp_f (f64, Add,              add_dqd)
d_m3CommutativeOp_f (f32, Multiply,         mul_fqd)      d_m3CommutativeOp_f (f64, Multiply,         mul_dqd)
d_m3Op_f (f32, Subtract,         sub_fqd_check_true)      d_m3Op_f (f64, Subtract,         sub_dqd_check_true)
d_m3Op_f (f32, Divide,                      div_fqd)      d_m3Op_f (f64, Divide,                      div_dqd)
#elif defined(USE_EID)
d_m3CommutativeOp_f (f32, Add,              add_fei)      d_m3CommutativeOp_f (f64, Add,              add_dei)
d_m3CommutativeOp_f (f32, Multiply,         mul_fei)      d_m3CommutativeOp_f (f64, Multiply,         mul_dei)
d_m3Op_f (f32, Subtract,         sub_fei_check_true)      d_m3Op_f (f64, Subtract,         sub_dei_check_true)
d_m3Op_f (f32, Divide,                      div_fei)      d_m3Op_f (f64, Divide,                      div_dei)
#elif defined(USE_DD) || defined(USE_EFTSAN)
d_m3CommutativeOp_f (f32, Add,              add_fi)      d_m3CommutativeOp_f (f64, Add,              add_di)
d_m3CommutativeOp_f (f32, Multiply,         mul_fi)      d_m3CommutativeOp_f (f64, Multiply,         mul_di)
d_m3Op_f (f32, Subtract,         sub_fi_check_true)      d_m3Op_f (f64, Subtract,         sub_di_check_true)
d_m3Op_f (f32, Divide,                      div_fi)      d_m3Op_f (f64, Divide,                      div_di)
#else
d_m3CommutativeOp_f (f32, Add,              +)      d_m3CommutativeOp_f (f64, Add,              +)
d_m3CommutativeOp_f (f32, Multiply,         *)      d_m3CommutativeOp_f (f64, Multiply,         *)
d_m3Op_f (f32, Subtract,                    -)      d_m3Op_f (f64, Subtract,                    -)
d_m3Op_f (f32, Divide,                      /)      d_m3Op_f (f64, Divide,                      /)
#endif

#endif

d_m3OpFunc_i(u32, Rotl, rotl32)
d_m3OpFunc_i(u32, Rotr, rotr32)
d_m3OpFunc_i(u64, Rotl, rotl64)
d_m3OpFunc_i(u64, Rotr, rotr64)

d_m3OpMacro_i(u32, Divide, OP_DIV_U);
d_m3OpMacro_i(i32, Divide, OP_DIV_S, INT32_MIN);
d_m3OpMacro_i(u64, Divide, OP_DIV_U);
d_m3OpMacro_i(i64, Divide, OP_DIV_S, INT64_MIN);

d_m3OpMacro_i(u32, Remainder, OP_REM_U);
d_m3OpMacro_i(i32, Remainder, OP_REM_S, INT32_MIN);
d_m3OpMacro_i(u64, Remainder, OP_REM_U);
d_m3OpMacro_i(i64, Remainder, OP_REM_S, INT64_MIN);

#if d_m3HasFloat

#ifdef USE_MPFR
d_m3OpFunc_f(f32, Min, min_f32_fmp);
d_m3OpFunc_f(f32, Max, max_f32_fmp);
d_m3OpFunc_f(f64, Min, min_f64_dmp);
d_m3OpFunc_f(f64, Max, max_f64_dmp);
d_m3OpFunc_f(f32, CopySign, copysign_fmp);
d_m3OpFunc_f(f64, CopySign, copysign_dmp);
#elif defined(USE_MPFR_DD)
d_m3OpFunc_f(f32, Min, min_f32_fmd);
d_m3OpFunc_f(f32, Max, max_f32_fmd);
d_m3OpFunc_f(f64, Min, min_f64_dmd);
d_m3OpFunc_f(f64, Max, max_f64_dmd);
d_m3OpFunc_f(f32, CopySign, copysign_fmd);
d_m3OpFunc_f(f64, CopySign, copysign_dmd);
#elif defined(USE_QD)
d_m3OpFunc_f(f32, Min, min_f32_fqd);
d_m3OpFunc_f(f32, Max, max_f32_fqd);
d_m3OpFunc_f(f64, Min, min_f64_dqd);
d_m3OpFunc_f(f64, Max, max_f64_dqd);
d_m3OpFunc_f(f32, CopySign, copysign_fqd);
d_m3OpFunc_f(f64, CopySign, copysign_dqd);
#elif defined(USE_EID)
d_m3OpFunc_f(f32, Min, min_f32_fei);
d_m3OpFunc_f(f32, Max, max_f32_fei);
d_m3OpFunc_f(f64, Min, min_f64_dei);
d_m3OpFunc_f(f64, Max, max_f64_dei);
d_m3OpFunc_f(f32, CopySign, copysign_fei);
d_m3OpFunc_f(f64, CopySign, copysign_dei);
#elif defined(USE_DD) || defined(USE_EFTSAN)
d_m3OpFunc_f(f32, Min, min_f32_fi);
d_m3OpFunc_f(f32, Max, max_f32_fi);
d_m3OpFunc_f(f64, Min, min_f64_di);
d_m3OpFunc_f(f64, Max, max_f64_di);
d_m3OpFunc_f(f32, CopySign, copysign_fi);
d_m3OpFunc_f(f64, CopySign, copysign_di);
#else
d_m3OpFunc_f(f32, Min, min_f32);
d_m3OpFunc_f(f32, Max, max_f32);
d_m3OpFunc_f(f64, Min, min_f64);
d_m3OpFunc_f(f64, Max, max_f64);
d_m3OpFunc_f(f32, CopySign, copysignf);
d_m3OpFunc_f(f64, CopySign, copysign);
#endif

#endif

// Unary operations
// Note: This macro follows the principle of d_m3OpMacro

#define d_m3UnaryMacro(RES, REG, TYPE, NAME, OP, ...)   \
d_m3Op(TYPE##_##NAME##_r)                           \
{                                                   \
    OP((RES), (TYPE) REG, ##__VA_ARGS__);           \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_s)                           \
{                                                   \
    TYPE operand = slot (TYPE);                     \
    OP((RES), operand, ##__VA_ARGS__);              \
    nextOp ();                                      \
}

#if defined(USE_MPFR) || defined(USE_MPFR_DD)

#define d_m3UnaryMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ...)   \
d_m3Op(TYPE##_##NAME##_r)                           \
{                                                   \
    OP((&(RES_V)), (&(RES_E)),                      \
       (TYPE) REG_V, (mpfr_ptr) REG_E,              \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_s)                           \
{                                                   \
    TYPE operand_v = slot (TYPE);                   \
    mpfr_ptr operand_e = slot_new (mpfr_ptr);       \
    OP((&(RES_V)), (&(RES_E)),                      \
       operand_v, operand_e, ##__VA_ARGS__);        \
    nextOp ();                                      \
}

#elif defined(USE_QD)

#define d_m3UnaryMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ...)   \
d_m3Op(TYPE##_##NAME##_r)                           \
{                                                   \
    OP((&(RES_V)), (&(RES_E)),                      \
       (TYPE) REG_V, (f64*) REG_E,                  \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_s)                           \
{                                                   \
    TYPE operand_v = slot (TYPE);                   \
    f64* operand_e = slot_new (f64*);               \
    OP((&(RES_V)), (&(RES_E)),                      \
       operand_v, operand_e, ##__VA_ARGS__);        \
    nextOp ();                                      \
}

#elif defined(USE_EID)

#define d_m3UnaryMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ...)   \
d_m3Op(TYPE##_##NAME##_r)                           \
{                                                   \
    eid reg_e = *(REG_E);                           \
    RES_E = DEFAULT_EID_REGISTER;                   \
    OP((&(RES_V)), &(RES_E),                        \
       (TYPE) REG_V, &reg_e,                        \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_s)                           \
{                                                   \
    RES_E = DEFAULT_EID_REGISTER;                   \
    TYPE operand_v = slot (TYPE);                   \
    eid_t operand_e = slot_eid();                   \
    OP((&(RES_V)), &(RES_E),                        \
       operand_v, operand_e, ##__VA_ARGS__);        \
    nextOp ();                                      \
}

#elif defined(USE_DD) || defined(USE_EFTSAN)

#define d_m3UnaryMacro_new(RES_V, RES_E, REG_V, REG_E, TYPE, NAME, OP, ...)   \
d_m3Op(TYPE##_##NAME##_r)                           \
{                                                   \
    OP((&(RES_V)), (&(RES_E)),                      \
       (TYPE) REG_V, (f64) REG_E, ##__VA_ARGS__);   \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_s)                           \
{                                                   \
    TYPE operand_v = slot (TYPE);                   \
    f64 operand_e = slot_new (f64);                 \
    OP((&(RES_V)), (&(RES_E)),                      \
       operand_v, operand_e, ##__VA_ARGS__);        \
    nextOp ();                                      \
}

#else
#endif

#define M3_UNARY(RES, X, OP) (RES) = OP(X)
#define M3_UNARY_TIMED(RES, X, OP) do { \
    unsigned long long __m3_fp_cycle_start = read_fp_cycle_counter(); \
    (RES) = OP(X); \
    accumulate_fp_cycles(__m3_fp_cycle_start); \
} while (0)
#define M3_UNARY_FP(RES_V, RES_E, X_V, X_E, OP) do { \
    unsigned long long __m3_fp_cycle_start = read_fp_cycle_counter(); \
    OP((X_V), (X_E), RES_V, RES_E); \
    accumulate_fp_cycles(__m3_fp_cycle_start); \
} while (0)
#define d_m3UnaryOp_i(TYPE, NAME, OPERATION)        d_m3UnaryMacro( _r0,  _r0, TYPE, NAME, M3_UNARY, OPERATION)

#if d_m3HasErrorSlot
#define d_m3UnaryOp_f(TYPE, NAME, OPERATION)        d_m3UnaryMacro_new(_fp0, _fp1, _fp0, _fp1, TYPE, NAME, M3_UNARY_FP, OPERATION)
#else
#define d_m3UnaryOp_f(TYPE, NAME, OPERATION)        d_m3UnaryMacro(_fp0, _fp0, TYPE, NAME, M3_UNARY_TIMED, OPERATION)
#endif

#if d_m3HasFloat

#ifdef USE_MPFR
d_m3UnaryOp_f (f32, Abs,        fabs_fmp);         d_m3UnaryOp_f (f64, Abs,        fabs_dmp);
d_m3UnaryOp_f (f32, Ceil,       ceil_fmp);         d_m3UnaryOp_f (f64, Ceil,       ceil_dmp);
d_m3UnaryOp_f (f32, Floor,      floor_fmp);        d_m3UnaryOp_f (f64, Floor,      floor_dmp);
d_m3UnaryOp_f (f32, Trunc,      trunc_fmp);        d_m3UnaryOp_f (f64, Trunc,      trunc_dmp);
d_m3UnaryOp_f (f32, Sqrt,       sqrt_fmp);         d_m3UnaryOp_f (f64, Sqrt,       sqrt_dmp);
d_m3UnaryOp_f (f32, Nearest,    rint_fmp);         d_m3UnaryOp_f (f64, Nearest,    rint_dmp);
d_m3UnaryOp_f (f32, Negate,     neg_fmp);          d_m3UnaryOp_f (f64, Negate,     neg_dmp);
#elif defined(USE_MPFR_DD)
d_m3UnaryOp_f (f32, Abs,        fabs_fmd);         d_m3UnaryOp_f (f64, Abs,        fabs_dmd);
d_m3UnaryOp_f (f32, Ceil,       ceil_fmd);         d_m3UnaryOp_f (f64, Ceil,       ceil_dmd);
d_m3UnaryOp_f (f32, Floor,      floor_fmd);        d_m3UnaryOp_f (f64, Floor,      floor_dmd);
d_m3UnaryOp_f (f32, Trunc,      trunc_fmd);        d_m3UnaryOp_f (f64, Trunc,      trunc_dmd);
d_m3UnaryOp_f (f32, Sqrt,       sqrt_fmd);         d_m3UnaryOp_f (f64, Sqrt,       sqrt_dmd);
d_m3UnaryOp_f (f32, Nearest,    rint_fmd);         d_m3UnaryOp_f (f64, Nearest,    rint_dmd);
d_m3UnaryOp_f (f32, Negate,     neg_fmd);          d_m3UnaryOp_f (f64, Negate,     neg_dmd);
#elif defined(USE_QD)
d_m3UnaryOp_f (f32, Abs,        fabs_fqd);         d_m3UnaryOp_f (f64, Abs,        fabs_dqd);
d_m3UnaryOp_f (f32, Ceil,       ceil_fqd);         d_m3UnaryOp_f (f64, Ceil,       ceil_dqd);
d_m3UnaryOp_f (f32, Floor,      floor_fqd);        d_m3UnaryOp_f (f64, Floor,      floor_dqd);
d_m3UnaryOp_f (f32, Trunc,      trunc_fqd);        d_m3UnaryOp_f (f64, Trunc,      trunc_dqd);
d_m3UnaryOp_f (f32, Sqrt,       sqrt_fqd);         d_m3UnaryOp_f (f64, Sqrt,       sqrt_dqd);
d_m3UnaryOp_f (f32, Nearest,    rint_fqd);         d_m3UnaryOp_f (f64, Nearest,    rint_dqd);
d_m3UnaryOp_f (f32, Negate,     neg_fqd);          d_m3UnaryOp_f (f64, Negate,     neg_dqd);
#elif defined(USE_EID)
d_m3UnaryOp_f (f32, Abs,        fabsf_fei);         d_m3UnaryOp_f (f64, Abs,        fabs_dei);
d_m3UnaryOp_f (f32, Ceil,       ceilf_fei);         d_m3UnaryOp_f (f64, Ceil,       ceil_dei);
d_m3UnaryOp_f (f32, Floor,      floorf_fei);        d_m3UnaryOp_f (f64, Floor,      floor_dei);
d_m3UnaryOp_f (f32, Trunc,      truncf_fei);        d_m3UnaryOp_f (f64, Trunc,      trunc_dei);
d_m3UnaryOp_f (f32, Sqrt,       sqrtf_fei);         d_m3UnaryOp_f (f64, Sqrt,       sqrt_dei);
d_m3UnaryOp_f (f32, Nearest,    rintf_fei);         d_m3UnaryOp_f (f64, Nearest,    rint_dei);
d_m3UnaryOp_f (f32, Negate,     negf_fei);          d_m3UnaryOp_f (f64, Negate,     neg_dei);
#elif defined(USE_DD) || defined(USE_EFTSAN)
d_m3UnaryOp_f (f32, Abs,        fabsf_fi);         d_m3UnaryOp_f (f64, Abs,        fabs_di);
d_m3UnaryOp_f (f32, Ceil,       ceilf_fi);         d_m3UnaryOp_f (f64, Ceil,       ceil_di);
d_m3UnaryOp_f (f32, Floor,      floorf_fi);        d_m3UnaryOp_f (f64, Floor,      floor_di);
d_m3UnaryOp_f (f32, Trunc,      truncf_fi);        d_m3UnaryOp_f (f64, Trunc,      trunc_di);
d_m3UnaryOp_f (f32, Sqrt,       sqrtf_fi);         d_m3UnaryOp_f (f64, Sqrt,       sqrt_di);
d_m3UnaryOp_f (f32, Nearest,    rintf_fi);         d_m3UnaryOp_f (f64, Nearest,    rint_di);
d_m3UnaryOp_f (f32, Negate,     negf_fi);          d_m3UnaryOp_f (f64, Negate,     neg_di);
#else
d_m3UnaryOp_f (f32, Abs,        fabsf);         d_m3UnaryOp_f (f64, Abs,        fabs);
d_m3UnaryOp_f (f32, Ceil,       ceilf);         d_m3UnaryOp_f (f64, Ceil,       ceil);
d_m3UnaryOp_f (f32, Floor,      floorf);        d_m3UnaryOp_f (f64, Floor,      floor);
d_m3UnaryOp_f (f32, Trunc,      truncf);        d_m3UnaryOp_f (f64, Trunc,      trunc);
d_m3UnaryOp_f (f32, Sqrt,       sqrtf);         d_m3UnaryOp_f (f64, Sqrt,       sqrt);
d_m3UnaryOp_f (f32, Nearest,    rintf);         d_m3UnaryOp_f (f64, Nearest,    rint);
d_m3UnaryOp_f (f32, Negate,     -);             d_m3UnaryOp_f (f64, Negate,     -);
#endif

#endif

#define OP_EQZ(x) ((x) == 0)

d_m3UnaryOp_i (i32, EqualToZero, OP_EQZ)
d_m3UnaryOp_i (i64, EqualToZero, OP_EQZ)

// clz(0), ctz(0) results are undefined for rest platforms, fix it
#if (defined(__i386__) || defined(__x86_64__)) && !(defined(__AVX2__) || (defined(__ABM__) && defined(__BMI__)))
    #define OP_CLZ_32(x) (M3_UNLIKELY((x) == 0) ? 32 : __builtin_clz(x))
    #define OP_CTZ_32(x) (M3_UNLIKELY((x) == 0) ? 32 : __builtin_ctz(x))
    // for 64-bit instructions branchless approach more preferable
    #define OP_CLZ_64(x) (__builtin_clzll((x) | (1LL <<  0)) + OP_EQZ(x))
    #define OP_CTZ_64(x) (__builtin_ctzll((x) | (1LL << 63)) + OP_EQZ(x))
#elif defined(__ppc__) || defined(__ppc64__)
// PowerPC is defined for __builtin_clz(0) and __builtin_ctz(0).
// See (https://github.com/aquynh/capstone/blob/master/MathExtras.h#L99)
    #define OP_CLZ_32(x) __builtin_clz(x)
    #define OP_CTZ_32(x) __builtin_ctz(x)
    #define OP_CLZ_64(x) __builtin_clzll(x)
    #define OP_CTZ_64(x) __builtin_ctzll(x)
#else
    #define OP_CLZ_32(x) (M3_UNLIKELY((x) == 0) ? 32 : __builtin_clz(x))
    #define OP_CTZ_32(x) (M3_UNLIKELY((x) == 0) ? 32 : __builtin_ctz(x))
    #define OP_CLZ_64(x) (M3_UNLIKELY((x) == 0) ? 64 : __builtin_clzll(x))
    #define OP_CTZ_64(x) (M3_UNLIKELY((x) == 0) ? 64 : __builtin_ctzll(x))
#endif

d_m3UnaryOp_i (u32, Clz, OP_CLZ_32)
d_m3UnaryOp_i (u64, Clz, OP_CLZ_64)

d_m3UnaryOp_i (u32, Ctz, OP_CTZ_32)
d_m3UnaryOp_i (u64, Ctz, OP_CTZ_64)

d_m3UnaryOp_i (u32, Popcnt, __builtin_popcount)
d_m3UnaryOp_i (u64, Popcnt, __builtin_popcountll)

#define OP_WRAP_I64(X) ((X) & 0x00000000ffffffff)

d_m3Op(i32_Wrap_i64_r)
{
    _r0 = OP_WRAP_I64((i64) _r0);
    nextOp ();
}

d_m3Op(i32_Wrap_i64_s)
{
    i64 operand = slot (i64);
    _r0 = OP_WRAP_I64(operand);
    nextOp ();
}

// Integer sign extension operations
#define OP_EXTEND8_S_I32(X)  ((int32_t)(int8_t)(X))
#define OP_EXTEND16_S_I32(X) ((int32_t)(int16_t)(X))
#define OP_EXTEND8_S_I64(X)  ((int64_t)(int8_t)(X))
#define OP_EXTEND16_S_I64(X) ((int64_t)(int16_t)(X))
#define OP_EXTEND32_S_I64(X) ((int64_t)(int32_t)(X))

d_m3UnaryOp_i (i32, Extend8_s,  OP_EXTEND8_S_I32)
d_m3UnaryOp_i (i32, Extend16_s, OP_EXTEND16_S_I32)
d_m3UnaryOp_i (i64, Extend8_s,  OP_EXTEND8_S_I64)
d_m3UnaryOp_i (i64, Extend16_s, OP_EXTEND16_S_I64)
d_m3UnaryOp_i (i64, Extend32_s, OP_EXTEND32_S_I64)

#define d_m3TruncMacro(DEST, SRC, TYPE, NAME, FROM, OP, ...)   \
d_m3Op(TYPE##_##NAME##_##FROM##_r_r)                \
{                                                   \
    OP((DEST), (FROM) SRC, ##__VA_ARGS__);          \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_r_s)                \
{                                                   \
    FROM * stack = slot_ptr (FROM);                 \
    OP((DEST), (* stack), ##__VA_ARGS__);           \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_s_r)                \
{                                                   \
    TYPE * dest = slot_ptr (TYPE);                  \
    OP((* dest), (FROM) SRC, ##__VA_ARGS__);        \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_s_s)                \
{                                                   \
    FROM * stack = slot_ptr (FROM);                 \
    TYPE * dest = slot_ptr (TYPE);                  \
    OP((* dest), (* stack), ##__VA_ARGS__);         \
    nextOp ();                                      \
}

#if defined(USE_MPFR) || defined(USE_MPFR_DD)

#define d_m3TruncMacro_new(DEST, SRC_V, SRC_E, TYPE, NAME, FROM, OP, ...)   \
d_m3Op(TYPE##_##NAME##_##FROM##_r_r)                \
{                                                   \
    OP((DEST), (FROM) SRC_V, (mpfr_ptr) SRC_E,      \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_r_s)                \
{                                                   \
    FROM * stack_v = slot_ptr (FROM);               \
    mpfr_ptr * stack_e = slot_ptr_new (mpfr_ptr);   \
    OP((DEST), (* stack_v), (* stack_e),            \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_s_r)                \
{                                                   \
    TYPE * dest = slot_ptr (TYPE);                  \
    OP((* dest), (FROM) SRC_V, (mpfr_ptr) SRC_E,    \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_s_s)                \
{                                                   \
    FROM * stack_v = slot_ptr (FROM);               \
    mpfr_ptr * stack_e = slot_ptr_new (mpfr_ptr);   \
    TYPE * dest = slot_ptr (TYPE);                  \
    OP((* dest), (* stack_v), (* stack_e),          \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}

#elif defined(USE_QD)

#define d_m3TruncMacro_new(DEST, SRC_V, SRC_E, TYPE, NAME, FROM, OP, ...)   \
d_m3Op(TYPE##_##NAME##_##FROM##_r_r)                \
{                                                   \
    OP((DEST), (FROM) SRC_V, (f64*) SRC_E,          \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_r_s)                \
{                                                   \
    FROM * stack_v = slot_ptr (FROM);               \
    f64* * stack_e = slot_ptr_new (f64*);           \
    OP((DEST), (* stack_v), (* stack_e),            \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_s_r)                \
{                                                   \
    TYPE * dest = slot_ptr (TYPE);                  \
    OP((* dest), (FROM) SRC_V, (f64*) SRC_E,        \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_s_s)                \
{                                                   \
    FROM * stack_v = slot_ptr (FROM);               \
    f64* * stack_e = slot_ptr_new (f64*);           \
    TYPE * dest = slot_ptr (TYPE);                  \
    OP((* dest), (* stack_v), (* stack_e),          \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}

#elif defined(USE_EID)

#define d_m3TruncMacro_new(DEST, SRC_V, SRC_E, TYPE, NAME, FROM, OP, ...)   \
d_m3Op(TYPE##_##NAME##_##FROM##_r_r)                \
{                                                   \
    eid src_e = *(SRC_E);                           \
    OP((DEST), (FROM) SRC_V, &src_e,                \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_r_s)                \
{                                                   \
    FROM * stack_v = slot_ptr (FROM);               \
    eid_t stack_e = slot_eid();                     \
    OP((DEST), (* stack_v), stack_e,                \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_s_r)                \
{                                                   \
    TYPE * dest = slot_ptr (TYPE);                  \
    eid src_e = *(SRC_E);                           \
    OP((* dest), (FROM) SRC_V, &src_e,              \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_s_s)                \
{                                                   \
    FROM * stack_v = slot_ptr (FROM);               \
    eid_t stack_e = slot_eid();                     \
    TYPE * dest = slot_ptr (TYPE);                  \
    OP((* dest), (* stack_v), stack_e,              \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}

#elif defined(USE_DD) || defined(USE_EFTSAN)

#define d_m3TruncMacro_new(DEST, SRC_V, SRC_E, TYPE, NAME, FROM, OP, ...)   \
d_m3Op(TYPE##_##NAME##_##FROM##_r_r)                \
{                                                   \
    OP((DEST), (FROM) SRC_V, (f64) SRC_E,           \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_r_s)                \
{                                                   \
    FROM * stack_v = slot_ptr (FROM);               \
    f64 * stack_e = slot_ptr_new (f64);             \
    OP((DEST), (* stack_v), (* stack_e),            \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_s_r)                \
{                                                   \
    TYPE * dest = slot_ptr (TYPE);                  \
    OP((* dest), (FROM) SRC_V, (f64) SRC_E,         \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}                                                   \
d_m3Op(TYPE##_##NAME##_##FROM##_s_s)                \
{                                                   \
    FROM * stack_v = slot_ptr (FROM);               \
    f64 * stack_e = slot_ptr_new (f64);             \
    TYPE * dest = slot_ptr (TYPE);                  \
    OP((* dest), (* stack_v), (* stack_e),          \
       ##__VA_ARGS__);                              \
    nextOp ();                                      \
}

#else
#endif

#if d_m3HasFloat

#if d_m3HasErrorSlot
d_m3TruncMacro_new(_r0, _fp0, _fp1, i32, Trunc, f32, OP_I32_TRUNC_F32_new)
d_m3TruncMacro_new(_r0, _fp0, _fp1, u32, Trunc, f32, OP_U32_TRUNC_F32_new)
d_m3TruncMacro_new(_r0, _fp0, _fp1, i32, Trunc, f64, OP_I32_TRUNC_F64_new)
d_m3TruncMacro_new(_r0, _fp0, _fp1, u32, Trunc, f64, OP_U32_TRUNC_F64_new)

d_m3TruncMacro_new(_r0, _fp0, _fp1, i64, Trunc, f32, OP_I64_TRUNC_F32_new)
d_m3TruncMacro_new(_r0, _fp0, _fp1, u64, Trunc, f32, OP_U64_TRUNC_F32_new)
d_m3TruncMacro_new(_r0, _fp0, _fp1, i64, Trunc, f64, OP_I64_TRUNC_F64_new)
d_m3TruncMacro_new(_r0, _fp0, _fp1, u64, Trunc, f64, OP_U64_TRUNC_F64_new)

d_m3TruncMacro_new(_r0, _fp0, _fp1, i32, TruncSat, f32, OP_I32_TRUNC_SAT_F32_new)
d_m3TruncMacro_new(_r0, _fp0, _fp1, u32, TruncSat, f32, OP_U32_TRUNC_SAT_F32_new)
d_m3TruncMacro_new(_r0, _fp0, _fp1, i32, TruncSat, f64, OP_I32_TRUNC_SAT_F64_new)
d_m3TruncMacro_new(_r0, _fp0, _fp1, u32, TruncSat, f64, OP_U32_TRUNC_SAT_F64_new)

d_m3TruncMacro_new(_r0, _fp0, _fp1, i64, TruncSat, f32, OP_I64_TRUNC_SAT_F32_new)
d_m3TruncMacro_new(_r0, _fp0, _fp1, u64, TruncSat, f32, OP_U64_TRUNC_SAT_F32_new)
d_m3TruncMacro_new(_r0, _fp0, _fp1, i64, TruncSat, f64, OP_I64_TRUNC_SAT_F64_new)
d_m3TruncMacro_new(_r0, _fp0, _fp1, u64, TruncSat, f64, OP_U64_TRUNC_SAT_F64_new)
#else
d_m3TruncMacro(_r0, _fp0, i32, Trunc, f32, OP_I32_TRUNC_F32)
d_m3TruncMacro(_r0, _fp0, u32, Trunc, f32, OP_U32_TRUNC_F32)
d_m3TruncMacro(_r0, _fp0, i32, Trunc, f64, OP_I32_TRUNC_F64)
d_m3TruncMacro(_r0, _fp0, u32, Trunc, f64, OP_U32_TRUNC_F64)

d_m3TruncMacro(_r0, _fp0, i64, Trunc, f32, OP_I64_TRUNC_F32)
d_m3TruncMacro(_r0, _fp0, u64, Trunc, f32, OP_U64_TRUNC_F32)
d_m3TruncMacro(_r0, _fp0, i64, Trunc, f64, OP_I64_TRUNC_F64)
d_m3TruncMacro(_r0, _fp0, u64, Trunc, f64, OP_U64_TRUNC_F64)

d_m3TruncMacro(_r0, _fp0, i32, TruncSat, f32, OP_I32_TRUNC_SAT_F32)
d_m3TruncMacro(_r0, _fp0, u32, TruncSat, f32, OP_U32_TRUNC_SAT_F32)
d_m3TruncMacro(_r0, _fp0, i32, TruncSat, f64, OP_I32_TRUNC_SAT_F64)
d_m3TruncMacro(_r0, _fp0, u32, TruncSat, f64, OP_U32_TRUNC_SAT_F64)

d_m3TruncMacro(_r0, _fp0, i64, TruncSat, f32, OP_I64_TRUNC_SAT_F32)
d_m3TruncMacro(_r0, _fp0, u64, TruncSat, f32, OP_U64_TRUNC_SAT_F32)
d_m3TruncMacro(_r0, _fp0, i64, TruncSat, f64, OP_I64_TRUNC_SAT_F64)
d_m3TruncMacro(_r0, _fp0, u64, TruncSat, f64, OP_U64_TRUNC_SAT_F64)
#endif

#endif

#define d_m3TypeModifyOp(REG_TO, REG_FROM, TO, NAME, FROM)  \
d_m3Op(TO##_##NAME##_##FROM##_r)                            \
{                                                           \
    REG_TO = (TO) ((FROM) REG_FROM);                        \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s)                            \
{                                                           \
    FROM from = slot (FROM);                                \
    REG_TO = (TO) (from);                                   \
    nextOp ();                                              \
}

#if defined(USE_MPFR) || defined(USE_MPFR_DD)

#define d_m3TypeModifyOp_new(REG_TO_V, REG_TO_E, REG_FROM_V, REG_FROM_E, TO, NAME, FROM)  \
d_m3Op(TO##_##NAME##_##FROM##_r)                            \
{                                                           \
    REG_TO_V = (TO) ((FROM) REG_FROM_V);                    \
    REG_TO_E = (mpfr_ptr) REG_FROM_E;                       \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s)                            \
{                                                           \
    FROM from_v = slot (FROM);                              \
    mpfr_ptr from_e = slot_new (mpfr_ptr);                  \
    REG_TO_V = (TO) (from_v);                               \
    REG_TO_E = from_e;                                      \
    nextOp ();                                              \
}

#elif defined(USE_QD)

#define d_m3TypeModifyOp_new(REG_TO_V, REG_TO_E, REG_FROM_V, REG_FROM_E, TO, NAME, FROM)  \
d_m3Op(TO##_##NAME##_##FROM##_r)                            \
{                                                           \
    REG_TO_V = (TO) ((FROM) REG_FROM_V);                    \
    REG_TO_E = (f64*) REG_FROM_E;                           \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s)                            \
{                                                           \
    FROM from_v = slot (FROM);                              \
    f64* from_e = slot_new (f64*);                          \
    REG_TO_V = (TO) (from_v);                               \
    REG_TO_E = from_e;                                      \
    nextOp ();                                              \
}

#elif defined(USE_EID)

#define d_m3TypeModifyOp_new(REG_TO_V, REG_TO_E, REG_FROM_V, REG_FROM_E, TO, NAME, FROM)  \
d_m3Op(TO##_##NAME##_##FROM##_r)                            \
{                                                           \
    FROM from_v = (FROM) REG_FROM_V;                        \
    TO   to_v   = (TO) from_v;                              \
    eid from_e = *(REG_FROM_E);                             \
    bool had_shadow = from_e.v != 0.0 || from_e.op != 0 || from_e.id != 0 || from_e.id_aux != 0 || from_e.absorb || from_e.possible_zero; \
                                                            \
    REG_TO_E = DEFAULT_EID_REGISTER;                        \
    REG_TO_V = to_v;                                        \
    *(REG_TO_E) = from_e;                                   \
    if (had_shadow) {                                       \
        (REG_TO_E)->v += (double) from_v - (double) to_v;   \
    }                                                       \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s)                            \
{                                                           \
    FROM  from_v = slot (FROM);                             \
    eid_t from_e = slot_eid();                              \
    TO    to_v   = (TO) from_v;                             \
    bool had_shadow = from_e->v != 0.0 || from_e->op != 0 || from_e->id != 0 || from_e->id_aux != 0 || from_e->absorb || from_e->possible_zero; \
                                                            \
    REG_TO_E = DEFAULT_EID_REGISTER;                        \
    REG_TO_V = to_v;                                        \
    *(REG_TO_E) = *from_e;                                  \
    if (had_shadow) {                                       \
        (REG_TO_E)->v += (double) from_v - (double) to_v;   \
    }                                                       \
    nextOp ();                                              \
}

#elif defined(USE_DD) || defined(USE_EFTSAN)

#define d_m3TypeModifyOp_new(REG_TO_V, REG_TO_E, REG_FROM_V, REG_FROM_E, TO, NAME, FROM)  \
d_m3Op(TO##_##NAME##_##FROM##_r)                            \
{                                                           \
    REG_TO_V = (TO) ((FROM) REG_FROM_V);                    \
    REG_TO_E = (f64) REG_FROM_E;                            \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s)                            \
{                                                           \
    FROM from_v = slot (FROM);                              \
    f64 from_e = slot_new (f64);                            \
    REG_TO_V = (TO) (from_v);                               \
    REG_TO_E = from_e;                                      \
    nextOp ();                                              \
}

#else
#endif

// Int to int
d_m3TypeModifyOp (_r0, _r0, i64, Extend, i32);
d_m3TypeModifyOp (_r0, _r0, i64, Extend, u32);

// Float to float
#if d_m3HasFloat

#if d_m3HasErrorSlot
d_m3TypeModifyOp_new (_fp0, _fp1, _fp0, _fp1, f32, Demote, f64);
d_m3TypeModifyOp_new (_fp0, _fp1, _fp0, _fp1, f64, Promote, f32);
#else
d_m3TypeModifyOp (_fp0, _fp0, f32, Demote, f64);
d_m3TypeModifyOp (_fp0, _fp0, f64, Promote, f32);
#endif

#endif

#define d_m3TypeConvertOp(REG_TO, REG_FROM, TO, NAME, FROM) \
d_m3Op(TO##_##NAME##_##FROM##_r_r)                          \
{                                                           \
    REG_TO = (TO) ((FROM) REG_FROM);                        \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s_r)                          \
{                                                           \
    slot (TO) = (TO) ((FROM) REG_FROM);                     \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_r_s)                          \
{                                                           \
    FROM from = slot (FROM);                                \
    REG_TO = (TO) (from);                                   \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s_s)                          \
{                                                           \
    FROM from = slot (FROM);                                \
    slot (TO) = (TO) (from);                                \
    nextOp ();                                              \
}

// USE_MPFR & USE_MPFR_DD don't share the same logic here
#ifdef USE_MPFR

#define d_m3TypeConvertOp_new(REG_TO_V, REG_TO_E, REG_FROM, TO, NAME, FROM) \
d_m3Op(TO##_##NAME##_##FROM##_r_r)                          \
{                                                           \
    REG_TO_V = (TO) ((FROM) REG_FROM);                      \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, REG_TO_V, MPFR_RNDN);              \
    REG_TO_E = *e;                                          \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s_r)                          \
{                                                           \
    TO value = (TO) ((FROM) REG_FROM);                      \
    slot (TO) = value;                                      \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, value, MPFR_RNDN);                 \
    slot_new (mpfr_ptr) = *e;                               \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_r_s)                          \
{                                                           \
    FROM from = slot (FROM);                                \
    REG_TO_V = (TO) (from);                                 \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, REG_TO_V, MPFR_RNDN);              \
    REG_TO_E = *e;                                          \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s_s)                          \
{                                                           \
    FROM from = slot (FROM);                                \
    TO value = (TO) (from);                                 \
    slot (TO) = value;                                      \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, value, MPFR_RNDN);                 \
    slot_new (mpfr_ptr) = *e;                               \
    nextOp ();                                              \
}

#elif defined(USE_MPFR_DD)

#define d_m3TypeConvertOp_new(REG_TO_V, REG_TO_E, REG_FROM, TO, NAME, FROM) \
d_m3Op(TO##_##NAME##_##FROM##_r_r)                          \
{                                                           \
    REG_TO_V = (TO) ((FROM) REG_FROM);                      \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, 0.0, MPFR_RNDN);                   \
    REG_TO_E = *e;                                          \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s_r)                          \
{                                                           \
    TO value = (TO) ((FROM) REG_FROM);                      \
    slot (TO) = value;                                      \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, 0.0, MPFR_RNDN);                   \
    slot_new (mpfr_ptr) = *e;                               \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_r_s)                          \
{                                                           \
    FROM from = slot (FROM);                                \
    REG_TO_V = (TO) (from);                                 \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, 0.0, MPFR_RNDN);                   \
    REG_TO_E = *e;                                          \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s_s)                          \
{                                                           \
    FROM from = slot (FROM);                                \
    TO value = (TO) (from);                                 \
    slot (TO) = value;                                      \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, 0.0, MPFR_RNDN);                   \
    slot_new (mpfr_ptr) = *e;                               \
    nextOp ();                                              \
}

#elif defined(USE_QD)

#define d_m3TypeConvertOp_new(REG_TO_V, REG_TO_E, REG_FROM, TO, NAME, FROM) \
d_m3Op(TO##_##NAME##_##FROM##_r_r)                          \
{                                                           \
    REG_TO_V = (TO) ((FROM) REG_FROM);                      \
    f64* e = (f64*) malloc(sizeof(f64) * 4);                \
    c_qd_copy_d((f64)REG_TO_V, e);                          \
    REG_TO_E = e;                                           \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s_r)                          \
{                                                           \
    TO value = (TO) ((FROM) REG_FROM);                      \
    slot (TO) = value;                                      \
    f64* e = (f64*) malloc(sizeof(f64) * 4);                \
    c_qd_copy_d((f64)value, e);                             \
    slot_new (f64*) = e;                                    \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_r_s)                          \
{                                                           \
    FROM from = slot (FROM);                                \
    REG_TO_V = (TO) (from);                                 \
    f64* e = (f64*) malloc(sizeof(f64) * 4);                \
    c_qd_copy_d((f64)REG_TO_V, e);                          \
    REG_TO_E = e;                                           \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s_s)                          \
{                                                           \
    FROM from = slot (FROM);                                \
    TO value = (TO) (from);                                 \
    slot (TO) = value;                                      \
    f64* e = (f64*) malloc(sizeof(f64) * 4);                \
    c_qd_copy_d((f64)value, e);                             \
    slot_new (f64*) = e;                                    \
    nextOp ();                                              \
}

#elif defined(USE_EID)

#define d_m3TypeConvertOp_new(REG_TO_V, REG_TO_E, REG_FROM, TO, NAME, FROM) \
d_m3Op(TO##_##NAME##_##FROM##_r_r)                          \
{                                                           \
    REG_TO_V = (TO) ((FROM) REG_FROM);                      \
    REG_TO_E = DEFAULT_EID_REGISTER;                        \
    *(REG_TO_E) = EID_DEFAULT;                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s_r)                          \
{                                                           \
    TO value = (TO) ((FROM) REG_FROM);                      \
    slot (TO) = value;                                      \
    *slot_eid() = EID_DEFAULT;                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_r_s)                          \
{                                                           \
    FROM from = slot (FROM);                                \
    REG_TO_V = (TO) (from);                                 \
    REG_TO_E = DEFAULT_EID_REGISTER;                        \
    *(REG_TO_E) = EID_DEFAULT;                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s_s)                          \
{                                                           \
    FROM from = slot (FROM);                                \
    TO value = (TO) (from);                                 \
    slot (TO) = value;                                      \
    *slot_eid() = EID_DEFAULT;                              \
    nextOp ();                                              \
}

#elif defined(USE_DD) || defined(USE_EFTSAN)

#define d_m3TypeConvertOp_new(REG_TO_V, REG_TO_E, REG_FROM, TO, NAME, FROM) \
d_m3Op(TO##_##NAME##_##FROM##_r_r)                          \
{                                                           \
    REG_TO_V = (TO) ((FROM) REG_FROM);                      \
    REG_TO_E = (f64) ((FROM) 0);                            \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s_r)                          \
{                                                           \
    slot (TO) = (TO) ((FROM) REG_FROM);                     \
    slot_new (f64) = (f64) ((FROM) 0);                      \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_r_s)                          \
{                                                           \
    FROM from = slot (FROM);                                \
    REG_TO_V = (TO) (from);                                 \
    REG_TO_E = (f64) (0);                                   \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_##NAME##_##FROM##_s_s)                          \
{                                                           \
    FROM from = slot (FROM);                                \
    slot (TO) = (TO) (from);                                \
    slot_new (f64) = (f64) (0);                             \
    nextOp ();                                              \
}

#else
#endif

// Int to float
#if d_m3HasFloat

#if d_m3HasErrorSlot
d_m3TypeConvertOp_new (_fp0, _fp1, _r0, f64, Convert, i32);
d_m3TypeConvertOp_new (_fp0, _fp1, _r0, f64, Convert, u32);
d_m3TypeConvertOp_new (_fp0, _fp1, _r0, f64, Convert, i64);
d_m3TypeConvertOp_new (_fp0, _fp1, _r0, f64, Convert, u64);

d_m3TypeConvertOp_new (_fp0, _fp1, _r0, f32, Convert, i32);
d_m3TypeConvertOp_new (_fp0, _fp1, _r0, f32, Convert, u32);
d_m3TypeConvertOp_new (_fp0, _fp1, _r0, f32, Convert, i64);
d_m3TypeConvertOp_new (_fp0, _fp1, _r0, f32, Convert, u64);
#else
d_m3TypeConvertOp (_fp0, _r0, f64, Convert, i32);
d_m3TypeConvertOp (_fp0, _r0, f64, Convert, u32);
d_m3TypeConvertOp (_fp0, _r0, f64, Convert, i64);
d_m3TypeConvertOp (_fp0, _r0, f64, Convert, u64);

d_m3TypeConvertOp (_fp0, _r0, f32, Convert, i32);
d_m3TypeConvertOp (_fp0, _r0, f32, Convert, u32);
d_m3TypeConvertOp (_fp0, _r0, f32, Convert, i64);
d_m3TypeConvertOp (_fp0, _r0, f32, Convert, u64);
#endif

#endif

#define d_m3ReinterpretOp(REG, TO, SRC, FROM)               \
d_m3Op(TO##_Reinterpret_##FROM##_r_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC;                                       \
    REG = u.t;                                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_r_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = slot (FROM);                                      \
    REG = u.t;                                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC;                                       \
    slot (TO) = u.t;                                        \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = slot (FROM);                                      \
    slot (TO) = u.t;                                        \
    nextOp ();                                              \
}

#if defined(USE_MPFR) || defined(USE_MPFR_DD)  // Do not consider error terms in FP->INT conversion

#define d_m3ReinterpretOp_fi(REG, TO, SRC_V, SRC_E, FROM)   \
d_m3Op(TO##_Reinterpret_##FROM##_r_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC_V;                                     \
    REG = u.t;                                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_r_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    FROM value = slot (FROM);                               \
    mpfr_ptr error = slot_new (mpfr_ptr);                   \
    u.c = value;                                            \
    REG = u.t;                                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC_V;                                     \
    slot (TO) = u.t;                                        \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    FROM value = slot (FROM);                               \
    mpfr_ptr error = slot_new (mpfr_ptr);                   \
    u.c = value;                                            \
    slot (TO) = u.t;                                        \
    nextOp ();                                              \
}

#elif defined(USE_QD)

#define d_m3ReinterpretOp_fi(REG, TO, SRC_V, SRC_E, FROM)   \
d_m3Op(TO##_Reinterpret_##FROM##_r_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC_V;                                     \
    REG = u.t;                                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_r_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    FROM value = slot (FROM);                               \
    f64* error = slot_new (f64*);                           \
    u.c = value;                                            \
    REG = u.t;                                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC_V;                                     \
    slot (TO) = u.t;                                        \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    FROM value = slot (FROM);                               \
    f64* error = slot_new (f64*);                           \
    u.c = value;                                            \
    slot (TO) = u.t;                                        \
    nextOp ();                                              \
}

#elif defined(USE_EID)

#define d_m3ReinterpretOp_fi(REG, TO, SRC_V, SRC_E, FROM)   \
d_m3Op(TO##_Reinterpret_##FROM##_r_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC_V;                                     \
    REG = u.t;                                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_r_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    FROM value = slot (FROM);                               \
    u.c = value;                                            \
    REG = u.t;                                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC_V;                                     \
    slot (TO) = u.t;                                        \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    FROM value = slot (FROM);                               \
    u.c = value;                                            \
    slot (TO) = u.t;                                        \
    nextOp ();                                              \
}

#elif defined(USE_DD) || defined(USE_EFTSAN)

#define d_m3ReinterpretOp_fi(REG, TO, SRC_V, SRC_E, FROM)   \
d_m3Op(TO##_Reinterpret_##FROM##_r_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC_V;                                     \
    REG = u.t;                                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_r_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    FROM value = slot (FROM);                               \
    f64 error = slot_new (f64);                             \
    u.c = value;                                            \
    REG = u.t;                                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC_V;                                     \
    slot (TO) = u.t;                                        \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    FROM value = slot (FROM);                               \
    f64 error = slot_new (f64);                             \
    u.c = value;                                            \
    slot (TO) = u.t;                                        \
    nextOp ();                                              \
}

#else
#endif

#if d_m3HasFloat

#if d_m3HasErrorSlot
d_m3ReinterpretOp_fi (_r0, i32, _fp0, _fp1, f32)
d_m3ReinterpretOp_fi (_r0, i64, _fp0, _fp1, f64)
#else
d_m3ReinterpretOp (_r0, i32, _fp0, f32)
d_m3ReinterpretOp (_r0, i64, _fp0, f64)
#endif

#endif

// Need update whenever data structure for eid is changed
#ifdef USE_MPFR

#define d_m3ReinterpretOp_if(REG_V, REG_E, TO, SRC, FROM)   \
d_m3Op(TO##_Reinterpret_##FROM##_r_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC;                                       \
    REG_V = u.t;                                            \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, REG_V, MPFR_RNDN);                 \
    REG_E = *e;                                             \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_r_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = slot (FROM);                                      \
    REG_V = u.t;                                            \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, REG_V, MPFR_RNDN);                 \
    REG_E = *e;                                             \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC;                                       \
    slot (TO) = u.t;                                        \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, u.t, MPFR_RNDN);                   \
    slot_new (mpfr_ptr) = *e;                               \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = slot (FROM);                                      \
    slot (TO) = u.t;                                        \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, u.t, MPFR_RNDN);                   \
    slot_new (mpfr_ptr) = *e;                               \
    nextOp ();                                              \
}

#elif defined(USE_MPFR_DD)

#define d_m3ReinterpretOp_if(REG_V, REG_E, TO, SRC, FROM)   \
d_m3Op(TO##_Reinterpret_##FROM##_r_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC;                                       \
    REG_V = u.t;                                            \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, 0.0, MPFR_RNDN);                   \
    REG_E = *e;                                             \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_r_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = slot (FROM);                                      \
    REG_V = u.t;                                            \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, 0.0, MPFR_RNDN);                   \
    REG_E = *e;                                             \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC;                                       \
    slot (TO) = u.t;                                        \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, 0.0, MPFR_RNDN);                   \
    slot_new (mpfr_ptr) = *e;                               \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = slot (FROM);                                      \
    slot (TO) = u.t;                                        \
    mpfr_t* e = malloc(sizeof(mpfr_t));                     \
    mpfr_init_set_d (*e, 0.0, MPFR_RNDN);                   \
    slot_new (mpfr_ptr) = *e;                               \
    nextOp ();                                              \
}

#elif defined(USE_QD)

#define d_m3ReinterpretOp_if(REG_V, REG_E, TO, SRC, FROM)   \
d_m3Op(TO##_Reinterpret_##FROM##_r_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC;                                       \
    REG_V = u.t;                                            \
    f64* e = (f64*) malloc(sizeof(f64) * 4);                \
    c_qd_copy_d((f64)REG_V, e);                             \
    REG_E = e;                                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_r_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = slot (FROM);                                      \
    REG_V = u.t;                                            \
    f64* e = (f64*) malloc(sizeof(f64) * 4);                \
    c_qd_copy_d((f64)REG_V, e);                             \
    REG_E = e;                                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC;                                       \
    slot (TO) = u.t;                                        \
    f64* e = (f64*) malloc(sizeof(f64) * 4);                \
    c_qd_copy_d((f64)u.t, e);                               \
    slot_new (f64*) = e;                                    \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = slot (FROM);                                      \
    slot (TO) = u.t;                                        \
    f64* e = (f64*) malloc(sizeof(f64) * 4);                \
    c_qd_copy_d((f64)u.t, e);                               \
    slot_new (f64*) = e;                                    \
    nextOp ();                                              \
}

#elif defined(USE_EID)

#define d_m3ReinterpretOp_if(REG_V, REG_E, TO, SRC, FROM)   \
d_m3Op(TO##_Reinterpret_##FROM##_r_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC;                                       \
    REG_V = u.t;                                            \
    REG_E = DEFAULT_EID_REGISTER;                           \
    *(REG_E) = EID_DEFAULT;                                 \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_r_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = slot (FROM);                                      \
    REG_V = u.t;                                            \
    REG_E = DEFAULT_EID_REGISTER;                           \
    *(REG_E) = EID_DEFAULT;                                 \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC;                                       \
    slot (TO) = u.t;                                        \
    *slot_eid() = EID_DEFAULT;                              \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = slot (FROM);                                      \
    slot (TO) = u.t;                                        \
    *slot_eid() = EID_DEFAULT;                              \
    nextOp ();                                              \
}

#elif defined(USE_DD) || defined(USE_EFTSAN)

#define d_m3ReinterpretOp_if(REG_V, REG_E, TO, SRC, FROM)   \
d_m3Op(TO##_Reinterpret_##FROM##_r_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC;                                       \
    REG_V = u.t;                                            \
    REG_E = (f64) 0;                                        \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_r_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = slot (FROM);                                      \
    REG_V = u.t;                                            \
    REG_E = (f64) 0;                                        \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_r)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = (FROM) SRC;                                       \
    slot (TO) = u.t;                                        \
    slot_new (f64) = (f64) 0;                               \
    nextOp ();                                              \
}                                                           \
                                                            \
d_m3Op(TO##_Reinterpret_##FROM##_s_s)                       \
{                                                           \
    union { FROM c; TO t; } u;                              \
    u.c = slot (FROM);                                      \
    slot (TO) = u.t;                                        \
    slot_new (f64) = (f64) 0;                               \
    nextOp ();                                              \
}

#else
#endif

#if d_m3HasFloat

#if d_m3HasErrorSlot
d_m3ReinterpretOp_if (_fp0, _fp1, f32, _r0, i32)
d_m3ReinterpretOp_if (_fp0, _fp1, f64, _r0, i64)
#else
d_m3ReinterpretOp (_fp0, f32, _r0, i32)
d_m3ReinterpretOp (_fp0, f64, _r0, i64)
#endif

#endif

d_m3Op  (GetGlobal_s32)
{
    u32 * global = immediate (u32 *);
    slot (u32) = * global;                        //  printf ("get global: %p %" PRIi64 "\n", global, *global);

    nextOp ();
}


d_m3Op  (GetGlobal_s64)
{
    u64 * global = immediate (u64 *);
    slot (u64) = * global;                        // printf ("get global: %p %" PRIi64 "\n", global, *global);

    nextOp ();
}


d_m3Op  (SetGlobal_i32)
{
    u32 * global = immediate (u32 *);
    * global = (u32) _r0;                         //  printf ("set global: %p %" PRIi64 "\n", global, _r0);

    nextOp ();
}


d_m3Op  (SetGlobal_i64)
{
    u64 * global = immediate (u64 *);
    * global = (u64) _r0;                         //  printf ("set global: %p %" PRIi64 "\n", global, _r0);

    nextOp ();
}


d_m3Op  (Call)
{
    pc_t callPC                 = immediate (pc_t);
    i32 stackOffset             = immediate (i32);
    IM3Memory memory            = m3MemInfo (_mem);
    IM3Memory s_memory          = m3MemInfo_new (_mem);

    m3stack_t sp = _sp + stackOffset;
    m3stack_t ssp = _ssp + (stackOffset * M3_SHADOW_STACK_SLOT_SCALE);
    // printf("Call: %p, %p\n", sp, ssp);

# if (d_m3EnableOpProfiling || d_m3EnableOpTracing)
#  if defined(USE_EID)
    m3ret_t r = Call (callPC, sp, ssp, _mem, _smem, d_m3BaseOpDefaultArgs, 0., d_m3CurrentEidRegister(((_mem) ? (_mem)->runtime : NULL), ssp), d_m3BaseCstr);
#  else
    m3ret_t r = Call (callPC, sp, ssp, _mem, _smem, d_m3OpDefaultArgs, d_m3BaseCstr);
#  endif
# else
#  if defined(USE_EID)
    m3ret_t r = Call (callPC, sp, ssp, _mem, _smem, d_m3BaseOpDefaultArgs, 0., d_m3CurrentEidRegister(((_mem) ? (_mem)->runtime : NULL), ssp));
#  else
    m3ret_t r = Call (callPC, sp, ssp, _mem, _smem, d_m3OpDefaultArgs);
#  endif
# endif

    _mem = memory->mallocated;
    _smem = s_memory->mallocated;

    if (M3_LIKELY(not r))
        nextOp ();
    else
    {
        pushBacktraceFrame ();
        forwardTrap (r);
    }
}


d_m3Op  (CallIndirect)
{
    u32 tableIndex              = slot (u32);
    IM3Module module            = immediate (IM3Module);
    IM3FuncType type            = immediate (IM3FuncType);
    i32 stackOffset             = immediate (i32);
    IM3Memory memory            = m3MemInfo (_mem);
    IM3Memory s_memory          = m3MemInfo_new (_mem);

    m3stack_t sp = _sp + stackOffset;
    m3stack_t ssp = _ssp + (stackOffset * M3_SHADOW_STACK_SLOT_SCALE);

    m3ret_t r = m3Err_none;

    if (M3_LIKELY(tableIndex < module->table0Size))
    {
        IM3Function function = module->table0 [tableIndex];

        if (M3_LIKELY(function))
        {
            if (M3_LIKELY(type == function->funcType))
            {
                if (M3_UNLIKELY(not function->compiled))
                    r = CompileFunction (function);

                if (M3_LIKELY(not r))
                {

# if (d_m3EnableOpProfiling || d_m3EnableOpTracing)
#  if defined(USE_EID)
                    r = Call (function->compiled, sp, ssp, _mem, _smem, d_m3BaseOpDefaultArgs, 0., d_m3CurrentEidRegister(((_mem) ? (_mem)->runtime : NULL), ssp), d_m3BaseCstr);
#  else
                    r = Call (function->compiled, sp, ssp, _mem, _smem, d_m3OpDefaultArgs, d_m3BaseCstr);
#  endif
# else
#  if defined(USE_EID)
                    r = Call (function->compiled, sp, ssp, _mem, _smem, d_m3BaseOpDefaultArgs, 0., d_m3CurrentEidRegister(((_mem) ? (_mem)->runtime : NULL), ssp));
#  else
                    r = Call (function->compiled, sp, ssp, _mem, _smem, d_m3OpDefaultArgs);
#  endif
# endif

                    _mem = memory->mallocated;
                    _smem = s_memory->mallocated;

                    if (M3_LIKELY(not r))
                        nextOpDirect ();
                    else
                    {
                        pushBacktraceFrame ();
                        forwardTrap (r);
                    }
                }
            }
            else r = m3Err_trapIndirectCallTypeMismatch;
        }
        else r = m3Err_trapTableElementIsNull;
    }
    else r = m3Err_trapTableIndexOutOfRange;

    if (M3_UNLIKELY(r))
        newTrap (r);
    else forwardTrap (r);
}


d_m3Op  (CallRawFunction)
{
    d_m3TracePrepare

    M3ImportContext ctx;

    M3RawCall call = (M3RawCall) (* _pc++);
    ctx.function = immediate (IM3Function);
    ctx.userdata = immediate (void *);
    u64* const sp = ((u64*)_sp);
    IM3Memory memory = m3MemInfo (_mem);

    IM3Runtime runtime = m3MemRuntime(_mem);

#if d_m3EnableStrace
    IM3FuncType ftype = ctx.function->funcType;

    FILE* out = stderr;
    char outbuff[1024];
    char* outp = outbuff;
    char* oute = outbuff+1024;

    outp += snprintf(outp, oute-outp, "%s!%s(", ctx.function->import.moduleUtf8, ctx.function->import.fieldUtf8);

    const int nArgs = ftype->numArgs;
    const int nRets = ftype->numRets;
    u64 * args = sp + nRets;
    for (int i=0; i<nArgs; i++) {
        const int type = ftype->types[nRets + i];
        switch (type) {
        case c_m3Type_i32:  outp += snprintf(outp, oute-outp, "%" PRIi32, *(i32*)(args+i)); break;
        case c_m3Type_i64:  outp += snprintf(outp, oute-outp, "%" PRIi64, *(i64*)(args+i)); break;
        case c_m3Type_f32:  outp += snprintf(outp, oute-outp, "%" PRIf32, *(f32*)(args+i)); break;
        case c_m3Type_f64:  outp += snprintf(outp, oute-outp, "%" PRIf64, *(f64*)(args+i)); break;
        default:            outp += snprintf(outp, oute-outp, "<type %d>", type);         break;
        }
        outp += snprintf(outp, oute-outp, (i < nArgs-1) ? ", " : ")");
    }
# if d_m3EnableStrace >= 2
    outp += snprintf(outp, oute-outp, " { <native> }");
# endif
#endif

    // m3_Call uses runtime->stack to set-up initial exported function stack.
    // Reconfigure the stack to enable recursive invocations of m3_Call.
    // I.e. exported/table function can be called from an impoted function.
    void* stack_backup = runtime->stack;
    runtime->stack = sp;
    m3ret_t possible_trap = call (runtime, &ctx, sp, m3MemData(_mem));
    runtime->stack = stack_backup;

#if d_m3EnableStrace
    if (M3_UNLIKELY(possible_trap)) {
        d_m3TracePrint("%s -> %s", outbuff, (char*)possible_trap);
    } else {
        switch (GetSingleRetType(ftype)) {
        case c_m3Type_none: d_m3TracePrint("%s", outbuff); break;
        case c_m3Type_i32:  d_m3TracePrint("%s = %" PRIi32, outbuff, *(i32*)sp); break;
        case c_m3Type_i64:  d_m3TracePrint("%s = %" PRIi64, outbuff, *(i64*)sp); break;
        case c_m3Type_f32:  d_m3TracePrint("%s = %" PRIf32, outbuff, *(f32*)sp); break;
        case c_m3Type_f64:  d_m3TracePrint("%s = %" PRIf64, outbuff, *(f64*)sp); break;
        }
    }
#endif

    if (M3_UNLIKELY(possible_trap)) {
        _mem = memory->mallocated;
        pushBacktraceFrame ();
    }
    forwardTrap (possible_trap);
}


d_m3Op  (MemSize)
{
    IM3Memory memory            = m3MemInfo (_mem);

    _r0 = memory->numPages;

    nextOp ();
}


d_m3Op  (MemGrow)
{
    IM3Runtime runtime          = m3MemRuntime(_mem);
    IM3Memory memory            = & runtime->memory;
    IM3Memory s_memory          = & runtime->s_memory;

    i32 numPagesToGrow = _r0;
    if (numPagesToGrow >= 0) {
        _r0 = memory->numPages;

        if (M3_LIKELY(numPagesToGrow))
        {
            u32 requiredPages = memory->numPages + numPagesToGrow;

            M3Result r = ResizeMemory (runtime, requiredPages);
            if (r)
                _r0 = -1;

            _mem = memory->mallocated;
            _smem = s_memory->mallocated;
        }
    }
    else
    {
        _r0 = -1;
    }

    nextOp ();
}


d_m3Op  (MemCopy)
{
    u32 size = (u32) _r0;
    u64 source = slot (u32);
    u64 destination = slot (u32);

    if (M3_LIKELY(destination + size <= _mem->length) && M3_LIKELY(destination * M3_SHADOW_MEMORY_SCALE + ((u64) size * M3_SHADOW_MEMORY_SCALE) <= _smem->length))
    {
        if (M3_LIKELY(source + size <= _mem->length) && M3_LIKELY(source * M3_SHADOW_MEMORY_SCALE + ((u64) size * M3_SHADOW_MEMORY_SCALE) <= _smem->length))
        {
            u8 * dst = m3MemData (_mem) + destination;
            u8 * src = m3MemData (_mem) + source;
            u8 * s_dst = m3MemData (_smem) + (destination * M3_SHADOW_MEMORY_SCALE);
            u8 * s_src = m3MemData (_smem) + (source * M3_SHADOW_MEMORY_SCALE);
            memmove (dst, src, size);
            memmove (s_dst, s_src, (size * M3_SHADOW_MEMORY_SCALE));

            nextOp ();
        }
        else d_outOfBoundsMemOp (source, size);
    }
    else d_outOfBoundsMemOp (destination, size);
}


d_m3Op  (MemFill)
{
    u32 size = (u32) _r0;
    u32 byte = slot (u32);
    u64 destination = slot (u32);

    if (M3_LIKELY(destination + size <= _mem->length) && M3_LIKELY(destination * M3_SHADOW_MEMORY_SCALE + ((u64) size * M3_SHADOW_MEMORY_SCALE) <= _smem->length))
    {
        u8 * mem8 = m3MemData (_mem) + destination;
        u8 * s_mem8 = m3MemData (_smem) + (destination * M3_SHADOW_MEMORY_SCALE);
        memset (mem8, (u8) byte, size);
        memset (s_mem8, 0x0, (size * M3_SHADOW_MEMORY_SCALE));
        nextOp ();
    }
    else d_outOfBoundsMemOp (destination, size);
}


// it's a debate: should the compilation be trigger be the caller or callee page.
// it's a much easier to put it in the caller pager. if it's in the callee, either the entire page
// has be left dangling or it's just a stub that jumps to a newly acquired page.  In Gestalt, I opted
// for the stub approach. Stubbing makes it easier to dynamically free the compilation. You can also
// do both.
d_m3Op  (Compile)
{
    rewrite_op (op_Call);

    IM3Function function        = immediate (IM3Function);

    m3ret_t result = m3Err_none;

    if (M3_UNLIKELY(not function->compiled)) // check to see if function was compiled since this operation was emitted.
        result = CompileFunction (function);

    if (not result)
    {
        // patch up compiled pc and call rewritten op_Call
        * ((void**) --_pc) = (void*) (function->compiled);
        --_pc;
        nextOpDirect ();
    }

    newTrap (result);
}



d_m3Op  (Entry)
{
    d_m3ClearRegisters

    d_m3TracePrepare

    IM3Function function = immediate (IM3Function);
    IM3Memory memory = m3MemInfo (_mem);
    IM3Memory s_memory = m3MemInfo_new (_mem);

#if d_m3SkipStackCheck
    if (true)
#else
    if (M3_LIKELY ((void *) (_sp + function->maxStackSlots) < _mem->maxStack) &&
        M3_LIKELY ((void *) (_ssp + (function->maxStackSlots * M3_SHADOW_STACK_SLOT_SCALE)) < _smem->maxStack))
#endif
    {
#if defined(DEBUG)
        function->hits++;
#endif
        u8 * stack = (u8 *) ((m3slot_t *) _sp + function->numRetAndArgSlots);
        u8 * s_stack = (u8 *) ((m3slot_t *) _ssp + (function->numRetAndArgSlots * M3_SHADOW_STACK_SLOT_SCALE));
        memset (stack, 0x0, function->numLocalBytes);

#if defined(USE_MPFR) || defined(USE_MPFR_DD)
        size_t numLocalSlots = (function->numLocalBytes * M3_SHADOW_STACK_SLOT_SCALE) / sizeof(mpfr_ptr);
        for (size_t i = 0; i < numLocalSlots; ++i) {
            ((mpfr_ptr *)s_stack)[i] = NULL;
        }
#elif defined(USE_QD)
        size_t numLocalSlots = (function->numLocalBytes * M3_SHADOW_STACK_SLOT_SCALE) / sizeof(f64*);
        for (size_t i = 0; i < numLocalSlots; ++i) {
            ((f64 **)s_stack)[i] = NULL;
        }
#elif defined(USE_EID)
        memset (s_stack, 0x0, (function->numLocalBytes * M3_SHADOW_STACK_SLOT_SCALE));
#elif defined(USE_DD) || defined(USE_EFTSAN)
        memset (s_stack, 0x0, (function->numLocalBytes * M3_SHADOW_STACK_SLOT_SCALE));
#else
#endif

        stack += function->numLocalBytes;
        s_stack += (function->numLocalBytes * M3_SHADOW_STACK_SLOT_SCALE);

        if (function->constants)
        {
            memcpy (stack, function->constants, function->numConstantBytes);

#if defined(USE_MPFR) || defined(USE_MPFR_DD)
            size_t numConstSlots = (function->numConstantBytes * M3_SHADOW_STACK_SLOT_SCALE) / sizeof(mpfr_ptr);
            for (size_t i = 0; i < numConstSlots; ++i) {
                ((mpfr_ptr *)s_stack)[i] = NULL;
            }
#elif defined(USE_QD)
            size_t numConstSlots = (function->numConstantBytes * M3_SHADOW_STACK_SLOT_SCALE) / sizeof(f64*);
            for (size_t i = 0; i < numConstSlots; ++i) {
                ((f64 **)s_stack)[i] = NULL;
            }
#elif defined(USE_EID)
            memset (s_stack, 0x0, (function->numConstantBytes * M3_SHADOW_STACK_SLOT_SCALE));
#elif defined(USE_DD) || defined(USE_EFTSAN)
            memset (s_stack, 0x0, (function->numConstantBytes * M3_SHADOW_STACK_SLOT_SCALE));
#else
#endif

        }

#if d_m3EnableStrace >= 2
        d_m3TracePrint("%s %s {", m3_GetFunctionName(function), SPrintFunctionArgList (function, _sp + function->numRetSlots));
        trace_rt->callDepth++;
#endif

        m3ret_t r = nextOpImpl ();

#if d_m3EnableStrace >= 2
        trace_rt->callDepth--;

        if (r) {
            d_m3TracePrint("} !trap = %s", (char*)r);
        } else {
            int rettype = GetSingleRetType(function->funcType);
            if (rettype != c_m3Type_none) {
                char str [128] = { 0 };
                SPrintArg (str, 127, _sp, rettype);
                d_m3TracePrint("} = %s", str);
            } else {
                d_m3TracePrint("}");
            }
        }
#endif

        if (M3_UNLIKELY(r)) {
            _mem = memory->mallocated;
            _smem = s_memory->mallocated;
            fillBacktraceFrame ();
        }
        forwardTrap (r);
    }
    else newTrap (m3Err_trapStackOverflow);
}


d_m3Op  (Loop)
{
    d_m3TracePrepare

    // regs are unused coming into a loop anyway
    // this reduces code size & stack usage
    d_m3ClearRegisters

    m3ret_t r;

    IM3Memory memory = m3MemInfo (_mem);
    IM3Memory s_memory = m3MemInfo_new (_mem);

    do
    {
#if d_m3EnableStrace >= 3
        d_m3TracePrint("iter {");
        trace_rt->callDepth++;
#endif
        r = nextOpImpl ();

#if d_m3EnableStrace >= 3
        trace_rt->callDepth--;
        d_m3TracePrint("}");
#endif
        // linear memory pointer needs refreshed here because the block it's looping over
        // can potentially invoke the grow operation.
        _mem = memory->mallocated;
        _smem = s_memory->mallocated;
    }
    while (r == _pc);

    forwardTrap (r);
}


d_m3Op  (Branch)
{
    jumpOp (* _pc);
}


d_m3Op  (If_r)
{
    i32 condition = (i32) _r0;

    pc_t elsePC = immediate (pc_t);

    if (condition)
        nextOp ();
    else
        jumpOp (elsePC);
}


d_m3Op  (If_s)
{
    i32 condition = slot (i32);

    pc_t elsePC = immediate (pc_t);

    if (condition)
        nextOp ();
    else
        jumpOp (elsePC);
}


d_m3Op  (BranchTable)
{
    u32 branchIndex = slot (u32);           // branch index is always in a slot
    u32 numTargets  = immediate (u32);

    pc_t * branches = (pc_t *) _pc;

    if (branchIndex > numTargets)
        branchIndex = numTargets; // the default index

    jumpOp (branches [branchIndex]);
}


#define d_m3SetRegisterSetSlot(TYPE, REG) \
d_m3Op  (SetRegister_##TYPE)            \
{                                       \
    REG = slot (TYPE);                  \
    nextOp ();                          \
}                                       \
                                        \
d_m3Op (SetSlot_##TYPE)                 \
{                                       \
    slot (TYPE) = (TYPE) REG;           \
    nextOp ();                          \
}                                       \
                                        \
d_m3Op (PreserveSetSlot_##TYPE)         \
{                                       \
    TYPE * stack     = slot_ptr (TYPE); \
    TYPE * preserve  = slot_ptr (TYPE); \
                                        \
    * preserve = * stack;               \
    * stack = (TYPE) REG;               \
                                        \
    nextOp ();                          \
}

#if defined(USE_MPFR) || defined(USE_MPFR_DD)

#define d_m3SetRegisterSetSlot_new(TYPE, REG_V, REG_E) \
d_m3Op  (SetRegister_##TYPE)            \
{                                       \
    REG_V = slot (TYPE);                \
    REG_E = slot_new (mpfr_ptr);        \
    nextOp ();                          \
}                                       \
                                        \
d_m3Op (SetSlot_##TYPE)                 \
{                                       \
    slot (TYPE) = (TYPE) REG_V;         \
    slot_new (mpfr_ptr) = (mpfr_ptr) REG_E;           \
    nextOp ();                          \
}                                       \
                                        \
d_m3Op (PreserveSetSlot_##TYPE)         \
{                                       \
    TYPE * stack = slot_ptr (TYPE);     \
    mpfr_ptr * stack_s = slot_ptr_new (mpfr_ptr);     \
    TYPE * preserve = slot_ptr (TYPE);  \
    mpfr_ptr * preserve_s = slot_ptr_new (mpfr_ptr);  \
    * preserve = * stack;               \
    * preserve_s = * stack_s;           \
    * stack = (TYPE) REG_V;             \
    * stack_s = (mpfr_ptr) REG_E;       \
    nextOp ();                          \
}

#elif defined(USE_QD)

#define d_m3SetRegisterSetSlot_new(TYPE, REG_V, REG_E) \
d_m3Op  (SetRegister_##TYPE)            \
{                                       \
    REG_V = slot (TYPE);                \
    REG_E = slot_new (f64*);            \
    nextOp ();                          \
}                                       \
                                        \
d_m3Op (SetSlot_##TYPE)                 \
{                                       \
    slot (TYPE) = (TYPE) REG_V;         \
    slot_new (f64*) = (f64*) REG_E;     \
    nextOp ();                          \
}                                       \
                                        \
d_m3Op (PreserveSetSlot_##TYPE)         \
{                                       \
    TYPE * stack = slot_ptr (TYPE);     \
    f64* * stack_s = slot_ptr_new (f64*);     \
    TYPE * preserve = slot_ptr (TYPE);  \
    f64* * preserve_s = slot_ptr_new (f64*);  \
    * preserve = * stack;               \
    * preserve_s = * stack_s;           \
    * stack = (TYPE) REG_V;             \
    * stack_s = (f64*) REG_E;           \
    nextOp ();                          \
}

#elif defined(USE_EID)

#define d_m3SetRegisterSetSlot_new(TYPE, REG_V, REG_E) \
d_m3Op  (SetRegister_##TYPE)            \
{                                       \
    i32 offset = immediate (i32);       \
    TYPE * stack_v = (TYPE *) (_sp + offset); \
    eid_t stack_e = (eid_t) (_ssp + (offset * M3_SHADOW_STACK_SLOT_SCALE)); \
    REG_V = *stack_v;                   \
    REG_E = DEFAULT_EID_REGISTER;       \
    *(REG_E) = *stack_e;                \
    nextOp ();                          \
}                                       \
                                        \
d_m3Op (SetSlot_##TYPE)                 \
{                                       \
    i32 offset = immediate (i32);       \
    TYPE * stack_v = (TYPE *) (_sp + offset); \
    eid_t stack_e = (eid_t) (_ssp + (offset * M3_SHADOW_STACK_SLOT_SCALE)); \
    *stack_v = (TYPE) REG_V;            \
    *stack_e = *(REG_E);                \
    nextOp ();                          \
}                                       \
                                        \
d_m3Op (PreserveSetSlot_##TYPE)         \
{                                       \
    i32 stack_offset = immediate (i32); \
    TYPE * stack = (TYPE *) (_sp + stack_offset); \
    eid_t stack_s = (eid_t) (_ssp + (stack_offset * M3_SHADOW_STACK_SLOT_SCALE)); \
    i32 preserve_offset = immediate (i32); \
    TYPE * preserve = (TYPE *) (_sp + preserve_offset); \
    eid_t preserve_s = (eid_t) (_ssp + (preserve_offset * M3_SHADOW_STACK_SLOT_SCALE)); \
    * preserve = * stack;               \
    * preserve_s = * stack_s;           \
    * stack = (TYPE) REG_V;             \
    * stack_s = *(REG_E);               \
    nextOp ();                          \
}

#elif defined(USE_DD) || defined(USE_EFTSAN)

#define d_m3SetRegisterSetSlot_new(TYPE, REG_V, REG_E) \
d_m3Op  (SetRegister_##TYPE)            \
{                                       \
    REG_V = slot (TYPE);                \
    REG_E = slot_new (f64);             \
    nextOp ();                          \
}                                       \
                                        \
d_m3Op (SetSlot_##TYPE)                 \
{                                       \
    slot (TYPE) = (TYPE) REG_V;         \
    slot_new (f64) = (f64) REG_E;       \
    nextOp ();                          \
}                                       \
                                        \
d_m3Op (PreserveSetSlot_##TYPE)         \
{                                       \
    TYPE * stack = slot_ptr (TYPE);     \
    f64 * stack_s = slot_ptr_new (f64); \
    TYPE * preserve = slot_ptr (TYPE);  \
    f64 * preserve_s = slot_ptr_new (f64);           \
    * preserve = * stack;               \
    * preserve_s = * stack_s;           \
    * stack = (TYPE) REG_V;             \
    * stack_s = (f64) REG_E;            \
    nextOp ();                          \
}

#else
#endif

d_m3SetRegisterSetSlot (i32, _r0)
d_m3SetRegisterSetSlot (i64, _r0)
#if d_m3HasFloat

#if d_m3HasErrorSlot
d_m3SetRegisterSetSlot_new (f32, _fp0, _fp1)
d_m3SetRegisterSetSlot_new (f64, _fp0, _fp1)
#else
d_m3SetRegisterSetSlot (f32, _fp0)
d_m3SetRegisterSetSlot (f64, _fp0)
#endif

#endif

d_m3Op (CopySlot_32)
{
    i32 dst_offset = immediate (i32);
    u32 * dst_v = (u32 *) (_sp + dst_offset);
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    u8 * dst_e = (u8 *) (_ssp + (dst_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #else
    u64 * dst_e = (u64 *) (_ssp + (dst_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #endif
    #endif
    i32 src_offset = immediate (i32);
    u32 * src_v = (u32 *) (_sp + src_offset);
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    u8 * src_e = (u8 *) (_ssp + (src_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #else
    u64 * src_e = (u64 *) (_ssp + (src_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #endif
    #endif

    * dst_v = * src_v;
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    memcpy (dst_e, src_e, sizeof (eid));
    #else
    * dst_e = * src_e;
    #endif
    #endif

    nextOp ();
}


d_m3Op (PreserveCopySlot_32)
{
    i32 dest_offset = immediate (i32);
    u32 * dest_v      = (u32 *) (_sp + dest_offset);
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    u8 * dest_e      = (u8 *) (_ssp + (dest_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #else
    u64 * dest_e      = (u64 *) (_ssp + (dest_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #endif
    #endif
    i32 src_offset = immediate (i32);
    u32 * src_v       = (u32 *) (_sp + src_offset);
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    u8 * src_e       = (u8 *) (_ssp + (src_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #else
    u64 * src_e       = (u64 *) (_ssp + (src_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #endif
    #endif
    i32 preserve_offset = immediate (i32);
    u32 * preserve_v  = (u32 *) (_sp + preserve_offset);
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    u8 * preserve_e  = (u8 *) (_ssp + (preserve_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #else
    u64 * preserve_e  = (u64 *) (_ssp + (preserve_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #endif
    #endif

    * preserve_v = * dest_v;
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    memcpy (preserve_e, dest_e, sizeof (eid));
    #else
    * preserve_e = * dest_e;
    #endif
    #endif
    * dest_v = * src_v;
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    memcpy (dest_e, src_e, sizeof (eid));
    #else
    * dest_e = * src_e;
    #endif
    #endif

    nextOp ();
}


d_m3Op (CopySlot_64)
{
    i32 dst_offset = immediate (i32);
    u64 * dst_v = (u64 *) (_sp + dst_offset);
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    u8 * dst_e = (u8 *) (_ssp + (dst_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #else
    u64 * dst_e = (u64 *) (_ssp + (dst_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #endif
    #endif
    i32 src_offset = immediate (i32);
    u64 * src_v = (u64 *) (_sp + src_offset);
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    u8 * src_e = (u8 *) (_ssp + (src_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #else
    u64 * src_e = (u64 *) (_ssp + (src_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #endif
    #endif

    * dst_v = * src_v;                  // printf ("copy: %p <- %" PRIi64 " <- %p\n", dst, * dst, src);
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    memcpy (dst_e, src_e, sizeof (eid));
    #else
    * dst_e = * src_e;
    #endif
    #endif
    nextOp ();
}


d_m3Op (PreserveCopySlot_64)
{
    i32 dest_offset = immediate (i32);
    u64 * dest_v      = (u64 *) (_sp + dest_offset);
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    u8 * dest_e      = (u8 *) (_ssp + (dest_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #else
    u64 * dest_e      = (u64 *) (_ssp + (dest_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #endif
    #endif
    i32 src_offset = immediate (i32);
    u64 * src_v       = (u64 *) (_sp + src_offset);
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    u8 * src_e       = (u8 *) (_ssp + (src_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #else
    u64 * src_e       = (u64 *) (_ssp + (src_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #endif
    #endif
    i32 preserve_offset = immediate (i32);
    u64 * preserve_v  = (u64 *) (_sp + preserve_offset);
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    u8 * preserve_e  = (u8 *) (_ssp + (preserve_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #else
    u64 * preserve_e  = (u64 *) (_ssp + (preserve_offset * M3_SHADOW_STACK_SLOT_SCALE));
    #endif
    #endif

    * preserve_v = * dest_v;
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    memcpy (preserve_e, dest_e, sizeof (eid));
    #else
    * preserve_e = * dest_e;
    #endif
    #endif
    * dest_v = * src_v;
    #if d_m3HasErrorSlot
    #if defined(USE_EID)
    memcpy (dest_e, src_e, sizeof (eid));
    #else
    * dest_e = * src_e;
    #endif
    #endif

    nextOp ();
}


#if d_m3EnableOpTracing
//--------------------------------------------------------------------------------------------------------
d_m3Op  (DumpStack)
{
    u32 opcodeIndex         = immediate (u32);
    u32 stackHeight         = immediate (u32);
    IM3Function function    = immediate (IM3Function);

    cstr_t funcName = (function) ? m3_GetFunctionName(function) : "";

    printf (" %4d ", opcodeIndex);
    printf (" %-25s     r0: 0x%016" PRIx64 "  i:%" PRIi64 "  u:%" PRIu64 "\n", funcName, _r0, _r0, _r0);
#if d_m3HasFloat
    printf ("                                    fp0: %" PRIf64 "\n", _fp0);
#endif
    m3stack_t sp = _sp;

    for (u32 i = 0; i < stackHeight; ++i)
    {
        cstr_t kind = "";

        printf ("%p  %5s  %2d: 0x%" PRIx64 "  i:%" PRIi64 "\n", sp, kind, i, (u64) *(sp), (i64) *(sp));

        ++sp;
    }
    printf ("---------------------------------------------------------------------------------------------------------\n");

    nextOpDirect();
}
#endif


#define d_m3Select_i(TYPE, REG)                 \
d_m3Op  (Select_##TYPE##_rss)                   \
{                                               \
    i32 condition = (i32) _r0;                  \
                                                \
    TYPE operand2 = slot (TYPE);                \
    TYPE operand1 = slot (TYPE);                \
                                                \
    REG = (condition) ? operand1 : operand2;    \
                                                \
    nextOp ();                                  \
}                                               \
                                                \
d_m3Op  (Select_##TYPE##_srs)                   \
{                                               \
    i32 condition = slot (i32);                 \
                                                \
    TYPE operand2 = (TYPE) REG;                 \
    TYPE operand1 = slot (TYPE);                \
                                                \
    REG = (condition) ? operand1 : operand2;    \
                                                \
    nextOp ();                                  \
}                                               \
                                                \
d_m3Op  (Select_##TYPE##_ssr)                   \
{                                               \
    i32 condition = slot (i32);                 \
                                                \
    TYPE operand2 = slot (TYPE);                \
    TYPE operand1 = (TYPE) REG;                 \
                                                \
    REG = (condition) ? operand1 : operand2;    \
                                                \
    nextOp ();                                  \
}                                               \
                                                \
d_m3Op  (Select_##TYPE##_sss)                   \
{                                               \
    i32 condition = slot (i32);                 \
                                                \
    TYPE operand2 = slot (TYPE);                \
    TYPE operand1 = slot (TYPE);                \
                                                \
    REG = (condition) ? operand1 : operand2;    \
                                                \
    nextOp ();                                  \
}


d_m3Select_i (i32, _r0)
d_m3Select_i (i64, _r0)


#define d_m3Select_f(TYPE, REG, LABEL, SELECTOR)  \
d_m3Op  (Select_##TYPE##_##LABEL##ss)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2 = slot (TYPE);                \
    TYPE operand1 = slot (TYPE);                \
                                                \
    REG = (condition) ? operand1 : operand2;    \
                                                \
    nextOp ();                                  \
}                                               \
                                                \
d_m3Op  (Select_##TYPE##_##LABEL##rs)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2 = (TYPE) REG;                 \
    TYPE operand1 = slot (TYPE);                \
                                                \
    REG = (condition) ? operand1 : operand2;    \
                                                \
    nextOp ();                                  \
}                                               \
                                                \
d_m3Op  (Select_##TYPE##_##LABEL##sr)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2 = slot (TYPE);                \
    TYPE operand1 = (TYPE) REG;                 \
                                                \
    REG = (condition) ? operand1 : operand2;    \
                                                \
    nextOp ();                                  \
}

#if defined(USE_MPFR) || defined(USE_MPFR_DD)

#define d_m3Select_f_new(TYPE, REG_V, REG_E, LABEL, SELECTOR)  \
d_m3Op  (Select_##TYPE##_##LABEL##ss)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2_v = slot (TYPE);              \
    mpfr_ptr operand2_e = slot_new (mpfr_ptr);  \
    TYPE operand1_v = slot (TYPE);              \
    mpfr_ptr operand1_e = slot_new (mpfr_ptr);  \
                                                \
    REG_V = (condition) ?                       \
             operand1_v : operand2_v;           \
    REG_E = (condition) ?                       \
             operand1_e : operand2_e;           \
                                                \
    nextOp ();                                  \
}                                               \
                                                \
d_m3Op  (Select_##TYPE##_##LABEL##rs)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2_v = (TYPE) REG_V;             \
    mpfr_ptr operand2_e = (mpfr_ptr) REG_E;     \
    TYPE operand1_v = slot (TYPE);              \
    mpfr_ptr operand1_e = slot_new (mpfr_ptr);  \
                                                \
    REG_V = (condition) ?                       \
             operand1_v : operand2_v;           \
    REG_E = (condition) ?                       \
             operand1_e : operand2_e;           \
                                                \
    nextOp ();                                  \
}                                               \
                                                \
d_m3Op  (Select_##TYPE##_##LABEL##sr)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2_v = slot (TYPE);              \
    mpfr_ptr operand2_e = slot_new (mpfr_ptr);  \
    TYPE operand1_v = (TYPE) REG_V;             \
    mpfr_ptr operand1_e = (mpfr_ptr) REG_E;     \
                                                \
    REG_V = (condition) ?                       \
             operand1_v : operand2_v;           \
    REG_E = (condition) ?                       \
             operand1_e : operand2_e;           \
                                                \
    nextOp ();                                  \
}

#elif defined(USE_QD)

#define d_m3Select_f_new(TYPE, REG_V, REG_E, LABEL, SELECTOR)  \
d_m3Op  (Select_##TYPE##_##LABEL##ss)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2_v = slot (TYPE);              \
    f64* operand2_e = slot_new (f64*);          \
    TYPE operand1_v = slot (TYPE);              \
    f64* operand1_e = slot_new (f64*);          \
                                                \
    REG_V = (condition) ?                       \
             operand1_v : operand2_v;           \
    REG_E = (condition) ?                       \
             operand1_e : operand2_e;           \
                                                \
    nextOp ();                                  \
}                                               \
                                                \
d_m3Op  (Select_##TYPE##_##LABEL##rs)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2_v = (TYPE) REG_V;             \
    f64* operand2_e = (f64*) REG_E;             \
    TYPE operand1_v = slot (TYPE);              \
    f64* operand1_e = slot_new (f64*);          \
                                                \
    REG_V = (condition) ?                       \
             operand1_v : operand2_v;           \
    REG_E = (condition) ?                       \
             operand1_e : operand2_e;           \
                                                \
    nextOp ();                                  \
}                                               \
                                                \
d_m3Op  (Select_##TYPE##_##LABEL##sr)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2_v = slot (TYPE);              \
    f64* operand2_e = slot_new (f64*);          \
    TYPE operand1_v = (TYPE) REG_V;             \
    f64* operand1_e = (f64*) REG_E;             \
                                                \
    REG_V = (condition) ?                       \
             operand1_v : operand2_v;           \
    REG_E = (condition) ?                       \
             operand1_e : operand2_e;           \
                                                \
    nextOp ();                                  \
}

#elif defined(USE_EID)

#define d_m3Select_f_new(TYPE, REG_V, REG_E, LABEL, SELECTOR)  \
d_m3Op  (Select_##TYPE##_##LABEL##ss)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2_v = slot (TYPE);              \
    eid_t operand2_e = slot_eid();              \
    TYPE operand1_v = slot (TYPE);              \
    eid_t operand1_e = slot_eid();              \
                                                \
    REG_E = DEFAULT_EID_REGISTER;               \
    REG_V = (condition) ?                       \
             operand1_v : operand2_v;           \
    *(REG_E) = (condition) ?                    \
              *operand1_e : *operand2_e;        \
                                                \
    nextOp ();                                  \
}                                               \
                                                \
d_m3Op  (Select_##TYPE##_##LABEL##rs)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2_v = (TYPE) REG_V;             \
    eid operand2_e = *(REG_E);                  \
    TYPE operand1_v = slot (TYPE);              \
    eid_t operand1_e = slot_eid();              \
                                                \
    REG_E = DEFAULT_EID_REGISTER;               \
    REG_V = (condition) ?                       \
             operand1_v : operand2_v;           \
    *(REG_E) = (condition) ?                    \
              *operand1_e : operand2_e;         \
                                                \
    nextOp ();                                  \
}                                               \
                                                \
d_m3Op  (Select_##TYPE##_##LABEL##sr)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2_v = slot (TYPE);              \
    eid_t operand2_e = slot_eid();              \
    TYPE operand1_v = (TYPE) REG_V;             \
    eid operand1_e = *(REG_E);                  \
                                                \
    REG_E = DEFAULT_EID_REGISTER;               \
    REG_V = (condition) ?                       \
             operand1_v : operand2_v;           \
    *(REG_E) = (condition) ?                    \
              operand1_e : *operand2_e;         \
                                                \
    nextOp ();                                  \
}

#elif defined(USE_DD) || defined(USE_EFTSAN)

#define d_m3Select_f_new(TYPE, REG_V, REG_E, LABEL, SELECTOR)  \
d_m3Op  (Select_##TYPE##_##LABEL##ss)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2_v = slot (TYPE);              \
    f64 operand2_e = slot_new (f64);            \
    TYPE operand1_v = slot (TYPE);              \
    f64 operand1_e = slot_new (f64);            \
                                                \
    REG_V = (condition) ?                       \
             operand1_v : operand2_v;           \
    REG_E = (condition) ?                       \
             operand1_e : operand2_e;           \
                                                \
    nextOp ();                                  \
}                                               \
                                                \
d_m3Op  (Select_##TYPE##_##LABEL##rs)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2_v = (TYPE) REG_V;             \
    f64 operand2_e = (f64) REG_E;               \
    TYPE operand1_v = slot (TYPE);              \
    f64 operand1_e = slot_new (f64);            \
                                                \
    REG_V = (condition) ?                       \
             operand1_v : operand2_v;           \
    REG_E = (condition) ?                       \
             operand1_e : operand2_e;           \
                                                \
    nextOp ();                                  \
}                                               \
                                                \
d_m3Op  (Select_##TYPE##_##LABEL##sr)           \
{                                               \
    i32 condition = (i32) SELECTOR;             \
                                                \
    TYPE operand2_v = slot (TYPE);              \
    f64 operand2_e = slot_new (f64);            \
    TYPE operand1_v = (TYPE) REG_V;             \
    f64 operand1_e = (f64) REG_E;               \
                                                \
    REG_V = (condition) ?                       \
             operand1_v : operand2_v;           \
    REG_E = (condition) ?                       \
             operand1_e : operand2_e;           \
                                                \
    nextOp ();                                  \
}

#else
#endif

#if d_m3HasFloat

#if d_m3HasErrorSlot
d_m3Select_f_new (f32, _fp0, _fp1, r, _r0)
d_m3Select_f_new (f32, _fp0, _fp1, s, slot (i32))

d_m3Select_f_new (f64, _fp0, _fp1, r, _r0)
d_m3Select_f_new (f64, _fp0, _fp1, s, slot (i32))
#else
d_m3Select_f (f32, _fp0, r, _r0)
d_m3Select_f (f32, _fp0, s, slot (i32))

d_m3Select_f (f64, _fp0, r, _r0)
d_m3Select_f (f64, _fp0, s, slot (i32))
#endif

#endif

d_m3Op  (Return)
{
    m3StackCheck();
    return m3Err_none;
}


d_m3Op  (BranchIf_r)
{
    i32 condition   = (i32) _r0;
    pc_t branch     = immediate (pc_t);

    if (condition)
    {
        jumpOp (branch);
    }
    else nextOp ();
}


d_m3Op  (BranchIf_s)
{
    i32 condition   = slot (i32);
    pc_t branch     = immediate (pc_t);

    if (condition)
    {
        jumpOp (branch);
    }
    else nextOp ();
}


d_m3Op  (BranchIfPrologue_r)
{
    i32 condition   = (i32) _r0;
    pc_t branch     = immediate (pc_t);

    if (condition)
    {
        // this is the "prologue" that ends with
        // a plain branch to the actual target
        nextOp ();
    }
    else jumpOp (branch); // jump over the prologue
}


d_m3Op  (BranchIfPrologue_s)
{
    i32 condition   = slot (i32);
    pc_t branch     = immediate (pc_t);

    if (condition)
    {
        nextOp ();
    }
    else jumpOp (branch);
}


d_m3Op  (ContinueLoop)
{
    m3StackCheck();

    // TODO: this is where execution can "escape" the M3 code and callback to the client / fiber switch
    // OR it can go in the Loop operation. I think it's best to do here. adding code to the loop operation
    // has the potential to increase its native-stack usage. (don't forget ContinueLoopIf too.)

    void * loopId = immediate (void *);
    return loopId;
}


d_m3Op  (ContinueLoopIf)
{
    i32 condition = (i32) _r0;
    void * loopId = immediate (void *);

    if (condition)
    {
        return loopId;
    }
    else nextOp ();
}


d_m3Op  (Const32)
{
    u32 value = * (u32 *)_pc++;
    u32* value_ptr = slot_ptr(u32);
    *value_ptr = value;
#if defined(USE_EID)
    memset (slot_ptr_new (u8), 0x0, sizeof (eid));
#else
    u64* error_ptr = slot_ptr_new(u64);
    *error_ptr = 0;
#endif
    nextOp ();
}


d_m3Op  (Const64)
{
    u64 value = * (u64 *)_pc;
    _pc += (M3_SIZEOF_PTR == 4) ? 2 : 1;
    u64* value_ptr = slot_ptr(u64);
    *value_ptr = value;
#if defined(USE_EID)
    memset (slot_ptr_new (u8), 0x0, sizeof (eid));
#else
    u64* error_ptr = slot_ptr_new(u64);
    *error_ptr = 0;
#endif
    nextOp ();
}

d_m3Op  (Unsupported)
{
    newTrap ("unsupported instruction executed");
}

d_m3Op  (Unreachable)
{
    m3StackCheck();
    newTrap (m3Err_trapUnreachable);
}


d_m3Op  (End)
{
    m3StackCheck();
    return m3Err_none;
}


d_m3Op  (SetGlobal_s32)
{
    u32 * global = immediate (u32 *);
    * global = slot (u32);

    nextOp ();
}


d_m3Op  (SetGlobal_s64)
{
    u64 * global = immediate (u64 *);
    * global = slot (u64);

    nextOp ();
}

#if d_m3HasFloat
d_m3Op  (SetGlobal_f32)
{
    f32 * global = immediate (f32 *);
    * global = _fp0;

    nextOp ();
}


d_m3Op  (SetGlobal_f64)
{
    f64 * global = immediate (f64 *);
    * global = _fp0;

    nextOp ();
}
#endif


#if d_m3SkipMemoryBoundsCheck
#  define m3MemCheck(x) true
#else
#  define m3MemCheck(x) M3_LIKELY(x)
#endif

// memcpy here is to support non-aligned access on some platforms.

#define d_m3Load(REG,DEST_TYPE,SRC_TYPE)                \
d_m3Op(DEST_TYPE##_Load_##SRC_TYPE##_r)                 \
{                                                       \
    d_m3TracePrepare                                    \
    u32 offset = immediate (u32);                       \
    u64 operand = (u32) _r0;                            \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (SRC_TYPE) <= _mem->length     \
    )) {                                                \
        {                                               \
            u8* src8 = m3MemData(_mem) + operand;       \
            SRC_TYPE value;                             \
            memcpy(&value, src8, sizeof(value));        \
            M3_BSWAP_##SRC_TYPE(value);                 \
            REG = (DEST_TYPE)value;                     \
            d_m3TraceLoad(DEST_TYPE, operand, REG);     \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op(DEST_TYPE##_Load_##SRC_TYPE##_s)                 \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (SRC_TYPE) <= _mem->length     \
    )) {                                                \
        {                                               \
            u8* src8 = m3MemData(_mem) + operand;       \
            SRC_TYPE value;                             \
            memcpy(&value, src8, sizeof(value));        \
            M3_BSWAP_##SRC_TYPE(value);                 \
            REG = (DEST_TYPE)value;                     \
            d_m3TraceLoad(DEST_TYPE, operand, REG);     \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

#if defined(USE_MPFR) || defined(USE_MPFR_DD)

#define d_m3Load_new(REG_V,REG_E,DEST_TYPE,SRC_TYPE)    \
d_m3Op(DEST_TYPE##_Load_##SRC_TYPE##_r)                 \
{                                                       \
    d_m3TracePrepare                                    \
    u32 offset = immediate (u32);                       \
    u64 operand = (u32) _r0;                            \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (SRC_TYPE) <= _mem->length     \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (mpfr_ptr) <= _smem->length  \
    )) {                                                \
        {                                               \
            u8* src8 = m3MemData(_mem) + operand;       \
            u8* src8_e = m3MemData(_smem) + operand*2;  \
            SRC_TYPE value;                             \
            mpfr_ptr error;                             \
            memcpy(&value, src8, sizeof(value));        \
            memcpy(&error, src8_e, sizeof(error));      \
            M3_BSWAP_##SRC_TYPE(value);                 \
            M3_BSWAP_u64(error);                        \
            REG_V = (DEST_TYPE)value;                   \
            REG_E = (mpfr_ptr)error;                    \
            d_m3TraceLoad(DEST_TYPE, operand, REG_V);   \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op(DEST_TYPE##_Load_##SRC_TYPE##_s)                 \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (SRC_TYPE) <= _mem->length     \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (mpfr_ptr) <= _smem->length  \
    )) {                                                \
        {                                               \
            u8* src8 = m3MemData(_mem) + operand;       \
            u8* src8_e = m3MemData(_smem) + operand*2;  \
            SRC_TYPE value;                             \
            mpfr_ptr error;                             \
            memcpy(&value, src8, sizeof(value));        \
            memcpy(&error, src8_e, sizeof(error));      \
            M3_BSWAP_##SRC_TYPE(value);                 \
            M3_BSWAP_u64(error);                        \
            REG_V = (DEST_TYPE)value;                   \
            REG_E = (mpfr_ptr)error;                    \
            d_m3TraceLoad(DEST_TYPE, operand, REG_V);   \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

#elif defined(USE_QD)

#define d_m3Load_new(REG_V,REG_E,DEST_TYPE,SRC_TYPE)    \
d_m3Op(DEST_TYPE##_Load_##SRC_TYPE##_r)                 \
{                                                       \
    d_m3TracePrepare                                    \
    u32 offset = immediate (u32);                       \
    u64 operand = (u32) _r0;                            \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (SRC_TYPE) <= _mem->length     \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (f64*) <= _smem->length      \
    )) {                                                \
        {                                               \
            u8* src8 = m3MemData(_mem) + operand;       \
            u8* src8_e = m3MemData(_smem) + operand*2;  \
            SRC_TYPE value;                             \
            f64* error;                                 \
            memcpy(&value, src8, sizeof(value));        \
            memcpy(&error, src8_e, sizeof(error));      \
            M3_BSWAP_##SRC_TYPE(value);                 \
            M3_BSWAP_u64(error);                        \
            REG_V = (DEST_TYPE)value;                   \
            REG_E = (f64*)error;                        \
            d_m3TraceLoad(DEST_TYPE, operand, REG_V);   \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op(DEST_TYPE##_Load_##SRC_TYPE##_s)                 \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (SRC_TYPE) <= _mem->length     \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (f64*) <= _smem->length      \
    )) {                                                \
        {                                               \
            u8* src8 = m3MemData(_mem) + operand;       \
            u8* src8_e = m3MemData(_smem) + operand*2;  \
            SRC_TYPE value;                             \
            f64* error;                                 \
            memcpy(&value, src8, sizeof(value));        \
            memcpy(&error, src8_e, sizeof(error));      \
            M3_BSWAP_##SRC_TYPE(value);                 \
            M3_BSWAP_u64(error);                        \
            REG_V = (DEST_TYPE)value;                   \
            REG_E = (f64*)error;                        \
            d_m3TraceLoad(DEST_TYPE, operand, REG_V);   \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

#elif defined(USE_EID)

#define d_m3Load_new(REG_V,REG_E,DEST_TYPE,SRC_TYPE)    \
d_m3Op(DEST_TYPE##_Load_##SRC_TYPE##_r)                 \
{                                                       \
    d_m3TracePrepare                                    \
    u32 offset = immediate (u32);                       \
    u64 operand = (u32) _r0;                            \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (SRC_TYPE) <= _mem->length     \
    ) && m3MemCheck(                                    \
        operand * M3_SHADOW_MEMORY_SCALE + sizeof (eid) <= _smem->length \
    )) {                                                \
        {                                               \
            u8* src8 = m3MemData(_mem) + operand;       \
            u8* src8_e = m3MemData(_smem) + operand * M3_SHADOW_MEMORY_SCALE; \
            SRC_TYPE value;                             \
            memcpy(&value, src8, sizeof(value));        \
            M3_BSWAP_##SRC_TYPE(value);                 \
            REG_E = DEFAULT_EID_REGISTER;               \
            REG_V = (DEST_TYPE)value;                   \
            *(REG_E) = *(eid_t) src8_e;                 \
            d_m3TraceLoad(DEST_TYPE, operand, REG_V);   \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op(DEST_TYPE##_Load_##SRC_TYPE##_s)                 \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (SRC_TYPE) <= _mem->length     \
    ) && m3MemCheck(                                    \
        operand * M3_SHADOW_MEMORY_SCALE + sizeof (eid) <= _smem->length \
    )) {                                                \
        {                                               \
            u8* src8 = m3MemData(_mem) + operand;       \
            u8* src8_e = m3MemData(_smem) + operand * M3_SHADOW_MEMORY_SCALE; \
            SRC_TYPE value;                             \
            memcpy(&value, src8, sizeof(value));        \
            M3_BSWAP_##SRC_TYPE(value);                 \
            REG_E = DEFAULT_EID_REGISTER;               \
            REG_V = (DEST_TYPE)value;                   \
            *(REG_E) = *(eid_t) src8_e;                 \
            d_m3TraceLoad(DEST_TYPE, operand, REG_V);   \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

#elif defined(USE_DD) || defined(USE_EFTSAN)

#define d_m3Load_new(REG_V,REG_E,DEST_TYPE,SRC_TYPE)    \
d_m3Op(DEST_TYPE##_Load_##SRC_TYPE##_r)                 \
{                                                       \
    d_m3TracePrepare                                    \
    u32 offset = immediate (u32);                       \
    u64 operand = (u32) _r0;                            \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (SRC_TYPE) <= _mem->length     \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (f64) <= _smem->length       \
    )) {                                                \
        {                                               \
            u8* src8 = m3MemData(_mem) + operand;       \
            u8* src8_e = m3MemData(_smem) + operand*2;  \
            SRC_TYPE value;                             \
            f64 error;                                  \
            memcpy(&value, src8, sizeof(value));        \
            memcpy(&error, src8_e, sizeof(error));      \
            M3_BSWAP_##SRC_TYPE(value);                 \
            M3_BSWAP_f64(error);                        \
            REG_V = (DEST_TYPE)value;                   \
            REG_E = (f64)error;                         \
            d_m3TraceLoad(DEST_TYPE, operand, REG_V);   \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op(DEST_TYPE##_Load_##SRC_TYPE##_s)                 \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (SRC_TYPE) <= _mem->length     \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (f64) <= _smem->length       \
    )) {                                                \
        {                                               \
            u8* src8 = m3MemData(_mem) + operand;       \
            u8* src8_e = m3MemData(_smem) + operand*2;  \
            SRC_TYPE value;                             \
            f64 error;                                  \
            memcpy(&value, src8, sizeof(value));        \
            memcpy(&error, src8_e, sizeof(error));      \
            M3_BSWAP_##SRC_TYPE(value);                 \
            M3_BSWAP_f64(error);                        \
            REG_V = (DEST_TYPE)value;                   \
            REG_E = (f64)error;                         \
            d_m3TraceLoad(DEST_TYPE, operand, REG_V);   \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}
//  printf ("get: %d -> %d\n", operand + offset, (i64) REG);
#else
#endif

#define d_m3Load_i(DEST_TYPE, SRC_TYPE) d_m3Load(_r0, DEST_TYPE, SRC_TYPE)

#if d_m3HasErrorSlot
#define d_m3Load_f(DEST_TYPE, SRC_TYPE) d_m3Load_new(_fp0, _fp1, DEST_TYPE, SRC_TYPE)
#else
#define d_m3Load_f(DEST_TYPE, SRC_TYPE) d_m3Load(_fp0, DEST_TYPE, SRC_TYPE)
#endif

#if d_m3HasFloat
d_m3Load_f (f32, f32);
d_m3Load_f (f64, f64);
#endif

d_m3Load_i (i32, i8);
d_m3Load_i (i32, u8);
d_m3Load_i (i32, i16);
d_m3Load_i (i32, u16);
d_m3Load_i (i32, i32);

d_m3Load_i (i64, i8);
d_m3Load_i (i64, u8);
d_m3Load_i (i64, i16);
d_m3Load_i (i64, u16);
d_m3Load_i (i64, i32);
d_m3Load_i (i64, u32);
d_m3Load_i (i64, i64);

#define d_m3Store(REG, SRC_TYPE, DEST_TYPE)             \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_rs)             \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, REG);     \
            u8* mem8 = m3MemData(_mem) + operand;       \
            DEST_TYPE val = (DEST_TYPE) REG;            \
            M3_BSWAP_##DEST_TYPE(val);                  \
            memcpy(mem8, &val, sizeof(val));            \
            if (d_m3HasErrorSlot) {                     \
                u8* mem8_e = m3MemData(_smem) + operand * M3_SHADOW_MEMORY_SCALE; \
                memset(mem8_e, 0, sizeof(val) * M3_SHADOW_MEMORY_SCALE);     \
            }                                           \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_sr)             \
{                                                       \
    d_m3TracePrepare                                    \
    const SRC_TYPE value = slot (SRC_TYPE);             \
    u64 operand = (u32) _r0;                            \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, value);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            DEST_TYPE val = (DEST_TYPE) value;          \
            M3_BSWAP_##DEST_TYPE(val);                  \
            memcpy(mem8, &val, sizeof(val));            \
            if (d_m3HasErrorSlot) {                     \
                u8* mem8_e = m3MemData(_smem) + operand * M3_SHADOW_MEMORY_SCALE; \
                memset(mem8_e, 0, sizeof(val) * M3_SHADOW_MEMORY_SCALE);     \
            }                                           \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_ss)             \
{                                                       \
    d_m3TracePrepare                                    \
    const SRC_TYPE value = slot (SRC_TYPE);             \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, value);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            DEST_TYPE val = (DEST_TYPE) value;          \
            M3_BSWAP_##DEST_TYPE(val);                  \
            memcpy(mem8, &val, sizeof(val));            \
            if (d_m3HasErrorSlot) {                     \
                u8* mem8_e = m3MemData(_smem) + operand * M3_SHADOW_MEMORY_SCALE; \
                memset(mem8_e, 0, sizeof(val) * M3_SHADOW_MEMORY_SCALE);     \
            }                                           \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

// both operands can be in regs when storing a float
#define d_m3StoreFp(REG, TYPE)                          \
d_m3Op  (TYPE##_Store_##TYPE##_rr)                      \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = (u32) _r0;                            \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (TYPE) <= _mem->length         \
    )) {                                                \
        {                                               \
            d_m3TraceStore(TYPE, operand, REG);         \
            u8* mem8 = m3MemData(_mem) + operand;       \
            TYPE val = (TYPE) REG;                      \
            M3_BSWAP_##TYPE(val);                       \
            memcpy(mem8, &val, sizeof(val));            \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

#if defined(USE_MPFR) || defined(USE_MPFR_DD)

#define d_m3Store_new(REG_V, REG_E, SRC_TYPE, DEST_TYPE)    \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_rs)             \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (mpfr_ptr) <= _smem->length  \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, REG_V);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand*2;  \
            DEST_TYPE val = (DEST_TYPE) REG_V;          \
            mpfr_ptr err = (mpfr_ptr) REG_E;            \
            M3_BSWAP_##DEST_TYPE(val);                  \
            M3_BSWAP_u64(err);                          \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_sr)             \
{                                                       \
    d_m3TracePrepare                                    \
    const SRC_TYPE value = slot (SRC_TYPE);             \
    const mpfr_ptr error = slot_new (mpfr_ptr);         \
    u64 operand = (u32) _r0;                            \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (mpfr_ptr) <= _smem->length  \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, value);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand*2;  \
            DEST_TYPE val = (DEST_TYPE) value;          \
            mpfr_ptr err = (mpfr_ptr) error;            \
            M3_BSWAP_##DEST_TYPE(val);                  \
            M3_BSWAP_u64(err);                          \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_ss)             \
{                                                       \
    d_m3TracePrepare                                    \
    const SRC_TYPE value = slot (SRC_TYPE);             \
    const mpfr_ptr error = slot_new (mpfr_ptr);         \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (mpfr_ptr) <= _smem->length  \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, value);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand*2;  \
            DEST_TYPE val = (DEST_TYPE) value;          \
            mpfr_ptr err = (mpfr_ptr) error;            \
            M3_BSWAP_##DEST_TYPE(val);                  \
            M3_BSWAP_u64(err);                          \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

// both operands can be in regs when storing a float
#define d_m3StoreFp_new(REG_V, REG_E, TYPE)             \
d_m3Op  (TYPE##_Store_##TYPE##_rr)                      \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = (u32) _r0;                            \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (TYPE) <= _mem->length         \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (mpfr_ptr) <= _smem->length  \
    )) {                                                \
        {                                               \
            d_m3TraceStore(TYPE, operand, REG_V);       \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand*2;  \
            TYPE val = (TYPE) REG_V;                    \
            mpfr_ptr err = (mpfr_ptr) REG_E;            \
            M3_BSWAP_##TYPE(val);                       \
            M3_BSWAP_u64(err);                          \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

#elif defined(USE_QD)

#define d_m3Store_new(REG_V, REG_E, SRC_TYPE, DEST_TYPE)    \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_rs)             \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (f64*) <= _smem->length      \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, REG_V);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand*2;  \
            DEST_TYPE val = (DEST_TYPE) REG_V;          \
            f64* err = (f64*) REG_E;                    \
            M3_BSWAP_##DEST_TYPE(val);                  \
            M3_BSWAP_u64(err);                          \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_sr)             \
{                                                       \
    d_m3TracePrepare                                    \
    const SRC_TYPE value = slot (SRC_TYPE);             \
    const f64* error = slot_new (f64*);                 \
    u64 operand = (u32) _r0;                            \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (f64*) <= _smem->length      \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, value);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand*2;  \
            DEST_TYPE val = (DEST_TYPE) value;          \
            f64* err = (f64*) error;                    \
            M3_BSWAP_##DEST_TYPE(val);                  \
            M3_BSWAP_u64(err);                          \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_ss)             \
{                                                       \
    d_m3TracePrepare                                    \
    const SRC_TYPE value = slot (SRC_TYPE);             \
    const f64* error = slot_new (f64*);                 \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (f64*) <= _smem->length      \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, value);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand*2;  \
            DEST_TYPE val = (DEST_TYPE) value;          \
            f64* err = (f64*) error;                    \
            M3_BSWAP_##DEST_TYPE(val);                  \
            M3_BSWAP_u64(err);                          \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

// both operands can be in regs when storing a float
#define d_m3StoreFp_new(REG_V, REG_E, TYPE)             \
d_m3Op  (TYPE##_Store_##TYPE##_rr)                      \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = (u32) _r0;                            \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (TYPE) <= _mem->length         \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (f64*) <= _smem->length      \
    )) {                                                \
        {                                               \
            d_m3TraceStore(TYPE, operand, REG_V);       \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand*2;  \
            TYPE val = (TYPE) REG_V;                    \
            f64* err = (f64*) REG_E;                    \
            M3_BSWAP_##TYPE(val);                       \
            M3_BSWAP_u64(err);                          \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

#elif defined(USE_EID)

#define d_m3Store_new(REG_V, REG_E, SRC_TYPE, DEST_TYPE)    \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_rs)             \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    ) && m3MemCheck(                                    \
        operand * M3_SHADOW_MEMORY_SCALE + sizeof (eid) <= _smem->length \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, REG_V);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand * M3_SHADOW_MEMORY_SCALE; \
            DEST_TYPE val = (DEST_TYPE) REG_V;          \
            eid err = *(REG_E);                         \
            M3_BSWAP_##DEST_TYPE(val);                  \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_sr)             \
{                                                       \
    d_m3TracePrepare                                    \
    const SRC_TYPE value = slot (SRC_TYPE);             \
    const eid error = *slot_eid();                      \
    u64 operand = (u32) _r0;                            \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    ) && m3MemCheck(                                    \
        operand * M3_SHADOW_MEMORY_SCALE + sizeof (eid) <= _smem->length \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, value);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand * M3_SHADOW_MEMORY_SCALE; \
            DEST_TYPE val = (DEST_TYPE) value;          \
            eid err = error;                            \
            M3_BSWAP_##DEST_TYPE(val);                  \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_ss)             \
{                                                       \
    d_m3TracePrepare                                    \
    const SRC_TYPE value = slot (SRC_TYPE);             \
    const eid error = *slot_eid();                      \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    ) && m3MemCheck(                                    \
        operand * M3_SHADOW_MEMORY_SCALE + sizeof (eid) <= _smem->length \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, value);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand * M3_SHADOW_MEMORY_SCALE; \
            DEST_TYPE val = (DEST_TYPE) value;          \
            eid err = error;                            \
            M3_BSWAP_##DEST_TYPE(val);                  \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

// both operands can be in regs when storing a float
#define d_m3StoreFp_new(REG_V, REG_E, TYPE)             \
d_m3Op  (TYPE##_Store_##TYPE##_rr)                      \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = (u32) _r0;                            \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (TYPE) <= _mem->length         \
    ) && m3MemCheck(                                    \
        operand * M3_SHADOW_MEMORY_SCALE + sizeof (eid) <= _smem->length \
    )) {                                                \
        {                                               \
            d_m3TraceStore(TYPE, operand, REG_V);       \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand * M3_SHADOW_MEMORY_SCALE; \
            TYPE val = (TYPE) REG_V;                    \
            eid err = *(REG_E);                         \
            M3_BSWAP_##TYPE(val);                       \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

#elif defined(USE_DD) || defined(USE_EFTSAN)

#define d_m3Store_new(REG_V, REG_E, SRC_TYPE, DEST_TYPE)    \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_rs)             \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (f64) <= _smem->length       \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, REG_V);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand*2;  \
            DEST_TYPE val = (DEST_TYPE) REG_V;          \
            f64 err = (f64) REG_E;                      \
            M3_BSWAP_##DEST_TYPE(val);                  \
            M3_BSWAP_f64(err);                          \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_sr)             \
{                                                       \
    d_m3TracePrepare                                    \
    const SRC_TYPE value = slot (SRC_TYPE);             \
    const f64 error = slot_new (f64);                   \
    u64 operand = (u32) _r0;                            \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (f64) <= _smem->length       \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, value);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand*2;  \
            DEST_TYPE val = (DEST_TYPE) value;          \
            f64 err = (f64) error;                      \
            M3_BSWAP_##DEST_TYPE(val);                  \
            M3_BSWAP_f64(err);                          \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}                                                       \
d_m3Op  (SRC_TYPE##_Store_##DEST_TYPE##_ss)             \
{                                                       \
    d_m3TracePrepare                                    \
    const SRC_TYPE value = slot (SRC_TYPE);             \
    const f64 error = slot_new (f64);                   \
    u64 operand = slot (u32);                           \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (DEST_TYPE) <= _mem->length    \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (f64) <= _smem->length       \
    )) {                                                \
        {                                               \
            d_m3TraceStore(SRC_TYPE, operand, value);   \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand*2;  \
            DEST_TYPE val = (DEST_TYPE) value;          \
            f64 err = (f64) error;                      \
            M3_BSWAP_##DEST_TYPE(val);                  \
            M3_BSWAP_f64(err);                          \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

// both operands can be in regs when storing a float
#define d_m3StoreFp_new(REG_V, REG_E, TYPE)             \
d_m3Op  (TYPE##_Store_##TYPE##_rr)                      \
{                                                       \
    d_m3TracePrepare                                    \
    u64 operand = (u32) _r0;                            \
    u32 offset = immediate (u32);                       \
    operand += offset;                                  \
                                                        \
    if (m3MemCheck(                                     \
        operand + sizeof (TYPE) <= _mem->length         \
    ) && m3MemCheck(                                    \
        operand*2 + sizeof (f64) <= _smem->length       \
    )) {                                                \
        {                                               \
            d_m3TraceStore(TYPE, operand, REG_V);       \
            u8* mem8 = m3MemData(_mem) + operand;       \
            u8* mem8_e = m3MemData(_smem) + operand*2;  \
            TYPE val = (TYPE) REG_V;                    \
            f64 err = (f64) REG_E;                      \
            M3_BSWAP_##TYPE(val);                       \
            M3_BSWAP_f64(err);                          \
            memcpy(mem8, &val, sizeof(val));            \
            memcpy(mem8_e, &err, sizeof(err));          \
        }                                               \
        nextOp ();                                      \
    } else d_outOfBounds;                               \
}

#else
#endif

#define d_m3Store_i(SRC_TYPE, DEST_TYPE) d_m3Store(_r0, SRC_TYPE, DEST_TYPE)

#if d_m3HasErrorSlot
#define d_m3Store_f(SRC_TYPE, DEST_TYPE) d_m3Store_new(_fp0, _fp1, SRC_TYPE, DEST_TYPE) d_m3StoreFp_new(_fp0, _fp1, SRC_TYPE);
#else
#define d_m3Store_f(SRC_TYPE, DEST_TYPE) d_m3Store(_fp0, SRC_TYPE, DEST_TYPE) d_m3StoreFp (_fp0, SRC_TYPE);
#endif

#if d_m3HasFloat
d_m3Store_f (f32, f32)
d_m3Store_f (f64, f64)
#endif

d_m3Store_i (i32, u8)
d_m3Store_i (i32, i16)
d_m3Store_i (i32, i32)

d_m3Store_i (i64, u8)
d_m3Store_i (i64, i16)
d_m3Store_i (i64, i32)
d_m3Store_i (i64, i64)

#undef m3MemCheck


//---------------------------------------------------------------------------------------------------------------------
// debug/profiling
//---------------------------------------------------------------------------------------------------------------------
#if d_m3EnableOpTracing
d_m3RetSig  debugOp  (d_m3OpSig, cstr_t i_opcode)
{
    char name [100];
    strcpy (name, strstr (i_opcode, "op_") + 3);
    char * bracket = strstr (name, "(");
    if (bracket) {
        *bracket  = 0;
    }

    puts (name);
    nextOpDirect();
}
# endif

# if d_m3EnableOpProfiling
d_m3RetSig  profileOp  (d_m3OpSig, cstr_t i_operationName)
{
    ProfileHit (i_operationName);

    nextOpDirect();
}
# endif

d_m3EndExternC

#endif // m3_exec_h
