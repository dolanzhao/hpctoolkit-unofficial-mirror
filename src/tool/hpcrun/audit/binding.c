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

#define _GNU_SOURCE

#include "binding.h"

#include "../messages/messages.h"

#include <dlfcn.h>
#include <stddef.h>

extern inline void hpcrun_bind(const char* libname, ...);
extern inline void hpcrun_bind_private(const char* libname, ...);

// libhpcrun.so is in the main namespace, so we can handle it with dlopen.
void hpcrun_bind_v(const char* libname, va_list bindings) {
  // Before anything else, try to load the library.
  void* handle = dlopen(libname, RTLD_NOW | RTLD_LOCAL);
  if (handle == NULL) {
    EEMSG("Unable to bind to '%s': failed to load: %s", libname, dlerror());
    hpcrun_terminate();
  }

  // Bind each requested symbol in turn
  dlerror();  // Clear any errors
  for (const char* sym = va_arg(bindings, const char*); sym != NULL;
       sym = va_arg(bindings, const char*)) {
    void** dst = va_arg(bindings, void**);
    *dst = dlsym(handle, sym);
    const char* error = dlerror();
    if (*dst == NULL && error != NULL) {
      EEMSG("Unable to bind to '%s': %s", libname, error);
      hpcrun_terminate();
    }
  }
}

#ifndef HPCRUN_STATIC_LINK

#include "audit-api.h"

// In the dynamic case, the private namespace is the auditor's namespace, and as such
// hpcrun_bind_private_v is implemented by the the auditor API.
void hpcrun_bind_private_v(const char* libname, va_list bindings) {
  return auditor_exports->hpcrun_bind_v(libname, bindings);
}

#else  // ! HPCRUN_STATIC_LINK

#include <threads.h>

#include "hpctoolkit-config.h"

// In the static case, we load the private namespace ourselves, exactly once.

static void* private_ns = NULL;
static void (*hpcrun_bind_private_v_real)(const char*, va_list) = NULL;

static once_flag private_once;
static void private_setup() {
  private_ns = dlmopen(LM_ID_NEWLM, HPCTOOLKIT_INSTALL_PREFIX "/lib/hpctoolkit/libhpcrun_private_ns.so", RTLD_NOW);
  if (private_ns == NULL) {
    EEMSG("Unable to create private linkage namespace: %s", dlerror());
    hpcrun_terminate();
  }

  hpcrun_bind_private_v_real = dlsym(private_ns, "hpcrun_bind_v");
  if (hpcrun_bind_private_v_real == NULL) {
    EEMSG("Unable to get private binding function: %s", dlerror());
    hpcrun_terminate();
  }
}

void hpcrun_bind_private_v(const char* libname, va_list bindings) {
  call_once(&private_once, private_setup);
  return hpcrun_bind_private_v_real(libname, bindings);
}

#endif  // HPCRUN_STATIC_LINK