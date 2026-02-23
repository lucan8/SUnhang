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


void VectorClock::merge_into(const VectorClock& other) {
    for(const auto &[thread_id, value]: other._vector_clock) {
        auto vc = this->_vector_clock.find(thread_id);
        if(vc == this->_vector_clock.end()) {
            this->_vector_clock[thread_id] = value;
        } else if(vc->second < value) {
            vc->second = value;
        }
    }
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

bool operator==(const VectorClock& vc1, const VectorClock& vc2){
    return vc1 <= vc2 && vc2 <= vc1;
}

// ';' delimiter
std::string VectorClock::show() {
  std::string s;
   for(const auto &[thread_id, value] : this->_vector_clock) {
     s = s + std::to_string(thread_id) + ":" + std::to_string(value) + ";";
    }

   return s;
}