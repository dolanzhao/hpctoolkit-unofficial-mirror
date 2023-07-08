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
// Copyright ((c)) 2002-2023, Rice University
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

//***************************************************************************
//
// File:
//   gtpin-instrumentation.h
//
// Purpose:
//   define API for instrumenting Intel GPU binaries with GTPin
//
//***************************************************************************

#ifndef gtpin_instrumentation_h
#define gtpin_instrumentation_h


#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
// local include files
//*****************************************************************************

#include <hpcrun/gpu/gpu-instrumentation.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>
#include <hpcrun/cct/cct.h>

#include "gtpin-hpcrun-api.h"


  
//*****************************************************************************
// interface functions
//*****************************************************************************

void gtpin_instrumentation_options(gpu_instrumentation_t *);
void gtpin_produce_runtime_callstack(gpu_op_ccts_t *);
void process_block_instructions(cct_node_t *);
void init_gtpin_hpcrun_api(gtpin_hpcrun_api_t *);


 
#ifdef __cplusplus
};
#endif

#endif
