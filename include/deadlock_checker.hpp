# pragma once

#include "ord_dep_graph.hpp"

// Helper struct that holds together the actual cycle nodes(dependency + events)
// And the wrapped version of the events
struct Cycle{
    const std::vector<EventLazyQueue>& wrapped_events;
    const NodeChainT& nodes;

    Cycle(const std::vector<EventLazyQueue>& wrapped_events, const NodeChainT& nodes)
        : wrapped_events(wrapped_events), nodes(nodes){
        assert(nodes.size() == wrapped_events.size()); // sanity check
    }

    size_t size() const{
        return nodes.size();
    }
};

struct DeadlockChecker{
    CSHist& cs_hist;
    
    DeadlockChecker(CSHist& cs_hist)
        : cs_hist(cs_hist){}

    bool is_dlk(const NodeChainT& cycle);
    bool is_abs_dlk_pattern(const NodeChainT& cycle);

    // In reality at the end of the function the cycle remains unchanged
    bool is_sync_preserving_dlk(const NodeChainT& cycle);

    void _get_sync_pres_closure(VectorClock& vc);

    bool _check_sync_pres_closure(const std::vector<EventLazyQueue>& cycle_evt, const VectorClock& closure_vc) const;

    // Update vc using the first event of each abstract dependency node
    void _update_vc_with_curr_cycle(const Cycle& cycle_evt, VectorClock& vc) const;

    // "Removes" all events that are <= vc
    // Returns true if any node became empty during the process(no more events to verify)
    bool _update_abs_dep_start_ev(std::span<EventLazyQueue> cycle_evt, const VectorClock& vc) const;

    // Wraps the the events (that the nodes in the cycle point to) in a lazy queue
    // TODO: This should probably not be here as it mereley does conversion
    std::vector<EventLazyQueue> _node_chain_to_lazy_q_vec(const NodeChainT& cycle) const;
};