#pragma once

#include <string>
#include <vector>

typedef int thread_id_t;
typedef int trace_pos_t;
typedef int resource_name_t;
typedef int src_loc_t;

// Event stuff
enum events_t {
  RD = 1,
  WR = 2,
  FORK = 3,
  JOIN = 4,
  LK = 5,
  UK = 6,
  NONE = 7
};

struct EventInfo{
  thread_id_t thread_id;
  events_t event_type;
  resource_name_t target;
  src_loc_t src_loc;
  trace_pos_t line; // line in trace file 

  void print(){
    std::cout << "Line " << line << ": " << thread_id << "|" << event_type << "(" << target << ")" << "|" << src_loc << std::endl;
  }
};

