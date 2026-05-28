#include "../include/formatters.hpp"
#include "../include/deadlock_checker.hpp"
#include "../include/logger.hpp"

bool DeadlockChecker::is_abs_dlk_pattern_gen(const NodeChainT& cycle) const{
    ULocksetT desired_locks;
    UThreadSetT threads;
    ULocksetT acq_locks;

    for (auto node : cycle){
        // Insert and check thread
        auto inserted_thread = threads.insert(node->thread_id);
        if (!inserted_thread.second)
            return false;

        // Insert and check desired lock
        auto inserted_des_lock = desired_locks.insert(node->resource_id);
        if (!inserted_des_lock.second)
            return false;
        
        // Insert all locks of the node's lockset and check all of them were inserted
        if (!insert_lockset(node->lockset, acq_locks))
            return false;
    }

    return true;
}

void DeadlockChecker::_cartesian_prod_loc(const NodeLocToEvMapT& dep_loc_map, const NodeChainT& cycle, int curr_node_idx, AbsDlkPattern curr_res, std::vector<AbsDlkPattern>& res) const{
    if (curr_node_idx == cycle.size()){
        assert(curr_res.nodes.size() == curr_res.events.size()); // Sanity check
        res.push_back(std::move(curr_res));
        return;
    }

    auto node = cycle[curr_node_idx];
    AbsDlkPattern next_res(std::move(curr_res));

    for (const auto& [src_loc, evts] : dep_loc_map.at(node)) {
        next_res.nodes[curr_node_idx] = SimpleNode(node->thread_id, node->resource_id, src_loc);
        next_res.events[curr_node_idx] = ViewLazyQueue(evts);
        _cartesian_prod_loc(dep_loc_map, cycle, curr_node_idx + 1, next_res, res);
    }
}

std::vector<AbsDlkPattern> DeadlockChecker::get_abs_dlk_patterns(const NodeChainT& cycle){
    if (!is_abs_dlk_pattern_gen(cycle)){
        return {};
    }

    std::vector<AbsDlkPattern> res;
    _cartesian_prod_loc(dep_loc_map, cycle, 0, AbsDlkPattern(cycle.size()), res);
    return res;
}

bool DeadlockChecker::is_sync_preserving_dlk(AbsDlkPattern& cycle){
    VectorClock vc;
    cs_hist.reset();

    bool all_nodes_alive = true;
    while(all_nodes_alive){
        _update_vc_with_curr_cycle(cycle.events, vc);
        _get_sync_pres_closure(vc);
        
        if (_check_sync_pres_closure(cycle.events, vc)){
            return true;
        }
        
        all_nodes_alive = _update_abs_dep_start_ev(cycle.events, vc);
    }

    return {};
}

// Computes the sync preserving closure of vc and updates it in place
// TODO: Verifying theat max_cs_ind == -1 eveytime is a waste
void DeadlockChecker::_get_sync_pres_closure(VectorClock& vc){
    bool changed = false;
    
    do{
        for (auto& [res_id, th_cs_umap] : cs_hist._cs_hist){
            std::vector<const CSInfo*> lock_crit_sections;
            lock_crit_sections.reserve(th_cs_umap.size());

            int max_cs_ind = -1; // Using index instead of iterator as it is more stable

            for (auto& [th_id, cs_queue] : th_cs_umap){
                auto cs_opt = cs_queue.pop_until(vc, CSInfoComp(), false).first;
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
        const Event& ev = *ev_lazy_q.start_elem;
        if (ev.vc <= closure_vc)
            return false;
    }
    return true;
}

// TODO: Do we really need the predecessor?
void DeadlockChecker::_update_vc_with_curr_cycle(const std::vector<EventLazyQueue>& cycle_evt, VectorClock& vc) const{
    for (int i = 0; i < cycle_evt.size(); ++i){
        Event& ev = const_cast<Event&>(*cycle_evt[i].start_elem);
        
        // Merge the event predecessor in the vc 
        vc.th_pred_merge_into(ev.vc);
    }
}

// For each node, "removes" all events <= vc
// Returns false if any node became empty during the process(no more events to verify), true otherwise
// False is early returned, meaning not all nodes were updated!
bool DeadlockChecker::_update_abs_dep_start_ev(std::vector<EventLazyQueue>& cycle_evt, const VectorClock& vc) const{
    for (auto& ev_lazy_q : cycle_evt){
        ev_lazy_q.pop_until(vc, EventComp(), true);

        if (ev_lazy_q.empty())
            return false;
    }

    return true;
}