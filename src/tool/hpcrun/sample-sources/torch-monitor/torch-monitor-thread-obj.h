#ifndef TORCH_MONITOR_THREAD_OBJ_H
#define TORCH_MONITOR_THREAD_OBJ_H

#include "../../cct/cct.h"
#include <stdbool.h>
#include <torch_monitor.h>

typedef struct torch_monitor_thread_obj {
  torch_monitor_thread_state_t thread_state;
  torch_monitor_domain_t domain;

  bool python_states_cached;
  size_t python_max_num_states;
  size_t python_cur_num_states;
  torch_monitor_python_state_t *python_states;

  ip_normalized_t function_ip_norm;
  cct_node_t *function_cct;
  cct_node_t *prev_cct;
} torch_monitor_thread_obj_t;

torch_monitor_thread_obj_t *torch_monitor_thread_obj_get();

#endif  // TORCH_MONITOR_THREAD_OBJ_H
