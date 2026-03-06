# pragma once

#include "ord_dep_graph.hpp"

struct DeadlockChecker{
    bool is_dlk(const NodeChainT& cycle);
    bool is_abs_dlk_pattern(const NodeChainT& cycle);
    bool is_sync_preserving_dlk(const NodeChainT& cycle);
};