# pragma once

#include <cassert>

#include "ord_dep_graph.hpp"
#include "critical_section_history.hpp"



// Helper struct that holds only the relevant info about a node for output 
struct SimpleNode{
    ThreadIdT tid;
    ResourceIdT res_id;
    SrcLocT src_loc;

    auto operator<=>(const SimpleNode&) const = default;
};

typedef std::vector<SimpleNode> SimpleNodeChainT;

// Struct that represents the cycles from to perspectives
// The actual nodes of the cycle and their events
struct AbsDlkPattern{
    std::vector<EventLazyQueue> events;
    std::vector<SimpleNode> nodes;

    AbsDlkPattern() = default;
    AbsDlkPattern(const std::vector<EventLazyQueue>& events, const SimpleNodeChainT& nodes)
        : events(events), nodes(nodes){
        assert(nodes.size() == events.size()); // sanity check
    }
    AbsDlkPattern(size_t size) : nodes(size), events(size, std::vector<Event>()){
    }

    size_t size() const{
        return nodes.size();
    }
};


template <>
struct std::formatter<SimpleNode> : std::formatter<std::string> {
  auto format(const SimpleNode& node, auto& ctx) const {
      return std::format_to(ctx.out(), "<{}, {}, {}>", node.tid, node.res_id, node.src_loc);
  }
};

struct DeadlockChecker{
    CSHist cs_hist;
    DepLocToEvMapT dep_loc_ev_map;
    
    DeadlockChecker(CSHist&& cs_hist, DepLocToEvMapT&& dep_loc_ev_map)
        : cs_hist(std::move(cs_hist)), dep_loc_ev_map(std::move(dep_loc_ev_map)){}

    // Answers the question: Can cycle generate abstract deadlock patterns?
    bool is_abs_dlk_pattern_gen(const NodeChainT& cycle) const;
    std::vector<AbsDlkPattern> get_abs_dlk_patterns(const NodeChainT& cycle);

    bool is_sync_preserving_dlk(AbsDlkPattern& cycle);

    void _get_sync_pres_closure(VectorClock& vc);

    bool _check_sync_pres_closure(const std::vector<EventLazyQueue>& cycle_evt, const VectorClock& closure_vc) const;

    // Update vc using the first event of each abstract dependency node
    void _update_vc_with_curr_cycle(const std::vector<EventLazyQueue>& cycle_evt, VectorClock& vc) const;

    // "Removes" all events that are <= vc
    // Returns true if any node became empty during the process(no more events to verify)
    bool _update_abs_dep_start_ev(std::vector<EventLazyQueue>& cycle_evt, const VectorClock& vc) const;

    // Helper function to calculate cartesian product
    // TODO: Generalize and move in util
    void _cartesian_prod_loc(const DepLocToEvMapT& dep_loc_ev_map, const NodeChainT& cycle, int curr_node_idx, AbsDlkPattern curr_res, std::vector<AbsDlkPattern>& res) const;
};