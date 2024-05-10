// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
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

//
// attribute GPU performance metrics
//

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../cct2metrics.h"
#include "../memory/hpcrun-malloc.h"
#include "../metrics.h"
#include "../safe-sampling.h"
#include "../thread_data.h"

#include "activity/gpu-activity.h"
#include "gpu-metrics.h"
#include "common/gpu-monitoring.h"

//*****************************************************************************
// macros
//*****************************************************************************

#define TRACK_NVLINK 0

#define FORMAT_DISPLAY_PERCENTAGE "%6.2f %%"
#define FORMAT_DISPLAY_INT "%6.0f"

#define FORALL_INDEXED_METRIC_KINDS(macro) \
  macro(GMEM, 0)                           \
  macro(GMSET, 1)                          \
  macro(GPU_INST_STALL, 2)                 \
  macro(GXCOPY, 3)                         \
  macro(GSYNC, 4)                          \
  macro(GGMEM, 5)                          \
  macro(GLMEM, 6)

#define FORALL_SCALAR_METRIC_KINDS(macro)                        \
  macro(GBR, 7)                                                  \
  macro(GICOPY, 8)                                               \
  macro(GPU_INST, 9)                                             \
  macro(GTIMES, 10)                                              \
  macro(KINFO, 12)                                               \
  macro(GSAMP, 13)                                               \
  macro(GXFER, 14)                                               \
  macro(INTEL_OPTIMIZATION, 15)                                  \
  macro(BLAME_SHIFT, 16)                                         \
  macro(GPU_UTILIZATION, 17)


#define FORALL_METRIC_KINDS(macro)   \
  FORALL_INDEXED_METRIC_KINDS(macro) \
  FORALL_SCALAR_METRIC_KINDS(macro)

//------------------------------------------------------------------------------
// macros for declaring metrics and kinds
//------------------------------------------------------------------------------

// macros for counting entries in a FORALL macro
// The last entry of a indexed metric is reserved for API count
#define COUNT_FORALL_CLAUSE(a, b, c) +1
#define NUM_CLAUSES(forall_macro) 0 forall_macro(COUNT_FORALL_CLAUSE)

#define METRIC_KIND(name) \
  name##_metric_kind

#define INITIALIZE_METRIC_KINDS(name, value) \
  static kind_info_t *METRIC_KIND(name) = NULL;

#define METRIC_ID(name) \
  name##_metric_id

