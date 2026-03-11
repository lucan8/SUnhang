#pragma once

#include <unordered_map>
#include <string>
#include <format>
#include "comm_types.hpp"

using VCValueT = int;

using ThEpochConstIt = std::unordered_map<ThreadIdT, VCValueT>::const_iterator;
using ThEpoch = std::unordered_map<ThreadIdT, VCValueT>::value_type;

struct VectorClock {
    std::unordered_map<ThreadIdT, VCValueT> _vector_clock;
    VectorClock();
    VectorClock(ThreadIdT increment_thread_id);

    // TODO: Remove this
    VCValueT find(ThreadIdT thread_id) const;
    
    VectorClock merge(const VectorClock& other) const;
    
    // Merges other into this, returns true if any change occured
    bool merge_into(const VectorClock& other);

    // Get the thread's epoch and merge it's predecessor into this
    bool th_pred_merge_into(const VectorClock& other, ThreadIdT tid);
    
    // Get the thread's epoch and merge it's predecessor into this
    bool pred_merge_into_epoch(const VectorClock& other, ThreadIdT tid);

    // Meges epoch in this vector clock
    bool merge_into_epoch(const ThEpoch& epoch);
    
    void set(ThreadIdT thread_id, VCValueT val);
    
    void increment(ThreadIdT thread_id);
    void decrement(ThreadIdT thread_id);

    //TODO: Pack these together in one
    // all epoches have to be <=
    friend bool operator<=(const VectorClock& vc1, const VectorClock& vc2);
    // all epoches need to be <= and at least one has to be <
    friend bool operator<(const VectorClock& vc1, const VectorClock& vc2);
    
    // at least one epoch has to be >, the rest can even be below!
    friend bool operator>(const VectorClock& vc1, const VectorClock& vc2);
    
    friend bool operator==(const VectorClock& vc1, const VectorClock& vc2);
    
    bool empty() const;
};

template <>
struct std::formatter<VectorClock> : std::formatter<std::string> {
    auto format(const VectorClock& vc, format_context& ctx) const {
        auto out = ctx.out();
        for (const auto& [tid, vc_val] : vc._vector_clock)
          std::format_to(out, "{}:{}, ", tid, vc_val);
        return out;
    }
};