#include <algorithm>
#include <vector>

#include "../include/vectorclock.hpp"


AdaptiveVectorClock::AdaptiveVectorClock() : owner_idx(INVALID){}

AdaptiveVectorClock::AdaptiveVectorClock(ThreadIdT owner_tid) : owner_idx(1){
    // Start in sparse mode (will transition to dense when needed)
    // index 0: tid, index 1: value
    _raw_memory.push_back(owner_tid);
    _raw_memory.push_back(1);

    // THIS IS ONLY FOR TESTING DENSE VCs
    // is_dense = true;
    // _raw_memory.resize(meta_info.header.THREAD_COUNT, 0);
    // owner_idx = owner_tid;
    // _raw_memory[owner_idx] = 1;

}

// INTERFACE FUNCTIONS THAT DECIDE WHICH (SPARSE OR DENSE) VERSION SHOULD BE USED

// Merges this into other. Return true if any change occured, false otherwise
bool AdaptiveVectorClock::merge_into(const AdaptiveVectorClock& other){
    bool res = false;
    
    if (!is_dense){
        if (!other.is_dense) res = merge_into_sparse(other);
        else res = sparse_merge_into_dense(other);
    } 
    else if (other.is_dense) res = merge_into_dense(other);
    else res = dense_merge_into_sparse(other);

    // It is important to try the conversion at the end to avoid having a sparse vc with more than T / 2
    // active elements, as that will break other assumptions
    try_convert_to_dense();

    return res;
}

// It decrements other, merges into this and increments other back
bool AdaptiveVectorClock::th_pred_merge_into(AdaptiveVectorClock& other) {
    other.decrement();
    bool res = merge_into(other);
    other.increment();

    return res;
}

// Returns true if all point-wise(based on tid) values are <= to other, false otherwise
bool AdaptiveVectorClock::operator<=(const AdaptiveVectorClock& other) const {
    if (!is_dense){
        if (!other.is_dense) return less_equal_sparse(other);
        else return sparse_less_equal_dense(other);
    } 
    else if (other.is_dense) return less_equal_dense(other);
    
    return dense_less_equal_sparse(other);
}

// Returns true if all point-wise(based on tid) values are <= to other and there is at least one strictly smaller
// false otherwise
bool AdaptiveVectorClock::operator<(const AdaptiveVectorClock& other) const {
    if (!is_dense){
        if (!other.is_dense) return less_than_sparse(other);
        else return sparse_less_than_dense(other);
    } 
    else if (other.is_dense) return less_than_dense(other);
    
    return dense_less_than_sparse(other);
}

// Increments the value pointed to by owner_idx
// You better not call this without owners or bad things will happen
void AdaptiveVectorClock::increment() {
    this->_raw_memory[owner_idx]++;
}

// Decrements the value pointed to by owner_idx. Doesn't go past 0
// You better not call this without owners or bad things will happen
void AdaptiveVectorClock::decrement() {
    this->_raw_memory[owner_idx] = std::max(0LL, this->_raw_memory[owner_idx] - 1);
}

// Returns the number of active elements of the vector clock(non-zero values)
size_t AdaptiveVectorClock::size() const {
    if (!is_dense){
        return sparse_size();
    }
    return dense_size();
}

// SPARSE FUNCTIONS. THESE FUNCTIONS SHOULD BE CALLED WHEN is_dense IS FALSE

ThEpoch* AdaptiveVectorClock::sparse_begin() { 
    return reinterpret_cast<ThEpoch*>(_raw_memory.data()); 
}

const ThEpoch* AdaptiveVectorClock::sparse_begin() const { 
    return reinterpret_cast<const ThEpoch*>(_raw_memory.data()); 
}

const ThEpoch* AdaptiveVectorClock::sparse_end() const {
    return sparse_begin() + sparse_size();
}

bool AdaptiveVectorClock::should_convert() const {
    return !is_dense && _raw_memory.size() >= meta_info.header.THREAD_COUNT;
}

// ThEpoch is two int64_t, so the number of such elements is half the size of _raw_memory
ThreadIdT AdaptiveVectorClock::sparse_size() const {
    return _raw_memory.size() >> 1;
}

// owner_idx points to the val associated with the owner tid
// so the owner tid must be the value preceeding it
ThreadIdT AdaptiveVectorClock::get_owner_tid_sparse() const{
    return owner_idx == INVALID ? INVALID : _raw_memory[owner_idx - 1];
}

// convert_to_dense if it should, does nothing otherwise
void AdaptiveVectorClock::try_convert_to_dense() {
    if (!should_convert()) return;
    convert_to_dense();
}

