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
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "../../../messages/messages.h"

#include "../../gpu-monitoring-thread-api.h"

#include "../../activity/gpu-activity.h"
#include "../../activity/gpu-activity-channel.h"
#include "../../activity/gpu-activity-process.h"
#include "../../activity/correlation/gpu-host-correlation-map.h"

#include "ompt-gpu-api.h"
#include "ompt-activity-translate.h"



//******************************************************************************
// interface operations
//******************************************************************************

void
ompt_buffer_completion_notify
(
 void
)
{
  gpu_monitoring_thread_activities_ready();
}


void
ompt_activity_process
(
 ompt_record_ompt_t *record
)
{
  gpu_activity_t gpu_activity;
  uint64_t correlation_id;
  ompt_activity_translate(&gpu_activity, record, &correlation_id);

  if (correlation_id == 0) {
    return;
  }

  uint32_t thread_id =
    gpu_activity_channel_correlation_id_get_thread_id(correlation_id);
  gpu_activity_channel_t *channel = gpu_activity_channel_lookup(thread_id);
  if (channel == NULL) {
    TMSG(GPU, "Cannot find gpu_activity_channel "
              "(correlation ID: %" PRIu64 ")", correlation_id);
    return;
  }

  gpu_activity_channel_send(channel, &gpu_activity);
}
