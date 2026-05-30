#pragma once

#include <unordered_map>
#include <map>
#include <vector>
#include <queue>

#include "abstract_dependency.hpp"
#include "ord_dep_graph.hpp"
#include "event.hpp"
#include "critical_section_history.hpp"
#include "lockset.hpp"
#include "lru.hpp"

// HELPER STRUCTURES
using SyncStatusT = std::variant<NodeConstItT, ResourceIdT>;

// Custom hasher for SyncStatus
//TODO: Can't this have collisions?
struct SyncStatusHash {
  std::size_t operator()(const SyncStatusT& sync_status) const {
    if (std::holds_alternative<ResourceIdT>(sync_status)) {
        return std::hash<ResourceIdT>{}(std::get<ResourceIdT>(sync_status));
    }

    return IteratorHasher()(std::get<NodeConstItT>(sync_status));
  }
};

using RecentSyncStatusContT = CircularLRU<SyncStatusT, 8, SyncStatusHash>;

struct ThreadInfo{
  UReentrantLocksetT u_reen_lockset;
  RecentSyncStatusContT recent_sync_status_cont;
  VectorClock vec_clock;
  bool is_asleep = false;

  ThreadInfo(ThreadIdT tid, size_t lock_depth) : vec_clock(tid), u_reen_lockset(lock_depth){};
};

// Contains: Timestamp of notify event and the number of threads that should receive the notif
using NotifQueue = std::queue<std::pair<VectorClock, uint32_t>>;

struct CVInfo{
  NotifQueue notif_queue;
  size_t to_notify_count = 0; // The number of threads that still need to be notified

  void sleep_thread();
  void notify_thread(const VectorClock& notif_vc, bool notif_all);
  void wake_thread();
};

// ACTUAL EVENT HANDLER
struct EventHandler{
  // OUTPUT

  // GRAPH RELATED OUTPUT
  AbsDepContainerT abs_deps;

  // Intermediary step that helps to build the neighbour list of the graph
  LockDepMapT lock_dep_map;
  
  // DEADLOCK CHECKING RELATED OUPUT
  CSHist cs_hist;
  DepLocToEvMapT dep_loc_ev_map;

  // INTERNAL STUFF

  // These use the id of the thread/var as index in the respective vectors
  size_t alive_th_count;
  std::vector<ThreadInfo> thread_map;
  std::vector<VectorClock> last_write;
  std::unordered_map<ResourceIdT, CVInfo> cv_map;

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

  // Helper function that creates (and adds) a new dependency to the contaianer
  AbsDepConstItT create_dep(ThreadIdT tid, ResourceIdT desired_res, const LocksetT& lockset,
                            SrcLocT src_loc, const Event& evt);
  
  // Helper function that updates old_dep to contain new_res in it's lockset
  AbsDepConstItT update_dep(AbsDepConstItT old_dep, ResourceIdT new_res);

  // Wakes up the thread if asleep and merges the corresponding notify event timestamp
  void handle_sleepness(ThreadInfo& th_info, ResourceIdT ass_lock_id);

  // Helper that creates dependency(if needed) and adds it to the recent status of the thread (if needed)
  void handle_dep_creation(ThreadInfo& th_info, const EventInfo& evt_info, const Event& evt);

  void print_abs_deps() const;
  void print_comm_abs_deps() const;
  void print_lock_deps_map() const;

  void print_summary(std::FILE* log_file) const;
  void print_summary() const;

  void print_th_exit_with_locks() const;
  void print_th_vc_info() const;
  void print_th_lockset_info() const;
};