// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#ifndef debug_flag_h
#define debug_flag_h

//*****************************************************************************
// File: debug-flag.c
//
// Description:
//   debug flags management for hpcrun
//
// History:
//   23 July 2009 - John Mellor-Crummey
//     created by splitting off from messages.h, messages-sync.c
//
//*****************************************************************************


//*****************************************************************************
// macros
//*****************************************************************************

#define DBG_PREFIX(s) DBG_##s

#define DBG(f)            debug_flag_get(DBG_PREFIX(f))
#define SET(f,v)          debug_flag_set(DBG_PREFIX(f), v)

#define ENABLE(f)         SET(f,1)
#define DISABLE(f)        SET(f,0)

#define ENABLED(f)        DBG(f)
#define DISABLED(f)       (! DBG(f))

#define IF_ENABLED(f)     if ( ENABLED(f) )
#define IF_DISABLED(f)    if ( ! ENABLED(f) )

#define ALL_DBG_FLAGS(E) \
 E(INIT), \
 E(FINI), \
 E(FIND), \
 E(INTV), \
 E(SUSPICIOUS_INTERVAL), \
 E(GENERIC1), /* generic flag, settable & freeable INTERNALLY ONLY */ \
 E(COALESCE), \
 E(ITIMER_HANDLER), \
 E(ITIMER_CTL), \
 E(_TST_CTL), \
 E(_TST_HANDLER), \
 E(NONE_CTL), \
 E(SYNC_CTL), \
 E(UNW), \
 E(LUNW), /* LUSH unwind */ \
 E(LIBUNW_TRACE), /* trace libunwind version of init_cursor */ \
 E(UNW_SEGV_STOP), /* will cause real abort if unw has a segv */ \
 E(UW_RECIPE_MAP), \
 E(UW_RECIPE_MAP_VERIFY), \
 E(UW_RECIPE_MAP_LOOKUP), \
 E(DLOPEN_RISKY), \
 E(SYSCALL_RISKY), \
 E(GA), \
 E(IO), \
 E(MEMLEAK), \
 E(MEMLEAK_NO_HEADER), \
 E(MEMORY), \
 E(MPI_RISKY), \
 E(MPI), \
 E(INTV2), \
 E(INTV_ERR), \
 E(TROLL), \
 E(DROP), \
 E(UNW_INIT), \
 E(BASIC_INIT), \
 E(SAMPLE), \
 E(SAMPLE_CALLPATH), \
 E(SAMPLE_METRIC_DATA), \
 E(USE_TRAMP), \
 E(TRAMP), \
 E(RETCNT_CTL), \
 E(SWIZZLE), \
 E(FINALIZE), \
 E(DATA_WRITE), \
 E(DATA_WRITE_CCT), \
 E(DATA_WRITE_CCT_LEAF_NODE), \
 E(DATA_WRITE_CCT_EX), \
 E(SKIP_WRITE_EMPTY_EPOCH), \
 E(PAPI), \
 E(PAPI_SAMPLE), \
 E(PAPI_EVENT_NAME), \
 E(LINUX_PERF), \
 E(UPC), \
 E(DBG), \
 E(THREAD), \
 E(THREAD_CTXT), \
 E(IN_THREAD_CTXT), \
 E(CCT_CTXT), \
 E(PROCESS), \
 E(LOADMAP), \
 E(EPOCH), \
 E(EPOCH_CHK), \
 E(SINGLE_EPOCH), \
 E(WARN_MULTI_EPOCH), \
 E(FLUSH_EVERY_SAMPLE), \
 E(CCT), \
 E(CCT_FRAME), \
 E(CCT_SPLAY), \
 E(CCT_TYPE), \
 E(UNSAFE), \
 E(MSG_L), \
 E(STATE), \
 E(EPOCH_RESET), \
 E(HANDLING_SAMPLE), \
 E(MEM), \
 E(MEM2), \
 E(SAMPLE_FILTER), \
 E(THREAD_SPECIFIC), \
 E(DSO), \
 E(DL_BOUND), \
 E(ADD_MODULE_BASE), \
 E(DL_ADD_MODULE), \
 E(OPTIONS), \
 E(FORK), \
 E(EVENTS), \
 E(FNBOUNDS), \
 E(FNBOUNDS_EXT), \
 E(FNBOUNDS_CLIENT), \
 E(SS_ALL), \
 E(SS_COMMON), \
 E(SAMPLE_SOURCE), \
 E(METRICS), \
 E(METRIC_INC), \
 E(METRICS_FINALIZE), \
 E(UNW_CONFIG), \
 E(UNW_STRATEGY), \
 E(UNW_STRATEGY_ERROR), \
 E(COLD_CODE), \
 E(BACKTRACE), \
 E(DUMP_BACKTRACES), \
 E(SUSPENDED_SAMPLE), \
 E(MMAP), \
 E(MALLOC), \
 E(CSP_MALLOC), \
 E(MEM__ALLOC), \
 E(FREEABLE), \
 E(SPECIAL), \
 E(NO_SAMPLE_FILTERING), \
 E(DL_BOUND_REDIR), \
 E(DL_BOUND_RETAIN_TMP), \
 E(DL_BOUND_SCRIPT_DEBUG), \
 E(PID), \
 E(TROLL_WAIT), \
 E(PREFER_SP), \
 E(DBG_UNW_STEP), \
 E(DBG_INTV_MEM), \
 E(TST), \
 E(TROLL_CHK), \
 E(NO_TROLLING), \
 E(VALIDATE_UNW), \
 E(ROUTINE_INFO), \
 E(TAIL_CALL), \
 E(UNW_VALID), \
 E(VALID_RECORD_ALL), \
 E(IBS_SAMPLE), \
 E(OMP_SKIP_MSB), \
 E(LUSH), \
 E(LOGICAL_CTX), \
 E(LOGICAL_UNWIND), \
 E(LOGICAL_CTX_PYTHON), \
 E(BT), \
 E(BT_INSERT), \
 E(CUSTOM_INIT), \
 E(NORM_IP), \
 E(NORM_IP_DBG), \
 E(PARTIAL_UNW), \
 E(NO_PARTIAL_UNW), \
 E(DEBUG_PARTIAL_UNW), \
 E(SEGV), \
 E(CCT2METRICS), \
 E(ATTACH_THREAD_CTXT), \
 E(SET_DEFER_WRITE), \
 E(OMPT_TASK_FULL_CTXT), \
 E(OMPT_LOCAL_VIEW), \
 E(OMPT_KEEP_ALL_FRAMES), \
 E(DEFER_CTXT), \
 E(FENCE_UNW), \
 E(FENCE), \
 E(REC_COMPRESS), \
 E(CPU_GPU), \
 E(CPU_GPU_BLAME_CTL), \
 E(GPU), \
 E(CUDA), \
 E(CUDA_DEVICE), \
 E(CUPTI), \
 E(OPENCL), \
 E(INTEL), \
 E(LEVEL0), \
 E(CUPTI_TRACE), \
 E(CUDA_CUBIN), \
 E(CUPTI_ACTIVITY), \
 E(TORCH_MONITOR), \
 E(ROCM), \
 E(DATACENTRIC), \
 E(IDLE), \
 E(MAIN_BOUNDS), \
 E(INTERVALS_PRINT), \
 E(MAP_EXEC), \
 E(LOCKWAIT), \
 E(DEADLOCK), \
 E(TBB_EACH), \
 E(TBB), \
 E(IGNORE), \
 E(TRACE), \
 E(TRACE1), \
 E(TRACE2), \
 E(TRACE3), \
 E(TRACE4), \
 E(CHECK_MAIN), \
 E(NU), \
 /* ALL_DBG_FLAGS END */


//*****************************************************************************
// type declarations
//*****************************************************************************


typedef enum {
ALL_DBG_FLAGS(DBG_PREFIX)
} pmsg_category;

typedef pmsg_category dbg_category;

//*****************************************************************************
// forward declarations
//*****************************************************************************

void debug_flag_init();

int  debug_flag_get(dbg_category flag);
void debug_flag_set(dbg_category flag, int v);

void debug_flag_dump();

#endif // debug_flag_h
