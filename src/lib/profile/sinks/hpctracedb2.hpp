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

#ifndef HPCTOOLKIT_PROFILE_SINKS_HPCTRACEDB2_H
#define HPCTOOLKIT_PROFILE_SINKS_HPCTRACEDB2_H

#include "../sink.hpp"

#include "../util/file.hpp"

#include <chrono>
#include <shared_mutex>

namespace hpctoolkit::sinks {

class HPCTraceDB2 final : public ProfileSink {
public:
  ~HPCTraceDB2() = default;

  /// Constructor, with a reference to the output database directory.
  HPCTraceDB2(const stdshim::filesystem::path&);

  /// Write out as much data as possible. See ProfileSink::write.
  void write() override;

  DataClass wavefronts() const noexcept override { return DataClass::threads; }

  DataClass accepts() const noexcept override {
    using ds = DataClass;
    return ds::attributes | ds::threads | ds::contexts | ds::ctxTimepoints;
  }

  ExtensionClass requirements() const noexcept override {
    using es = ExtensionClass;
    return es::identifier;
  }

  void notifyPipeline() noexcept override;

  void notifyWavefront(DataClass) override;
  void notifyThread(const Thread&) override;
  void notifyTimepoints(const Thread&, const std::vector<
    std::pair<std::chrono::nanoseconds, std::reference_wrapper<const Context>>>&) override;
  void notifyCtxTimepointRewindStart(const Thread&) override;
  void notifyThreadFinal(std::shared_ptr<const PerThreadTemporary>) override;

private:
  std::optional<hpctoolkit::util::File> tracefile;
  bool has_traces;
  size_t totalNumTraces;
  uint64_t footerPos;

  struct uds;

  class traceHdr {
  public:
    traceHdr(const Thread&, HPCTraceDB2& tdb);
    ~traceHdr() = default;

    uint32_t prof_info_idx;
    uint64_t start;
    uint64_t end;
  };

  class udContext {
  public:
    udContext(const Context&, HPCTraceDB2& tdb) : uds(tdb.uds), used(false) {};
    ~udContext() = default;

    struct uds& uds;
    std::atomic<bool> used;
  };

  class udThread {
  public:
    udThread(const Thread&, HPCTraceDB2&);
    ~udThread() = default;

    struct uds& uds;
    bool has_trace = false;

    traceHdr hdr;
    std::optional<hpctoolkit::util::File::Instance> inst;
    std::uint_fast64_t off = -1;
    std::array<char, 12 * 1024 * 86> buffer;
    size_t buffer_cursor = 0;
    uint64_t tmcntr = 0;
    bool lastWasBlank = false;

    std::shared_mutex prebuffer_lock;
    bool prebuffer_done = false;
    bool hdr_prebuffered = false;
    std::vector<char> prebuffer;
  };

  struct uds {
    Context::ud_t::typed_member_t<udContext> context;
    Thread::ud_t::typed_member_t<udThread> thread;
  } uds;

  void writeHdrFor(udThread&, util::File::Instance&);


  //***************************************************************************
  // trace_hdr
  //***************************************************************************
  #define INVALID_HDR    -1
  #define MULTIPLE_8(v) ((v + 7) & ~7)

  uint64_t getTotalNumTraces();
  std::vector<uint64_t> calcStartEnd();
  void assignHdrs(const std::vector<uint64_t>& trace_offs);

  template <typename T>
  void exscan(std::vector<T>& data);


};

}

#endif  // HPCTOOLKIT_PROFILE_SINKS_HPCTRACEDB2_H
