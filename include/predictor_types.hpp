#pragma once

// TODO: For CSHist, look into adding making the functions return const references as well
#include <string>
#include <vector>
#include <format>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <cassert>
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

struct ThreadInfo{
  LocksetT lockset;
  VectorClock vec_clock;
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
  
  // return true if thread_ids differ and locksets don't intersect, false otherwise
  bool is_valid_neigh_cand_opt(const AbsDependency& other) const{
    return thread_id != other.thread_id && !lockset_intersection(lockset, other.lockset);
  }
};

// Format for AbsDependency
template <>
struct std::formatter<AbsDependency> : std::formatter<std::string> {
    auto format(const AbsDependency& dep, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}, {}, ({})", dep.thread_id, dep.resource_id, dep.lockset);
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
  NONE = 7
};

inline std::unordered_map<std::string, EventsT> std_event_map = {
    {"r", EventsT::RD}, {"w", EventsT::WR}, {"fork", EventsT::FORK}, {"join", EventsT::JOIN}, {"acq", EventsT::LK}, {"rel", EventsT::UK}
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

  Event(VectorClock vc, TracePosT tr_pos) 
    : vc(std::move(vc)), tr_pos(tr_pos) {}
  
  Event(){}

  // Compares vc and tr_pos
  bool operator<=(const Event& other) const{
    return vc <= other.vc && tr_pos <= other.tr_pos;
  }
};

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

  // Comapres the lock event vc to vc
  // Made static to be used as comparator outside class
  static bool less_than_vc(const CSInfo& cs, const VectorClock& vc) {
    return cs.lock_ev.vc < vc;
  }
};

// Critical section history
struct CSHist{
  std::unordered_map<ResourceIdT, std::unordered_map<ThreadIdT, LazyQueue<CSInfo>>> _cs_hist;

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
  CSInfo& add_unlock_ev(ResourceIdT res_id, ThreadIdT tid, Event unlock_ev){
    std::optional<CSInfo*> cs = get_back(res_id, tid);
    assert(cs.has_value()); // There should be a lock before an unlock!

    cs.value()->unlock_ev = std::move(unlock_ev);
    return *cs.value();
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