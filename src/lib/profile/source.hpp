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

#ifndef HPCTOOLKIT_PROFILE_SOURCE_H
#define HPCTOOLKIT_PROFILE_SOURCE_H

#include "pipeline.hpp"

#include "util/locked_unordered.hpp"
#include "util/ref_wrappers.hpp"

namespace hpctoolkit {

/// Base class for all sources of metric data. Not to be confused with "sample
/// sources" which are the mechanisms hpcrun uses to get any data at all.
/// Since the rest of the system really doesn't care how the data gets there,
/// but the formats could (potentially) differ widely, this uses a virtual table
/// to hide the difference.
class ProfileSource {
public:
  virtual ~ProfileSource() = default;

  /// Instantiates the proper Source for the given arguments. In time more
  /// overloadings may be added that will handle more interesting cases.
  // MT: Internally Synchronized
  static std::unique_ptr<ProfileSource> create_for(const stdshim::filesystem::path&, const stdshim::filesystem::path&);

  /// Most format errors from a Source can be handled within the Source itself,
  /// but if errors happen during construction callers (create_for) will want to
  /// know. This gives a path for that information.
  // MT: Externally Synchronized
  virtual bool valid() const noexcept;

  /// Bind this Source to a Pipeline to emit the actual bits and bobs.
  // MT: Externally Synchronized
  void bindPipeline(ProfilePipeline::Source&& se) noexcept;

  /// Query what Classes this Source can actually provide to the Pipeline.
  // MT: Safe (const)
  virtual DataClass provides() const noexcept = 0;

  /// Read in enough data to satisfy a request. Unlike the former overload, this
  /// handles cases where the implementation does not obey timeouts, or there is
  /// a better implementation if the limit is not imposed.
  // MT: Externally Synchronized
  virtual void read(const DataClass&) = 0;

  /// In many cases there are dependencies between data reads, due to the nature
  /// of the conversion. This call allows a Source to adjust its request input
  /// based on the dependencies it requires. Note that tracking of previously
  /// requested data is done automatically, so keep this simple and constant.
  virtual DataClass finalizeRequest(const DataClass&) const noexcept = 0;

protected:
  /// Destination for read data. Since Sources may have various needs and orders
  /// for their outputs, they need constant access to a "sink" for whatever they
  /// happen to pull out.
  // MT Internally Synchronized (Implicit)
  ProfilePipeline::Source sink;

  // You should never create a ProfileSource directly
  ProfileSource() = default;
};

}

#endif  // HPCTOOLKIT_PROFILE_SOURCE_H
