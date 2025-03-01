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

#ifndef LOGICAL_COMMON_H
#define LOGICAL_COMMON_H

#include "../frame.h"
#include "../utilities/ip-normalized.h"
#include "../unwind/common/unwind.h"

#ifdef ENABLE_LOGICAL_PYTHON
#include "python.h"
#endif

#include "../../../lib/prof-lean/spinlock.h"

#include <stdint.h>
#include <stdbool.h>
#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif

// --------------------------------------
// Logical stack and mutator functions
// --------------------------------------

typedef union logical_frame_t logical_frame_t;

/// Descriptor for a single "logical" region providing an alternative
/// interpretation for a series of "physical" unwind frames.
///
/// When a logical region is pushed onto the logical region stack, every sample
/// after that point is "logicalized" by having a portion of its backtrace
/// completely replaced. Specifically, the physical frames below (and including)
/// #exit/#afterexit and above #beforeenter are replaced by frames generated by
/// iteration #generator. Usually, these frames are replaced by a "logical"
/// interpretation of events, such as function calls in scripting languages.
///
/// The #generator is given access to the associated logical substack to store
/// or cache data during the "logical unwind." Frames can be pushed by other
/// callbacks onto the logical substack via #hpcrun_logical_substack_push.
typedef struct logical_region_t {
  /// The pointer(s) to the topmost physical frame(s) within the logical region.
  /// The value is the unwind cursor's `sp` field. Number of valid elements
  /// ("length") is provided in #exit_len.
  ///
  /// All frames indicated in this array and below (callers of) these will be
  /// replaced, up to #beforeenter, will be replaced during the "logicalization"
  /// of the backtrace. #afterexit can be used to replace more frames above the
  /// ones indicated in this array.
  ///
  /// Multiple frames may be listed to account for cases where the stable
  /// physical frame cannot be determined, in this case `#exit_len > 1`.
  /// If `#exit_len == 0`, all frames above #beforeenter are considered part
  /// of the logical region, i.e. we haven't "left" the logical region yet.
  /// This is allowed only if this is the topmost region on the logical region
  /// stack.
  void* exit[4];

  /// Number of valid elements ("length") of #exit. See #exit for details.
  uint8_t exit_len;

  /// Optional finalizer for #exit, allowing frames to be selected for
  /// replacement that are after (callees of) those listed in #exit.
  ///
  /// If `NULL`, acts as if `cur` was returned.
  ///
  /// \param reg Pointer to this #logical_region_t.
  /// \param cur Pointer to the topmost found frame listed in #exit.
  /// \param top Pointer to the topmost frame in the whole backtrace.
  ///            Always `<= cur`.
  /// \returns The topmost frame to replace in the backtrace. Must be between
  ///          `top` and `cur`, inclusive.
  const frame_t* (*afterexit)(struct logical_region_t* reg, const frame_t* cur,
                              const frame_t* top);

  /// The unwind cursor for the topmost physical frame NOT within this logical
  /// region. The only field used is the `sp` field.
  ///
  /// All frames above (callees of) this frame, up to those indicated by #exit
  /// and #afterexit, will be replaced during the "logicalization" of the
  /// backtrace.
  hpcrun_unw_cursor_t beforeenter;

  // Generator for the logical stack frames contained within this region.
  // Replaces `frame` with the appropriate data for the `index`'th logical frame.
  // Repeatedly called with increasing `index` until `false` is returned.
  // `store` can be used to save state during a single generation pass.
  // `logical_frame` may be NULL if there are not enough logical frames in the region.
  /// Generator for the logical frames in this logical region. This function is
  /// called multiple times to perform a "logical unwind," writing the resulting
  /// logical frames in sequence to `frame`, until `false` is returned.
  ///
  /// Note that this is considered an "unwind," so the produced `frame`s are in
  /// topmost-first order (i.e. callee before caller). Note also that at least
  /// one "logical frame" must be produced via this method.
  ///
  /// \pure
  /// \param reg Pointer to this #logical_region_t.
  /// \param store Storage for user data, the same value is passed to every call
  ///              within the same logical unwind. `*store` is initialized by
  ///              the caller to `NULL` before the first call.
  /// \param index Number of logical frames produced so far in this logical
  ///              unwind. Counts up from 0.
  /// \param logical_frame The `index`'th topmost frame in the logical substack,
  ///                      or NULL if the logical substack is not deep enough.
  /// \param[out] frame The resulting frame to include in the "logicalized"
  ///                   backtrace.
  /// \returns `true` if there are logical frames remaining to be produced by
  ///          subsequent calls, `false` otherwise.
  bool (*generator)(struct logical_region_t* reg, void** store,
                    unsigned int index, logical_frame_t* logical_frame,
                    frame_t* frame);

  /// Upper bound on the number of logical frames #generator will produce. Used
  /// to adjust memory allocations during the "logicalization" of a backtrace.
  size_t expected;

  /// Storage for the logical substack for this region. Can be used to pass data
  /// to #generator, or by #generator to cache data found during a logical
  /// unwind. Very generic.
  struct logical_frame_segment_t* subhead;

  /// Total number of valid #logical_frame_t in the logical substack (#subhead).
  size_t subdepth;

  /// Logical handler-specific data.
  union {
    uintptr_t generic;
#ifdef ENABLE_LOGICAL_PYTHON
    logical_python_region_t python;
#endif
  } specific;
} logical_region_t;