#define INITIALIZE_INDEXED_METRIC(name, value) \
  static int METRIC_ID(name)[NUM_CLAUSES(FORALL_##name)];

#define INITIALIZE_SCALAR_METRIC(string, name, desc) \
  static int METRIC_ID(name);

#define INITIALIZE_SCALAR_METRIC_KIND(kind, value) \
  FORALL_##kind(INITIALIZE_SCALAR_METRIC)

//------------------------------------------------------------------------------
// metric initialization
//------------------------------------------------------------------------------

#define APPLY(f, n) f(n)

#define INITIALIZE_METRIC_KIND() \
  APPLY(METRIC_KIND, CURRENT_METRIC) = hpcrun_metrics_new_kind()

#define FINALIZE_METRIC_KIND() \
  hpcrun_close_kind(APPLY(METRIC_KIND, CURRENT_METRIC))

#define INITIALIZE_INDEXED_METRIC_INT(metric_name, index, metric_desc)                                    \
  APPLY(METRIC_ID, CURRENT_METRIC)                                                                        \
  [index] =                                                                                               \
      hpcrun_set_new_metric_desc_and_period(APPLY(METRIC_KIND, CURRENT_METRIC), metric_name, metric_desc, \
                                            MetricFlags_ValFmt_Int, 1, metric_property_none);

#define INITIALIZE_INDEXED_METRIC_REAL(metric_name, index, metric_desc)                                   \
  APPLY(METRIC_ID, CURRENT_METRIC)                                                                        \
  [index] =                                                                                               \
      hpcrun_set_new_metric_desc_and_period(APPLY(METRIC_KIND, CURRENT_METRIC), metric_name, metric_desc, \
                                            MetricFlags_ValFmt_Real, 1, metric_property_none);

#define INITIALIZE_SCALAR_METRIC_INT(metric_name, metric_var, metric_desc)                                \
  METRIC_ID(metric_var) =                                                                                 \
      hpcrun_set_new_metric_desc_and_period(APPLY(METRIC_KIND, CURRENT_METRIC), metric_name, metric_desc, \
                                            MetricFlags_ValFmt_Int, 1, metric_property_none);

#define INITIALIZE_SCALAR_METRIC_INT_MOVE2PROC(metric_name, metric_var, metric_desc) \
  INITIALIZE_SCALAR_METRIC_INT(metric_name, metric_var, metric_desc)                 \
  hpcrun_set_move2proc(METRIC_ID(metric_var), true);

#define INITIALIZE_SCALAR_METRIC_REAL(metric_name, metric_var, metric_desc)                               \
  METRIC_ID(metric_var) =                                                                                 \
      hpcrun_set_new_metric_desc_and_period(APPLY(METRIC_KIND, CURRENT_METRIC), metric_name, metric_desc, \
                                            MetricFlags_ValFmt_Real, 1, metric_property_none);

#define INITIALIZE_SCALAR_METRIC_REAL_MOVE2PROC(metric_name, metric_var, metric_desc) \
  INITIALIZE_SCALAR_METRIC_REAL(metric_name, metric_var, metric_desc)                 \
  hpcrun_set_move2proc(METRIC_ID(metric_var), true);

#define SET_DISPLAY_INDEXED_METRIC(name, index, val) \
  hpcrun_set_display(APPLY(METRIC_ID, CURRENT_METRIC)[index], val);

#define SET_DISPLAY_SCALAR_METRIC(name, val) \
  hpcrun_set_display(APPLY(METRIC_ID, name), val);

#define HIDE_INDEXED_METRIC(string, name, desc) \
  SET_DISPLAY_INDEXED_METRIC(name, name, HPCRUN_FMT_METRIC_HIDE);

#define HIDE_SCALAR_METRIC(string, name, desc) \
  SET_DISPLAY_SCALAR_METRIC(name, HPCRUN_FMT_METRIC_HIDE);

#define DIVISION_FORMULA(name)                                                          \
  hpcrun_set_display(METRIC_ID(name##_ACUMU), HPCRUN_FMT_METRIC_INVISIBLE);             \
  hpcrun_set_percent(METRIC_ID(name), 0);                                               \
  hpcrun_set_display(METRIC_ID(name), HPCRUN_FMT_METRIC_SHOW_EXCLUSIVE);                \
  reg_metric = hpcrun_id2metric_linked(METRIC_ID(name));                                \
  reg_formula = hpcrun_malloc_safe(sizeof(char) * MAX_CHAR_FORMULA);                    \
  sprintf(reg_formula, "#%d/#%d", METRIC_ID(name##_ACUMU), METRIC_ID(GPU_KINFO_COUNT)); \
  reg_metric->formula = reg_formula;                                                    \
  reg_metric->format = FORMAT_DISPLAY_INT

#define OCCUPANCY_FORMULA(name)                                                                                  \
  hpcrun_set_percent(METRIC_ID(name), 0);                                                                        \
  hpcrun_set_display(METRIC_ID(name), HPCRUN_FMT_METRIC_SHOW_EXCLUSIVE);                                         \
  reg_metric = hpcrun_id2metric_linked(METRIC_ID(name));                                                         \
  reg_formula = hpcrun_malloc_safe(sizeof(char) * MAX_CHAR_FORMULA);                                             \
  sprintf(reg_formula, "100*(#%d/#%d)", METRIC_ID(GPU_KINFO_FGP_ACT_ACUMU), METRIC_ID(GPU_KINFO_FGP_MAX_ACUMU)); \
  reg_metric->formula = reg_formula;                                                                             \
  reg_metric->format = FORMAT_DISPLAY_PERCENTAGE

#define GPU_UTILIZATION_FORMULA()                                                               \
  hpcrun_set_display(METRIC_ID(EU_ACT), HPCRUN_FMT_METRIC_INVISIBLE);                           \
  hpcrun_set_display(METRIC_ID(EU_STL), HPCRUN_FMT_METRIC_INVISIBLE);                           \
  hpcrun_set_display(METRIC_ID(EU_IDLE), HPCRUN_FMT_METRIC_INVISIBLE);                          \
  hpcrun_set_display(METRIC_ID(GPU_UTIL_DENOMINATOR), HPCRUN_FMT_METRIC_INVISIBLE);             \
  hpcrun_set_percent(METRIC_ID(EU_ACT_PERCENT), 0);                                             \
  hpcrun_set_percent(METRIC_ID(EU_STL_PERCENT), 0);                                             \
  hpcrun_set_percent(METRIC_ID(EU_IDLE_PERCENT), 0);                                            \
  active_metric = hpcrun_id2metric_linked(METRIC_ID(EU_ACT_PERCENT));                           \
  stalled_metric = hpcrun_id2metric_linked(METRIC_ID(EU_STL_PERCENT));                          \
  idle_metric = hpcrun_id2metric_linked(METRIC_ID(EU_IDLE_PERCENT));                            \
  active_formula = hpcrun_malloc_safe(sizeof(char) * MAX_CHAR_FORMULA);                         \
  stall_formula = hpcrun_malloc_safe(sizeof(char) * MAX_CHAR_FORMULA);                          \
  idle_formula = hpcrun_malloc_safe(sizeof(char) * MAX_CHAR_FORMULA);                           \
  sprintf(active_formula, "100*(#%d/#%d)", METRIC_ID(EU_ACT), METRIC_ID(GPU_UTIL_DENOMINATOR)); \
  sprintf(stall_formula, "100*(#%d/#%d)", METRIC_ID(EU_STL), METRIC_ID(GPU_UTIL_DENOMINATOR));  \
  sprintf(idle_formula, "100*(#%d/#%d)", METRIC_ID(EU_IDLE), METRIC_ID(GPU_UTIL_DENOMINATOR));  \
  active_metric->formula = active_formula;                                                      \
  stalled_metric->formula = stall_formula;                                                      \
  idle_metric->formula = idle_formula;                                                          \
  active_metric->format = FORMAT_DISPLAY_PERCENTAGE;                                            \
  stalled_metric->format = FORMAT_DISPLAY_PERCENTAGE;                                           \
  idle_metric->format = FORMAT_DISPLAY_PERCENTAGE

// NOTE: to ensure that this metric is 0 if no latency is measured, rather than computing it as:
//     1 + (uncovered_latency/covered_latency)
// instead, compute it as:
//     (covered_latency/covered_latency) + (uncovered_latency/covered_latency)
// when covered_latency is present (as it would be with any latency measurement), the first term will
// be 1 as expected. when no latency measurements are available, the first term will be 0, causing the
// metric to be unavailable
#define THREADS_TO_COVER_LATENCY_FORMULA()                                                                                                            \
  hpcrun_set_display(METRIC_ID(GPU_INSTRUCTION_THREADS_TO_COVER_LATENCY), HPCRUN_FMT_METRIC_SHOW);                                         \
  thrds_to_cover_latency_metric = hpcrun_id2metric_linked(METRIC_ID(GPU_INSTRUCTION_THREADS_TO_COVER_LATENCY));                            \
  thrds_to_cover_latency_formula = hpcrun_malloc_safe(sizeof(char) * MAX_CHAR_FORMULA);                                                               \
  sprintf(thrds_to_cover_latency_formula, "1 + (#%d/#%d)", METRIC_ID(GPU_INSTRUCTION_UNCOVERED_LATENCY), METRIC_ID(GPU_INSTRUCTION_COVERED_LATENCY)); \
  sprintf(thrds_to_cover_latency_formula, "(#%d/#%d) + (#%d/#%d)",                                                                                    \
          METRIC_ID(GPU_INSTRUCTION_COVERED_LATENCY), METRIC_ID(GPU_INSTRUCTION_COVERED_LATENCY),                                                     \
          METRIC_ID(GPU_INSTRUCTION_UNCOVERED_LATENCY), METRIC_ID(GPU_INSTRUCTION_COVERED_LATENCY));                                                  \
  thrds_to_cover_latency_metric->formula = thrds_to_cover_latency_formula;                                                                            \
  thrds_to_cover_latency_metric->format = FORMAT_DISPLAY_INT

//*****************************************************************************
// local variables
//*****************************************************************************

FORALL_METRIC_KINDS(INITIALIZE_METRIC_KINDS);

FORALL_INDEXED_METRIC_KINDS(INITIALIZE_INDEXED_METRIC);

FORALL_SCALAR_METRIC_KINDS(INITIALIZE_SCALAR_METRIC_KIND);

static kind_info_t *GPU_COUNTER_METRIC_KIND_INFO = NULL;
static int *gpu_counter_hpcrun_metric_id_array = NULL;

static const unsigned int MAX_CHAR_FORMULA = 32;

//*****************************************************************************
// private operations
//*****************************************************************************

static void
gpu_metrics_attribute_metric_int
(
 metric_data_list_t *metrics,
 int metric_index,
 uint64_t value
)
{
  hpcrun_metric_std_inc(metric_index, metrics, (cct_metric_data_t){.i = value});
}

static void
gpu_metrics_attribute_metric_real
(
 metric_data_list_t *metrics,
 int metric_index,
 double value
)
{
  hpcrun_metric_std_inc(metric_index, metrics, (cct_metric_data_t){.r = value});
}

static void
gpu_metrics_attribute_metric_time_interval(
    cct_node_t *cct_node,
    int time_index,
    gpu_interval_t *i)
{
  metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, time_index);

  // convert from ns to s
  gpu_metrics_attribute_metric_real(metrics, time_index, (i->end - i->start) / 1.0e9);
}

static void
gpu_metrics_attribute_pc_sampling(
    gpu_activity_t *activity)
{
  uint64_t sample_period =
    1 << gpu_monitoring_instruction_sample_frequency_get();

  gpu_pc_sampling_t *sinfo = &(activity->details.pc_sampling);
  cct_node_t *cct_node = activity->cct_node;

  uint64_t inst_count = sinfo->samples * sample_period;

  metric_data_list_t *inst_metric =
    hpcrun_reify_metric_set(cct_node, METRIC_ID(GPU_INSTRUCTION_EXECUTION_COUNT));

  // instruction execution metric
  gpu_metrics_attribute_metric_int(inst_metric, METRIC_ID(GPU_INSTRUCTION_EXECUTION_COUNT),
                                   inst_count);

  if (sinfo->stallReason != GPU_INST_STALL_INVALID) {
    int stall_summary_metric_index =
      METRIC_ID(GPU_INST_STALL)[GPU_INST_STALL_ANY];

    int stall_kind_metric_index = METRIC_ID(GPU_INST_STALL)[sinfo->stallReason];

    metric_data_list_t *stall_metrics =
      hpcrun_reify_metric_set(cct_node, stall_kind_metric_index);

    uint64_t stall_count = sinfo->latencySamples * sample_period;

    if (sinfo->stallReason != GPU_INST_STALL_NONE)
    {
      // stall summary metric
      gpu_metrics_attribute_metric_int(stall_metrics,
                                       stall_summary_metric_index, stall_count);
    }

    // stall reason specific metric
    gpu_metrics_attribute_metric_int(stall_metrics,
                                     stall_kind_metric_index, stall_count);
  }
}

static void
gpu_metrics_attribute_pc_sampling_info(
    gpu_activity_t *activity)
{
  gpu_pc_sampling_info_t *s = &(activity->details.pc_sampling_info);
  cct_node_t *cct_node = activity->cct_node;

  metric_data_list_t *metrics =
    hpcrun_reify_metric_set(cct_node, METRIC_ID(GPU_SAMPLE_DROPPED));

  // OK to use set here because sampling cycle is changed during execution
  hpcrun_metric_std_set(METRIC_ID(GPU_SAMPLE_PERIOD), metrics,
                        (cct_metric_data_t){.i = s->samplingPeriodInCycles});

  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_SAMPLE_TOTAL),
                                   s->totalSamples);

  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_SAMPLE_EXPECTED),
                                   s->fullSMSamples);

  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_SAMPLE_DROPPED),
                                   s->droppedSamples);
}

