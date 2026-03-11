#include "../include/deadlock_checker.hpp"
#include "../include/logger.hpp"
bool DeadlockChecker::is_dlk(const NodeChainT& cycle) {
    return is_abs_dlk_pattern(cycle) && is_sync_preserving_dlk(cycle);
}

bool DeadlockChecker::is_abs_dlk_pattern(const NodeChainT& cycle){
    ULocksetT desired_locks;
    UThreadSetT threads;
    ULocksetT acq_locks;

    for (auto node : cycle){
        // Insert and check thread
        auto inserted_thread = threads.insert(node->first.thread_id);
        if (!inserted_thread.second)
            return false;

        // Insert and check desired lock
        auto inserted_des_lock = desired_locks.insert(node->first.resource_id);
        if (!inserted_des_lock.second)
            return false;
        
        // Insert all locks of the node's lockset and check all of them were inserted
        size_t init_acq_locks_size = acq_locks.size();
        acq_locks.insert(node->first.lockset.begin(), node->first.lockset.end());
        if (acq_locks.size() - init_acq_locks_size != node->first.lockset.size())
            return false;
    }

    return true;
}

// 
bool DeadlockChecker::is_sync_preserving_dlk(const NodeChainT& cycle){
    VectorClock vc;
    size_t alive_nodes_count = cycle.size();

    // Wrap the event vectors of the dependencies in lazy queues
    std::vector<EventLazyQueue> cycle_evt = _node_chain_to_lazy_q_vec(cycle);

    while(alive_nodes_count){
        _update_vc_with_curr_cycle(Cycle(cycle_evt, cycle), vc);

        _get_sync_pres_closure(vc);
        
        Logger::print(LogType::DBG, "{}", cycle_evt);

        bool is_sync_pres_dlk = _check_sync_pres_closure(cycle_evt, vc);
        
        Logger::print(LogType::NONE, "{}", is_sync_pres_dlk);
        
        if (is_sync_pres_dlk)
            return true;
        
        
        alive_nodes_count -= _update_abs_dep_start_ev(cycle_evt, vc);
    }

    return false;
}

// Computes the sync preserving closure of vc and updates it in place
// TODO: Verifying theat max_cs_ind == -1 eveytime is a waste
void DeadlockChecker::_get_sync_pres_closure(VectorClock& vc){
    bool changed = false;
    cs_hist.reset();
    
    do{
        for (auto& [res_id, th_cs_umap] : cs_hist._cs_hist){
            std::vector<const CSInfo*> lock_crit_sections;
            lock_crit_sections.reserve(th_cs_umap.size());

            int max_cs_ind = -1; // Using index instead of iterator as it is more stable

            for (auto& [th_id, cs_queue] : th_cs_umap){
                auto cs_opt = cs_queue.pop_until(vc, CSInfoComp(), false);
                if (cs_opt.has_value()){
                    // Add critical section to vector
                    const CSInfo* cs = cs_opt.value();
                    lock_crit_sections.push_back(cs);
                    
                    // Update maximum based on trace ordering
                    if (max_cs_ind == -1 || lock_crit_sections[max_cs_ind]->less_than_tr(*cs)){
                        max_cs_ind = lock_crit_sections.size() - 1;
                    }
                }
            }

            // Update the vector clock with the matching releases ignoring max_cs_ind
            for (int i = 0 ; i < max_cs_ind; ++i)
                changed = vc.merge_into(lock_crit_sections[i]->unlock_ev.vc);

            for (int i = max_cs_ind + 1; i < lock_crit_sections.size(); ++i)
                changed = vc.merge_into(lock_crit_sections[i]->unlock_ev.vc);
        }
    } while (changed);
}

// Update vc using the first events of each abstract dependency
bool DeadlockChecker::_check_sync_pres_closure(const std::vector<EventLazyQueue>& cycle_evt, const VectorClock& closure_vc) const {
    for (const auto& ev_lazy_q : cycle_evt){
        if (ev_lazy_q.empty())
            continue;

        const Event* ev = *ev_lazy_q.start_elem;
        if (ev->vc > closure_vc)
            return true;
    }
    return false;
}

// TODO: Do we really need the predecessor?
void DeadlockChecker::_update_vc_with_curr_cycle(const Cycle& cycle_evt, VectorClock& vc) const{
    for (int i = 0; i < cycle_evt.size(); ++i){
        EventLazyQueue ev_lazy_q = cycle_evt.wrapped_events[i];
        if (ev_lazy_q.empty())
            continue;

        const Event* ev = *ev_lazy_q.start_elem;
        ThreadIdT tid = cycle_evt.nodes[i]->first.thread_id;
        
        // Merge the event predecessor in the vc 
        vc.pred_merge_into_epoch(ev->vc, tid);
    }
}

// For each node, "removes" all events <= vc
// Returns the number of nodes that are now empty
size_t DeadlockChecker::_update_abs_dep_start_ev(std::span<EventLazyQueue> cycle_evt, const VectorClock& vc) const{
    size_t dead_node_count = 0;
    
    for (auto& ev_lazy_q : cycle_evt){
        if (ev_lazy_q.empty())
            continue;
        ev_lazy_q.pop_until(vc, EventPtrComp(), true);

        if (ev_lazy_q.empty())
            dead_node_count += 1;
    }

    return dead_node_count;
}

 // Wraps the the events (that the nodes in the cycle point to) in a lazy queue
// TODO: This should probably not be here as it mereley does conversion
std::vector<EventLazyQueue> DeadlockChecker::_node_chain_to_lazy_q_vec(const NodeChainT& cycle) const{
    std::vector<EventLazyQueue> nodes_events;
    nodes_events.reserve(cycle.size());

    for (const auto& node : cycle)
        nodes_events.emplace_back(node->second);
    
    return nodes_events;
}