// Converts the sparse vector to dense
// Do not call this if already in dense mode, you will get undefined behaviour
// It is also not advisied to call this if the second cond in should_convert is false, it will cause re-allocations
void AdaptiveVectorClock::convert_to_dense() {
    // Allocates only once, this will be the new _raw_memory of this object
    // Only one allocation happens because when this is called, _raw_memory.size() >= meta_info.header.THREAD_COUNT
    // So swapping at the end gives back a vector of a greater size, which will be reused at the next call
    static std::vector<int64_t> workspace;
    workspace.assign(meta_info.header.THREAD_COUNT, 0); 

    // Update owner to be the tid
    owner_idx = get_owner_tid_sparse();
    
    // Transform workspace into the dense version of this sparse vc
    for (auto curr = sparse_begin(); curr != sparse_end(); ++curr) {
        workspace[curr->tid] = curr->val;
    }

    // Swap the pointers of _raw_memory and workspace
    _raw_memory.swap(workspace);
    is_dense = true;
}

// Merges two sparse vector clocks
bool AdaptiveVectorClock::merge_into_sparse(const AdaptiveVectorClock& other) {
    bool changed = false;
    
    auto this_start = sparse_begin();
    auto this_end = sparse_end();

    auto other_start = other.sparse_begin();
    auto other_end = other.sparse_end();

    // Get the owner thread based on owner_idx
    ThreadIdT owner_tid = get_owner_tid_sparse();

    // Re-using the same workspace as much as possible
    // This will re-allocate, but less frequently as the program progresses
    static std::vector<int64_t> workspace;
    workspace.clear(); 

    // Max merge operaiton
    while (this_start != this_end && other_start != other_end) {
        ThreadIdT inserted_tid = INVALID;

        if (this_start->tid == other_start->tid) { // Pick the maximum value
            VCValueT max_val = std::max(this_start->val, other_start->val);
            if (max_val > this_start->val) changed = true;
            
            workspace.push_back(this_start->tid);
            workspace.push_back(max_val);
            inserted_tid = this_start->tid;
            
            ++this_start; ++other_start;
        } else if (this_start->tid < other_start->tid) {// absent in other implicitly means it's value is 0, so we take the value of this
            workspace.push_back(this_start->tid);
            workspace.push_back(this_start->val);
            inserted_tid = this_start->tid;
            
            ++this_start;
        } else { // absent in this implicitly means it's value is 0, so we take the value of other
            workspace.push_back(other_start->tid);
            workspace.push_back(other_start->val);
            inserted_tid = other_start->tid;
            changed = true;
            
            ++other_start;
        }

        // Update owner if needed
        if (inserted_tid == owner_tid) {
            owner_idx = workspace.size() - 1; 
        }
    }

    // What was left from the this vector clock
    while (this_start != this_end) { 
        workspace.push_back(this_start->tid);
        workspace.push_back(this_start->val);
        
        // Update owner if needed
        if (this_start->tid == owner_tid) {
            owner_idx = workspace.size() - 1;
        }
        this_start++; 
    }

    // What was left from the other vector clock 
    while (other_start != other_end) { 
        workspace.push_back(other_start->tid);
        workspace.push_back(other_start->val);
        changed = true; 
        other_start++;
    }

    // Swap the memory adresses of raw_memory and workspace only if needed
    if (changed) {
        _raw_memory.swap(workspace);
    }

    return changed;
}

// this: sparse, other: dense, merges other into this
// this becomes dense
bool AdaptiveVectorClock::sparse_merge_into_dense(const AdaptiveVectorClock& other) {
    // Copy the sparse version and define bounds
    std::vector<int64_t> this_copy(_raw_memory);
    auto this_copy_start = reinterpret_cast<ThEpoch*>(this_copy.data());
    auto this_copy_end = this_copy_start + (this_copy.size() >> 1);

    // Update owner to be the tid
    owner_idx = get_owner_tid_sparse();

    // raw_memory is the same as other now
    _raw_memory.assign(other._raw_memory.begin(), other._raw_memory.end());
    
    // Max merge operation
    for (auto curr = this_copy_start; curr != this_copy_end; ++curr) {
        _raw_memory[curr->tid] = std::max(_raw_memory[curr->tid], curr->val);
    }

    is_dense = true;
    
    // Merging dense into sparse 100% updates the sparse one
    // Dense vector clock have at least THREAD_COUNT / 2  elements active (val > 0)
    // Sparse vector clocks have less than THREAD_COUNT / 2 elements active
    // otherwise they would have transformed into dense
    // This means at least one value will always be updated using the dense vector's value
    return true;
}

