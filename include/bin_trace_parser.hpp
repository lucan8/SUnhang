#pragma once

#include <unordered_map>
#include <vector>
#include <stdint.h>

#include "bin_trace_formatter.hpp"
#include "event_handler.hpp"

// Helper structure that answers the question: "Is this resource used by two or more threads?"
struct SharedObjTracker{
    inline const static int8_t INVALID_TID = -1;
    inline const static int8_t IS_SHARED = -2;
    
    size_t _shared_count;

    // Compressed vector that either has the value of 
    // the id of the thread that first accessed this
    // INVALID_TID : no thread accessed yet
    // IS_SHARED : two threads accessed it
    std::vector<ThreadIdT> _shared_cand_map;

    SharedObjTracker(size_t res_count = 0);

    void reset(size_t res_count);

    void update(ThreadIdT tid, ResourceIdT res_id);

    bool is_shared(ResourceIdT res_id) const;

    size_t get_unshared_count() const;
};

// REIMPLEMENTED FROM THE ARTIFACT OF SPDOFFLINE(/root/spdoffline/src/parse/bin)
// The trace_file should be opened in rb mode
struct BinParser{
  constexpr static uint32_t EV_BLOCK_CNT = (4096 * 16) / sizeof(BinEvT);

  // Input
  std::FILE* trace_file;

  // Results coming from preprocessing(1st pass) that will be used for the actual run(2nd pass)
  SharedObjTracker shared_locks, shared_vars;
  std::vector<uint8_t> ignored_events;

  // Initializes meta_info
  BinParser(std::FILE* trace_file);

  void _update_last_write(std::vector<uint8_t>& ignored_events, EventIdT event_idx, EventsT event_type,
                                ResourceIdT target, std::vector<EventIdT>& last_write) const;
  void preprocess_trace();

  void parse_and_handle_trace(EventHandler& event_handler);

  void print_summary(FILE* log_file) const;
};