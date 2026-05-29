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


// Groups the thread ID with its queue continuously in memory
struct ThreadCSHist {
    ThreadIdT tid;
    OwnedLazyQueue<std::vector<CSInfo>> queue;

    ThreadCSHist(ThreadIdT tid) : tid(tid) {
      queue.queue.reserve(10);
    }
};

struct CSHist {
    std::unordered_map<ResourceIdT, std::vector<ThreadCSHist>> _cs_hist;

    void reset() {
        for (auto& [res_id, th_vec] : _cs_hist) {
            for (auto& th_hist : th_vec) {
                th_hist.queue.reset();
            }
        }
    }

    CSInfo& add_lock_ev(ResourceIdT res_id, ThreadIdT tid, Event&& lock_ev) {
        auto& th_vec = _cs_hist[res_id];
        
        // Linearly search for the thread
        for (auto& th_hist : th_vec) {
            if (th_hist.tid == tid) {
                th_hist.queue.emplace(std::move(lock_ev));
                return th_hist.queue.back();
            }
        }

        // If thread hasn't used this lock yet, add it
        th_vec.emplace_back(tid);
        th_vec.back().queue.emplace(std::move(lock_ev));
        return th_vec.back().queue.back();
    }

    std::optional<CSInfo*> add_unlock_ev(ResourceIdT res_id, ThreadIdT tid, Event&& unlock_ev) {
        auto umap_it = _cs_hist.find(res_id);
        if (umap_it == _cs_hist.end()) return {};

        for (auto& th_hist : umap_it->second) {
            if (th_hist.tid == tid) {
                if (th_hist.queue.empty()) return {};
                
                CSInfo& cs = th_hist.queue.back();
                cs.unlock_ev = std::move(unlock_ev);
                return &cs;
            }
        }
        return {};
    }

    std::optional<CSInfo*> get_back(ResourceIdT res_id, ThreadIdT tid) {
        auto umap_it = _cs_hist.find(res_id);
        if (umap_it == _cs_hist.end()) return {};

        for (auto& th_hist : umap_it->second) {
            if (th_hist.tid == tid) {
                if (th_hist.queue.empty()) return {};
                return &th_hist.queue.back();
            }
        }
        return {};
    }
};
