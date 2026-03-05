#pragma once

#include <map>
#include <unordered_set>
#include <format>

#include "predictor_types.hpp"
#include "ord_dep_graph.hpp"

// Information about the dependency that is needed whilst computing the strongly connected component
struct AbsDepInfo{
    int index;
    int low_index;
    bool on_stack;

    AbsDepInfo(int index, int low_index, bool on_stack)
        : index(index), low_index(low_index), on_stack(on_stack){}
};


// Contains the strongly connected component as an unordered_set of nodes(subgraph) and a pointer to the minimum node 
struct MinSCC{
    NodeSetT nodes;
    NodeConstItT min_node;
    NodeConstItT sentinel_node;

    MinSCC(NodeConstItT sentinel_node): nodes(), min_node(sentinel_node), sentinel_node(sentinel_node){}

    bool operator<(const MinSCC& other) const{
        if (sentinel_node != other.sentinel_node)
            throw std::runtime_error("Can't compare iterators with different sentinel nodes!");
            
        return NodeItLess(sentinel_node)(min_node, other.min_node);
    }

    bool is_empty() const{
        return nodes.empty();
    }

    bool has(NodeConstItT node) const{
        return nodes.find(node) != nodes.end();
    }
};

// Formatter for MinSCC
template <>
struct std::formatter<MinSCC> : std::formatter<std::string> {
    auto format(const MinSCC& scc, format_context& ctx) const {
        auto out = ctx.out();

        for (const auto& node : scc.nodes) {
            out = std::format_to(out, "{}\n", node->first);
        }
        
        return std::format_to(out, "Min node: {}", scc.min_node->first);
    }
};

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