static void
gpu_metrics_attribute_mem_op
(
 cct_node_t *cct_node,
 int bytes_metric_index,
 int time_metric_index,
 int count_metric_index,
 gpu_mem_t *m
)
{
  metric_data_list_t *bytes_metrics =
    hpcrun_reify_metric_set(cct_node, bytes_metric_index);

  gpu_metrics_attribute_metric_int(bytes_metrics, bytes_metric_index, m->bytes);

  gpu_metrics_attribute_metric_time_interval(cct_node, time_metric_index,
                                             (gpu_interval_t *)m);

  // gpu operation summary metric
  gpu_metrics_attribute_metric_time_interval(cct_node, METRIC_ID(GPU_TIME_OP),
                                             (gpu_interval_t *)m);

  metric_data_list_t *count_metrics =
      hpcrun_reify_metric_set(cct_node, count_metric_index);

  // increment the count of mem op
  gpu_metrics_attribute_metric_int(count_metrics, count_metric_index, 1);
}

static void
gpu_metrics_attribute_memory
(
 gpu_activity_t *activity
)
{
  gpu_memory_t *m = &(activity->details.memory);
  cct_node_t *cct_node = activity->cct_node;

  int bytes_metric_index = METRIC_ID(GMEM)[m->memKind];

  int count_metric_index = METRIC_ID(GMEM)[GPU_MEM_COUNT];

  gpu_metrics_attribute_mem_op(cct_node, bytes_metric_index,
                               METRIC_ID(GPU_TIME_MEM), count_metric_index, (gpu_mem_t *)m);
}