/// "Rope" node for the logical region stack. Like a linked list, but contains
/// multiple elements to reduce the total number of allocations.
struct logical_region_segment_t {
  /// Mini-stack of logical regions. Grows upward (increasing index).
  logical_region_t regions[4];

  /// Previous node for lower (older, caller) logical regions, or `NULL`.
  struct logical_region_segment_t* prev;
};

/// Stack of logical regions all active at the same time. Used internally to
/// maintain state, see #logical_region_t for the user-facing part.
typedef struct logical_region_stack_t {
  /// Total number of valid #logical_region_t in the logical stack (#head).
  size_t depth;

  /// "Rope" of #logical_region_t that are active. Valid elements are determined
  /// by the #depth.
  struct logical_region_segment_t* head;

  /// Free-list for unused "rope" nodes.
  struct logical_region_segment_t* spare;

  /// Free-list for unused "rope" nodes for logical substacks.
  struct logical_frame_segment_t* subspare;
} logical_region_stack_t;

/// A single logical "frame." Used by logical handlers to store or cache data
/// about a logical frame.
union logical_frame_t {
  uintptr_t generic;
#ifdef ENABLE_LOGICAL_PYTHON
  logical_python_frame_t python;
#endif
};

/// "Rope" node for the logical substacks. Like a linked list, but contains
/// multiple elements to reduce the total number of allocations.
struct logical_frame_segment_t {
  /// Mini-stack of logical frames. Grows upward (increasing index).
  logical_frame_t frames[16];

  /// Previous node for lower (older, caller) logical regions, or `NULL`.
  struct logical_frame_segment_t* prev;
};

/// Initialize a #logical_region_stack_t. The stack starts out empty, with no
/// contained #logical_region_t.
///
/// \relatesalso logical_region_stack_t
extern void hpcrun_logical_stack_init(logical_region_stack_t*);

/// Fetch the topmost #logical_region_t from a #logical_region_stack_t, or
/// `NULL` if empty.
///
/// \relatesalso logical_region_stack_t
/// \returns The topmost #logical_region_t, or `NULL`.
extern logical_region_t* hpcrun_logical_stack_top(const logical_region_stack_t*);

/// Push a new #logical_region_t onto a #logical_region_stack_t. Returns the
/// newly pushed #logical_region_t as stored in the stack.
///
/// \relatesalso logical_region_stack_t
/// \param reg #logical_region_t to push on the stack. The contents of the
///            pointed-to value are copied into memory managed by the stack.
/// \returns The newly pushed #logical_region_t stored in the stack.
extern logical_region_t* hpcrun_logical_stack_push(logical_region_stack_t*,
    const logical_region_t* reg);

/// Truncate a #logical_region_stack_t to a particular depth. If the stack
/// contained less #logical_region_t than `n`, returns the difference between
/// the depth and `n`, otherwise returns `0`.
///
/// The memory for the "popped" #logical_region_t remains valid after this call,
/// until the next call that modifies the #logical_region_stack_t.
///
/// \relatesalso logical_region_stack_t
/// \param n Requested depth of the stack.
/// \returns `0` if the stack was truncated by this call, otherwise the
///          difference between stack's current depth and `n`.
extern size_t hpcrun_logical_stack_settop(logical_region_stack_t*, size_t n);

/// Pop up to `n` #logical_region_t off a #logical_region_stack_t. This may
/// leave the stack empty after this call.
///
/// \relatesalso logical_region_stack_t
/// \param n Number of #logical_region_t to pop off the stack.
static inline void hpcrun_logical_stack_pop(logical_region_stack_t* s, size_t n) {
  hpcrun_logical_stack_settop(s, s->depth > n ? s->depth - n : 0);
}

/// Fetch the topmost #logical_frame_t off the logical substack for a
/// #logical_region_t, or `NULL` if empty.
///
/// \relatesalso logical_region_t
/// \returns The topmost #logical_frame_t, or `NULL`.
extern logical_frame_t* hpcrun_logical_substack_top(const logical_region_t*);

