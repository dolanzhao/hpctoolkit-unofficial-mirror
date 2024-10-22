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

#ifndef HPCTOOLKIT_PROFILE_SINKS_PACKED_H
#define HPCTOOLKIT_PROFILE_SINKS_PACKED_H

#include "../sink.hpp"

#include "../finalizer.hpp"

#include "../util/parallel_work.hpp"
#include "../util/ref_wrappers.hpp"

#include <atomic>
#include <vector>

namespace hpctoolkit::sinks {

/// Base class for ProfileSinks that output byte-packed data. After packing,
/// this data can be saved or passed to another machine, and read in with the
/// corresponding method of sources::Packed.
class Packed : public ProfileSink {
public:
  Packed();
  ~Packed() = default;

  /// Simple Finalizer that prevents any later Finalizer from classifying.
  /// Needed when using Packed due to limitations in the format used.
  class DontClassify final : public ProfileFinalizer {
  public:
    DontClassify() = default;
    ~DontClassify() = default;

    ExtensionClass provides() const noexcept override {
      return ExtensionClass::classification;
    }
    ExtensionClass requirements() const noexcept override {
      return {};
    }

    std::optional<std::pair<util::optional_ref<Context>, Context&>>
    classify(Context& ancestor, NestedScope&) noexcept override {
      return {{std::nullopt, ancestor}};
    }
  };

  // This Sink uses the unique identifiers to make associations. Subclasses
  // should take care to append this to their own overrides.
  ExtensionClass requirements() const noexcept override {
    return ExtensionClass::identifier;
  }

protected:
  std::vector<std::reference_wrapper<const Metric>> metrics;

  /// Packs the available `attributes` data on the end of the given vector.
  /// Fills `metrics` with the proper Metric order.
  // MT: Externally Synchronized
  void packAttributes(std::vector<std::uint8_t>&) noexcept;

  std::unordered_map<const Module*, std::uint64_t> moduleIDs;

  /// Packs the available `references` data on the end of the given vector.
  /// Fills `moduleIDs` with the Module id mapping.
  // MT: Externally Synchronized
  void packReferences(std::vector<std::uint8_t>&) noexcept;

  /// Packs the available `contexts` data on the end of the given vector.
  // MT: Externally Synchronized
  void packContexts(std::vector<std::uint8_t>&) noexcept;

  /// Packs the available `metrics` data on the end of the given vector.
  /// Note that this packs the statistic accumulators, not the input metrics.
  /// Note also that this relies on identifiers being the same as on the
  /// reading end.
  // MT: Externally Synchronized
  void packMetrics(std::vector<std::uint8_t>&) noexcept;

  /// Packs the available `*Timepoints` data on the end of the given vector.
  /// Note that this does not actual pack the traces, just the bounds.
  // MT: Externally Synchronized
  void packTimepoints(std::vector<std::uint8_t>&) noexcept;
};

/// Extension of Packed that uses parallel algorithms for packing contexts and
/// metrics dataclasses.
class ParallelPacked : public Packed {
public:
  /// TODO: Implement `doContexts` once a parallel Context format is written
  /// If `doMetrics` is false, calling `packMetrics` is an error.
  ParallelPacked(bool doContexts, bool doMetrics);
  ~ParallelPacked() = default;

  // Make sure to call these for subclass overrides
  void notifyPipeline() noexcept override;
  void notifyContext(const Context&) override;

protected:
  /// Packs the available `attributes` data on the end of the given vector.
  /// Fills `metrics` with the proper Metric order.
  // MT: Externally Synchronized
  void packAttributes(std::vector<std::uint8_t>&) noexcept;

  /// Packs the available `metrics` data on the end of the given vector.
  /// Note that this packs the statistic accumulators, not the input metrics.
  /// Note also that this relies on identifiers being the same as on the
  /// reading end.
  /// Must only be called after the `write()` barrier. Can only be called once.
  // MT: Externally Synchronized, Internally Synchronized with helpPackMetrics()
  void packMetrics(std::vector<std::uint8_t>&) noexcept;

  /// Help a packMetrics call in another thread.
  // MT: Internally Synchronized
  util::WorkshareResult helpPackMetrics() noexcept;

private:
  bool doContexts;
  bool doMetrics;
  std::size_t bytesPerCtx;

  // Round-robin parallel buffers for Context groups
  std::atomic<std::size_t> ctxCnt;
  std::vector<std::mutex> groupLocks;
  std::vector<std::vector<std::reference_wrapper<const Context>>> packMetricsGroups;

  // Parallel workshares for the Context groups
  std::uint8_t* output = nullptr;
  util::ParallelForEach<std::pair<std::size_t,
      std::vector<std::reference_wrapper<const Context>>>> fePackMetrics;

  void packMetricGroup(std::pair<std::size_t, std::vector<std::reference_wrapper<const Context>>>&) noexcept;
};

}

#endif  // HPCTOOLKIT_PROFILE_SINKS_PACKED_H
