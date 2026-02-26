#pragma once
#include "predictor_types.hpp"
#include "vectorclock.hpp"
#include <unordered_map>
#include <map>
#include <vector>

// TODO: Split this into GraphConstructor and Predictor
struct Predictor{
  std::unordered_map<ThreadIdT, ThreadInfo> thread_map;

  std::map<AbsDependency, std::vector<VectorClock>> abs_deps_map;
  // Intermediary step that helps to build the neighbour list
  std::unordered_map<ResourceIdT, std::vector<const AbsDependency*>> lock_dep_map;
  std::unordered_map<AbsDependency*, std::vector<const AbsDependency*>> neigh_list; 

  std::unordered_map<ResourceIdT, VectorClock> last_write;

  // Calls handler associated with evt.event_type
  // Return true if event if valid, false otherwise
  bool handle_event(const EventInfo& evt);
  
  void read_event(const EventInfo& evt);
  void write_event(const EventInfo& evt);

  void acquire_event(const EventInfo& evt);
  void release_event(const EventInfo& evt);
  
  void fork_event(const EventInfo& evt);
  void join_event(const EventInfo& evt);

  void print_abs_deps() const;
  void print_lock_deps_map() const;
};