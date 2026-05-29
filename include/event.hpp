#pragma once
#include <stdint.h>
#include <optional>
#include <format>

#include "common_types.hpp"
#include "lazy_queue.hpp"
#include "vectorclock.hpp"

// TODO: TRY USING INT8_T
// NOTE: IF ADDING MORE OR REMOVING ALWAYS BE MIDNFULL OF UPDATING from_int16
// Event stuff
enum class EventsT : int16_t {
  LK = 0,
  UK = 1,
  RD = 2,
  WR = 3,
  FORK = 4,
  JOIN = 5,
  WAIT = 6,
  NOTIFY = 7,
  NOTIFYALL = 8
};

inline bool is_lock_type(EventsT ev_type){
  return ev_type == EventsT::LK || ev_type == EventsT::UK;
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

inline std::optional<EventsT> from_int16(int16_t val){
  if (val >= static_cast<int>(EventsT::LK) && 
        val <= static_cast<int>(EventsT::NOTIFYALL)) {
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
  EventIdT line; // line in trace file 

  EventInfo(){}
  EventInfo(ThreadIdT thread_id, EventsT event_type, ResourceIdT target, SrcLocT src_loc, EventIdT line = 0)
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
  EventIdT tr_pos;
  SrcLocT src_loc;
  
  Event(const VectorClock& vc, EventIdT tr_pos, SrcLocT src_loc) 
    : vc(vc), tr_pos(tr_pos), src_loc(src_loc) {}
  
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