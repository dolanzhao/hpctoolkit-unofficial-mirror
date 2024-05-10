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

#include "module.hpp"

#include "util/stable_hash.hpp"

using namespace hpctoolkit;

Module::Module(ud_t::struct_t& rs)
  : userdata(rs, std::cref(*this)) {}
Module::Module(Module&& m)
  : userdata(std::move(m.userdata), std::cref(*this)),
    u_path(std::move(m.u_path)),
    u_relative_path(std::move(m.u_relative_path)) {}
Module::Module(ud_t::struct_t& rs, stdshim::filesystem::path p)
  : userdata(rs, std::cref(*this)), u_path(std::move(p)) {}
Module::Module(ud_t::struct_t& rs, stdshim::filesystem::path p, stdshim::filesystem::path rp)
  : userdata(rs, std::cref(*this)), u_path(std::move(p)), u_relative_path(std::move(rp)) {}

util::stable_hash_state& hpctoolkit::operator<<(util::stable_hash_state& h, const Module& m) noexcept {
  return h << m.path();
}
