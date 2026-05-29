#include <unordered_map>
#include <vector>
#include <stdint.h>

#include "bin_trace_formatter.hpp"

struct StdParser{
  const static char trace_sep[2];
  const static uint8_t exp_trace_token_cnt;
  static const std::unordered_map<std::string, EventsT> std_event_map;

  std::FILE* trace_file;
  size_t line_index;

  // Maps for converting from std format
  StdIdMap lock_id_map, th_id_map, var_id_map;

  StdParser(std::FILE* trace_file) : trace_file(trace_file), line_index(0){}

  // THE ACTUAL PARSER FUNCTIONS

  bool events_remaining() const;

  // Always make sure the file did not reach eof before calling this using events_remaining
  std::optional<EventInfo> get_next_event();
  
  // Map the std formated results to our custom EventInfo using the std_* maps
  // std format ex: T1|acq(l1)|25 might turn into 1, 1, 1 uisng the std_* maps
  EventInfo _from_std(const std::string& tid, EventsT ev_type, const std::string& target,
                      const std::string& src_loc);
  
  // THE BINARY CONVERTER FUNCTIONS WHICH BUILD ON TOP OF THE PARSER ONES
  void to_bin_fmt(FILE* out_bin_file);
  void print_metadata(FILE* out_bin_file) const;
};