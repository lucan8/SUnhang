#pragma once

// TODO: For CSHist, look into adding making the functions return const references as well
#include <string>
#include <vector>
#include <format>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <cassert>
#include <variant>
#include <map>
#include "vectorclock.hpp"
#include "comm_types.hpp"
#include "util.hpp"


// Format for LocksetT
template <>
struct std::formatter<LocksetT> : std::formatter<std::string> {
    auto format(const LocksetT& lockset, format_context& ctx) const {
        auto out = ctx.out();
        for (const auto& res_id : lockset)
          std::format_to(out, "{}, ", res_id);
        return out;
    }
};

// Event stuff
enum class EventsT {
  RD = 1,
  WR = 2,
  FORK = 3,
  JOIN = 4,
  LK = 5,
  UK = 6,
  WAIT = 7,
  NOTIFY = 8,
  NOTIFYALL = 9
};

// Formats EventsT
// Needed when calling std::format
template <>
struct std::formatter<EventsT> : std::formatter<std::string> {
    auto format(EventsT e, format_context& ctx) const {
        std::string name;
        switch (e) {
            case EventsT::RD: name = "r"; break;
            case EventsT::WR:  name = "w"; break;
            case EventsT::FORK: name = "fork"; break;
            case EventsT::JOIN: name = "join"; break;
            case EventsT::LK: name = "acq"; break;
            case EventsT::UK: name = "rel"; break;
            case EventsT::WAIT: name = "wait"; break;
            case EventsT::NOTIFY: name = "notify"; break;
            case EventsT::NOTIFYALL: name = "notifyAll"; break;
            default: name = "UNKNOWN"; break;
        }
        return formatter<std::string>::format(name, ctx);
    }
};

struct EventInfo{
  ThreadIdT thread_id;
  EventsT event_type;
  ResourceIdT target;
  SrcLocT src_loc;
  TracePosT line; // line in trace file 

  EventInfo(){}
  EventInfo(ThreadIdT thread_id, EventsT event_type, ResourceIdT target, SrcLocT src_loc, TracePosT line)
    : thread_id(thread_id), event_type(event_type), target(target), src_loc(src_loc), line(line){}
};

template <>
struct std::formatter<EventInfo> : std::formatter<std::string> {
  auto format(const EventInfo& e, format_context& ctx) const {
      return std::format_to(ctx.out(), "Line {}: {}|{}({})|{}", e.line, e.thread_id, e.event_type, e.target, e.src_loc);
  }
};

// An Event is defined by it's vector clock and position in the trace (so by two moments in time)
struct Event{
  VectorClock vc;
  TracePosT tr_pos;
  SrcLocT src_loc;
  
  Event(const VectorClock& vc, TracePosT tr_pos, SrcLocT src_loc) 
    : vc(vc), tr_pos(tr_pos), src_loc(src_loc) {}
  
  Event(){}

  // Compares vc and tr_pos
  bool operator<=(const Event& other) const{
    return vc <= other.vc && tr_pos <= other.tr_pos;
  }
};

// Comparator between Event and VectorClock
// The order matters! Always put vc to the right as it usually is the sync preserving closure
struct EventComp{
    bool operator()(const Event& ev, const VectorClock& vc) const {
        return ev.vc < vc;
    }

    bool operator()(const VectorClock& vc, const Event& ev) const {
        return ev.vc > vc;
    }
};

// Comparator between Event pointer and VectorClock
// The order matters! Always put vc to the right as it usually is the sync preserving closure
struct EventPtrComp{
    bool operator()(const Event* ev, const VectorClock& vc) const {
        return ev->vc < vc;
    }

    bool operator()(const VectorClock& vc, const Event* ev) const {
        return ev->vc > vc;
    }
};

using EventLazyQueue = ViewLazyQueue<std::vector<Event>>;

// Critical section stuff

// Helper struct that hold the events for a critical section(one for lock, one for unlock)
struct CSInfo{
  Event lock_ev;
  Event unlock_ev;

  CSInfo(Event lock_ev, Event unlock_ev = {})
    : lock_ev(std::move(lock_ev)), unlock_ev(std::move(unlock_ev)){}
  
  CSInfo(){}
  
  void set_unlock_ev(const Event& unlock_ev){
    this->unlock_ev = unlock_ev;
  }

  // Compares two critical sections using trace location
  bool less_than_eq_tr(const CSInfo& other) const{
    return lock_ev.tr_pos <= other.lock_ev.tr_pos && unlock_ev.tr_pos <= other.lock_ev.tr_pos;
  }
  
  // Compares two critical sections using trace location
  bool less_than_tr(const CSInfo& other) const{
    return lock_ev.tr_pos < other.lock_ev.tr_pos && unlock_ev.tr_pos < other.lock_ev.tr_pos;
  }
};

// Comparator between CSInfo and VectorClock'
// The order matters! Always put vc to the right as it usually is the sync preserving closure
struct CSInfoComp{
    bool operator()(const CSInfo& cs, const VectorClock& vc) const {
        return cs.lock_ev.vc < vc;
    }

    bool operator()(const VectorClock& vc, const CSInfo& cs) const {
        return cs.lock_ev.vc > vc;
    }
};

// Critical section history
struct CSHist{
  std::unordered_map<ResourceIdT, std::unordered_map<ThreadIdT, OwnedLazyQueue<std::deque<CSInfo>>>> _cs_hist;