/// Push a new #logical_frame_t onto the substack of a #logical_region_t.
/// Returns the newly pushed #logical_frame_t as stored in the substack.
///
/// \relatesalso logical_region_t
/// \param rstack #logical_region_stack_t containing this #logical_region_t.
/// \param frame #logical_frame_t to push onto the substack. The contents of the
///              pointed-to value are copied into memory managed by the region.
/// \returns The newly pushed #logical_frame_t stored in the region's substack.
extern logical_frame_t*
hpcrun_logical_substack_push(logical_region_stack_t* rstack, logical_region_t*,
    const logical_frame_t* frame);

/// Truncate the substack of a #logical_region_t to a particular depth. If the
/// substack contained less #logical_frame_t than `n`, returns the difference
/// between the depth and `n`, otherwise returns `0`.
///
/// The memory for the "popped" #logical_frame_t remains valid after this call,
/// until the next call that modifies the #logical_region_t or
/// #logical_region_stack_t.
///
/// \relatesalso logical_region_t
/// \param rstack #logical_region_stack_t containing this #logical_region_t.
/// \param n Requested depth of the substack.
/// \returns `0` if the substack was truncated by this call, otherwise the
///          difference between substack's current depth and `n`.
extern size_t hpcrun_logical_substack_settop(logical_region_stack_t* rstack,
    logical_region_t*, size_t n);

/// Pop up to `n` #logical_frame_t off the substack of a #logical_region_t.
/// This may leave the substack empty after this call.
///
/// \relatesalso logical_region_t
/// \param n Number of #logical_frame_t to pop off the substack.
static inline void hpcrun_logical_substack_pop(logical_region_stack_t* s, logical_region_t* r, size_t n) {
  hpcrun_logical_substack_settop(s, r, r->subdepth > n ? r->subdepth - n : 0);
}

/// Construct the unwind cursor for the N'th caller (physical) frame. Useful for
/// filling logical_region_t::beforeenter.
///
/// \param CUR `hpcrun_unw_cursor_t*` to store the resulting unwind cursor in.
/// \param N Number of frames to unwind "up" from this (caller's) frame.
#define hpcrun_logical_frame_cursor(CUR, N) ({    \
  ucontext_t ctx;                                 \
  if (getcontext(&ctx) != 0)                       \
    hpcrun_terminate();                           \
  hpcrun_logical_frame_cursor_real(&ctx, CUR, N); \
})
__attribute__((always_inline))
static inline void hpcrun_logical_frame_cursor_real(ucontext_t* ctx, hpcrun_unw_cursor_t* cur, size_t n) {
  hpcrun_unw_init_cursor(cur, ctx);
  for(size_t i = 0; i < n; i++) {
    if(hpcrun_unw_step(cur) <= STEP_STOP)
      break;
  }
}

/// Construct the stack pointer for the N'th caller (physical) frame. Useful for
/// filling logical_region_t::exit.
///
/// \param N Number of frames to unwind "up" from this (caller's) frame.
#define hpcrun_logical_frame_sp(N) ({             \
  hpcrun_unw_cursor_t cur;                        \
  hpcrun_logical_frame_cursor(&cur, N);           \
  cur.sp;                                         \
})

// --------------------------------------
// Logical load module management
// --------------------------------------

/// Storage for logical "metadata," to be written out to a file alongside the
/// the final measurement profile. This acts as the load module for logical
/// frames, where the function or file names can only be obtained at runtime.
/// hpcprof later parses the logical metadata and uses it to properly attribute
/// metrics during analysis.
///
/// These are expected to be `static` variables and registered via
/// #hpcrun_logical_metadata_register for eventual write-out.
typedef struct logical_metadata_store_t {
  /// Lock protecting all the fields below, making many operations thread-safe.
  spinlock_t lock;

  /// Next unallocated metadata entry ID. Starts at 1 and counts up.
  uint32_t nextid;

  /// Hash table for metadata entries. Always a power of 2 array indexed by
  /// a prefix of the hash, size given in #tablesize.
  struct logical_metadata_store_entry_t* idtable;

  /// Current size of the #idtable. Always a power of 2.
  size_t tablesize;

  /// Unique string identifier used for this store. Used to separate the
  /// resulting files in the end. Must be `static const` data.
  const char* generator;

  /// Load module identifier used to refer to this particular metadata store.
  /// Don't access directly, call #hpcrun_logical_metadata_lmid instead.
#ifdef __cplusplus
  std::
#endif
  atomic_uint_fast16_t lm_id;

  /// Full path to the metadata output file.
  char* path;

  /// Pointer to the next registered metadata store. Needed for cleanup during
  /// #hpcrun_logical_fini.
  struct logical_metadata_store_t* next;
} logical_metadata_store_t;