static void
gpu_metrics_attribute_memcpy
(
    gpu_activity_t *activity
)
{
  gpu_memcpy_t *m = &(activity->details.memcpy);
  cct_node_t *cct_node = activity->cct_node;

  int bytes_metric_index = METRIC_ID(GXCOPY)[m->copyKind];

  int count_metric_index = METRIC_ID(GXCOPY)[GPU_MEMCPY_COUNT];

  gpu_metrics_attribute_mem_op(cct_node, bytes_metric_index,
                               METRIC_ID(GPU_TIME_XCOPY), count_metric_index, (gpu_mem_t *)m);
}

static void
gpu_metrics_attribute_memset
(
 gpu_activity_t *activity
)
{
  gpu_memset_t *m = &(activity->details.memset);
  cct_node_t *cct_node = activity->cct_node;

  int bytes_metric_index = METRIC_ID(GMSET)[m->memKind];

  int count_metric_index = METRIC_ID(GMSET)[GPU_MEM_COUNT];

  gpu_metrics_attribute_mem_op(cct_node, bytes_metric_index,
                               METRIC_ID(GPU_TIME_MSET), count_metric_index, (gpu_mem_t *)m);
}

static void
gpu_metrics_attribute_kernel(
    gpu_activity_t *activity)
{
  gpu_kernel_t *k = &(activity->details.kernel);
  cct_node_t *cct_node = activity->cct_node;

  metric_data_list_t *metrics =
    hpcrun_reify_metric_set(cct_node, METRIC_ID(GPU_KINFO_STMEM_ACUMU));

  if (METRIC_KIND(KINFO)) {
    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_STMEM_ACUMU),
                                     k->staticSharedMemory);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_DYMEM_ACUMU),
                                     k->dynamicSharedMemory);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_LMEM_ACUMU),
                                     k->localMemoryTotal);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_FGP_ACT_ACUMU),
                                     k->activeWarpsPerSM);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_FGP_MAX_ACUMU),
                                     k->maxActiveWarpsPerSM);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_REGISTERS_ACUMU),
                                     k->threadRegisters);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_BLK_THREADS_ACUMU),
                                     k->blockThreads);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_BLK_SMEM_ACUMU),
                                     k->blockSharedMemory);

    gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_BLKS_ACUMU),
                                     k->blocks);
  }

  // number of kernel launches
  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_KINFO_COUNT), 1);

  // kernel execution time
  gpu_metrics_attribute_metric_time_interval(cct_node, METRIC_ID(GPU_TIME_KER),
                                             (gpu_interval_t *)k);

  // gpu operation summary metric
  gpu_metrics_attribute_metric_time_interval(cct_node, METRIC_ID(GPU_TIME_OP),
                                             (gpu_interval_t *)k);
}

block_metrics_t
fetch_block_metrics
(
 cct_node_t *block
)
{
  block_metrics_t result;
  metric_data_list_t *metrics = hpcrun_get_metric_data_list(block);
  result.execution_count = fetch_metric(metrics, METRIC_ID(GPU_BLOCK_EXECUTION_COUNT))->i;
  result.latency = fetch_metric(metrics, METRIC_ID(GPU_BLOCK_LATENCY))->i;
  result.active_simd_lanes = fetch_metric(metrics, METRIC_ID(GPU_BLOCK_ACTIVE_SIMD_LANES))->i;
  return result;
}

