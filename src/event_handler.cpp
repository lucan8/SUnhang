#include "../include/event_handler.hpp"
#include "../include/logger.hpp"
#include <algorithm>
#include <numeric>
#include <memory>

EventHandler::EventHandler(size_t thread_count, size_t var_count) 
    : alive_th_count(1), last_write(var_count){
    // Initialize thread_map
    thread_map.reserve(thread_count);
    for (int i = 0; i < thread_count; ++i){
        thread_map.emplace_back(i);
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
    Event evt = Event(th_info.vec_clock, evt_info.line, evt_info.src_loc);
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
        //     Event evt = Event(th_info.vec_clock, evt_info.line, evt_info.src_loc);
        //     LocksetT lockset;
        //     lockset.insert(evt_info.target);
            
        //     // Add the new dependency to the new recent statuses array
        //     NodeConstItT new_dep = create_dep(evt_info.thread_id, res_id, lockset, evt);
        //     new_rec_sync_status_cont.push(new_dep);
        // } else if (std::holds_alternative<NodeConstItT>(sync_status)) { // Sync status is a dep
            NodeConstItT old_dep = std::get<NodeConstItT>(sync_status);
            NodeConstItT new_dep = update_dep(old_dep, evt_info.target);

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
    Event evt = Event(th_info.vec_clock, evt_info.line, evt_info.src_loc);

    // Only create dependency (BEFORE acquiring the lock) if thread is not alone.
    if (alive_th_count > 1){
        handle_dep_creation(th_info, evt_info, evt);
    }

    // Add ev to critical section history and add lock to lockset
    CSInfo& cs_info = cs_hist.add_lock_ev(evt_info.target, evt_info.thread_id, std::move(evt));
    th_info.u_reen_lockset.acquire(evt_info.target);
}

void EventHandler::release_event(const EventInfo& evt_info) {
    ThreadInfo& th_info = thread_map[evt_info.thread_id];
    
    // TODO: This could use a safe mode that checks the release was spurious
    // Check it using the u_reen_lockset, cs_hist is not very reliable
    th_info.u_reen_lockset.release(evt_info.target);
    
    cs_hist.add_unlock_ev(evt_info.target, evt_info.thread_id, std::move(Event(th_info.vec_clock, evt_info.line, evt_info.src_loc)));
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

NodeConstItT EventHandler::update_dep(NodeConstItT old_dep, ResourceIdT new_res){ 
    // Nothing to update here
    if (old_dep->first.lockset.contains(new_res)){
        return old_dep;
    }

    // Get old dep
    auto node_handle = graph_view.graph.abs_deps_map.extract(old_dep);
    
    // Add the new resource to the lockset and re-insert
    node_handle.key().lockset.insert(new_res);
    auto new_dep = graph_view.graph.abs_deps_map.insert(std::move(node_handle));
    auto new_dep_it = new_dep.position;
    node_handle = std::move(new_dep.node);

    // Dependency already exists? Add the new events to the list
    if (!new_dep.inserted){
        new_dep.position->second.append_range(node_handle.mapped());
        // Remove the old_dep mappings
        for (auto res : old_dep->first.lockset._vec){
            lock_dep_map[res].erase(old_dep);
        }
    }
    else{
        // Update with lock_dep_map with the new iterator only if there is a new iterator
        for (auto res : old_dep->first.lockset._vec){
            lock_dep_map[res].erase(old_dep);
            lock_dep_map[res].insert(new_dep_it);
        }
        // Add new_res to the lock_dep_map
        lock_dep_map[new_res].insert(new_dep_it);
    }

    return new_dep_it;
}

NodeConstItT EventHandler::create_dep(ThreadIdT tid, ResourceIdT desired_res, const LocksetT& lockset,
                               const Event& evt){

    // Search for entry using a view(less overhead)
    auto it = graph_view.graph.abs_deps_map.find(AbsDepView{tid, desired_res, lockset});

    // Create the object and insert only if found
    if (it == graph_view.graph.abs_deps_map.end()) {
        AbsDependency dep(tid, desired_res, lockset);
        auto [inserted_it, inserted] = graph_view.graph.abs_deps_map.emplace(std::move(dep), std::vector<Event>{});
        it = inserted_it;

        // Locks from lockset should point to this dependency
        if (inserted)
            for (const auto lock : lockset._vec)
                lock_dep_map[lock].insert(it);
    }

    // Add the event of this dependency
    it->second.push_back(evt);
    
    return it;
}

void EventHandler::build_neigh_list() {
    for (auto node_it = graph_view.get_real_nodes_start(); node_it != graph_view.get_nodes_end(); ++node_it){
        // Get candidate neighbours
        auto lock_dep_it = lock_dep_map.find(node_it->first.resource_id);
        if (lock_dep_it == lock_dep_map.end())
            continue;
        
        // Add valid candidates to the neigbour list of dep
        for (auto cand : lock_dep_it->second)
            if (node_it->first.is_valid_neigh_cand_soft(cand->first))
                graph_view.graph.neigh_list[node_it].push_back(cand);
    }

    // Sort the neighbour list of each node, this will be needed later
    for (auto&[dep, neigh_list] : graph_view.graph.neigh_list){
        std::sort(neigh_list.begin(), neigh_list.end(), NodeItLess(graph_view.get_nodes_end()));
    }

    graph_view.init_start_structs();
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
        NodeConstItT dep = create_dep(evt_info.thread_id, evt_info.target, th_info.u_reen_lockset._active_locks, evt);
        th_info.recent_sync_status_cont.push(dep);
    }
    else if(!th_info.u_reen_lockset.empty()){ // otherwise just create the dep if lockset is not empty
        create_dep(evt_info.thread_id, evt_info.target, th_info.u_reen_lockset._active_locks, evt);
    }
}

void EventHandler::print_abs_deps() const{
    Logger::print(LogType::INFO, "ABSTRACT DEPENDENCIES");
    Logger::print(LogType::INFO, "------------------------------------");

    for (const auto& [dep, timestamps] : graph_view.graph.abs_deps_map){
        Logger::print(LogType::DBG, "{}: {}", dep ,timestamps.size());
    }

    Logger::print(LogType::INFO, "Num deps: {}", graph_view.graph.abs_deps_map.size());
    Logger::print(LogType::INFO, "------------------------------------");
}

void EventHandler::print_lock_deps_map() const{
    Logger::print(LogType::INFO, "LOCK DEPENDENCIES MAP");
    Logger::print(LogType::INFO, "------------------------------------");

    for (const auto& [lock, dep_vec] : lock_dep_map){
        Logger::print(LogType::DBG, "(Lock){}: {}(Dep count)", lock, dep_vec.size());
        for (const auto dep : dep_vec)
            Logger::print(LogType::DBG, "{}", dep->first);
    }

    Logger::print(LogType::INFO, "Num locks: {}", lock_dep_map.size());
    Logger::print(LogType::INFO, "------------------------------------");
}

void EventHandler::print_neigh_list() const{
    Logger::print(LogType::INFO, "NEIGHBOUR LIST");
    Logger::print(LogType::INFO, "------------------------------------");

    for (const auto& [dep, neigh_list] : graph_view.graph.neigh_list){
        Logger::print(LogType::DBG, "{}(dep): {}(neigh count)", dep->first, neigh_list.size());
        for (const auto neigh : neigh_list)
            Logger::print(LogType::DBG, "{}", neigh->first);
    }

    Logger::print(LogType::INFO, "Num deps that have neigh: {}", graph_view.graph.neigh_list.size());
    Logger::print(LogType::INFO, "------------------------------------");
}

void EventHandler::print_abs_deps(std::FILE* out_file) const{
    Logger::print(out_file, "ABSTRACT DEPENDENCIES");
    Logger::print(out_file, "------------------------------------");

    for (const auto& [dep, timestamps] : graph_view.graph.abs_deps_map){
        Logger::print(out_file, "{}: {}", dep ,timestamps.size());
    }

    Logger::print(out_file, "Num deps: {}", graph_view.graph.abs_deps_map.size());
    Logger::print(out_file, "------------------------------------");
}

void EventHandler::print_comm_abs_deps() const{
    Logger::print(LogType::DBG, "COMMUNICATION ABSTRACT DEPENDENCIES");
    Logger::print(LogType::DBG, "------------------------------------");

    size_t count = 0;
    for (const auto& [dep, timestamps] : graph_view.graph.abs_deps_map){
        if (is_cond_var(dep.resource_id)){
            Logger::print(LogType::DBG, "{}: {}", dep, timestamps.size());
            count += 1;
       }
    }

    Logger::print(LogType::DBG, "Num deps: {}", count);
    Logger::print(LogType::DBG, "------------------------------------");
}

void EventHandler::print_neigh_list(std::FILE* out_file) const{
    Logger::print(out_file, "NEIGHBOUR LIST");
    Logger::print(out_file, "------------------------------------");

    for (const auto& [dep, neigh_list] : graph_view.graph.neigh_list){
        Logger::print(out_file, "{}(dep): {}(neigh count)", dep->first, neigh_list.size());
        for (const auto neigh : neigh_list)
            Logger::print(out_file, "{}", neigh->first);
    }

    Logger::print(out_file, "Num deps that have neigh: {}", graph_view.graph.neigh_list.size());
    Logger::print(out_file, "------------------------------------");
}

void EventHandler::print_summary(std::FILE* log_file) const{
    Logger::print(log_file, "num acq/req: {}", acq_count);
    Logger::print(log_file, "num deps: {}", graph_view.graph.abs_deps_map.size());
}

void EventHandler::print_summary() const{
    Logger::print(LogType::DBG, "num acq/req: {}", acq_count);

    auto [lock_dep_count, cond_dep_count] = graph_view.graph.get_split_dep_counts();
    Logger::print(LogType::DBG, "num lock deps: {}", lock_dep_count);
    Logger::print(LogType::DBG, "num cond deps: {}", cond_dep_count);
}

void EventHandler::print_th_exit_with_locks() const{
    for (const auto& [tid, th_info] : std::views::enumerate(thread_map)){
        LocksetT lockset = th_info.u_reen_lockset._active_locks;
        if (!lockset._vec.empty()){
        Logger::print(LogType::WARN, "Thread {} exited holding locks {}", tid, lockset);
        }
    }
}

void EventHandler::print_th_vc_info() const{
    uint64_t sum = 0;
    for (const auto& [tid, th_info] : std::views::enumerate(thread_map)){
        sum += th_info.vec_clock._vector_clock.size();
    }
    Logger::print(LogType::DBG, "mean={}, count={}", sum / thread_map.size(), thread_map.size());
}

void EventHandler::print_th_lockset_info() const{
    uint64_t sum = 0;
    for (const auto& [tid, th_info] : std::views::enumerate(thread_map)){
        sum += th_info.u_reen_lockset._u_lock_map.size();
    }
    Logger::print(LogType::DBG, "mean={}, count={}", sum / thread_map.size(), thread_map.size());
}