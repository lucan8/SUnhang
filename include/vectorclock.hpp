#pragma once

#include <vector>


#include "common_types.hpp"
#include "meta_info.hpp"

using VCValueT = EventIdT;

struct ThEpoch{
    int64_t tid;
    int64_t val;
};

// A vector clock implementation that starts as sparse and transforms into dense when needed
// A vector clock is just something that maps thread ids to numbers (the number of events each thread executed)
// The sparse version is, logically, std::vector<ThEpoch> and grows in size when merging
// The dense version is, logically std::vector<int64_t>, using the thread id as index, it has constant size
// When the number of elements in the sparse version is half the total number of threads(the size of the dense version)
// The sparse version transforms into dense, reusing the memory of the sparse one, as that "half point"
// is exactly where the memory usage and merging complexity become the same for sparse and dense
// When a value is absent from the sparse vector we logically assume the value associated to the thread to be 0
struct AdaptiveVectorClock {
    const static int8_t INVALID = -1;

    // In sparse mode, it is logically represented by a vector of ThEpoch
    // In dense mode, it is logically represented by a vector of EventIdT
    std::vector<int64_t> _raw_memory;
    
    // This points to the value part of the ThEpoch of the owner when sparse and to the tid in dense mode
    ThreadIdT owner_idx;
    bool is_dense = false;

    AdaptiveVectorClock();

    AdaptiveVectorClock(ThreadIdT owner_tid);

    // INTERFACE FUNCTIONS THAT DECIDE WHICH (SPARSE OR DENSE) VERSION SHOULD BE USED
    
    bool merge_into(const AdaptiveVectorClock& other);

    // It decrements other, merges into this and increments other back
    bool th_pred_merge_into(AdaptiveVectorClock& other);

    bool operator<=(const AdaptiveVectorClock& other) const;
    bool operator<(const AdaptiveVectorClock& other) const;
    bool operator>(const AdaptiveVectorClock& other) const;
    bool operator==(const AdaptiveVectorClock& other) const;

    // You better not call increment or decrement without owners!
    void increment();
    void decrement();
    size_t size() const;

    // SPARSE FUNCTIONS. THESE FUNCTIONS SHOULD BE CALLED WHEN is_dense IS FALSE
    ThEpoch* sparse_begin();
    const ThEpoch* sparse_begin() const;
    const ThEpoch* sparse_end() const;
    ThreadIdT sparse_size() const;
    ThreadIdT get_owner_tid_sparse() const;

    bool should_convert() const;
    void try_convert_to_dense();
    void convert_to_dense();

    // Merges two sparse vector clocks
    bool merge_into_sparse(const AdaptiveVectorClock& other);

    // this: sparse, other: dense, merges other into this
    // this becomes dense
    bool sparse_merge_into_dense(const AdaptiveVectorClock& other);

    // Assumes both this and other are in sparse mode
    bool less_equal_sparse(const AdaptiveVectorClock& other) const;
    bool less_than_sparse(const AdaptiveVectorClock& other) const;

    bool sparse_less_equal_dense(const AdaptiveVectorClock& other) const;
    bool sparse_less_than_dense(const AdaptiveVectorClock& other) const;

    // DENSE FUNCTIONS. THESE FUNCTIONS SHOULD BE CALLED WHEN is_dense == true
    EventIdT* dense_begin();
    const EventIdT* dense_begin() const;
    const EventIdT* dense_end() const;
    size_t dense_size() const;

    // Merges two dense vector clocks
    bool merge_into_dense(const AdaptiveVectorClock& other);

    // this: desne, other: sparse, merges other into this
    bool dense_merge_into_sparse(const AdaptiveVectorClock& other);

    // Assumes both this and other are in dense mode
    bool less_equal_dense(const AdaptiveVectorClock& other) const;
    bool less_than_dense(const AdaptiveVectorClock& other) const;

    bool dense_less_equal_sparse(const AdaptiveVectorClock& other) const;
    bool dense_less_than_sparse(const AdaptiveVectorClock& other) const;
};

typedef AdaptiveVectorClock VectorClock;