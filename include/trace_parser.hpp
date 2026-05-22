#pragma once

#include <unordered_map>
#include <map>
#include <vector>
#include <fstream>
#include "predictor_types.hpp"
#include "ord_dep_graph.hpp"
#include "vectorclock.hpp"

// Helper structure that answers the question: "Is this resource shared?"
struct SharedObjTracker{
  // ResourceIdT is used as index both both
  //TODO: COULD USE A DYNAMIC BITSET OR AT LEAST COMBINE THE TWO STRUCTURES INTO ONE
  std::vector<uint8_t> _is_shared;
  size_t _shared_count;
  std::vector<ThreadIdT> _shared_cand_map;

  SharedObjTracker(size_t obj_count) : _is_shared(obj_count), _shared_count(0), _shared_cand_map(obj_count, -1){}

  void update(ThreadIdT tid, ResourceIdT obj_id){
    if (_is_shared[obj_id]){
      return;
    }

    if (_shared_cand_map[obj_id] == -1){
      _shared_cand_map[obj_id] = tid;
    }
    else if (_shared_cand_map[obj_id] != tid){
      _shared_count += 1;
      _is_shared[obj_id] = 1;
    }
  }

  bool is_shared(ResourceIdT res_id) const{
    return _is_shared[res_id];
  }

  size_t get_unshared_count() const{
    return _is_shared.size() - _shared_count;
  }
};

struct TraceParser{
  std::FILE* trace_file;
  size_t line_index;

  const static char trace_sep[2];
  const static uint8_t exp_trace_token_cnt;

  // Maps for converting from std format
  StdIdMap lock_id_map, th_id_map, var_id_map;
  static const std::unordered_map<std::string, EventsT> std_event_map;

  // Note: These only work if the whole trace is parsed
  SharedObjTracker shared_locks, shared_vars;

  TraceParser(std::FILE* trace_file) 
    : trace_file(trace_file), line_index(0), 
      shared_locks(meta::LOCK_COUNT), shared_vars(meta::VAR_COUNT){}

  bool events_remaining();

  // Always make sure the file did not reach eof before calling this using events_remaining
  std::optional<EventInfo> get_next_event();

  // Helper function for parse_full_trace. Should not be used anywhere else
  void _update_last_write(std::vector<EventInfo>& events, size_t ev_count, std::vector<int>& last_write) const;

  std::vector<EventInfo> parse_full_trace();

  // Map the std formated results to our custom EventInfo using the std_* maps
  // std format ex: T1|acq(l1)|25 might turn into 1, 1, 1 uisng the std_* maps
  EventInfo from_std(const std::string& tid, EventsT ev_type, const std::string& target,
                     const std::string& src_loc);

  void print_summary(FILE* log_file) const;
};