// Assumes both this and other are in sparse mode
bool AdaptiveVectorClock::less_equal_sparse(const AdaptiveVectorClock& other) const{
    auto this_start = sparse_begin();
    auto this_end = sparse_end();

    auto other_start = other.sparse_begin();
    auto other_end = other.sparse_end();

    while (this_start != this_end && other_start != other_end) {
        if (this_start->tid == other_start->tid) {
            if (this_start->val > other_start->val) return false;
            ++this_start; ++other_start;
        } else if (this_start->tid < other_start->tid) {
            return false;
        } else {
            ++other_start;
        }
    }

    // this_start == this_end means no values are left in the vc and maybe there are in other
    // which implicitly means we compare 0's (from this) to potentially non-0 from other
    // which means this is <= (so we return true)
    return this_start == this_end;
}

// Assumes both this and other are in sparse mode
bool AdaptiveVectorClock::less_than_sparse(const AdaptiveVectorClock& other) const {
    bool one_strictly_less = false;

    auto this_start = sparse_begin();
    auto this_end = sparse_end();

    auto other_start = other.sparse_begin();
    auto other_end = other.sparse_end();

    while (this_start != this_end && other_start != other_end) {
        if (this_start->tid == other_start->tid) {
            if (this_start->val > other_start->val) return false;
            else if (this_start->val < other_start->val) one_strictly_less = true;
            ++this_start; ++other_start;
        } else if (this_start->tid < other_start->tid) {
            return false;
        } else {
            one_strictly_less = true;
            ++other_start;
        }
    }

    return this_start == this_end && (one_strictly_less || other_start != other_end);
}

bool AdaptiveVectorClock::sparse_less_equal_dense(const AdaptiveVectorClock& other) const{
    for (auto curr = sparse_begin(); curr != sparse_end(); ++curr) {
        if (curr->val > other._raw_memory[curr->tid]){
            return false;
        }
    }

    return true;
}

// Same as sparse_less_equal_dense because of the reason described in sparse_merge_into_dense
bool AdaptiveVectorClock::sparse_less_than_dense(const AdaptiveVectorClock& other) const{
    return sparse_less_equal_dense(other);
}

// DENSE FUNCTIONS. THESE FUNCTIONS SHOULD BE CALLED WHEN is_dense == true

EventIdT* AdaptiveVectorClock::dense_begin() { 
    return reinterpret_cast<EventIdT*>(_raw_memory.data()); 
}

const EventIdT* AdaptiveVectorClock::dense_begin() const { 
    return reinterpret_cast<const EventIdT*>(_raw_memory.data()); 
}

const EventIdT* AdaptiveVectorClock::dense_end() const {
    return dense_begin() + meta_info.header.THREAD_COUNT;
}

size_t AdaptiveVectorClock::dense_size() const {
    return _raw_memory.size();
}

// Merges two dense vector clocks, taking the point-wise maximum.
bool AdaptiveVectorClock::merge_into_dense(const AdaptiveVectorClock& other){
    bool changed = false;

    for (size_t i = 0; i < _raw_memory.size(); ++i) {
        if (this->_raw_memory[i] < other._raw_memory[i]) {
            this->_raw_memory[i] = other._raw_memory[i];
            changed = true;
        }
    }
    return changed;
}

// this: dense, other: sparse, merges other into this
bool AdaptiveVectorClock::dense_merge_into_sparse(const AdaptiveVectorClock& other) {
    bool changed = false;
    
    // Iterate over other's entries and take it's value if it's greater than this's
    for (auto curr = other.sparse_begin(); curr != other.sparse_end(); ++curr) {
        if (_raw_memory[curr->tid] < curr->val){
            _raw_memory[curr->tid] = curr->val;
            changed = true;
        }
    }

    return changed;
}

bool AdaptiveVectorClock::less_equal_dense(const AdaptiveVectorClock& other) const{
    for (size_t i = 0; i < _raw_memory.size(); ++i) {
        if (_raw_memory[i] > other._raw_memory[i]) {
            return false;
        }
    }
    
    return true;
}

bool AdaptiveVectorClock::less_than_dense(const AdaptiveVectorClock& other) const{
    bool one_strictly_less = false; 

    for (size_t i = 0; i < _raw_memory.size(); ++i) {
        if (_raw_memory[i] > other._raw_memory[i]) {
            return false;
        } else if (_raw_memory[i] < other._raw_memory[i]) {
            one_strictly_less = true;
        }
        
    }

    return one_strictly_less;
}

// Always false because of the reason described in sparse_merge_into_dense
bool AdaptiveVectorClock::dense_less_equal_sparse(const AdaptiveVectorClock& other) const{
    return false;
}

// Always false because of the reason described in sparse_merge_into_dense
bool AdaptiveVectorClock::dense_less_than_sparse(const AdaptiveVectorClock& other) const{
    return false;
}