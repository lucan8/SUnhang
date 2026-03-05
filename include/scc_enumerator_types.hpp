#pragma once

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