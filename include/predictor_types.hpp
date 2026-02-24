#pragma once

#include <string>
#include <vector>
#include <format>
#include <unordered_map>
#include <unordered_set>
#include "vectorclock.hpp"
#include "comm_types.hpp"


// Format for LocksetT
template <>
struct std::formatter<LocksetT> : std::formatter<std::string> {
    auto format(const LocksetT& lockset, format_context& ctx) const {
        std::string result;
        for (const auto& res_id : lockset)
          result += ", " + std::to_string(res_id);
        return formatter<std::string>::format(result, ctx);
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
  
  std::string show() const{
    return std::format("{}, {}, ({})", thread_id, resource_id, lockset);
  }

  // Implements all comparison operators in the default way(first compare by thread, then resource, then lockset)
  auto operator<=>(const AbsDependency&) const = default;
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

  std::string show() const{
    return std::format("Line {}: {}|{}({})|{}", line, thread_id, event_type, target, src_loc);
  }
};