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

//*****************************************************************************
// system includes
//*****************************************************************************

#define _GNU_SOURCE

#include <string.h>                       // memset


//*****************************************************************************
// local includes
//*****************************************************************************

#include "../../../../../../../lib/prof-lean/splay-uint64.h"
#include "../../../../../../../lib/prof-lean/spinlock.h"
#include "../../../../gpu-splay-allocator.h" // typed_splay_alloc
#include "buffer-map.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#define st_insert                               \
  typed_splay_insert(queue)

#define st_lookup                               \
  typed_splay_lookup(queue)

#define st_delete                               \
  typed_splay_delete(queue)

#define st_forall                               \
  typed_splay_forall(queue)

#define st_count                                \
  typed_splay_count(queue)

#define st_alloc(free_list)                     \
  typed_splay_alloc(free_list, buffer_map_entry_t)

#define st_free(free_list, node)                \
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(queue) buffer_map_entry_t

typedef struct typed_splay_node(queue) {
  struct typed_splay_node(queue) *left;
  struct typed_splay_node(queue) *right;
  uint64_t buffer_id; // key

  int H2D_count;
  int D2H_count;
} typed_splay_node(queue);


//******************************************************************************
// local data
//******************************************************************************

static buffer_map_entry_t *map_root = NULL;
static buffer_map_entry_t *free_list = NULL;

static spinlock_t buffer_map_lock = SPINLOCK_UNLOCKED;



//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(queue)


static buffer_map_entry_t *
buffer_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static buffer_map_entry_t *
buffer_map_entry_new
(
 uint64_t buffer_id
)
{
  buffer_map_entry_t *e = buffer_map_entry_alloc();

  e->buffer_id = buffer_id;
  e->H2D_count = 0;
  e->D2H_count = 0;

  return e;
}



//*****************************************************************************
// interface operations
//*****************************************************************************

buffer_map_entry_t *
buffer_map_lookup
(
 uint64_t buffer_id
)
{
  spinlock_lock(&buffer_map_lock);

  uint64_t id = buffer_id;
  buffer_map_entry_t *result = st_lookup(&map_root, id);

  spinlock_unlock(&buffer_map_lock);

  return result;
}


// called on clEnqueueReadBuffer and clEnqueueWriteBuffer
buffer_map_entry_t*
buffer_map_update
(
 uint64_t buffer_id,
 int H2D_incr,
 int D2H_incr
)
{
  spinlock_lock(&buffer_map_lock);

  buffer_map_entry_t *entry = st_lookup(&map_root, buffer_id);
  if (!entry) {
    entry = buffer_map_entry_new(buffer_id);
    st_insert(&map_root, entry);
  }

  entry->H2D_count += H2D_incr;
  entry->D2H_count += D2H_incr;

  spinlock_unlock(&buffer_map_lock);
  return entry;
}


// called on clReleaseMemObject
void
buffer_map_delete
(
 uint64_t buffer_id
)
{
  spinlock_lock(&buffer_map_lock);

  buffer_map_entry_t *entry = st_lookup(&map_root, buffer_id);
  if (entry) {
    // in some cases, we observed clReleaseMemObject is called on H2D buffers without calls to clEnqueueWriteBuffer.
    // For avoiding a segfault for those cases, we use this if-clause
    buffer_map_entry_t *node = st_delete(&map_root, buffer_id);
    st_free(&free_list, node);
  }

  spinlock_unlock(&buffer_map_lock);
}


uint64_t
buffer_map_entry_buffer_id_get
(
 buffer_map_entry_t *entry
)
{
  return entry->buffer_id;
}


int
buffer_map_entry_D2H_count_get
(
 buffer_map_entry_t *entry
)
{
  return entry->D2H_count;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
buffer_map_count
(
 void
)
{
  return st_count(map_root);
}
