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

#define _GNU_SOURCE

#include "audit-api.h"

#include <dlfcn.h>
#include <elf.h>
#include <pthread.h>
#include <sched.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/wait.h>
#include <unistd.h>

static bool verbose = false;
static const char* vdso_path = NULL;

// We keep a record of all the base addresses of currently loaded stuff, to
// know when stuff was added or removed.
typedef struct shadow_map_entry_t {
  struct shadow_map_entry_t* next;
  ElfW(Addr) addr;
  bool seen;
  bool new;
  auditor_map_entry_t entry;
} shadow_map_entry_t;
static shadow_map_entry_t* shadow_map;
static pthread_mutex_t shadow_lock;

// Storage for all the bits we need
static auditor_exports_t exports;
static auditor_hooks_t hooks;

static void mainlib_connected(const char*);
static void mainlib_disconnect();
static bool update_shadow();

static void* (*real_dlopen)(const char*, int) = NULL;
static int (*real_dlclose)(void*) = NULL;
static void* private_ns = NULL;
typedef int (*pfn_iterate_phdr_t)(int (*callback)(struct dl_phdr_info*, size_t, void*), void* data);


// Initialization can happen from multiple locations, but always looks like this:
static bool initialized = false;
__attribute__((visibility("default")))
void hpcrun_init_fake_auditor() {
  if(initialized) return;
  initialized = true;

  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&shadow_lock, &mattr);
  pthread_mutexattr_destroy(&mattr);

  real_dlopen = dlsym(RTLD_NEXT, "dlopen");
  real_dlclose = dlsym(RTLD_NEXT, "dlclose");

  // Load the private namespace and get the binding function out of it
  private_ns = dlmopen(LM_ID_NEWLM, "libhpcrun_private_ns.so", RTLD_NOW);
  if (private_ns == NULL) {
    fprintf(stderr, "[fake audit] ERROR: Unable to create private linkage namespace: %s", dlerror());
    abort();
  }
  pfn_iterate_phdr_t** hpcrun_iterate_phdr = dlsym(private_ns, "hpcrun_iterate_phdr");
  if (hpcrun_iterate_phdr == NULL) {
    fprintf(stderr, "ERROR: Unable to get private dl_iterate_phdr override: %s", dlerror());
    abort();
  }
  *hpcrun_iterate_phdr = &hooks.dl_iterate_phdr;
  exports.hpcrun_bind_v = dlsym(private_ns, "hpcrun_bind_v");
  if (exports.hpcrun_bind_v == NULL) {
    fprintf(stderr, "ERROR: Unable to get private binding function: %s", dlerror());
    abort();
  }

  void (*fill_exports)(auditor_exports_t*) = dlsym(private_ns, "fill_exports");
  if (exports.hpcrun_bind_v == NULL) {
    fprintf(stderr, "ERROR: Unable to get private exports function: %s", dlerror());
    abort();
  }
  fill_exports(&exports);

  exports.mainlib_connected = mainlib_connected;
  exports.mainlib_disconnect = mainlib_disconnect;

  verbose = getenv("HPCRUN_AUDIT_DEBUG");
  vdso_path = "[vdso]";  // Set later to something more reasonable.

  // Generate the purified environment before the app changes it
  // NOTE: Consider removing only ourselves to allow for Spindle-like optimizations.
  // TODO: Export to another file for sharing with auditor.c.
  {
    size_t envsz = 0;
    for(char** e = environ; *e != NULL; e++) envsz++;
    exports.pure_environ = malloc((envsz+1)*sizeof exports.pure_environ[0]);
    size_t idx = 0;
    for(char** e = environ; *e != NULL; e++) {
      if(strncmp(*e, "LD_PRELOAD=", 11) == 0) continue;
      if(strncmp(*e, "LD_AUDIT=", 9) == 0) continue;
      exports.pure_environ[idx++] = *e;
    }
    exports.pure_environ[idx] = NULL;
  }

  // Attach to the rest of the world.
  if(verbose)
    fprintf(stderr, "[fake audit] Attaching to mainlib\n");
  hpcrun_auditor_attach(&exports, &hooks);
}

// The earliest we can do anything without LD_AUDIT is with the constructor
__attribute__((constructor))
static void hpcrun_constructor_init_fake_auditor() {
  hpcrun_init_fake_auditor();
}

