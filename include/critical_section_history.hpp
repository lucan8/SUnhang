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

// Comparator between CSInfo and VectorClock.
// The order matters
struct CSInfoLess{
    // cs.lock_ev.vc < vc
    bool operator()(const CSInfo& cs, const VectorClock& vc) const {
        return cs.lock_ev.vc < vc;
    }

    // !(cs.lock_ev.vc <= vc)
    bool operator()(const VectorClock& vc, const CSInfo& cs) const {
        return !(cs.lock_ev.vc <= vc);
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

struct CSHist {
    // Index in the first is lock id, index in the second is thread id
    std::vector<std::vector<OwnedLazyQueue<std::vector<CSInfo>>>> _cs_hist;

    CSHist(size_t lock_count, size_t thread_count) 
        : _cs_hist(lock_count, std::vector<OwnedLazyQueue<std::vector<CSInfo>>>(thread_count)){}

    void reset() {
        for (auto& th_vec : _cs_hist) {
            for (auto& th_hist : th_vec) {
                th_hist.reset();
            }
        }
    }

    CSInfo& add_lock_ev(ResourceIdT res_id, ThreadIdT tid, Event&& lock_ev) {
        return _cs_hist[res_id][tid].emplace(std::move(lock_ev));
    }

    std::optional<const CSInfo*> add_unlock_ev(ResourceIdT res_id, ThreadIdT tid, Event&& unlock_ev) {
        auto cs_opt = get_back(res_id, tid);
        if (!cs_opt.has_value()) return {};

        CSInfo* cs = const_cast<CSInfo*>(cs_opt.value());
        cs->unlock_ev = std::move(unlock_ev);
        return cs;
    }

    std::optional<const CSInfo*> get_back(ResourceIdT res_id, ThreadIdT tid) const {
        auto& th_vec = _cs_hist[res_id][tid];

        // Are you releasing without locking first?
        if (th_vec.empty()) return {};

        return &th_vec.back();
    }
};