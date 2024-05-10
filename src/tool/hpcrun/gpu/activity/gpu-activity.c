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


//******************************************************************************
// system includes
//******************************************************************************




//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../messages/messages.h"

#include "gpu-activity.h"


#define DEBUG 0

#include "../common/gpu-print.h"

#define GPU_INTERVAL_ENDPOINT_INVALID 0
#define GPU_INTERVAL_ENDPOINT_ISINVALID(time) \
  ((time) == GPU_INTERVAL_ENDPOINT_INVALID)



//******************************************************************************
// macros
//******************************************************************************

#define UNIT_TEST 0

#define FORALL_OPENCL_KINDS(macro)                                      \
  macro(GPU_ACTIVITY_UNKNOWN)                                                   \
  macro(GPU_ACTIVITY_KERNEL)           \
  macro(GPU_ACTIVITY_MEMCPY)

#define FORALL_OPENCL_MEM_TYPES(macro)                                  \
  macro(GPU_MEMCPY_UNK)                                                 \
  macro(GPU_MEMCPY_H2D)           \
  macro(GPU_MEMCPY_D2H)

#define CODE_TO_STRING(e) case e: return #e;



//******************************************************************************
// interface functions
//******************************************************************************

void
gpu_activity_init
(
 gpu_activity_t *activity
)
{
  memset(activity, 0, sizeof(gpu_activity_t));
}


void
gpu_context_activity_dump
(
 const gpu_activity_t *activity,
 const char *context
)
{
  PRINT("context %s gpu activity %p kind = %d\n", context, activity, activity->kind);
}

void
gpu_activity_dump
(
 const gpu_activity_t *activity
)
{
  gpu_context_activity_dump(activity, "DEBUGGER");
}

void
gpu_interval_set
(
 gpu_interval_t* interval,
 uint64_t start,
 uint64_t end
)
{
  if (start > end) {
    EMSG("WARNING: Suppressing reversed time interval for GPU activity: %u > %u",
      (unsigned long)start, (unsigned long)end);
    end = start = GPU_INTERVAL_ENDPOINT_INVALID;
  }

  interval->start = start;
  interval->end = end;
  PRINT("gpu interval: [%lu, %lu) delta = %ld\n", interval->start,
        interval->end, interval->end - interval->start);
}


bool
gpu_interval_is_invalid
(
  gpu_interval_t *gi
)
{
  return GPU_INTERVAL_ENDPOINT_ISINVALID(gi->start) |
         GPU_INTERVAL_ENDPOINT_ISINVALID(gi->end);
}


const char*
gpu_kind_to_string
(
gpu_activity_kind_t kind
)
{
  switch (kind)
  {
    FORALL_OPENCL_KINDS(CODE_TO_STRING)
    default: return "CL_unknown_kind";
  }
}


const char*
gpu_type_to_string
(
gpu_memcpy_type_t type
)
{
  switch (type)
  {
    FORALL_OPENCL_MEM_TYPES(CODE_TO_STRING)
    default: return "CL_unknown_type";
  }
}
