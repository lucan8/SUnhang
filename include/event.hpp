#pragma once
#include <stdint.h>
#include <optional>
#include <format>

#include "common_types.hpp"
#include "lazy_queue.hpp"
#include "vectorclock.hpp"

// TODO: TRY USING INT8_T
// NOTE: IF ADDING MORE OR REMOVING ALWAYS BE MIDNFULL OF UPDATING from_int16
enum class EventsT : int16_t {
  // Common in our and the author's implementation
  LK = 0,
  UK = 1,
  RD = 2,
  WR = 3,
  FORK = 4,
  JOIN = 5,

  // These are not used in our implementation, except for REQ
  BEGIN = 6,
  END = 7,
  REQ = 8,
  BRANCH = 9,

  // These are only in our implementation
  WAIT = 10,
  NOTIFY = 11,
  NOTIFYALL = 12
};

inline bool is_lock_type(EventsT ev_type){
  return ev_type == EventsT::REQ || ev_type == EventsT::LK || ev_type == EventsT::UK;
}

inline bool is_access_type(EventsT ev_type){
  return ev_type == EventsT::RD || ev_type == EventsT::WR;
}

inline bool is_th_type(EventsT ev_type){
  return ev_type == EventsT::FORK || ev_type == EventsT::JOIN;
}

inline bool is_notif_type(EventsT ev_type){
  return ev_type == EventsT::NOTIFY || ev_type == EventsT::NOTIFYALL;
}

inline bool is_cv_type(EventsT ev_type){
  return ev_type == EventsT::WAIT || is_notif_type(ev_type);
}

inline bool is_unused_type(EventsT ev_type) {
  return ev_type == EventsT::BEGIN || ev_type == EventsT::END || ev_type == EventsT::BRANCH;
}

inline bool is_spd_type(EventsT ev_type) {
  return ev_type <= EventsT::BRANCH;
}

inline std::optional<EventsT> from_int16(int16_t val){
  if (val >= static_cast<int>(EventsT::LK) && val <= static_cast<int>(EventsT::NOTIFYALL)) {
    return static_cast<EventsT>(val);
  }
  return std::nullopt;
}

inline EventsT unsafe_from_int16(int16_t val){
    return static_cast<EventsT>(val);
}

// Formats EventsT
// Needed when calling std::format
template <>
struct std::formatter<EventsT> : std::formatter<std::string> {
    auto format(EventsT e, format_context& ctx) const {
        std::string name;
        switch (e) {
            case EventsT::LK: name = "acq"; break;
            case EventsT::UK: name = "rel"; break;
            case EventsT::RD: name = "r"; break;
            case EventsT::WR: name = "w"; break;
            case EventsT::FORK: name = "fork"; break;
            case EventsT::JOIN: name = "join"; break;
            case EventsT::BEGIN: name = "begin"; break;
            case EventsT::END: name = "end"; break;
            case EventsT::BRANCH: name = "branch"; break;
            case EventsT::REQ: name = "req"; break;
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
  SrcLocT src_loc; // line of source code that generated the events
  EventIdT tr_pos; // line in trace file 

  EventInfo(){}
  EventInfo(ThreadIdT thread_id, EventsT event_type, ResourceIdT target, SrcLocT src_loc, EventIdT tr_pos = 0)
    : thread_id(thread_id), event_type(event_type), target(target), src_loc(src_loc), tr_pos(tr_pos){}
};

template <>
struct std::formatter<EventInfo> : std::formatter<std::string> {
  auto format(const EventInfo& e, format_context& ctx) const {
      if (is_th_type(e.event_type)){
        return std::format_to(ctx.out(), "T{}|{}(T{})|{}", e.thread_id, e.event_type, e.target, e.src_loc);
      }
      return std::format_to(ctx.out(), "T{}|{}({})|{}", e.thread_id, e.event_type, e.target, e.src_loc);
  }
};

// An Event is defined by it's vector clock and position in the trace (so by two moments in time)
struct Event{
  VectorClock vc;
  EventIdT tr_pos;
  
  Event(const VectorClock& vc, EventIdT tr_pos) 
    : vc(vc), tr_pos(tr_pos) {}
  
  Event(){}

  // Compares vc and tr_pos
  bool operator<=(const Event& other) const{
    return vc <= other.vc && tr_pos <= other.tr_pos;
  }
};

template <>
struct std::formatter<Event> : std::formatter<std::string> {
  auto format(const Event& ev, auto& ctx) const {
      return std::format_to(ctx.out(), "{}", ev.tr_pos);
  }
};

// Comparator between Event and VectorClock
// The order of the arguments matters
struct EventLess{
    // ev.vc < vc
    bool operator()(const Event& ev, const VectorClock& vc) const {
        return ev.vc < vc;
    }

    // !(ev.vc <= vc)
    bool operator()(const VectorClock& vc, const Event& ev) const {
        return !(ev.vc <= vc);
    }
};

// Comparator between Event pointer and VectorClock
// The order of the arguments matters
struct EventPtrLess{
    // ev.vc < vc
    bool operator()(const Event* ev, const VectorClock& vc) const {
        return ev->vc < vc;
    }
    
    // !(ev.vc <= vc)
    bool operator()(const VectorClock& vc, const Event* ev) const {
        return !(ev->vc <= vc);
    }
};

using EventLazyQueue = ViewLazyQueue<std::vector<Event>>;