void
attribute_instruction_metrics
(
 cct_node_t *instruction,
 instruction_metrics_t metrics
)
{
  metric_data_list_t *ins_metrics = hpcrun_reify_metric_set(instruction, METRIC_ID(GPU_INSTRUCTION_EXECUTION_COUNT));
  if (metrics.execution_count) {
    gpu_metrics_attribute_metric_int(ins_metrics, METRIC_ID(GPU_INSTRUCTION_EXECUTION_COUNT), metrics.execution_count);
  }
  if (metrics.total_simd_lanes || metrics.active_simd_lanes || metrics.wasted_simd_lanes || metrics.scalar_simd_loss)
  {
    gpu_metrics_attribute_metric_int(ins_metrics, METRIC_ID(GPU_INSTRUCTION_TOTAL_SIMD_LANES), metrics.total_simd_lanes);
    gpu_metrics_attribute_metric_int(ins_metrics, METRIC_ID(GPU_INSTRUCTION_ACTIVE_SIMD_LANES), metrics.active_simd_lanes);
    gpu_metrics_attribute_metric_int(ins_metrics, METRIC_ID(GPU_INSTRUCTION_WASTED_SIMD_LANES), metrics.wasted_simd_lanes);
    gpu_metrics_attribute_metric_int(ins_metrics, METRIC_ID(GPU_INSTRUCTION_SCALAR_SIMD_LOSS), metrics.scalar_simd_loss);
  }
  if (metrics.latency)
  {
    gpu_metrics_attribute_metric_int(ins_metrics, METRIC_ID(GPU_INSTRUCTION_LATENCY), metrics.latency);
    uint64_t covered_latency = metrics.latency <= 0 ? 0 : metrics.execution_count;
    covered_latency = metrics.latency < covered_latency ? metrics.latency : covered_latency;
    uint64_t uncovered_latency = metrics.latency <= 0 ? 0 : metrics.latency - covered_latency;
    gpu_metrics_attribute_metric_int(ins_metrics, METRIC_ID(GPU_INSTRUCTION_COVERED_LATENCY), covered_latency);
    gpu_metrics_attribute_metric_int(ins_metrics, METRIC_ID(GPU_INSTRUCTION_UNCOVERED_LATENCY), uncovered_latency);
  }
}

static void
gpu_metrics_attribute_kernel_block
(
 gpu_activity_t *activity
)
{
  gpu_kernel_block_t *b = &(activity->details.kernel_block);
  cct_node_t *cct_node = activity->cct_node;

  metric_data_list_t *metrics =
    hpcrun_reify_metric_set(cct_node, METRIC_ID(GPU_BLOCK_EXECUTION_COUNT));

  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_BLOCK_EXECUTION_COUNT), b->execution_count);
  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_BLOCK_LATENCY), b->latency);
  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_BLOCK_ACTIVE_SIMD_LANES), b->active_simd_lanes);
}

static void
gpu_metrics_attribute_synchronization
(
 gpu_activity_t *activity
)
{
  gpu_synchronization_t *s = &(activity->details.synchronization);
  cct_node_t *cct_node = activity->cct_node;

  int sync_kind_metric_id = METRIC_ID(GSYNC)[s->syncKind];

  gpu_metrics_attribute_metric_time_interval(cct_node, sync_kind_metric_id,
                                             (gpu_interval_t *)s);

  gpu_metrics_attribute_metric_time_interval(cct_node,
                                             METRIC_ID(GPU_TIME_SYNC),
                                             (gpu_interval_t *)s);

  // gpu operation summary metric
  gpu_metrics_attribute_metric_time_interval(cct_node,
                                             METRIC_ID(GPU_TIME_OP),
                                             (gpu_interval_t *)s);

  int count_metric_index = METRIC_ID(GSYNC)[GPU_SYNC_COUNT];

  metric_data_list_t *count_metrics = hpcrun_reify_metric_set(cct_node, count_metric_index);

  // increment the count of sync op
  // use 1.0 because sync metrics array is initialized as REAL
  gpu_metrics_attribute_metric_real(count_metrics, count_metric_index, 1.0);
}

static void
gpu_metrics_attribute_global_access
(
 gpu_activity_t *activity
)
{
  gpu_global_access_t *g = &(activity->details.global_access);
  cct_node_t *cct_node = activity->cct_node;

  int type = g->type;

  int l2t_index = METRIC_ID(GGMEM)[GPU_GMEM_LD_CACHED_L2TRANS + type];

  metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, l2t_index);

  gpu_metrics_attribute_metric_int(metrics, l2t_index, g->l2_transactions);

  int l2t_theoretical_index =
    METRIC_ID(GMEM)[GPU_GMEM_LD_CACHED_L2TRANS_THEOR + type];

  gpu_metrics_attribute_metric_int(metrics, l2t_theoretical_index,
                                   g->theoreticalL2Transactions);

  int bytes_index = METRIC_ID(GMEM)[GPU_GMEM_LD_CACHED_BYTES + type];
  gpu_metrics_attribute_metric_int(metrics, bytes_index, g->bytes);
}

