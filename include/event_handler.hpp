#pragma once

#include <unordered_map>
#include <map>
#include <vector>
#include <fstream>
#include "predictor_types.hpp"
#include "ord_dep_graph.hpp"
#include "vectorclock.hpp"


struct CVInfo{
  NotifQueue notif_queue;
  size_t to_notify_count = 0; // The number of threads that still need to be notified

  void sleep_thread(){
    to_notify_count += 1;
  }

  void notify_thread(const VectorClock& notif_vc, bool notif_all){
    if (to_notify_count == 0){
      return;
    }

    size_t notified_th_count = notif_all ? to_notify_count : 1;
    notif_queue.emplace(notif_vc, notified_th_count);
    to_notify_count -= notified_th_count;
  }

  void wake_thread(){
    if (notif_queue.empty()){
      return;
    }

    notif_queue.front().second -= 1;
    if (notif_queue.front().second == 0){
      notif_queue.pop();
    }
  }
};

struct EventHandler{
  // RESULTS
  OrdDepGraphView graph_view;
  CSHist cs_hist;
  NodeLocToEvMapT dep_loc_map;

  // INTERNAL STUFF

  // These use the id of the thread/var as index in the respective vectors
  size_t alive_th_count;
  std::vector<ThreadInfo> thread_map;
  std::vector<VectorClock> last_write;
  std::unordered_map<ResourceIdT, CVInfo> cv_map;

  // Intermediary step that helps to build the neighbour list of the graph
  // std::unordered_map<ResourceIdT, NodeUSetT> lock_dep_map;
  std::unordered_map<ResourceIdT, SortedVector<NodeConstItT, NodeConstItTComp>> lock_dep_map;

  // Statistical info
  uint32_t acq_count = 0;

  EventHandler(size_t thread_count, size_t var_count);
  // Calls handler associated with evt_info.event_type
  // Return true if event if valid, false otherwise
  bool handle_event(const EventInfo& evt_info);

  void read_event(const EventInfo& evt_info);
  void write_event(const EventInfo& evt_info);

  void acquire_event(const EventInfo& evt_info);
  void release_event(const EventInfo& evt_info);
  
  void wait_event(const EventInfo& evt_info);
  void notify_event(const EventInfo& evt_info, bool notify_all);
  
  void fork_event(const EventInfo& evt_info);
  void join_event(const EventInfo& evt_info);

  // Helper function that creates (and adds) a new dependency to the graph
  NodeConstItT create_dep(ThreadIdT tid, ResourceIdT desired_res, const LocksetT& lockset, const Event& evt);
  NodeConstItT update_dep(NodeConstItT old_dep, ResourceIdT new_res);

  void build_neigh_list();

  // Wakes up the thread if asleep and merges the corresponding notify event timestamp
  void handle_sleepness(ThreadInfo& th_info, ResourceIdT ass_lock_id);

  // Helper that creates dependency(if needed) and adds it to the recent status of the thread (if needed)
  void handle_dep_creation(ThreadInfo& th_info, const EventInfo& evt_info, const Event& evt);

  void print_abs_deps() const;
  void print_comm_abs_deps() const;
  void print_lock_deps_map() const;
  void print_neigh_list() const;

  void print_abs_deps(std::FILE* out_file) const;
  void print_neigh_list(std::FILE* out_file) const;

  void print_summary(std::FILE* log_file) const;
  void print_summary() const;

  void print_th_exit_with_locks() const;
  void print_th_vc_info() const;
  void print_th_lockset_info() const;
};