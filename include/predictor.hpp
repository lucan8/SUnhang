#pragma once

#include <unordered_map>
#include <map>
#include <vector>

#include "predictor_types.hpp"
#include "ord_dep_graph.hpp"
#include "vectorclock.hpp"

struct Predictor{
  OrdDepGraphView graph_view;
  CSHist cs_hist;

  std::unordered_map<ThreadIdT, ThreadInfo> thread_map;
  std::unordered_map<ResourceIdT, VectorClock> last_write;

  // Intermediary step that helps to build the neighbour list of the graph
  std::unordered_map<ResourceIdT, NodeChainT> lock_dep_map;

  // Statistical info
  uint32_t acq_count = 0;

  // Calls handler associated with evt.event_type
  // Return true if event if valid, false otherwise
  bool handle_event(const EventInfo& evt);
  
  void read_event(const EventInfo& evt);
  void write_event(const EventInfo& evt);

  void acquire_event(const EventInfo& evt);
  void release_event(const EventInfo& evt);
  
  void fork_event(const EventInfo& evt);
  void join_event(const EventInfo& evt);

  void build_neigh_list();

  void print_abs_deps() const;
  void print_lock_deps_map() const;
  void print_neigh_list() const;
};