  // All that which was "removed" from the history is now back
  void reset(){
    for (auto& [res_id, th_cs_umap] : _cs_hist){
      for (auto& [th_id, cs_queue] : th_cs_umap){
        cs_queue.reset();
      }
    }
  }

  // Adds the lock event to the history
  CSInfo& add_lock_ev(ResourceIdT res_id, ThreadIdT tid, Event lock_ev){
    return _cs_hist[res_id][tid].emplace(std::move(lock_ev));
  }

  // Sets the unlock event in the history
  std::optional<CSInfo*> add_unlock_ev(ResourceIdT res_id, ThreadIdT tid, Event unlock_ev){
    std::optional<CSInfo*> cs = get_back(res_id, tid);
    // assert(cs.has_value()); // There should be a lock before an unlock!
    if (!cs.has_value())
      return {};

    cs.value()->unlock_ev = std::move(unlock_ev);
    return cs.value();
  }
 
  // Returns the last cs of tid involving res_id if it exists
  std::optional<CSInfo*> get_back(ResourceIdT res_id, ThreadIdT tid){
    auto umap_it = _cs_hist.find(res_id);
    if (umap_it == _cs_hist.end())
      return {};

    auto vec_it = umap_it->second.find(tid);
    if (vec_it == umap_it->second.end() || vec_it->second.empty())
      return {};
    
    return &vec_it->second.back();
  }
};

struct StdIdMap{
  size_t id_counter;
  std::unordered_map<std::string, int> _map;

  StdIdMap() : id_counter(1){}

  void reset(){
    id_counter = 1;
    _map.clear();
  }

  // Returns the corresponding id for std_id from _map
  // Updates the _map and counter if not found
  int get(const std::string& std_id){
    auto map_entry = _map.find(std_id);
    int result_id;

    if(map_entry == _map.end()) {
        _map[std_id] = id_counter;
        result_id = id_counter;
        id_counter += 1;
      } else {
        result_id = map_entry->second;
      }

      return result_id;
  }

};

// TODO: Clear it from time to time
// TODO: Should probaly put checks to not go below 0!
struct UReentrantLocksetT{
  std::unordered_map<ResourceIdT, int> _u_lock_map;
  int global_cnt;

  UReentrantLocksetT(): global_cnt(0){}

  void acquire(ResourceIdT lock_id){
    global_cnt += 1;

    _u_lock_map[lock_id] += 1;
  }
  
  void release(ResourceIdT lock_id){
    global_cnt -= 1;

    _u_lock_map[lock_id] -= 1;
  }

  bool contains(ResourceIdT lock_id) const{
    auto it = _u_lock_map.find(lock_id);
    return _contains(it);
  }

  bool contains_only(ResourceIdT lock_id) const{
    auto it = _u_lock_map.find(lock_id);
    if (!_contains(it)){
      return false;
    }

    return global_cnt == it->second;
  }

  // Returns the number of locks with a count > 0
  size_t size() const{
    size_t res = 0;
    for (const auto& [lock, cnt] : _u_lock_map){
      res += cnt > 0;
    }
    return res;
  }

  bool empty() const{
    return global_cnt <= 0;
  }
  
  LocksetT to_lockset() const{
    LocksetT result;
    
    for (const auto& [lock_id, cnt] : this->_u_lock_map){
      if (cnt > 0){
        result.insert(lock_id);
      }
    }
    
    return result;
  }

  // Check that the iterator is valid and has a counter > 0
  bool _contains(std::unordered_map<ResourceIdT, int>:: const_iterator it) const{
    return it != _u_lock_map.end() && it->second > 0;
  }
};

struct AbsDependency{
  ThreadIdT thread_id;
  ResourceIdT resource_id;
  LocksetT lockset;

  AbsDependency(ThreadIdT thread_id, ResourceIdT resource_id, const LocksetT& lockset)
    : thread_id(thread_id), resource_id(resource_id), lockset(lockset){}

  // Implements all comparison operators in the default way(first compare by thread, then resource, then lockset)
  auto operator<=>(const AbsDependency&) const = default;

  // return true if thread_ids and resource_ids differ and locksets don't intersect, false otherwise
  bool is_valid_neigh_cand(const AbsDependency& other) const{
    return thread_id != other.thread_id && resource_id != other.resource_id && !lockset_intersection(lockset, other.lockset);
  }
  
  // return true if thread_ids and resource_ids differ and locksets don't softly intersect, false otherwise
  bool is_valid_neigh_cand_soft(const AbsDependency& other) const{
    return thread_id != other.thread_id && resource_id != other.resource_id && !lockset_intersection_soft(lockset, other.lockset);
  }
  
  // return true if thread_ids differ and locksets don't intersect, false otherwise
  bool is_valid_neigh_cand_opt(const AbsDependency& other) const{
    return thread_id != other.thread_id && !lockset_intersection(lockset, other.lockset);
  }
};

typedef std::map<AbsDependency, std::vector<Event>> NodeContainerT;
typedef std::map<AbsDependency, std::vector<Event>>::const_iterator NodeConstItT;

// Format for AbsDependency
template <>
struct std::formatter<AbsDependency> : std::formatter<std::string> {
    auto format(const AbsDependency& dep, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}, {}, ({})", dep.thread_id, dep.resource_id, dep.lockset);
    }
};

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
};