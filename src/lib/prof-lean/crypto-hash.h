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

//***************************************************************************
//
// File:
//   crypto-hash.h
//
// Purpose:
//   compute a cryptographic hash of a sequence of bytes. this is used
//   to name information presented to hpcrun in memory (e.g. a GPU binary)
//   that needs to be saved for post-mortem analysis.
//
//***************************************************************************

#ifndef _HPCTOOLKIT_CRYPTO_HASH_H_
#define _HPCTOOLKIT_CRYPTO_HASH_H_

#define MD5_HASH_NBYTES 16

#define CRYPTO_HASH_LENGTH MD5_HASH_NBYTES

#define CRYPTO_HASH_STRING_LENGTH (1 + (CRYPTO_HASH_LENGTH << 1))

//*****************************************************************************
// interface operations
//*****************************************************************************

#if defined(__cplusplus)
extern "C" {
#endif


//-----------------------------------------------------------------------------
// function:
//   crypto_compute_hash
//
// arguments:
//   input:
//     pointer to a vector of bytes that will be cryptographically hashed
//   input_length:
//     length in bytes of the input
//   hash:
//     pointer to a vector of bytes of length >= crypto_hash_length()
//
// return value:
//   0: success
//   non-zero: failure
//-----------------------------------------------------------------------------
int
crypto_compute_hash
(
  const void *input,
  size_t input_bytes,
  unsigned char *hash,
  unsigned int hash_length
);


//-----------------------------------------------------------------------------
// function:
//   crypto_hash_to_hexstring
//
// arguments:
//   hash:
//     pointer to cryptographic hash computed by cryto_hash_compute
//   hash_string:
//     pointer to character buffer where string equivalent of the hash code
//     will be written
//   hash_string_length:
//     length of the hash string must be > 2 * crypto_hash_length()
//
// return value:
//   0: success
//   non-zero: failure
//-----------------------------------------------------------------------------
int
crypto_hash_to_hexstring
(
  const unsigned char *hash,
  char *hash_string,
  unsigned int hash_string_length
);


//-----------------------------------------------------------------------------
// function:
//   crypto_compute_hash_string
//
// arguments:
//   data:
//     pointer to data to hash
//   data_size:
//     length of data in bytes
//   hash_string:
//     pointer to result string from hashing data bytes
//   hash_string_length:
//     length of the hash string must be >= CRYPTO_HASH_STRING_LENGTH
//
// return value:
//   0: success
//   non-zero: failure
//-----------------------------------------------------------------------------
int
crypto_compute_hash_string
(
 const void *data,
 size_t data_bytes,
 char *hash_string,
 unsigned int hash_string_length
);


#if defined(__cplusplus)
}
#endif

#endif
