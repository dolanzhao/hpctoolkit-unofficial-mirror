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

//******************************************************************************
// system includes
//******************************************************************************

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <libelf.h>

#include "igc_binary_decoder.h"
#include "gen_symbols_decoder.h"



//******************************************************************************
// local includes
//******************************************************************************

#include "../../prof-lean/crypto-hash.h"
#include "../ElfHelper.hpp"
#include "../../support/diagnostics.h"
#include "../../support/RealPathMgr.hpp"
#include "IntelGPUBinutils.hpp"


//******************************************************************************
// macros
//******************************************************************************

#define DBG 1

#define INTEL_GPU_DEBUG_SECTION_NAME "Intel(R) OpenCL Device Debug"



//******************************************************************************
// private operations
//******************************************************************************

static __attribute__((unused)) const char *
opencl_elf_section_type
(
  Elf64_Word sh_type
)
{
  switch (sh_type) {
    case SHT_OPENCL_SOURCE:
      return "SHT_OPENCL_SOURCE";
    case SHT_OPENCL_HEADER:
      return "SHT_OPENCL_HEADER";
    case SHT_OPENCL_LLVM_TEXT:
      return "SHT_OPENCL_LLVM_TEXT";
    case SHT_OPENCL_LLVM_BINARY:
      return "SHT_OPENCL_LLVM_BINARY";
    case SHT_OPENCL_LLVM_ARCHIVE:
      return "SHT_OPENCL_LLVM_ARCHIVE";
    case SHT_OPENCL_DEV_BINARY:
      return "SHT_OPENCL_DEV_BINARY";
    case SHT_OPENCL_OPTIONS:
      return "SHT_OPENCL_OPTIONS";
    case SHT_OPENCL_PCH:
      return "SHT_OPENCL_PCH";
    case SHT_OPENCL_DEV_DEBUG:
      return "SHT_OPENCL_DEV_DEBUG";
    case SHT_OPENCL_SPIRV:
      return "SHT_OPENCL_SPIRV";
    case SHT_OPENCL_NON_COHERENT_DEV_BINARY:
      return "SHT_OPENCL_NON_COHERENT_DEV_BINARY";
    case SHT_OPENCL_SPIRV_SC_IDS:
      return "SHT_OPENCL_SPIRV_SC_IDS";
    case SHT_OPENCL_SPIRV_SC_VALUES:
      return "SHT_OPENCL_SPIRV_SC_VALUES";
    default:
      return "unknown type";
  }
}



//******************************************************************************
// interface operations
//******************************************************************************

#ifdef ENABLE_IGC
bool
findIntelGPUBins
(
 const std::string &file_name,
 const char *file_buffer,
 size_t file_size,
 ElfFileVector *filevector
)
{
  const char *ptr = file_buffer;

  // make sure the file is long enough to contain a patch token binary header
  if (file_size < sizeof(SProgramDebugDataHeaderIGC)) return false;

  const SProgramDebugDataHeaderIGC* header =
    reinterpret_cast<const SProgramDebugDataHeaderIGC*>(ptr);

  // check for expected magic number
  if (header->Magic != MAGIC_CL) return false;

  ptr += sizeof(SProgramDebugDataHeaderIGC);

  if (header->NumberOfKernels == 0) {
    return false;
  }

  for (uint32_t i = 0; i < header->NumberOfKernels; ++i) {
    const SKernelDebugDataHeaderIGC *kernel_header =
      reinterpret_cast<const SKernelDebugDataHeaderIGC*>(ptr);
    ptr += sizeof(SKernelDebugDataHeaderIGC);
    std::string kernel_name(ptr);

    unsigned kernel_name_size_aligned = sizeof(uint32_t) *
      (1 + (kernel_header->KernelNameSize - 1) / sizeof(uint32_t));
    ptr += kernel_name_size_aligned;

    if (kernel_header->SizeVisaDbgInBytes > 0) {
      size_t kernel_size = kernel_header->SizeVisaDbgInBytes;
      char *kernel_buffer = (char *)malloc(kernel_size);
      memcpy(kernel_buffer, ptr, kernel_size);

      // Compute hash for the kernel name
      char kernel_name_hash[CRYPTO_HASH_STRING_LENGTH];
      crypto_compute_hash_string(kernel_name.c_str(),
                                 kernel_name.size(),
                                 kernel_name_hash, sizeof(kernel_name_hash));

      std::string real_kernel_name =
        file_name + "." + std::string((char *)kernel_name_hash);

      auto elf_file = new ElfFile;
      if (elf_file->open(kernel_buffer, kernel_size, real_kernel_name)) {
        FILE *fptr = fopen(real_kernel_name.c_str(), "wb");
        fwrite(kernel_buffer, sizeof(char), kernel_size, fptr);
        fclose(fptr);

        elf_file->setGPUKernelName(kernel_name);
        filevector->push_back(elf_file);
      } else {
        // kernel_buffer is released with elf_file
        delete elf_file;
      }
    } else {
      // Kernel does not have debug info
      return false;
    }

    ptr += kernel_header->SizeVisaDbgInBytes;
    ptr += kernel_header->SizeGenIsaDbgInBytes;
  }

  return true;
}

#endif // ENABLE_IGC
