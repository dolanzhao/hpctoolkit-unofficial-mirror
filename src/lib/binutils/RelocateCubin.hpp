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

//***************************************************************************
//
// File: relocate_cubin.hpp
//
// Purpose:
//   Interface definition for in-memory cubin relocation.
//
// Description:
//   The associated implementation performs in-memory relocation of a cubin so
//   that all text segments and functions are non-overlapping. Following
//   relocation
//     - each text segment begins at its offset in the segment
//     - each function, which is in a unique text segment, has its symbol
//       adjusted to point to the new position of the function in its relocated
//       text segment
//     - addresses in line map entries are adjusted to account for the new
//       offsets of the instructions to which they refer
//
//***************************************************************************

#ifndef __RelocateCubin_hpp__
#define __RelocateCubin_hpp__

#include "ElfHelper.hpp"

bool
relocateCubin
(
 char *cubin_ptr,
 size_t cubin_size,
 Elf *cubin_elf
);

#endif