static void
gpu_metrics_attribute_local_access
(
 gpu_activity_t *activity
)
{
  gpu_local_access_t *l = &(activity->details.local_access);
  cct_node_t *cct_node = activity->cct_node;

  int type = l->type;

  int lmem_trans_index = METRIC_ID(GLMEM)[GPU_LMEM_LD_TRANS + type];

  metric_data_list_t *metrics =
      hpcrun_reify_metric_set(cct_node, lmem_trans_index);

  gpu_metrics_attribute_metric_int(metrics, lmem_trans_index,
                                   l->sharedTransactions);

  int lmem_trans_theor_index = METRIC_ID(GLMEM)[GPU_LMEM_LD_TRANS_THEOR + type];
  gpu_metrics_attribute_metric_int(metrics, lmem_trans_theor_index,
                                   l->theoreticalSharedTransactions);

  int bytes_index = METRIC_ID(GLMEM)[GPU_LMEM_LD_BYTES + type];
  gpu_metrics_attribute_metric_int(metrics, bytes_index, l->bytes);
}

static void
gpu_metrics_attribute_branch
(
 gpu_activity_t *activity
)
{
  gpu_branch_t *b = &(activity->details.branch);
  cct_node_t *cct_node = activity->cct_node;

  metric_data_list_t *metrics =
    hpcrun_reify_metric_set(cct_node, METRIC_ID(GPU_BR_DIVERGED));

  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_BR_DIVERGED),
                                   b->diverged);

  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_BR_EXECUTED),
                                   b->executed);
}

static void
gpu_metrics_attribute_counter
(
 gpu_activity_t *activity
)
{
  gpu_counter_t *c = &(activity->details.counters);
  cct_node_t *cct_node = activity->cct_node;

  metric_data_list_t *metrics =
    hpcrun_reify_metric_set(cct_node, gpu_counter_hpcrun_metric_id_array[0]);

  for (int i = 0; i < c->total_counters; ++i) {
    gpu_metrics_attribute_metric_real(metrics, gpu_counter_hpcrun_metric_id_array[i], c->values[i]);
  }

  free(c->values);
}

#if TRACK_NVLINK
static void
gpu_metrics_attribute_link
(
 gpu_activity_t *activity
)
{

  printf("Attribute NVLINK not implemented\n\n");
}
#endif

static void
metrics_attribute_intel_optimization
(
 gpu_activity_t *activity
)
{
  intel_optimization_t *i = &(activity->details.intel_optimization);
  cct_node_t *cct_node = activity->cct_node;

  int metric_id = METRIC_ID(INORDER_QUEUE) + i->intelOptKind;
  metric_data_list_t *metrics =
    hpcrun_reify_metric_set(cct_node, metric_id);

  gpu_metrics_attribute_metric_int(metrics, metric_id, i->val);
}

static void
gpu_metrics_attribute_blame_shift
(
 gpu_activity_t *activity
)
{
  gpu_blame_shift_t *bs = &(activity->details.blame_shift);
  cct_node_t *cct_node = activity->cct_node;

  metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, METRIC_ID(CPU_IDLE));
  gpu_metrics_attribute_metric_real(metrics, METRIC_ID(CPU_IDLE), bs->cpu_idle_time);
  gpu_metrics_attribute_metric_real(metrics, METRIC_ID(GPU_IDLE_CAUSE), bs->gpu_idle_cause_time);
  gpu_metrics_attribute_metric_real(metrics, METRIC_ID(CPU_IDLE_CAUSE), bs->cpu_idle_cause_time);
}

static void
gpu_metrics_attribute_gpu_utilization(
    gpu_activity_t *activity)
{
  gpu_utilization_t *gpu_info = &(activity->details.gpu_utilization_info);
  cct_node_t *cct_node = activity->cct_node;

  metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, METRIC_ID(EU_ACT));
  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(EU_ACT), gpu_info->active);
  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(EU_STL), gpu_info->stalled);
  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(EU_IDLE), gpu_info->idle);
  gpu_metrics_attribute_metric_int(metrics, METRIC_ID(GPU_UTIL_DENOMINATOR), 100);
}

//******************************************************************************
// interface operations
//******************************************************************************

void gpu_metrics_attribute(
    gpu_activity_t *activity)
{
  thread_data_t *td = hpcrun_get_thread_data();
  td->overhead++;
  hpcrun_safe_enter();

  switch (activity->kind) {
  case GPU_ACTIVITY_PC_SAMPLING:
    gpu_metrics_attribute_pc_sampling(activity);
    break;

  case GPU_ACTIVITY_PC_SAMPLING_INFO:
    gpu_metrics_attribute_pc_sampling_info(activity);
    break;

  case GPU_ACTIVITY_MEMCPY:
    gpu_metrics_attribute_memcpy(activity);
    break;

  case GPU_ACTIVITY_MEMSET:
    gpu_metrics_attribute_memset(activity);
    break;

  case GPU_ACTIVITY_KERNEL:
    gpu_metrics_attribute_kernel(activity);
    break;

  case GPU_ACTIVITY_KERNEL_BLOCK:
    gpu_metrics_attribute_kernel_block(activity);
    break;

  case GPU_ACTIVITY_SYNCHRONIZATION:
    gpu_metrics_attribute_synchronization(activity);
    break;

  case GPU_ACTIVITY_MEMORY:
    gpu_metrics_attribute_memory(activity);
    break;

  case GPU_ACTIVITY_GLOBAL_ACCESS:
    gpu_metrics_attribute_global_access(activity);
    break;

  case GPU_ACTIVITY_LOCAL_ACCESS:
    gpu_metrics_attribute_local_access(activity);
    break;

  case GPU_ACTIVITY_BRANCH:
    gpu_metrics_attribute_branch(activity);
    break;

  case GPU_ACTIVITY_COUNTER:
    gpu_metrics_attribute_counter(activity);
    break;

  case GPU_ACTIVITY_INTEL_OPTIMIZATION:
    metrics_attribute_intel_optimization(activity);
    break;

  case GPU_ACTIVITY_BLAME_SHIFT:
    gpu_metrics_attribute_blame_shift(activity);
    break;

  case GPU_ACTIVITY_INTEL_GPU_UTILIZATION:
    gpu_metrics_attribute_gpu_utilization(activity);
    break;

  default:
    break;
  }

  hpcrun_safe_exit();
  td->overhead--;
}

