// -*-Mode: C++;-*-

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

#ifndef HPCTOOLKIT_PROFILE_UTIL_STABLE_HASH_H
#define HPCTOOLKIT_PROFILE_UTIL_STABLE_HASH_H

#include <xxhash.h>

#include <type_traits>
#include <cstdint>
#include <array>
#include <string>
#include <string_view>
#include <memory>

/// \file
/// Alternative to std::hash that is guaranteed to be stable and consistent
/// across machines, while still being fast and usable for hash tables.

namespace hpctoolkit::util {

/// State machine used to collect and hash arbitrary input data. Unlike
/// std::hash where each intermediate hash function is based on lower std::hash
/// functions, this object collects all the bits directly and generates the hash.
///
/// Internally, uses a sponge construction with 256-bit state, 64-bit capacity.
class stable_hash_state {
public:
  stable_hash_state();
  ~stable_hash_state() = default;

  stable_hash_state(const stable_hash_state&);
  stable_hash_state& operator=(const stable_hash_state&);
  stable_hash_state(stable_hash_state&&) = default;
  stable_hash_state& operator=(stable_hash_state&&) = default;

  /// Squeeze out the final hash from the collected state. Calling this multiple
  /// times without any other changes will return the same value.
  std::uint64_t squeeze() noexcept;

  /// Inject hashable data into the hash-state.
  stable_hash_state& operator<<(std::uint64_t v) noexcept {
    std::array<std::uint8_t, 8> buf;
    for(std::size_t i = 0, o = 0; i < buf.size(); ++i, o += 8)
      buf[i] = (v >> o) & 0xFF;
    return inject(buf.data(), buf.size());
  }

  stable_hash_state& operator<<(std::uint32_t v) noexcept {
    std::array<std::uint8_t, 4> buf;
    for(std::size_t i = 0, o = 0; i < buf.size(); ++i, o += 8)
      buf[i] = (v >> o) & 0xFF;
    return inject(buf.data(), buf.size());
  }

  stable_hash_state& operator<<(std::uint16_t v) noexcept {
    std::array<std::uint8_t, 2> buf;
    for(std::size_t i = 0, o = 0; i < buf.size(); ++i, o += 8)
      buf[i] = (v >> o) & 0xFF;
    return inject(buf.data(), buf.size());
  }

  stable_hash_state& operator<<(std::uint8_t v) noexcept {
    return inject(&v, 1);
  }

  stable_hash_state& operator<<(std::string_view v) noexcept {
    return inject(v.data(), v.size());
  }

  stable_hash_state& operator<<(const std::string& v) noexcept {
    return operator<<(std::string_view(v));
  }

private:
  stable_hash_state& inject(const char* data, std::size_t size) noexcept;
  stable_hash_state& inject(const std::uint8_t* data, std::size_t size) noexcept;

  struct deleter {
    void operator()(XXH3_state_t*);
  };

  std::unique_ptr<XXH3_state_t, deleter> state;
};

/// Drop-in replacement for std::hash using the stable hash functions.
template<class T>
struct stable_hash {
  std::size_t operator()(const T& v) const noexcept {
    stable_hash_state s;
    s << v;
    return (std::size_t)s.squeeze();
  };
};

}  // namespace hpctoolkit::util

#endif  // HPCTOOLKIT_PROFILE_UTIL_STABLE_HASH_H
