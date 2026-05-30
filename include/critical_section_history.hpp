#pragma once

#include <unordered_map>
#include "event.hpp"

// TODO: For CSHist, look into making the functions return const references as well

// Helper struct that hold the events for a critical section(one for lock, one for unlock)
struct CSInfo{
  Event lock_ev;
  Event unlock_ev;

  CSInfo(Event&& lock_ev, Event&& unlock_ev)
    : lock_ev(std::move(lock_ev)), unlock_ev(std::move(unlock_ev)){}

  CSInfo(Event&& lock_ev)
    : lock_ev(std::move(lock_ev)){}
  
  CSInfo() = default;
  
  // Compares two critical sections using trace location
  bool less_than_eq_tr(const CSInfo& other) const{
    return lock_ev.tr_pos <= other.lock_ev.tr_pos && unlock_ev.tr_pos <= other.lock_ev.tr_pos;
  }
  
  // Compares two critical sections using trace location
  bool less_than_tr(const CSInfo& other) const{
    return lock_ev.tr_pos < other.lock_ev.tr_pos && unlock_ev.tr_pos < other.lock_ev.tr_pos;
  }
};

// Comparator between CSInfo and VectorClock'
// The order matters! Always put vc to the right as it usually is the sync preserving closure
struct CSInfoComp{
    bool operator()(const CSInfo& cs, const VectorClock& vc) const {
        return cs.lock_ev.vc < vc;
    }

    bool operator()(const VectorClock& vc, const CSInfo& cs) const {
        return cs.lock_ev.vc > vc;
    }
};

struct ThreadCSHist {
    ThreadIdT tid;
    OwnedLazyQueue<std::vector<CSInfo>> queue;

    auto operator<=>(const ThreadCSHist& other) const {
        return tid <=> other.tid;
    }

    auto operator==(const ThreadCSHist& other) const{
        return tid == other.tid;
    }

    ThreadCSHist(ThreadIdT tid) : tid(tid) {}
};

// THE POINTERS MIGHT BECOME DANGLING AS THE PROGRAM EXECUTES
// THEIR SOLE PURPOSE IS TO GIVE A VIEW ON THE INTERNAL OBJECTS(TO AVOID COPYING)
// VERY NOT THREAD SAFE
struct CSHist {
    std::unordered_map<ResourceIdT, SortedVector<ThreadCSHist>> _cs_hist;

    void reset() {
        for (auto& [res_id, th_vec] : _cs_hist) {
            for (auto& th_hist : th_vec._vec) {
                th_hist.queue.reset();
            }
        }
    }

    CSInfo& add_lock_ev(ResourceIdT res_id, ThreadIdT tid, Event&& lock_ev) {
        auto& th_vec = _cs_hist[res_id];
        auto& th_hist = th_vec.unique_insert(tid);    
        th_hist.queue.emplace(std::move(lock_ev));

        return th_hist.queue.back();
    }

    std::optional<const CSInfo*> add_unlock_ev(ResourceIdT res_id, ThreadIdT tid, Event&& unlock_ev) {
        auto cs_opt = get_back(res_id, tid);
        if (!cs_opt.has_value()) return {};

        CSInfo* cs = const_cast<CSInfo*>(cs_opt.value());
        cs->unlock_ev = std::move(unlock_ev);
        return cs;
    }

    std::optional<const CSInfo*> get_back(ResourceIdT res_id, ThreadIdT tid) const {
        auto umap_it = _cs_hist.find(res_id);

        // Are you releasing without locking first?
        if (umap_it == _cs_hist.end()) return {};
        
        auto& th_vec = umap_it->second;
        auto th_hist_it = th_vec.find(tid);

        // Are you releasing without locking first?
        if (th_hist_it == th_vec._vec.end() || th_hist_it->queue.empty()) return {};

        return &th_hist_it->queue.back();
    }
};
