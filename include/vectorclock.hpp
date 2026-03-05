#pragma once

#include <unordered_map>
#include <string>
#include "comm_types.hpp"

typedef int VCValueT;

struct VectorClock {
    std::unordered_map<ThreadIdT, VCValueT> _vector_clock = {};
    VectorClock();
    VectorClock(ThreadIdT increment_thread_id);

    VCValueT find(ThreadIdT thread_id) const;
    
    VectorClock merge(const VectorClock& other) const;
    void merge_into(const VectorClock& other); // Merges other into this
    
    void set(ThreadIdT thread_id, VCValueT);
    
    void increment(ThreadIdT thread_id);
    void decrement(ThreadIdT thread_id);

    friend bool operator<=(const VectorClock& vc1, const VectorClock& vc2);
    friend bool operator==(const VectorClock& vc1, const VectorClock& vc2);

    std::string show();
};