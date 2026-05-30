#pragma once

#include <optional>
#include "common_types.hpp"

using VCValueT = EventIdT;

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