#include "../include/vectorclock.hpp"
#include <algorithm>

const size_t MAX_THREADS = 401;

VectorClock::VectorClock() : _vector_clock(MAX_THREADS, 0) {}

VectorClock::VectorClock(ThreadIdT increment_thread_id) : _vector_clock(MAX_THREADS, 0) {
    this->set(increment_thread_id, 1);
}

VCValueT VectorClock::find(ThreadIdT thread_id) const {
    if (thread_id < MAX_THREADS) {
        return this->_vector_clock[thread_id];
    }
    return 0;
}

void VectorClock::set(ThreadIdT thread_id, VCValueT value) {
    if (thread_id < MAX_THREADS) {
        this->_vector_clock[thread_id] = value;
    }
}

VectorClock VectorClock::merge(const VectorClock& other) const {
    VectorClock result;

    for (size_t i = 0; i < MAX_THREADS; ++i) {
        result._vector_clock[i] = std::max(this->_vector_clock[i], other._vector_clock[i]);
    }

    return result;
}

bool VectorClock::merge_into(const VectorClock& other) {
    bool changed = false;
    
    for (size_t i = 0; i < MAX_THREADS; ++i) {
        if (other._vector_clock[i] > 0) {
            if (this->_vector_clock[i] < other._vector_clock[i]) {
                this->_vector_clock[i] = other._vector_clock[i];
                changed = true;
            }
        }
    }

    return changed;
}

bool VectorClock::th_pred_merge_into(const VectorClock& other, ThreadIdT tid) {
    bool changed = false;
    
    for (size_t i = 0; i < MAX_THREADS; ++i) {
        VCValueT val = other._vector_clock[i];
        if (val == 0) continue;

        if (i == tid) {
            changed = merge_into_epoch(ThEpoch(tid, std::max(0, val - 1))) || changed;
        } else {
            changed = merge_into_epoch(ThEpoch(i, val)) || changed;
        }
    }

    return changed;
}

bool VectorClock::pred_merge_into_epoch(const VectorClock& other, ThreadIdT tid) {
    if (tid >= MAX_THREADS) return false;
    
    VCValueT val = other._vector_clock[tid];
    if (val == 0) return false;
    
    ThEpoch epoch(tid, val - 1);
    return merge_into_epoch(epoch);
}

bool VectorClock::merge_into_epoch(const ThEpoch& epoch) {
    if (epoch.first >= MAX_THREADS) return false;

    if (this->_vector_clock[epoch.first] < epoch.second) {
        this->_vector_clock[epoch.first] = epoch.second;
        return true;
    }

    return false;
}

void VectorClock::increment(ThreadIdT thread_id) {
    if (thread_id < MAX_THREADS) {
        this->_vector_clock[thread_id]++;
    }
}

void VectorClock::decrement(ThreadIdT thread_id) {
    if (thread_id < MAX_THREADS) {
        this->_vector_clock[thread_id] = std::max(0, this->_vector_clock[thread_id] - 1);
    }
}

bool operator<=(const VectorClock& vc1, const VectorClock& vc2) {
    for (size_t i = 0; i < MAX_THREADS; ++i) {
        if (vc1._vector_clock[i] > 0 && vc1._vector_clock[i] > vc2._vector_clock[i]) {
            return false;
        }
    }
    return true;
}

bool operator<(const VectorClock& vc1, const VectorClock& vc2) {
    bool one_strictly_less = false; 
    
    for (size_t i = 0; i < MAX_THREADS; ++i) {
        if (vc1._vector_clock[i] > 0 || vc2._vector_clock[i] > 0) {
            if (vc1._vector_clock[i] > vc2._vector_clock[i]) {
                return false;
            } else if (vc1._vector_clock[i] < vc2._vector_clock[i]) {
                one_strictly_less = true;
            }
        }
    }

    return one_strictly_less;
}

bool operator>(const VectorClock& vc1, const VectorClock& vc2) {
    return !(vc1 <= vc2);
}

bool operator==(const VectorClock& vc1, const VectorClock& vc2){
    return vc1 <= vc2 && vc2 <= vc1;
}

bool VectorClock::empty() const {
    for (size_t i = 0; i < MAX_THREADS; ++i) {
        if (this->_vector_clock[i] > 0) {
            return false;
        }
    }
    return true;
}