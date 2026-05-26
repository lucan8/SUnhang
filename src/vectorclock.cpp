#include "../include/vectorclock.hpp"
#include "../include/util.hpp"
#include <algorithm>

VectorClock::VectorClock() {}

VectorClock::VectorClock(ThreadIdT tid) : owner_th(tid){
    this->_vector_clock.emplace_back(tid, 1);
}

bool VectorClock::merge_into(const VectorClock& other) {
    bool changed = false;

    // Static workspace
    static std::vector<ThEpoch> workspace;
    workspace.clear(); 

    auto it1 = this->_vector_clock.begin();
    auto it2 = other._vector_clock.begin();

    // Merge loop
    while (it1 != this->_vector_clock.end() && it2 != other._vector_clock.end()) {
        if (it1->tid == it2->tid) {
            VCValueT max_val = std::max(it1->val, it2->val);
            if (max_val > it1->val) changed = true;
            workspace.push_back({it1->tid, max_val});
            ++it1; ++it2;
        } else if (it1->tid < it2->tid) {
            workspace.push_back(*it1);
            ++it1;
        } else {
            workspace.push_back(*it2);
            changed = true;
            ++it2;
        }

        // Update the index of the owner thread
        if (owner_th.has_value() && workspace.back().tid == owner_th.value().tid){
            owner_th.value().idx = workspace.size() - 1;
        }
    }

    // Update the index of the owner thread
    while (it1 != this->_vector_clock.end()) { 
        workspace.push_back(*it1); 
        if (owner_th.has_value() && it1->tid == owner_th.value().tid) {
            owner_th.value().idx = workspace.size() - 1;
        }
        it1++; 
    }

    while (it2 != other._vector_clock.end()) { workspace.push_back(*it2++); changed = true; }

    if (changed) {
        this->_vector_clock.swap(workspace);
    }

    return changed;
}

// It decrements other, merges into this and increments other back
bool VectorClock::th_pred_merge_into(VectorClock& other) {
   other.decrement();
   bool res = merge_into(other);
   other.increment();

   return res;
}

// You better not call increment or decrement without owners!
void VectorClock::increment() {
    this->_vector_clock[owner_th.value().idx].val++;
}

void VectorClock::decrement() {
    size_t owner_th_idx = owner_th.value().idx;
    this->_vector_clock[owner_th_idx].val = std::max(0LL, this->_vector_clock[owner_th_idx].val - 1);
}

bool operator<=(const VectorClock& vc1, const VectorClock& vc2) {
    auto it1 = vc1._vector_clock.begin();
    auto it2 = vc2._vector_clock.begin();

    while (it1 != vc1._vector_clock.end() && it2 != vc2._vector_clock.end()) {
        if (it1->tid == it2->tid) {
            if (it1->val > it2->val) return false;
            ++it1; ++it2;
        } else if (it1->tid < it2->tid) {
            return false;
        } else {
            ++it2;
        }
    }

    return it1 == vc1._vector_clock.end();
}

bool operator<(const VectorClock& vc1, const VectorClock& vc2) {
    bool one_strictly_less = false;
    auto it1 = vc1._vector_clock.begin();
    auto it2 = vc2._vector_clock.begin();

    while (it1 != vc1._vector_clock.end() && it2 != vc2._vector_clock.end()) {
        if (it1->tid == it2->tid) {
            if (it1->val > it2->val) return false;
            else if (it1->val < it2->val) one_strictly_less = true;
            ++it1; ++it2;
        } else if (it1->tid < it2->tid) {
            return false;
        } else {
            one_strictly_less = true;
            ++it2;
        }
    }

    return it1 == vc1._vector_clock.end() && (one_strictly_less || it2 != vc2._vector_clock.end());
}

bool operator>(const VectorClock& vc1, const VectorClock& vc2) {
    return !(vc1 <= vc2);
}

bool operator==(const VectorClock& vc1, const VectorClock& vc2){
    return vc1 <= vc2 && vc2 <= vc1;
}

bool VectorClock::empty() const {
    return _vector_clock.empty();
}