// Whenever things change, we scan through with dl_iterate_phdr, update the
// shadow map and call the hook. Note that this is not async-signal-safe.
static int update_shadow_dl(struct dl_phdr_info*, size_t, void*);
struct update_shadow_args {
  bool dirty;
};
bool update_shadow() {
  pthread_mutex_lock(&shadow_lock);
  for(shadow_map_entry_t* m = shadow_map; m != NULL; m = m->next)
    m->seen = false;
  struct update_shadow_args args = {
    .dirty = false
  };
  dl_iterate_phdr(update_shadow_dl, &args);
  if(args.dirty) {
    for(shadow_map_entry_t* m = shadow_map, *p = NULL; m != NULL;
        p = m, m = m ? m->next : shadow_map) {
      if(!m->seen) {
        if(verbose)
          fprintf(stderr, "[fake audit] Delivering objclose for `%s'\n", m->entry.path);
        hooks.close(&m->entry);
        if(p) p->next = m->next;
        else shadow_map = m->next;
        free(m);
        m = p;
      } else if(m->new) {
        if(verbose)
          fprintf(stderr, "[fake audit] Delivering objopen for `%s' [%p, %p)"
                  " dl_info.dlpi_addr = %p\n", m->entry.path, m->entry.start,
                  m->entry.end, (void *)m->entry.dl_info.dlpi_addr);
        hooks.open(&m->entry);
        m->new = false;
      }
    }
  }
  pthread_mutex_unlock(&shadow_lock);
  return args.dirty;
}
int update_shadow_dl(struct dl_phdr_info* map, size_t sz, void* args_vp) {
  struct update_shadow_args* args = args_vp;
  shadow_map_entry_t* entry;
  // Make sure we can actually use this entry
  if(sz < offsetof(struct dl_phdr_info, dlpi_phnum)
          + sizeof entry->entry.dl_info.dlpi_phnum)
    return 0;
  // First check whether we have this entry already
  for(shadow_map_entry_t* m = shadow_map; m != NULL; m = m->next) {
    if(m->addr == map->dlpi_addr) {
      m->seen = true;
      return 0;
    }
  }

  entry = malloc(sizeof *entry);
  entry->addr = map->dlpi_addr;
  entry->seen = true;
  entry->new = true;

  uintptr_t start = UINTPTR_MAX;
  uintptr_t end = 0;
  for(size_t i = 0; i < map->dlpi_phnum; i++) {
    if((map->dlpi_phdr[i].p_flags & PF_X) != 0 && map->dlpi_phdr[i].p_memsz > 0) {
      if(map->dlpi_phdr[i].p_vaddr < start) start = map->dlpi_phdr[i].p_vaddr;
      if(map->dlpi_phdr[i].p_vaddr + map->dlpi_phdr[i].p_memsz > end)
        end = map->dlpi_phdr[i].p_vaddr + map->dlpi_phdr[i].p_memsz;
    }
  }

  entry->entry.start = (void*)map->dlpi_addr + start;
  entry->entry.end = (void*)map->dlpi_addr + end;

  entry->entry.path = NULL;
  if (map->dlpi_addr == getauxval(AT_SYSINFO_EHDR))
    entry->entry.path = realpath(vdso_path, NULL);
  else if(map->dlpi_phdr == (void*)getauxval(AT_PHDR)) {
    entry->entry.path = realpath("/proc/self/exe", NULL);
    if(entry->entry.path == NULL || strcmp(entry->entry.path, "/proc/self/exe") == 0)
      entry->entry.path = realpath((const char*)getauxval(AT_EXECFN), NULL);
  }
  if(entry->entry.path == NULL)
    entry->entry.path = realpath(map->dlpi_name, NULL);

  entry->entry.dl_info = *map;
  entry->entry.dl_info_sz = sz;

  entry->next = shadow_map;
  shadow_map = entry;

  args->dirty = true;

  return 0;
}

// Notification from the rest of the world that we have begun!
static bool connected = false;
void mainlib_connected(const char* new_vdso_path) {
  connected = true;
  vdso_path = new_vdso_path;
  update_shadow();
}
void mainlib_disconnect() {
  connected = false;
}

// The remainder here is overloads for dl* that update as per the usual
__attribute__((visibility("default")))
void* dlopen(const char* fn, int flags) {
  if(!real_dlopen)
    return ((void*(*)(const char*, int))dlsym(RTLD_NEXT, "dlopen"))(fn, flags);
  void* out = real_dlopen(fn, flags);
  if(connected && (flags & RTLD_NOLOAD) == 0 && update_shadow()) {
    if(verbose)
      fprintf(stderr, "[fake audit] Notifying stability (additive: 1)\n");
    hooks.stable(true);
  }
  return out;
}
__attribute__((visibility("default")))
int dlclose(void* handle) {
  if(!real_dlclose)
    return ((int(*)(void*))dlsym(RTLD_NEXT, "dlclose"))(handle);
  int out = real_dlclose(handle);
  if(connected && update_shadow()) {
    if(verbose)
      fprintf(stderr, "[fake audit] Notifying stability (additive: 0)\n");
    hooks.stable(false);
  }
  return out;
}
