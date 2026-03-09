# pragma once

#include "ord_dep_graph.hpp"

struct DeadlockChecker{
    CSHist& cs_hist;
    
    DeadlockChecker(CSHist& cs_hist)
        : cs_hist(cs_hist){}

    bool is_dlk(const NodeChainT& cycle);
    bool is_abs_dlk_pattern(const NodeChainT& cycle);
    bool is_sync_preserving_dlk(const NodeChainT& cycle);

    void _get_sync_pres_closure(VectorClock& vc);
};