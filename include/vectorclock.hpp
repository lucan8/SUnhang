#pragma once

#include <unordered_map>
#include <string>
#include <format>
#include <queue>
#include <flat_map>
#include "comm_types.hpp"

struct VectorClock;

using VCValueT = EventIdT;

// Contains: Timestamp of notify event and the number of threads that should receive the notif
using NotifQueue = std::queue<std::pair<VectorClock, uint32_t>>;

struct ThEpoch{
    ThreadIdT tid;
    VCValueT val;
};

struct OwnerThInfo{
    ThreadIdT tid;
    size_t idx = 0;
    OwnerThInfo(ThreadIdT tid) : tid(tid){} 
};

struct VectorClock {
    std::vector<ThEpoch> _vector_clock;
    std::optional<OwnerThInfo> owner_th;

    VectorClock();
    VectorClock(ThreadIdT owner_th);
    
    // Merges other into this, returns true if any change occured
    bool merge_into(const VectorClock& other);

    // Get the thread's epoch and merge it's predecessor into this
    bool th_pred_merge_into(VectorClock& other);
    
    void increment();
    void decrement();

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

// template <>
// struct std::formatter<VectorClock> : std::formatter<std::string> {
//     auto format(const VectorClock& vc, format_context& ctx) const {
//         auto out = ctx.out();
//         for (const auto& [tid, vc_val] : vc._vector_clock)
//           std::format_to(out, "{}:{}, ", tid, vc_val);
//         return out;
//     }
// };