#include "../include/event_handler.hpp"
#include "../include/logger.hpp"
#include <algorithm>
#include <memory>


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
            notify_event(evt_info);
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
    thread_map[evt_info.thread_id].vec_clock.increment(evt_info.thread_id);
    return true;
}

void EventHandler::read_event(const EventInfo& evt_info) {
    // Logger::print(LogType::DBG, "Read event");

    if (thread_map.size() <= 1)
        return;

    thread_map[evt_info.thread_id].vec_clock.merge_into(last_write[evt_info.target]);
}

void EventHandler::write_event(const EventInfo& evt_info) {
    // Logger::print(LogType::DBG, "Write event");

    if (thread_map.size() <= 1)
        return;

    last_write[evt_info.target] = thread_map[evt_info.thread_id].vec_clock;
}

void EventHandler::wait_event(const EventInfo& evt_info) {
    // Logger::print(LogType::DBG, "Wait event");
    
    ThreadInfo& th_info = thread_map[evt_info.thread_id];

    // TODO: CALLING WAIT AS A SINGLE THREAD IS PLAIN STUPID, PROBABLY SHOULD PRINT AN ERROR
    // TODO: CALLING WAIT WITH AN EMPTY LOCKSET IS EVEN WORSE, SHOULD PROBABLY PRINT AN ERROR
    if (th_info.u_reen_lockset.empty() || thread_map.size() <= 1)
        return;
    
    // If lockset only has associated lock for cond_var add it to the recent statuses
    if (th_info.u_reen_lockset.contains_only(get_ass_sync_obj(evt_info.target))){
        th_info.recent_sync_status_arr.push(evt_info.target);
    }
    else{ // Else create a new dep and add it as a recent status
        Event evt = Event(th_info.vec_clock, evt_info.line, evt_info.src_loc);
        NodeConstItT dep = create_dep(evt_info.thread_id, evt_info.target, th_info.u_reen_lockset.to_lockset(), evt);
        th_info.recent_sync_status_arr.push(dep);
    }
}

void EventHandler::notify_event(const EventInfo& evt_info) {
    // Logger::print(LogType::DBG, "Write event");

    ThreadInfo& th_info = thread_map[evt_info.thread_id];

    // TODO: CALLING NOTIFY AS A SINGLE THREAD IS PLAIN STUPID, PROBABLY SHOULD PRINT AN ERROR
    // TODO: CALLING NOTIFY WITH AN EMPTY LOCKSET IS EVEN WORSE, SHOULD PROBABLY PRINT AN ERROR
    if (th_info.u_reen_lockset.empty() || thread_map.size() <= 1)
        return;
    
    RecentSyncStatusArrT new_arr;
    RecentSyncStatusArrT& curr_arr = th_info.recent_sync_status_arr;

    for (auto it = curr_arr.begin(); it != curr_arr.end(); it = curr_arr.next(it)){
        auto sync_status = *it;
        if (std::holds_alternative<ResourceIdT>(sync_status)) {
            Event evt = Event(th_info.vec_clock, evt_info.line, evt_info.src_loc);

            ResourceIdT res_id = std::get<ResourceIdT>(sync_status);
            LocksetT lockset;
            NodeConstItT new_dep;
            
            // Create dep from lock to cond var if it is not the associated lock(if so it is ignored)
            if (res_id != get_ass_sync_obj(evt_info.target)){
                lockset.insert(evt_info.target);
                NodeConstItT new_dep = create_dep(evt_info.thread_id, res_id, lockset, evt);
            }
            
            // Add the new dependency to the new recent statuses array
            // QUESTION: WHY NOT JUST SET INSTEAD OF PUSH
            new_arr.push(new_dep);
        } else if (std::holds_alternative<NodeConstItT>(sync_status)) {
            // Get old dep
            NodeConstItT ptr = std::get<NodeConstItT>(sync_status);
            auto node_handle = graph_view.graph.abs_deps_map.extract(ptr);
            
            // Add the cond_var to the lockset and re-insert
            node_handle.key().lockset.insert(evt_info.target);
            auto new_dep = graph_view.graph.abs_deps_map.insert(std::move(node_handle));

            // Update the status with the new dep
            sync_status = new_dep.position;
        }
    }

    curr_arr.merge_into(new_arr);
}

void EventHandler::acquire_event(const EventInfo& evt_info) {
    // Logger::print(LogType::DBG, "Acquire event");
    acq_count++;
    
    ThreadInfo& th_info = thread_map[evt_info.thread_id];
    Event evt = Event(th_info.vec_clock, evt_info.line, evt_info.src_loc);

    // Single threaded or first level lock -> no dep, only status
    if (th_info.u_reen_lockset.empty() || thread_map.size() <= 1){
        th_info.recent_sync_status_arr.push(evt_info.target);
    }
    else{ // otherwise both
        NodeConstItT dep = create_dep(evt_info.thread_id, evt_info.target, th_info.u_reen_lockset.to_lockset(), evt);
        th_info.recent_sync_status_arr.push(dep);
    }

    // Add ev to critical section history and add lock to lockset
    CSInfo& cs_info = cs_hist.add_lock_ev(evt_info.target, evt_info.thread_id, evt);
    th_info.u_reen_lockset.acquire(evt_info.target);
}

void EventHandler::release_event(const EventInfo& evt_info) {
    // Logger::print(LogType::DBG, "Release event");
        
    ThreadInfo& th_info = thread_map[evt_info.thread_id];
    
    // TODO: This could use a safe mode that checks the release was spurious
    // Check it using the u_reen_lockset, cs_hist is not very reliable
    th_info.u_reen_lockset.release(evt_info.target);
    
    cs_hist.add_unlock_ev(evt_info.target, evt_info.thread_id, std::move(Event(th_info.vec_clock, evt_info.line, evt_info.src_loc)));
}

void EventHandler::fork_event(const EventInfo& evt_info) {
    // Logger::print(LogType::DBG, "Fork event");
    ThreadInfo& th_info = thread_map[evt_info.thread_id];
    ThreadInfo& target_info = thread_map.emplace(evt_info.target, ThreadInfo()).first->second;

    target_info.vec_clock.merge_into(th_info.vec_clock);
}

void EventHandler::join_event(const EventInfo& evt_info) {
    // Logger::print(LogType::DBG, "Join event");
    ThreadInfo& th_info = thread_map[evt_info.thread_id];
    ThreadInfo target_info = thread_map.extract(evt_info.target).mapped();

    th_info.vec_clock.merge_into(target_info.vec_clock);
}

NodeConstItT EventHandler::create_dep(ThreadIdT tid, ResourceIdT desired_res, const LocksetT& lockset,
                               const Event& evt){
    // Create abstract dependency and add it's instance to the vector
    AbsDependency dep(tid, desired_res, lockset);
    auto [it, inserted] = graph_view.graph.abs_deps_map.try_emplace(std::move(dep), std::vector<Event>{});
    it->second.push_back(evt);

    // Locks from lockset should point to this dependency
    if (inserted)
        for (const auto lock : lockset)
            lock_dep_map[lock].push_back(it);
    
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