void gpu_metrics_default_enable(
    void)
{

// Execution time metrics
#undef CURRENT_METRIC
#define CURRENT_METRIC GTIMES

  INITIALIZE_METRIC_KIND();

  FORALL_GTIMES(INITIALIZE_SCALAR_METRIC_REAL_MOVE2PROC)

  FINALIZE_METRIC_KIND();

// Memory alloc/free metrics
#undef CURRENT_METRIC
#define CURRENT_METRIC GMEM

  INITIALIZE_METRIC_KIND();

  FORALL_GMEM(INITIALIZE_INDEXED_METRIC_INT)

  FINALIZE_METRIC_KIND();

// Memset metrics
#undef CURRENT_METRIC
#define CURRENT_METRIC GMSET

  INITIALIZE_METRIC_KIND();

  FORALL_GMSET(INITIALIZE_INDEXED_METRIC_INT)

  FINALIZE_METRIC_KIND();

// GPU explicit copy merics
#undef CURRENT_METRIC
#define CURRENT_METRIC GXCOPY

  INITIALIZE_METRIC_KIND();

  FORALL_GXCOPY(INITIALIZE_INDEXED_METRIC_INT)

  FINALIZE_METRIC_KIND();

// GPU synchonrization merics
#undef CURRENT_METRIC
#define CURRENT_METRIC GSYNC

  INITIALIZE_METRIC_KIND();

  FORALL_GSYNC(INITIALIZE_INDEXED_METRIC_REAL)

  FORALL_GSYNC(HIDE_INDEXED_METRIC);

  FINALIZE_METRIC_KIND();
}

void gpu_metrics_KINFO_enable(
    void)
{
// GPU kernel characteristics metrics
#undef CURRENT_METRIC
#define CURRENT_METRIC KINFO

  INITIALIZE_METRIC_KIND();

  FORALL_KINFO(INITIALIZE_SCALAR_METRIC_INT_MOVE2PROC)

  FINALIZE_METRIC_KIND();

  metric_desc_t *reg_metric;
  char *reg_formula;

  DIVISION_FORMULA(GPU_KINFO_STMEM);
  DIVISION_FORMULA(GPU_KINFO_DYMEM);
  DIVISION_FORMULA(GPU_KINFO_LMEM);
  DIVISION_FORMULA(GPU_KINFO_FGP_ACT);
  DIVISION_FORMULA(GPU_KINFO_FGP_MAX);
  DIVISION_FORMULA(GPU_KINFO_REGISTERS);
  DIVISION_FORMULA(GPU_KINFO_BLK_THREADS);
  DIVISION_FORMULA(GPU_KINFO_BLK_SMEM);
  DIVISION_FORMULA(GPU_KINFO_BLKS);
  OCCUPANCY_FORMULA(GPU_KINFO_OCCUPANCY_THR);
}

void gpu_metrics_GICOPY_enable(
    void)
{
// GPU implicit copy metrics
#undef CURRENT_METRIC
#define CURRENT_METRIC GICOPY

  INITIALIZE_METRIC_KIND();

  FORALL_GICOPY(INITIALIZE_SCALAR_METRIC_REAL)

  FINALIZE_METRIC_KIND();
}

void gpu_metrics_GLMEM_enable(
    void)
{
// GPU local memory access metrics
#undef CURRENT_METRIC
#define CURRENT_METRIC GLMEM

  INITIALIZE_METRIC_KIND();

  FORALL_GLMEM(INITIALIZE_INDEXED_METRIC_INT)

  FINALIZE_METRIC_KIND();
}

void gpu_metrics_GGMEM_enable(
    void)
{
// GPU global memory access metrics
#undef CURRENT_METRIC
#define CURRENT_METRIC GGMEM

  INITIALIZE_METRIC_KIND();

  FORALL_GGMEM(INITIALIZE_INDEXED_METRIC_INT)

  FINALIZE_METRIC_KIND();
}

