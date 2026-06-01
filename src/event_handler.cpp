#include <algorithm>
#include <numeric>
#include <memory>

#include "../include/event_handler.hpp"
#include "../include/meta_info.hpp"
#include "../include/logger.hpp"

void CVInfo::sleep_thread(){
    to_notify_count += 1;
}

void CVInfo::notify_thread(const VectorClock& notif_vc, bool notif_all){
    if (to_notify_count == 0){
        return;
    }

    size_t notified_th_count = notif_all ? to_notify_count : 1;
    notif_queue.emplace(notif_vc, notified_th_count);
    to_notify_count -= notified_th_count;
}

void CVInfo::wake_thread(){
    if (notif_queue.empty()){
        return;
    }

    notif_queue.front().second -= 1;
    if (notif_queue.front().second == 0){
        notif_queue.pop();
    }
}

EventHandler::EventHandler(size_t thread_count, size_t var_count, size_t lock_count) 
    : alive_th_count(1), last_write(var_count), cs_hist(lock_count, thread_count){
    // Initialize thread_map
    thread_map.reserve(thread_count);
    for (int i = 0; i < thread_count; ++i){
        thread_map.emplace_back(i, meta_info.LOCK_DEPTH);
    }
}

bool EventHandler::handle_event(const EventInfo& evt_info){
    switch (evt_info.event_type){
        case EventsT::RD:
            read_event(evt_info); 
            break;
        case EventsT::WR:
            write_event(evt_info);
            break;
        case EventsT::LK:
            acquire_event(evt_info);
            break;
        case EventsT::UK:
            release_event(evt_info);
            break;
        case EventsT::WAIT:
            wait_event(evt_info);
            break;
        case EventsT::NOTIFY:
            notify_event(evt_info, false);
            break;
        case EventsT::NOTIFYALL:
            notify_event(evt_info, true);
            break;
        case EventsT::FORK:
            fork_event(evt_info);
            break;
        case EventsT::JOIN:
            join_event(evt_info);
            break;
        default:
            return false;
    }

    // Time passes for this thread(and it can't be stopped...)
    thread_map[evt_info.thread_id].vec_clock.increment();
    return true;
}

void EventHandler::read_event(const EventInfo& evt_info) {
    if (alive_th_count <= 1)
        return;

    thread_map[evt_info.thread_id].vec_clock.merge_into(last_write[evt_info.target]);
}

void EventHandler::write_event(const EventInfo& evt_info) {
    if (alive_th_count <= 1)
        return;

    last_write[evt_info.target] = thread_map[evt_info.thread_id].vec_clock;
}

void EventHandler::wait_event(const EventInfo& evt_info) {
    // TODO: CALLING WAIT AS A SINGLE THREAD IS PLAIN STUPID, PROBABLY SHOULD PRINT AN ERROR
    if (alive_th_count <= 1)
        return;
    
    ThreadInfo& th_info = thread_map[evt_info.thread_id];

    th_info.is_asleep = true;
    cv_map[evt_info.target].sleep_thread();

    //TODO: Unnecessary copy in handle_dep_creation
    Event evt = Event(th_info.vec_clock, evt_info.tr_pos);
    handle_dep_creation(th_info, evt_info, evt);
}

void EventHandler::notify_event(const EventInfo& evt_info, bool notify_all) {
    // TODO: CALLING NOTIFY AS A SINGLE THREAD IS PLAIN STUPID, PROBABLY SHOULD PRINT AN ERROR
    if (alive_th_count <= 1)
        return;
    
    ThreadInfo& th_info = thread_map[evt_info.thread_id];

    auto& rec_sync_status_cont = th_info.recent_sync_status_cont;
    auto& container = rec_sync_status_cont.container;
    RecentSyncStatusContT new_rec_sync_status_cont;

    for (auto it = container.begin(); it != container.end(); ++it){
        auto& sync_status = it->data.value();

        // // Sync status is just a resource(first level locks/cond_vars)
        // if (std::holds_alternative<ResourceIdT>(sync_status)) {
        //     ResourceIdT res_id = std::get<ResourceIdT>(sync_status);
            
        //     // Ignore assoicated lock
        //     if (res_id == get_ass_sync_obj(evt_info.target)){
        //         continue;
        //     }
            
        //     // Set event and lockset containing the cond var
        //     Event evt = Event(th_info.vec_clock, evt_info.tr_pos);
        //     LocksetT lockset;
        //     lockset.insert(evt_info.target);
            
        //     // Add the new dependency to the new recent statuses array
        //     AbsDepConstItT new_dep = create_dep(evt_info.thread_id, res_id, lockset, evt);
        //     new_rec_sync_status_cont.push(new_dep);
        // } else if (std::holds_alternative<AbsDepConstItT>(sync_status)) { // Sync status is a dep
            AbsDepConstItT old_dep = std::get<AbsDepConstItT>(sync_status);
            AbsDepConstItT new_dep = update_dep(old_dep, evt_info.target);

            new_rec_sync_status_cont.push(new_dep);
        //}
    }

    th_info.recent_sync_status_cont = std::move(new_rec_sync_status_cont);
    cv_map[evt_info.target].notify_thread(th_info.vec_clock, notify_all);
}

