#include "../include/deadlock_checker.hpp"

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
                auto cs_opt = cs_queue.pop_until(vc, CSInfo::less_than_vc);
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

bool DeadlockChecker::is_sync_preserving_dlk(const NodeChainT& cycle){

}