void gpu_metrics_GPU_INST_enable(
    void)
{
#undef CURRENT_METRIC
#define CURRENT_METRIC GPU_INST

  INITIALIZE_METRIC_KIND();

  FORALL_GPU_INST(INITIALIZE_SCALAR_METRIC_INT)

  // hide block level instruction metrics in hpcviewer
  hpcrun_set_display(METRIC_ID(GPU_BLOCK_EXECUTION_COUNT), HPCRUN_FMT_METRIC_INVISIBLE);
  hpcrun_set_display(METRIC_ID(GPU_BLOCK_LATENCY), HPCRUN_FMT_METRIC_INVISIBLE);
  hpcrun_set_display(METRIC_ID(GPU_BLOCK_ACTIVE_SIMD_LANES), HPCRUN_FMT_METRIC_INVISIBLE);

  FINALIZE_METRIC_KIND();

  metric_desc_t *thrds_to_cover_latency_metric;
  char *thrds_to_cover_latency_formula;
  THREADS_TO_COVER_LATENCY_FORMULA();
}

void gpu_metrics_GSAMP_enable(
    void)
{
#undef CURRENT_METRIC
#define CURRENT_METRIC GSAMP

  INITIALIZE_METRIC_KIND();

  FORALL_GSAMP_INT(INITIALIZE_SCALAR_METRIC_INT_MOVE2PROC);

  FORALL_GSAMP_INT(HIDE_SCALAR_METRIC);

  FORALL_GSAMP_REAL(INITIALIZE_SCALAR_METRIC_REAL_MOVE2PROC);

  FINALIZE_METRIC_KIND();

  hpcrun_set_percent(METRIC_ID(GPU_SAMPLE_UTILIZATION), 1);

  metric_desc_t *util_metric =
    hpcrun_id2metric_linked(METRIC_ID(GPU_SAMPLE_UTILIZATION));

  char *util_formula = hpcrun_malloc_safe(sizeof(char) * MAX_CHAR_FORMULA);

  sprintf(util_formula, "min(100, max(0, 100*#%d/#%d))", METRIC_ID(GPU_SAMPLE_TOTAL),
          METRIC_ID(GPU_SAMPLE_EXPECTED));

  util_metric->formula = util_formula;
  util_metric->format = FORMAT_DISPLAY_PERCENTAGE;
}

void gpu_metrics_GBR_enable(
    void)
{
// GPU branch instruction metrics
#undef CURRENT_METRIC
#define CURRENT_METRIC GBR

  INITIALIZE_METRIC_KIND();

  FORALL_GBR(INITIALIZE_SCALAR_METRIC_INT)

  FINALIZE_METRIC_KIND();
}

void gpu_metrics_GPU_INST_STALL_enable(
    void)
{
#undef CURRENT_METRIC
#define CURRENT_METRIC GPU_INST_STALL

  INITIALIZE_METRIC_KIND();

  FORALL_GPU_INST_STALL(INITIALIZE_INDEXED_METRIC_INT)

  FORALL_GPU_INST_STALL(HIDE_INDEXED_METRIC);

  SET_DISPLAY_INDEXED_METRIC(GPU_INST_STALL_ANY, GPU_INST_STALL_ANY,
                             HPCRUN_FMT_METRIC_SHOW);

  FINALIZE_METRIC_KIND();
}

void gpu_metrics_GPU_CTR_enable(
    int total,
    const char **counter_name,
    const char **counter_desc)
{
  gpu_counter_hpcrun_metric_id_array = (int *)malloc(sizeof(int) * total);

  GPU_COUNTER_METRIC_KIND_INFO = hpcrun_metrics_new_kind();

  for (int i = 0; i < total; ++i)
  {
    gpu_counter_hpcrun_metric_id_array[i] = hpcrun_set_new_metric_desc_and_period(
        GPU_COUNTER_METRIC_KIND_INFO, counter_name[i], counter_desc[i],
        MetricFlags_ValFmt_Real, 1, metric_property_none);
  }

  hpcrun_close_kind(GPU_COUNTER_METRIC_KIND_INFO);
}

void gpu_metrics_GXFER_enable(
    void)
{
#undef CURRENT_METRIC
#define CURRENT_METRIC GXFER

  INITIALIZE_METRIC_KIND();

  FORALL_GXFER(INITIALIZE_SCALAR_METRIC_INT)

  FINALIZE_METRIC_KIND();
}

void gpu_metrics_INTEL_OPTIMIZATION_enable(
    void)
{
#undef CURRENT_METRIC
#define CURRENT_METRIC INTEL_OPTIMIZATION

  INITIALIZE_METRIC_KIND();

  FORALL_INTEL_OPTIMIZATION(INITIALIZE_SCALAR_METRIC_INT)

  FINALIZE_METRIC_KIND();
}

void gpu_metrics_BLAME_SHIFT_enable(
    void)
{
#undef CURRENT_METRIC
#define CURRENT_METRIC BLAME_SHIFT

  INITIALIZE_METRIC_KIND();

  FORALL_BLAME_SHIFT(INITIALIZE_SCALAR_METRIC_REAL)
}

void gpu_metrics_gpu_utilization_enable(
    void)
{
#undef CURRENT_METRIC
#define CURRENT_METRIC GPU_UTILIZATION

  INITIALIZE_METRIC_KIND();

  FORALL_GPU_UTILIZATION(INITIALIZE_SCALAR_METRIC_INT)

  metric_desc_t *active_metric, *stalled_metric, *idle_metric;
  char *active_formula, *stall_formula, *idle_formula;
  GPU_UTILIZATION_FORMULA();
}
