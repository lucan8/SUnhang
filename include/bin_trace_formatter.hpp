#pragma once

#include <stdint.h>
#include <optional>

#include "common_types.hpp"
#include "predictor_types.hpp"

struct TraceBinFormatter{
  struct EvBinFormatter{
    const int16_t ev_bit_offset;
    const BinEvT ev_comp_mask;

    EvBinFormatter(int16_t count_bit_count, int16_t ev_bit_offset)
      : ev_bit_offset(ev_bit_offset), ev_comp_mask(((1LL << count_bit_count) - 1) << ev_bit_offset){}
    
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

  static EventInfo unsafe_bin_to_ev(BinEvT bin_ev) {
    return EventInfo(THREAD_FMT.bin_to_ev_comp(bin_ev),
                     unsafe_from_int16(OP_FMT.bin_to_ev_comp(bin_ev)),
	                   TARGET_FMT.bin_to_ev_comp(bin_ev),
		                 SRC_LOC_FMT.bin_to_ev_comp(bin_ev)
                    );
  }

  static std::optional<EventsT> extract_ev_type(BinEvT bin_ev) {
    return from_int16(OP_FMT.bin_to_ev_comp(bin_ev));
  }

  static EventsT unsafe_extract_ev_type(BinEvT bin_ev) {
    return unsafe_from_int16(OP_FMT.bin_to_ev_comp(bin_ev));
  }

  static ThreadIdT extract_tid(BinEvT bin_ev) {
    return THREAD_FMT.bin_to_ev_comp(bin_ev);
  }

  static ResourceIdT extract_target(BinEvT bin_ev) {
    return TARGET_FMT.bin_to_ev_comp(bin_ev);
  }

  static ResourceIdT extract_src_loc(BinEvT bin_ev) {
    return SRC_LOC_FMT.bin_to_ev_comp(bin_ev);
  }
};