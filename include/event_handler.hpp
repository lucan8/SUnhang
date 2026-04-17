#pragma once

#include <unordered_map>
#include <map>
#include <vector>
#include <fstream>
#include "predictor_types.hpp"
#include "ord_dep_graph.hpp"
#include "vectorclock.hpp"

struct EventHandler{
  // RESULTS
  OrdDepGraphView graph_view;
  CSHist cs_hist;

  // INTERNAL STUFF
  std::unordered_map<ThreadIdT, ThreadInfo> thread_map;
  std::unordered_map<ResourceIdT, VectorClock> last_write;

  // Intermediary step that helps to build the neighbour list of the graph
  // std::unordered_map<ResourceIdT, NodeChainT> lock_dep_map;
   std::unordered_map<ResourceIdT, NodeUSetT> lock_dep_map;

  // Statistical info
  uint32_t acq_count = 0;

  // Calls handler associated with evt_info.event_type
  // Return true if event if valid, false otherwise
  bool handle_event(const EventInfo& evt_info);
  
  void read_event(const EventInfo& evt_info);
  void write_event(const EventInfo& evt_info);

  void acquire_event(const EventInfo& evt_info);
  void release_event(const EventInfo& evt_info);
  
  void wait_event(const EventInfo& evt_info);
  void notify_event(const EventInfo& evt_info);
  void notify_all_event(const EventInfo& evt_info);
  
  void fork_event(const EventInfo& evt_info);
  void join_event(const EventInfo& evt_info);

  // Helper function that creates (and adds) a new dependency to the graph
  NodeConstItT create_dep(ThreadIdT tid, ResourceIdT desired_res, const LocksetT& lockset, const Event& evt);
  NodeConstItT update_dep(NodeConstItT old_dep, ResourceIdT new_res);

  void build_neigh_list();

  void print_abs_deps() const;
  void print_comm_abs_deps() const;
  void print_lock_deps_map() const;
  void print_neigh_list() const;

  void print_abs_deps(std::FILE* out_file) const;
  void print_neigh_list(std::FILE* out_file) const;

  void print_summary(std::FILE* log_file) const;
};