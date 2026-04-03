#pragma once

#include <unordered_map>
#include <map>
#include <vector>
#include <fstream>
#include "predictor_types.hpp"
#include "ord_dep_graph.hpp"
#include "vectorclock.hpp"

struct TraceParser{
  std::ifstream trace_file;
  size_t line_index;

  const static char trace_sep;
  const static uint8_t exp_split_trace_size;

  // Maps for converting from std format
  StdIdMap lock_id_map, th_id_map, var_id_map;
  static const std::unordered_map<std::string, EventsT> std_event_map;

  TraceParser(std::ifstream trace_file) : trace_file(std::move(trace_file)), line_index(0){}

  bool events_remaining();

  // Always make sure the file did not reach eof before calling this using events_remaining
  std::optional<EventInfo> get_next_event();

  // Map the std formated result to our custom result using the std_* maps
  // std format ex: T1|acq(l1)|25 might turn into 1, 1, 1 uisng the std_* maps
  std::optional<EventInfo> from_std(const std::vector<std::string>& current_result);

  void print_summary(FILE* log_file) const;
};