#pragma once

#include <unordered_map>
#include <map>
#include <vector>
#include <fstream>
#include <stdint.h>

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

  inline const static int8_t INVALID_TID = -1; 

  SharedObjTracker(size_t obj_count = 0) : _is_shared(obj_count), _shared_count(0), _shared_cand_map(obj_count, INVALID_TID){}

  void reset(size_t obj_count){
    _is_shared = std::vector<uint8_t>(obj_count, 0);
    _shared_count = 0;
    _shared_cand_map = std::vector<ThreadIdT>(obj_count, INVALID_TID);
  }

  void update(ThreadIdT tid, ResourceIdT obj_id){
    if (_is_shared[obj_id]){
      return;
    }

    if (_shared_cand_map[obj_id] == INVALID_TID){
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

struct Parser{
  std::FILE* trace_file;
  size_t line_index;

  // Note: These only work if the whole trace is parsed
  SharedObjTracker shared_locks, shared_vars;

  static const std::unordered_map<std::string, EventsT> std_event_map;

  Parser(std::FILE* trace_file) : trace_file(trace_file), line_index(0){}

  bool events_remaining();

  // Always make sure the file did not reach eof before calling this using events_remaining
  virtual std::optional<EventInfo> get_next_event() = 0;

  // Helper function for parse_full_trace. Should not be used anywhere else
  void _update_last_write(std::vector<EventInfo>& events, size_t curr_ev_idx, std::vector<int>& last_write) const;

  std::vector<EventInfo> parse_full_trace();

  virtual void print_summary(FILE* log_file) const = 0;

};

struct TraceBinFormater{
  //TODO: Probably don't need that count_bit_count
  struct EvBinFormatter{
    const int16_t count_bit_count; // The number of bits used to represent the count
    const int16_t ev_bit_offset;
    const BinEvT ev_comp_mask;

    EvBinFormatter(int16_t count_bit_count, int16_t ev_bit_offset)
      : count_bit_count(count_bit_count), ev_bit_offset(ev_bit_offset), 
        ev_comp_mask(((1LL << count_bit_count) - 1) << ev_bit_offset){}
    
    // Converts the event bin component from binary repr to the actual value
    BinEvT bin_to_ev_comp(BinEvT ev_bin_comp) const{
      return (ev_bin_comp & ev_comp_mask) >> ev_bit_offset;
    }

    BinEvT ev_comp_to_bin(BinEvT ev_comp) const{
      return (ev_comp << ev_bit_offset) & ev_comp_mask;
    }
  };

  inline const static EvBinFormatter THREAD_FMT{10, 0};
  inline const static EvBinFormatter OP_FMT{4, 10};
  inline const static EvBinFormatter TARGET_FMT{34, 14};
  inline const static EvBinFormatter SRC_LOC_FMT{15, 48};

  inline const static ThreadIdT THREAD_COUNT_MASK = 0x7FFF;
  inline const static ResourceIdT LOCK_COUNT_MASK = 0x7FFFFFFF;
  inline const static ResourceIdT VAR_COUNT_MASK = 0x7FFFFFFF;
  inline const static EventIdT EVENT_COUNT_MASK = 0x7FFFFFFFFFFFFFFF;

  template <typename T>
  static T mask_count(T count, T mask) {
      return count & mask;
  }

  static ThreadIdT mask_th_count(ThreadIdT count) {
      return mask_count(count, THREAD_COUNT_MASK);
  }

  static ResourceIdT mask_lock_count(ResourceIdT count) {
      return mask_count(count, LOCK_COUNT_MASK);
  }

  static ResourceIdT mask_var_count(ResourceIdT count) {
    return mask_count(count, VAR_COUNT_MASK);
  }

  static EventIdT mask_ev_count(EventIdT count){
    return mask_count(count, EVENT_COUNT_MASK);
  }

  static EventIdT ev_to_bin(const EventInfo& ev_info) {
    return THREAD_FMT.ev_comp_to_bin(ev_info.thread_id) + 
           OP_FMT.ev_comp_to_bin(static_cast<BinEvT>(ev_info.event_type)) +
           TARGET_FMT.ev_comp_to_bin(ev_info.target) +
           SRC_LOC_FMT.ev_comp_to_bin(ev_info.src_loc);
  }

  static std::optional<EventInfo> bin_to_ev(BinEvT bin_ev) {
    // Make sure event is valid
    std::optional<EventsT> ev_type_op = from_int16(OP_FMT.bin_to_ev_comp(bin_ev));
    if (!ev_type_op.has_value()) return {};

    return EventInfo(THREAD_FMT.bin_to_ev_comp(bin_ev),
                     ev_type_op.value(),
	                   TARGET_FMT.bin_to_ev_comp(bin_ev),
		                 SRC_LOC_FMT.bin_to_ev_comp(bin_ev)
                    );
  }
};

// REIMPLEMENTED FROM THE ARTIFACT OF SPDOFFLINE(/root/spdoffline/src/parse/bin)
// The files opened for this should be in rb mode
struct BinParser : public Parser{
  constexpr static uint32_t EV_BLOCK_CNT = (4096 * 16) / sizeof(BinEvT);

  // Initializes meta_info
  BinParser(std::FILE* trace_file) : Parser(trace_file){
    std::fread(&meta_info, meta_info.load_sizeof(), 1, trace_file);

    meta_info.THREAD_COUNT = TraceBinFormater::mask_th_count(meta_info.THREAD_COUNT);
    meta_info.LOCK_COUNT = TraceBinFormater::mask_lock_count(meta_info.LOCK_COUNT);
    meta_info.VAR_COUNT = TraceBinFormater::mask_var_count(meta_info.VAR_COUNT);
    meta_info.EVENT_COUNT = TraceBinFormater::mask_ev_count(meta_info.EVENT_COUNT);

    shared_locks.reset(meta_info.LOCK_COUNT);
    shared_vars.reset(meta_info.VAR_COUNT);
    meta_info.resize_notif_threads();
  }

  // Always make sure the file did not reach eof before calling this using events_remaining
  std::optional<EventInfo> get_next_event() override{    
    BinEvT bin_ev;
    std::fread(&bin_ev, sizeof(bin_ev), 1, trace_file);

    // Convert the binary event to EventInfo
    std::optional<EventInfo> ev_info_opt = TraceBinFormater::bin_to_ev(bin_ev);
    if (!ev_info_opt.has_value()) return {};

    line_index++;
    EventInfo& ev_info = ev_info_opt.value();
    ev_info.line = line_index;

    // Update shared trackers for lock and var, convert the id of the target for cond vars
    if(is_lock_type(ev_info.event_type)) {
        shared_locks.update(ev_info.thread_id, ev_info.target);
    }
    else if(is_access_type(ev_info.event_type)){
        shared_vars.update(ev_info.thread_id, ev_info.target);
    }
    else if (is_notif_type(ev_info.event_type)){
        meta_info.NOTIF_THREADS[ev_info.thread_id] = 1;
    }

    return ev_info;
  }

  std::vector<EventInfo> parse_full_trace_with_blocks() {   
    // The might be invalid events like req which we want to ignore
    std::vector<EventInfo> events;
    events.reserve(meta_info.EVENT_COUNT);

    std::vector<int> last_write(meta_info.VAR_COUNT, -1);
    size_t invalid_ev_cnt = 0;
    
    std::array<BinEvT, EV_BLOCK_CNT> bin_ev_block;
    size_t read_ev_cnt = 0;

    do{
      read_ev_cnt = std::fread(bin_ev_block.data(), sizeof(BinEvT), EV_BLOCK_CNT, trace_file);

      for (int i = 0; i < read_ev_cnt; ++i){
        BinEvT bin_ev = bin_ev_block[i];

        // Convert the binary event to EventInfo
        std::optional<EventInfo> ev_info_opt = TraceBinFormater::bin_to_ev(bin_ev);
        if (!ev_info_opt.has_value()){
          continue;
        }

        line_index++;
        EventInfo& ev_info = ev_info_opt.value();
        ev_info.line = line_index;

        // Update shared trackers for lock and var, convert the id of the target for cond vars
        if(is_lock_type(ev_info.event_type)) {
            shared_locks.update(ev_info.thread_id, ev_info.target);
        }
        else if(is_access_type(ev_info.event_type)){
            shared_vars.update(ev_info.thread_id, ev_info.target);
        }
        else if (is_notif_type(ev_info.event_type)){
            meta_info.NOTIF_THREADS[ev_info.thread_id] = 1;
        }

        events.emplace_back(std::move(ev_info));
        _update_last_write(events, events.size() - 1, last_write);
      }
    } while(read_ev_cnt == EV_BLOCK_CNT);

    return events;
  }

  void print_summary(FILE* log_file) const override{
    Logger::print(log_file, "num threads: {}", meta_info.THREAD_COUNT);
    Logger::print(log_file, "num events: {}", meta_info.EVENT_COUNT);
    Logger::print(log_file, "num locations: {}", meta_info.VAR_COUNT);
    Logger::print(log_file, "num locks: {}", meta_info.LOCK_COUNT);

    // Logger::print(LogType::DBG, "unshared lock count: {}", shared_locks.get_unshared_count());
    // Logger::print(LogType::DBG, "unshared var count: {}", shared_vars.get_unshared_count());
  }
};

struct StdParser : public Parser{
  const static char trace_sep[2];
  const static uint8_t exp_trace_token_cnt;

  // Maps for converting from std format
  StdIdMap lock_id_map, th_id_map, var_id_map;

  // This better be opened in read mode
  StdParser(std::FILE* trace_file) : Parser(trace_file){}

  // Always make sure the file did not reach eof before calling this using events_remaining
  std::optional<EventInfo> get_next_event() override;

  // Map the std formated results to our custom EventInfo using the std_* maps
  // std format ex: T1|acq(l1)|25 might turn into 1, 1, 1 uisng the std_* maps
  EventInfo _from_std(const std::string& tid, EventsT ev_type, const std::string& target,
                     const std::string& src_loc);
  
  
  void to_bin_fmt(FILE* out_bin_file){
    std::vector<BinEvT> events;
    // std::vector<int> last_write(meta::VAR_COUNT, -1);

    // Parse the trace, create events and convert them to binary format
    while (events_remaining()){
        auto event_opt = get_next_event();

        if (event_opt.has_value()){
          events.push_back(TraceBinFormater::ev_to_bin(event_opt.value()));
            // _update_last_write(events, i, last_write);
        }

        // if (ev_count % 100000 == 0){
        //     Logger::print(LogType::DBG, "Parsed {} events", ev_count);
        // }
    }

    // Print metadata
    print_metadata(out_bin_file);

    fwrite(events.data(), sizeof(events[0]), events.size(), out_bin_file);
  }

  void print_metadata(FILE* out_bin_file) const{
    meta tmp_meta_info(TraceBinFormater::mask_th_count(th_id_map._map.size()),
                       TraceBinFormater::mask_ev_count(line_index),
                       TraceBinFormater::mask_var_count(var_id_map._map.size()),
                       TraceBinFormater::mask_lock_count(lock_id_map._map.size())
                      );
    fwrite(&tmp_meta_info, tmp_meta_info.load_sizeof(), 1 , out_bin_file);
  }

  void print_summary(FILE* log_file) const override;
};