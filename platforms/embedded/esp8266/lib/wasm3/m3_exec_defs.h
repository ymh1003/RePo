//
//  m3_exec_defs.h
//
//  Created by Steven Massey on 5/1/19.
//  Copyright © 2019 Steven Massey. All rights reserved.
//

#ifndef m3_exec_defs_h
#define m3_exec_defs_h

#include "m3_core.h"
#include "mpfr.h"
#include "qd_library.h"
#include "eid_library.h"

d_m3BeginExternC

# define m3MemData(mem)                 (u8*)(((M3MemoryHeader*)(mem))+1)
# define m3MemRuntime(mem)              (((M3MemoryHeader*)(mem))->runtime)
# define m3MemInfo(mem)                 (&(((M3MemoryHeader*)(mem))->runtime->memory))
# define m3MemInfo_new(mem)             (&(((M3MemoryHeader*)(mem))->runtime->s_memory))
# define d_m3BaseOpSig                  pc_t _pc, m3stack_t _sp, m3stack_t _ssp, M3MemoryHeader * _mem, M3MemoryHeader * _smem, m3reg_t _r0
# define d_m3BaseOpArgs                 _sp, _ssp, _mem, _smem, _r0
# define d_m3BaseOpAllArgs              _pc, _sp, _ssp, _mem, _smem, _r0
# define d_m3BaseOpDefaultArgs          0
# define d_m3BaseClearRegisters         _r0 = 0;
# define d_m3BaseCstr                   ""

# define d_m3ExpOpSig(...)              d_m3BaseOpSig, __VA_ARGS__
# define d_m3ExpOpArgs(...)             d_m3BaseOpArgs, __VA_ARGS__
# define d_m3ExpOpAllArgs(...)          d_m3BaseOpAllArgs, __VA_ARGS__
# define d_m3ExpOpDefaultArgs(...)      d_m3BaseOpDefaultArgs, __VA_ARGS__
# define d_m3ExpClearRegisters(...)     d_m3BaseClearRegisters; __VA_ARGS__
# if d_m3HasFloat
#  if defined(USE_MPFR) || defined(USE_MPFR_DD)
#   define d_m3OpSig                d_m3ExpOpSig            (f64 _fp0, mpfr_ptr _fp1)
#   define d_m3OpArgs               d_m3ExpOpArgs           (_fp0, _fp1)
#   define d_m3OpAllArgs            d_m3ExpOpAllArgs        (_fp0, _fp1)
#   define d_m3OpDefaultArgs        d_m3ExpOpDefaultArgs    (0., NULL)
#   define d_m3ClearRegisters       d_m3ExpClearRegisters   (_fp0 = 0.; _fp1 = NULL;)
#  elif defined(USE_QD)
#   define d_m3OpSig                d_m3ExpOpSig            (f64 _fp0, qd_t _fp1)
#   define d_m3OpArgs               d_m3ExpOpArgs           (_fp0, _fp1)
#   define d_m3OpAllArgs            d_m3ExpOpAllArgs        (_fp0, _fp1)
#   define d_m3OpDefaultArgs        d_m3ExpOpDefaultArgs    (0., NULL)
#   define d_m3ClearRegisters       d_m3ExpClearRegisters   (_fp0 = 0.; _fp1 = NULL;)
# elif defined(USE_EID)
#   define d_m3OpSig                d_m3ExpOpSig            (f64 _fp0, eid_t _fp1)
#   define d_m3OpArgs               d_m3ExpOpArgs           (_fp0, _fp1)
#   define d_m3OpAllArgs            d_m3ExpOpAllArgs        (_fp0, _fp1)
#   define d_m3OpDefaultArgs        d_m3ExpOpDefaultArgs    (0., DEFAULT_EID_REGISTER)
#   define d_m3ClearRegisters       d_m3ExpClearRegisters   (_fp0 = 0.; _fp1 = DEFAULT_EID_REGISTER; *_fp1 = EID_DEFAULT;)
# elif defined(USE_DD) || defined(USE_EFTSAN)
#   define d_m3OpSig                d_m3ExpOpSig            (f64 _fp0, f64 _fp1)
#   define d_m3OpArgs               d_m3ExpOpArgs           (_fp0, _fp1)
#   define d_m3OpAllArgs            d_m3ExpOpAllArgs        (_fp0, _fp1)
#   define d_m3OpDefaultArgs        d_m3ExpOpDefaultArgs    (0., 0.)
#   define d_m3ClearRegisters       d_m3ExpClearRegisters   (_fp0 = 0.; _fp1 = 0.;)
#  else
#   define d_m3OpSig                d_m3ExpOpSig            (f64 _fp0)
#   define d_m3OpArgs               d_m3ExpOpArgs           (_fp0)
#   define d_m3OpAllArgs            d_m3ExpOpAllArgs        (_fp0)
#   define d_m3OpDefaultArgs        d_m3ExpOpDefaultArgs    (0.)
#   define d_m3ClearRegisters       d_m3ExpClearRegisters   (_fp0 = 0.;)
#  endif
# else
#   define d_m3OpSig                d_m3BaseOpSig
#   define d_m3OpArgs               d_m3BaseOpArgs
#   define d_m3OpAllArgs            d_m3BaseOpAllArgs
#   define d_m3OpDefaultArgs        d_m3BaseOpDefaultArgs
#   define d_m3ClearRegisters       d_m3BaseClearRegisters
# endif


#define d_m3RetSig                  static inline m3ret_t vectorcall
# if (d_m3EnableOpProfiling || d_m3EnableOpTracing)
    typedef m3ret_t (vectorcall * IM3Operation) (d_m3OpSig, cstr_t i_operationName);
#    define d_m3Op(NAME)                M3_NO_UBSAN d_m3RetSig op_##NAME (d_m3OpSig, cstr_t i_operationName)

#    define nextOpImpl()            ((IM3Operation)(* _pc))(_pc + 1, d_m3OpArgs, __FUNCTION__)
#    define jumpOpImpl(PC)          ((IM3Operation)(*  PC))( PC + 1, d_m3OpArgs, __FUNCTION__)
# else
    typedef m3ret_t (vectorcall * IM3Operation) (d_m3OpSig);
#    define d_m3Op(NAME)                M3_NO_UBSAN d_m3RetSig op_##NAME (d_m3OpSig)

#    define nextOpImpl()            ((IM3Operation)(* _pc))(_pc + 1, d_m3OpArgs)
#    define jumpOpImpl(PC)          ((IM3Operation)(*  PC))( PC + 1, d_m3OpArgs)
# endif

#define nextOpDirect()              M3_MUSTTAIL return nextOpImpl()
#define jumpOpDirect(PC)            M3_MUSTTAIL return jumpOpImpl((pc_t)(PC))

# if (d_m3EnableOpProfiling || d_m3EnableOpTracing)
d_m3RetSig  RunCode  (d_m3OpSig, cstr_t i_operationName)
# else
d_m3RetSig  RunCode  (d_m3OpSig)
# endif
{
    nextOpDirect();
}

d_m3EndExternC

#endif // m3_exec_defs_h