void EventHandler::acquire_event(const EventInfo& evt_info) {
    acq_count++;
    ThreadInfo& th_info = thread_map[evt_info.thread_id];

    // TODO: This shouldn't really be executed if the thread is alone
    handle_sleepness(th_info, evt_info.target);

    // Order matters: First call handle_dep_creation then move the event in cs_hist
    Event evt = Event(th_info.vec_clock, evt_info.tr_pos);
    
    // Only create dependency (BEFORE acquiring the lock) if thread is not alone.
    if (alive_th_count > 1){
        // Order matters: First call handle_dep_creation then move the event in cs_hist
        handle_dep_creation(th_info, evt_info, evt);
    }

    // // Add lock to lockset and if it's the first time acquiring, add the event to cs_hist as well(reentrant behaviour)
    if (th_info.u_reen_lockset.acquire(evt_info.target)){
        cs_hist.add_lock_ev(evt_info.target, evt_info.thread_id, std::move(evt));
    }
}

void EventHandler::release_event(const EventInfo& evt_info) {
    ThreadInfo& th_info = thread_map[evt_info.thread_id];
    
    // Release lock and add the event only if it was the last release(reentrant behaviour)
    if (th_info.u_reen_lockset.release(evt_info.target)){
        cs_hist.add_unlock_ev(evt_info.target, evt_info.thread_id, std::move(Event(th_info.vec_clock, evt_info.tr_pos)));
    }
}

void EventHandler::fork_event(const EventInfo& evt_info) {
    // Logger::print(LogType::DBG, "Fork event");
    ThreadInfo& th_info = thread_map[evt_info.thread_id];
    ThreadInfo& target_info = thread_map[evt_info.target];
    alive_th_count++;

    target_info.vec_clock.merge_into(th_info.vec_clock);
}

void EventHandler::join_event(const EventInfo& evt_info) {
    // Logger::print(LogType::DBG, "Join event");
    ThreadInfo& th_info = thread_map[evt_info.thread_id];
    ThreadInfo target_info = thread_map[evt_info.target];
    alive_th_count--;

    th_info.vec_clock.merge_into(target_info.vec_clock);
}

AbsDepConstItT EventHandler::update_dep(AbsDepConstItT old_dep, ResourceIdT new_res){ 
    // Nothing to update here
    if (old_dep->lockset.contains(new_res)){
        return old_dep;
    }

    // Get old dep
    auto dep_graph_nh = abs_deps.extract(old_dep);

    // Remove old entries
    for (auto res : old_dep->lockset._vec){
        lock_dep_map[res].erase(old_dep);
    }
    auto dep_loc_ev_map_nh = dep_loc_ev_map.extract(old_dep);
    
    // Add the new resource to the lockset and re-insert
    dep_graph_nh.value().lockset.insert(new_res);
    auto new_dep = abs_deps.insert(std::move(dep_graph_nh));
    auto new_dep_it = new_dep.position;

    // Dependency already exists? Add the new events to the list
    if (!new_dep.inserted){
        for (const auto& entry : dep_loc_ev_map_nh.mapped()){
           dep_loc_ev_map[new_dep_it][entry.first].append_range(entry.second);
        }
    }
    else{ // Update lock_dep_map with the new iterator
        for (auto res : new_dep_it->lockset._vec){
            lock_dep_map[res].insert(new_dep_it);
        }
        dep_loc_ev_map[new_dep_it] = std::move(dep_loc_ev_map_nh.mapped());
    }

    return new_dep_it;
}

AbsDepConstItT EventHandler::create_dep(ThreadIdT tid, ResourceIdT desired_res, const LocksetT& lockset,
                                        SrcLocT src_loc, const Event& evt){

    // Search for entry using a view(less overhead)
    auto it = abs_deps.find(AbsDepView{tid, desired_res, lockset});
    
    // Create the object and insert only if found
    if (it == abs_deps.end()) {
        AbsDependency dep(tid, desired_res, lockset);
        auto [inserted_it, inserted] = abs_deps.emplace(std::move(dep));
        it = inserted_it;

        // Locks from lockset should point to this dependency
        if (inserted)
            for (const auto lock : lockset._vec)
                lock_dep_map[lock].insert(it);
    }

    dep_loc_ev_map[it][src_loc].push_back(evt);
    
    return it;
}

