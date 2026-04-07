#pragma once

#include "ord_dep_graph.hpp"
#include "scc_enumerator.hpp"
#include "ord_dep_graph.hpp"

struct CycleEnumerator{
    OrdDepGraphView& graph_view;

    // Holds the resulted cycles from enum_cycles
    std::vector<NodeChainT> res_cycles;
    
    // Helper structures for the actual cycle algorithm
    std::unordered_set<NodeConstItT, IteratorHasher, IteratorHasher> blocked_nodes;
    std::unordered_map<NodeConstItT, NodeUSetT, IteratorHasher, IteratorHasher> rec_block_map;
    NodeChainT stack;
    MinSCC curr_min_scc;
    
    CycleEnumerator(OrdDepGraphView& graph_view)
        : graph_view(graph_view), curr_min_scc(graph_view.get_nodes_end()){
    }

    std::vector<NodeChainT> enum_cycles();
    bool _enum_cycles(NodeConstItT node);

    void _reset_helper_structs(const MinSCC& min_scc);
    void _unblock(NodeConstItT node);
    void _rec_block(NodeConstItT node, NodeChainRangeT neigh_list);

    void print_info() const;
};
