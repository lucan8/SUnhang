#pragma once

#include <unordered_map>
#include <string>
#include <format>
#include "comm_types.hpp"

typedef int VCValueT;

struct VectorClock {
    std::unordered_map<ThreadIdT, VCValueT> _vector_clock;
    VectorClock();
    VectorClock(ThreadIdT increment_thread_id);

    VCValueT find(ThreadIdT thread_id) const;
    
    VectorClock merge(const VectorClock& other) const;
    
    // Merges other into this, returns true if any change occured
    bool merge_into(const VectorClock& other);
    
    void set(ThreadIdT thread_id, VCValueT);
    
    void increment(ThreadIdT thread_id);
    void decrement(ThreadIdT thread_id);

    //TODO: Pack these together in one
    friend bool operator<=(const VectorClock& vc1, const VectorClock& vc2);
    friend bool operator<(const VectorClock& vc1, const VectorClock& vc2);
    
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