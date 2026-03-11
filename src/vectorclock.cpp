#include "../include/vectorclock.hpp"

VectorClock::VectorClock() {}
VectorClock::VectorClock(ThreadIdT increment_thread_id) {
    this->set(increment_thread_id, 1);
}

VCValueT VectorClock::find(ThreadIdT thread_id) const {
    auto vc = this->_vector_clock.find(thread_id);

    if(vc == this->_vector_clock.end()) {
        return 0;
    } else {
        return vc->second;
    }
}

void VectorClock::set(ThreadIdT thread_id, VCValueT value) {
    this->_vector_clock[thread_id] = value;
}


// Loop through all the keys of each vc, compare epoch with the other, take maximum.
VectorClock VectorClock::merge(const VectorClock& other) const{
    VectorClock result;

    // Initialize result to this vector clock 
    for(const auto &[thread_id, timestamp] : this->_vector_clock) {
        result.set(thread_id, timestamp);
    }

    // Iterate over the other vector clock and for each thread pick the maximum timestamp
    for(const auto &[thread_id, timestamp] : other._vector_clock) {
        VCValueT curr_timestamp = result.find(thread_id);
        if(curr_timestamp < timestamp) {
            result.set(thread_id, timestamp);
        }
    }

    return result;
}

bool VectorClock::merge_into(const VectorClock& other) {
    bool changed = false;
    
    for(const auto &epoch: other._vector_clock) {
        changed = merge_into_epoch(epoch) || changed;
    }

    return changed;
}

bool VectorClock::pred_merge_into_epoch(const VectorClock& other, ThreadIdT tid){
    // Find thread epoch
    auto epoch_it = other._vector_clock.find(tid);
    if (epoch_it == other._vector_clock.end())
        return false;
    
    // Get predecesor
    ThEpoch epoch = *epoch_it;
    epoch.second -= 1;
    
    return merge_into_epoch(epoch);
}

// Merges epoch in this vector clock
bool VectorClock::merge_into_epoch(const ThEpoch& epoch){
    auto vc = this->_vector_clock.find(epoch.first);
    bool changed = false;

    if(vc == this->_vector_clock.end()) {
        this->_vector_clock[epoch.first] = epoch.second;
        changed = true;
    } else if(vc->second < epoch.second) {
        vc->second = epoch.second;
        changed = true;
    }

    return changed;
}

void VectorClock::increment(ThreadIdT thread_id) {
    this->_vector_clock[thread_id]++;
}


void VectorClock::decrement(ThreadIdT thread_id) {
    VCValueT& curr_ts =  this->_vector_clock[thread_id];
    curr_ts = std::max(0, curr_ts - 1);
}

bool operator<=(const VectorClock& vc1, const VectorClock& vc2) {
  for(const auto &[thread_id, value]: vc1._vector_clock) {
        VCValueT other_vc_val = vc2.find(thread_id);

        if(value > other_vc_val) {
            return false;
        }
    }

    return true;
}

bool operator<(const VectorClock& vc1, const VectorClock& vc2) {
  for(const auto &[thread_id, value]: vc1._vector_clock) {
        VCValueT other_vc_val = vc2.find(thread_id);

        if(value >= other_vc_val) {
            return false;
        }
    }

    return true;
}

bool operator>(const VectorClock& vc1, const VectorClock& vc2) {
    for(const auto &[thread_id, value]: vc1._vector_clock) {
        VCValueT other_vc_val = vc2.find(thread_id);

        if(value <= other_vc_val) {
            return false;
        }
    }

    return true;
}

bool operator==(const VectorClock& vc1, const VectorClock& vc2){
    return vc1 <= vc2 && vc2 <= vc1;
}

bool VectorClock::empty() const{
    return _vector_clock.empty();
}