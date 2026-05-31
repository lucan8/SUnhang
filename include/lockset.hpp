#pragma once

#include "sorted_vector.hpp"
#include "common_types.hpp"

typedef SortedVector<ResourceIdT> LocksetT;
// Format for LocksetT
template <>
struct std::formatter<LocksetT> : std::formatter<std::string> {
    auto format(const LocksetT& lockset, format_context& ctx) const {
        auto out = ctx.out();
        for (const auto& res_id : lockset._vec)
          std::format_to(out, "{}, ", res_id);
        return out;
    }
};

struct LocksetEntry{
  ResourceIdT res_id;
  size_t count;

  auto operator<=>(const LocksetEntry& other) const {
    return res_id <=> other.res_id;
  }

  bool operator==(const LocksetEntry& other) const {
    return res_id == other.res_id;
  }

  LocksetEntry(ResourceIdT res_id) : res_id(res_id), count(0){}
};

struct UReentrantLocksetT{
    SortedVector<LocksetEntry> _active_locks; 

    UReentrantLocksetT(size_t lock_depth){
        _active_locks._vec.reserve(lock_depth); 
    }

    // Returns true if the lock is acquired for the first time
    bool acquire(ResourceIdT lock_id){
        LocksetEntry& entry = _active_locks.unique_insert(lock_id);
        entry.count += 1;

        return entry.count == 1;
    }
    
    // Returns true if the lock is released for the last time
    bool release(ResourceIdT lock_id){
        bool last_time = false;

        auto it = _active_locks.find(lock_id);
        size_t& count = const_cast<size_t&>(it->count);
        count -= 1;

        if (count == 0) {
           _active_locks._vec.erase(it);
           last_time = true;
        }

        return last_time;
    }

    bool contains(ResourceIdT lock_id) const{
        return _active_locks.contains(lock_id);
    }

    LocksetT to_lockset() const{
      LocksetT lockset(_active_locks._vec.size());
      for (int i = 0; i < lockset._vec.size(); ++i){
        lockset._vec[i] = _active_locks._vec[i].res_id;
      }
      return lockset;
    }

    size_t size() const{
        return _active_locks._vec.size(); 
    }

    bool empty() const{
        return size() <= 0;
    }
};

// res_id should be the id of either a lock or a cond_var
// for locks it returns the id of the associated cond var, for cond var the associated lock
inline ResourceIdT get_ass_sync_obj(ResourceIdT res_id){
    return -res_id;
}

inline bool is_cond_var(ResourceIdT res_id){
    return res_id < 0;
}

// Returns true if ls1 and ls2 intersect, false otherwise
// Currently iterates of ls2 and checks if all elements are in ls1
inline bool lockset_intersection(const LocksetT& ls1, const LocksetT& ls2){
    for (const auto lock : ls2._vec)
        if (ls1.contains(lock))
            return true;
    return false;
}

// Returns true if ls1 and ls2 intersect, false otherwise
// Doesn't count cond vars for intersection
inline bool lockset_intersection_soft(const LocksetT& ls1, const LocksetT& ls2){
    for (const auto lock : ls2._vec){
        if (ls1.contains(lock) && !is_cond_var(lock))
            return true;
    }
    return false;
}

// Insert the elements of src in dst ignoring cond vars
// Returns true if no duplicates were found false otherwise, stopping at the first found element
inline bool insert_lockset(const LocksetT& src, ULocksetT& dst){
    for (const auto& res_id : src._vec){
        if (!is_cond_var(res_id)){ // Ignore cond vars
            auto [it, inserted] = dst.insert(res_id);
            if (!inserted){
                return false;
            }
        }
    }

    return true;
}