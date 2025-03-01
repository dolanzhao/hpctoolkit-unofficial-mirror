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

#define _GNU_SOURCE

#include <string.h>
#include <stdbool.h>



static char *eventsToExclude[] =  { "realtime", "retcnt" };
static const int numEventsToExclude = 2;

/***
 * Check if an event needs to be excluded or not.
 * @param event
 * @return true if the event has to be excluded, false otherwise
 */
bool is_event_to_exclude(char *event)
{
  for(int i=0; i<numEventsToExclude ; i++) {
    if (strcmp(event, eventsToExclude[i]) == 0) {
      return true;
    }
  }
  return false;
}

#ifdef UNIT_TEST_THIS
#include <stdio.h>

int main() {
  char *events[] = { "re", "realtime", "ret", "retcnt", "", "//" };

  for(int i=0; i<6; i++) {
    printf("check %s : %d\n", events[i], is_event_to_exclude(events[i]));
  }
}
#endif