void EventHandler::handle_sleepness(ThreadInfo& th_info, ResourceIdT ass_lock_id){
    if (th_info.is_asleep){
        // Get the cv of this associated lock and it's info
        ResourceIdT cv_id = get_ass_sync_obj(ass_lock_id);
        auto cv_info_it = cv_map.find(cv_id);
        // assert(cv_info_it != cv_map.end()); //REMOVE THIS
        
        // Acknowledge the event as happening before this
        CVInfo& cv_info = cv_info_it->second;

        //TODO: The trace is bad if this happened
        if (!cv_info.notif_queue.empty()){
            th_info.vec_clock.merge_into(cv_info.notif_queue.front().first);
        }

        // Wake-up
        th_info.is_asleep = false;
        cv_info.wake_thread();
    }
}

void EventHandler::handle_dep_creation(ThreadInfo& th_info, const EventInfo& evt_info, const Event& evt){
    // Create dep and store in recent statuses if this is a notifying thread
    if (!meta_info.NOTIF_THREADS.empty() && meta_info.NOTIF_THREADS[evt_info.thread_id]){
        AbsDepConstItT dep = create_dep(evt_info.thread_id, evt_info.target, th_info.u_reen_lockset.to_lockset(), evt_info.src_loc, evt);
        th_info.recent_sync_status_cont.push(dep);
    }
    else if(!th_info.u_reen_lockset.empty()){ // otherwise just create the dep if lockset is not empty
        create_dep(evt_info.thread_id, evt_info.target, th_info.u_reen_lockset.to_lockset(), evt_info.src_loc, evt);
    }
}

void EventHandler::print_abs_deps() const{
    Logger::print(LogType::INFO, "ABSTRACT DEPENDENCIES");
    Logger::print(LogType::INFO, "------------------------------------");

    for (const auto& dep : abs_deps){
        Logger::print(LogType::DBG, "{}", dep);
    }

    Logger::print(LogType::INFO, "Num deps: {}", abs_deps.size());
    Logger::print(LogType::INFO, "------------------------------------");
}

void EventHandler::print_lock_deps_map() const{
    Logger::print(LogType::INFO, "LOCK DEPENDENCIES MAP");
    Logger::print(LogType::INFO, "------------------------------------");

    for (const auto& [lock, dep_vec] : lock_dep_map){
        Logger::print(LogType::DBG, "(Lock){}: {}(Dep count)", lock, dep_vec._vec.size());
        for (const auto dep : dep_vec._vec)
            Logger::print(LogType::DBG, "{}", *dep);
    }

    Logger::print(LogType::INFO, "Num locks: {}", lock_dep_map.size());
    Logger::print(LogType::INFO, "------------------------------------");
}

void EventHandler::print_comm_abs_deps() const{
    Logger::print(LogType::DBG, "COMMUNICATION ABSTRACT DEPENDENCIES");
    Logger::print(LogType::DBG, "------------------------------------");

    size_t count = 0;
    for (const auto& dep : abs_deps){
        if (is_cond_var(dep.resource_id)){
            Logger::print(LogType::DBG, "{}", dep);
            count += 1;
       }
    }

    Logger::print(LogType::DBG, "Num deps: {}", count);
    Logger::print(LogType::DBG, "------------------------------------");
}


void EventHandler::print_summary(std::FILE* log_file) const{
    Logger::print(log_file, "num acq/req: {}", acq_count);
    Logger::print(log_file, "num deps: {}", abs_deps.size());
}

void EventHandler::print_summary() const{
    Logger::print(LogType::DBG, "num acq/req: {}", acq_count);
}

void EventHandler::print_th_exit_with_locks() const{
    for (const auto& [tid, th_info] : std::views::enumerate(thread_map)){
        LocksetT lockset = th_info.u_reen_lockset.to_lockset();
        if (!lockset._vec.empty()){
        Logger::print(LogType::WARN, "Thread {} exited holding locks {}", tid, lockset);
        }
    }
}

void EventHandler::print_th_vc_info() const{
    uint64_t sum = 0;
    for (const auto& [tid, th_info] : std::views::enumerate(thread_map)){
        sum += th_info.vec_clock.size();
    }
    Logger::print(LogType::DBG, "mean={}, count={}", sum / thread_map.size(), thread_map.size());
}

void EventHandler::print_th_lockset_info() const{
    uint64_t sum = 0;
    for (const auto& [tid, th_info] : std::views::enumerate(thread_map)){
        sum += th_info.u_reen_lockset.size();
    }
    Logger::print(LogType::DBG, "mean={}, count={}", sum / thread_map.size(), thread_map.size());
}