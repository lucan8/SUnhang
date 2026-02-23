#pragma once
#include "predictor_types.hpp"
#include "vectorclock.hpp"
#include <unordered_map>
#include <map>


struct Predictor{
  std::unordered_map<ThreadIdT, ThreadInfo> thread_map;
  std::map<AbsDependency, std::vector<VectorClock>> abs_deps_map;
  std::unordered_map<ResourceIdT, VectorClock> last_write;

  // Calls handler associated with evt.event_type
  // Return true if event if valid, false otherwise
  bool handle_event(const EventInfo& evt);
  
  void read_event(const EventInfo& trace_line);
  void write_event(const EventInfo& trace_line);

  void acquire_event(const EventInfo& trace_line);
  void release_event(const EventInfo& trace_line);
  
  void fork_event(const EventInfo& trace_line);
  void join_event(const EventInfo& trace_line);

  void print_abs_deps() const;
};