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

bool DeadlockChecker::is_sync_preserving_dlk(const NodeChainT& cycle){

}
