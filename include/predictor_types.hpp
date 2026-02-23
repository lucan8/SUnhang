#pragma once

#include <string>
#include <vector>
#include <format>
#include <unordered_map>

typedef int ThreadIdT;
typedef int TracePosT;
typedef int ResourceNameT;
typedef int SrcLocT;

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
  ResourceNameT target;
  SrcLocT src_loc;
  TracePosT line; // line in trace file 

  std::string show(){
    return std::format("Line {}: {}|{}({})|{}", line, thread_id, event_type, target, src_loc);
  }
};