enum logical_mangling {
  LOGICAL_MANGLING_NONE = 0,  ///< No mangling was done, the name is not mangled.
  LOGICAL_MANGLING_CPP = 1,  ///< The function name was mangled according to the C++ ABI.
};

struct logical_metadata_store_entry_t {
  char* funcname;  ///< Name of the logical function, or `NULL`.
  enum logical_mangling funcmang;  ///< Mangling used on the #funcname.
  char* filename;  ///< Path/name of the source file, or `NULL`.
  uint32_t lineno; ///< Line number of the function, or `0`.
  size_t hash;  ///< Precomputed hash value for this entry.
  uint32_t id;  ///< Unique identifier to be used in #hpcrun_logical_metadata_ipnorm.
};

/// Register the given #logical_metadata_store_t, with a unique identifier.
/// Should be called exactly once per #logical_metadata_store_t before use.
///
/// \relatesalso logical_metadata_store_t
/// \param generator Unique string identifier for this metadata store, usually
///                  the name of the logical handler using the store. Must be a
///                  `static const` value.
extern void hpcrun_logical_metadata_register(logical_metadata_store_t*,
    const char* generator);

/// Internal function for #hpcrun_logical_metadata_lmid
extern void hpcrun_logical_metadata_generate_lmid(logical_metadata_store_t*);

/// Get the load module id for a #logical_metadata_store_t. If the load module
/// for the store was not yet registered, this will lazily register it first.
///
/// \relatesalso logical_metadata_store_t
/// \returns Load module id to use for #hpcrun_logical_metadata_ipnorm to refer
///          to this metadata store.
static inline uint16_t hpcrun_logical_metadata_lmid(logical_metadata_store_t* store) {
#ifdef __cplusplus
  using namespace std;
#endif
  uint16_t ret = atomic_load_explicit(&store->lm_id, memory_order_relaxed);
  if(ret == 0) {
    spinlock_lock(&store->lock);
    ret = atomic_load_explicit(&store->lm_id, memory_order_relaxed);
    if(ret == 0) {
      hpcrun_logical_metadata_generate_lmid(store);
      ret = atomic_load_explicit(&store->lm_id, memory_order_relaxed);
    }
    spinlock_unlock(&store->lock);
  }
  return ret;
}

/// Get a unique identifier for a particular logical lexical construct. Usually
/// passed to #hpcrun_logical_metadata_ipnorm.
///
/// The lexical construct described depends on `func`, `file` and `lineno`:
///  - `(NULL, NULL, <ignored>)` indicates an unknown logical region (id 0),
///  - `(NULL, "file", <ignored>)` indicates a source file "file",
///  - `("func", NULL, <ignored>)` indicates a function "func" with no source
///    file, and
///  - `("func", "file", lineno)` indicates a function "func" defined in "file"
///    at line `lineno`.
///
/// \relatesalso logical_metadata_store_t
/// \param func Function name, or `NULL`.
/// \param funcmang Mangling used for `func`.
/// \param file File path/name, or `NULL`.
/// \param lineno Line number of the described function, or `0`.
/// \returns A unique identifier for the described lexical construct.
extern uint32_t hpcrun_logical_metadata_fid(logical_metadata_store_t*,
    const char* func, enum logical_mangling funcmang, const char* file, uint32_t lineno);

/// Compose an #ip_normalized_t to represent a logical source line or lexical
/// construct. The resulting normalized IP can be recorded in the backtrace by a
/// logical_region_t::generator to represent logical frames.
///
/// \relatesalso logical_metadata_store_t
/// \param fid Identifier for the lexical construct containing the source line,
///            returned from #hpcrun_logical_metadata_fid.
/// \param lineno Line number of the source line, or `0` if no specific line.
/// \returns The appropriate normalized IP to record in the backtrace.
static inline ip_normalized_t hpcrun_logical_metadata_ipnorm(
    logical_metadata_store_t* store, uint32_t fid, uint32_t lineno) {
  ip_normalized_t ip = {
    .lm_id = hpcrun_logical_metadata_lmid(store), .lm_ip = fid,
  };
  ip.lm_ip = (ip.lm_ip << 32) + lineno;
  return ip;
}

/// Get metadata's path
extern const char *hpcrun_logical_metadata_path_get(logical_metadata_store_t *store);

// --------------------------------------
// Hook registration functions
// --------------------------------------

/// Initialize all enabled logical attribution sub-systems.
extern void hpcrun_logical_init();

/// Register the handler needed to modify backtraces for logical attribution.
extern void hpcrun_logical_register();

/// Finalize all enabled logical attribution sub-systems.
extern void hpcrun_logical_fini();

#endif  // LOGICAL_COMMON_H
