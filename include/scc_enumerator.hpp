#pragma once

#include <map>
#include "scc_enumerator_types.hpp"
#include "ord_dep_graph.hpp"

// Helper class that takes a dependency graph and gives the strongly connected component that has the smallest node
struct SCCEnumerator{
    // Read only graph view
    OrdDepGraphView& graph_view;

    // The strongest connected component that contains the minimum node
    MinSCC res_min_scc;

    // for debugging
    std::vector<MinSCC> res_scc_vec;

    // Structure holding metadata about each node that is needed by the get_min_strong_conn_comp functions
    std::unordered_map<NodeConstItT, AbsDepInfo, IteratorHasher, IteratorHasher> dep_info_map;
    
    // Also needed by get_min_strong_conn_comp
    int max_index;
    NodeChainT stack;

    SCCEnumerator(OrdDepGraphView& graph_view)
        : graph_view(graph_view), max_index(0), res_min_scc(graph_view.get_nodes_end()){}

    // Returns the SCC with the smallest node overall
    MinSCC get_min_strong_conn_comp();

    // Recursive function that is called on each unvisited dependency
    // This is the implementation of tarjan's algorithm for SCC enumeration modified to return the component with the minimum id 
    void _get_min_strong_conn_comp(NodeConstItT dep);

    // Should only be called after get_min_strong_conn_comp, prints information about the results
    void print_info() const;
};