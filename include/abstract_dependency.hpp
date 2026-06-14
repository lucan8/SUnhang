#pragma once

#include <string>
#include <vector>
#include <format>
#include <unordered_map>
#include <set>

#include "common_types.hpp"
#include "lockset.hpp"
#include "event.hpp"
#include "util.hpp"

struct AbsDepView {
    ThreadIdT thread_id;
    ResourceIdT resource_id;
    const LocksetT& lockset;
};

struct AbsDependency{
  ThreadIdT thread_id;
  ResourceIdT resource_id;
  LocksetT lockset;

  AbsDependency(ThreadIdT thread_id, ResourceIdT resource_id, const LocksetT& lockset)
    : thread_id(thread_id), resource_id(resource_id), lockset(lockset){}

  // Implements all comparison operators in the default way(first compare by thread, then resource, then lockset)
  auto operator<=>(const AbsDependency&) const = default;

  // Comparison agains the view
  auto operator<=>(const AbsDepView& rhs) const {
        if (auto cmp = thread_id <=> rhs.thread_id; cmp != 0) return cmp;
        if (auto cmp = resource_id <=> rhs.resource_id; cmp != 0) return cmp;
        return lockset._vec <=> rhs.lockset._vec;
    }

  // return true if thread_ids and resource_ids differ and locksets don't intersect, false otherwise
  bool is_valid_neigh_cand(const AbsDependency& other) const{
    return thread_id != other.thread_id && resource_id != other.resource_id && !lockset_intersection(lockset, other.lockset);
  }
  
  // return true if thread_ids and resource_ids differ and locksets don't softly intersect, false otherwise
  bool is_valid_neigh_cand_soft(const AbsDependency& other) const{
    return thread_id != other.thread_id && resource_id != other.resource_id && !lockset_intersection_soft(lockset, other.lockset);
  }
  
  // return true if thread_ids differ and locksets don't intersect, false otherwise
  bool is_valid_neigh_cand_opt(const AbsDependency& other) const{
    return thread_id != other.thread_id && !lockset_intersection(lockset, other.lockset);
  }

  // TODO: Change this when separating locks and cond var
  bool is_lock_dep() const{
    return resource_id >= 0;
  }
};

// Format for AbsDependency
template <>
struct std::formatter<AbsDependency> : std::formatter<std::string> {
    auto format(const AbsDependency& dep, format_context& ctx) const {
        return std::format_to(ctx.out(), "{}, {}, ({})", dep.thread_id, dep.resource_id, dep.lockset);
    }
};

// The explicit std::less<> is necesssary to allow transparent lookup 
typedef std::set<AbsDependency, std::less<>> AbsDepContainerT;
typedef AbsDepContainerT::const_iterator AbsDepConstItT;

struct AbsDepConstItComp {
    bool operator()(const AbsDepConstItT& a, const AbsDepConstItT& b) const {

        return *a < *b;
    }
};


typedef std::unordered_map<ResourceIdT, std::vector<AbsDepConstItT>> LockDepMapT;

typedef std::unordered_map<AbsDepConstItT, std::unordered_map<SrcLocT, std::vector<Event>>, IteratorHasher> DepLocToEvMapT;