#include <algorithm>

#include "../include/predictor.hpp"
#include "../include/logger.hpp"

bool Predictor::handle_event(const EventInfo& evt){
    switch (evt.event_type){
        case EventsT::LK:
            acquire_event(evt);
            break;
        case EventsT::UK:
            release_event(evt);
            break;
        case EventsT::RD:
            read_event(evt); 
            break;
        case EventsT::WR:
            write_event(evt);
            break;
        case EventsT::FORK:
            fork_event(evt);
            break;
        case EventsT::JOIN:
            join_event(evt);
            break;
        default:
            return false;
    }

    // Time passes for this thread(and it can't be stopped...)
    thread_map[evt.thread_id].vec_clock.increment(evt.thread_id);
    return true;
}

void Predictor::read_event(const EventInfo& evt) {
    // Logger::print(LogType::DBG, "Read event");

    if (thread_map.size() <= 1)
        return;

    thread_map[evt.thread_id].vec_clock.merge_into(last_write[evt.target]);
}

void Predictor::write_event(const EventInfo& evt) {
    // Logger::print(LogType::DBG, "Write event");

    if (thread_map.size() <= 1)
        return;

    last_write[evt.target] = thread_map[evt.thread_id].vec_clock;
}

void Predictor::acquire_event(const EventInfo& evt) {
    // Logger::print(LogType::DBG, "Acquire event");
    acq_count++;

    if (thread_map.size() <= 1)
        return;
    
    ThreadInfo& th_info = thread_map[evt.thread_id];

    // Add lock vc to critical section history and add lock to lockset
    CSInfo& cs_info = cs_hist.add_lock_ev(evt.target, evt.thread_id, Event(th_info.vec_clock, evt.line, evt.src_loc));

    // Don't create deps for first level lock acquisitions
    // Ignore deps created when only one thread executes
    if (!th_info.lockset.empty()){
        // Create abstract dependency and add it's instance's vc to the vector(as a ref to cs_hist's entry)
        AbsDependency dep(evt.thread_id, evt.target, th_info.lockset);
        auto [it, inserted] = graph_view.graph.abs_deps_map.try_emplace(std::move(dep), std::vector<const Event*>{});
        it->second.push_back(&cs_info.lock_ev);

        // Locks from lockset should point to this dependency
        if (inserted)
            for (const auto lock : th_info.lockset)
                lock_dep_map[lock].push_back(it);
    }

    th_info.lockset.insert(evt.target);
}

void Predictor::release_event(const EventInfo& evt) {
    // Logger::print(LogType::DBG, "Release event");

    if (thread_map.size() <= 1)
        return;
        
    ThreadInfo& th_info = thread_map[evt.thread_id];
    th_info.lockset.erase(evt.target);
    
    cs_hist.add_unlock_ev(evt.target, evt.thread_id, std::move(Event(th_info.vec_clock, evt.line, evt.src_loc)));
}

void Predictor::fork_event(const EventInfo& evt) {
    // Logger::print(LogType::DBG, "Fork event");
    ThreadInfo& th_info = thread_map[evt.thread_id];
    ThreadInfo& target_info = thread_map.emplace(evt.target, ThreadInfo()).first->second;

    target_info.vec_clock.merge_into(th_info.vec_clock);
}

void Predictor::join_event(const EventInfo& evt) {
    // Logger::print(LogType::DBG, "Join event");
    ThreadInfo& th_info = thread_map[evt.thread_id];
    ThreadInfo& target_info = thread_map.extract(evt.target).mapped();

    th_info.vec_clock.merge_into(target_info.vec_clock);
}

void Predictor::build_neigh_list() {
    for (auto node_it = graph_view.get_real_nodes_start(); node_it != graph_view.get_nodes_end(); ++node_it){
        // Get candidate neighbours
        auto lock_dep_it = lock_dep_map.find(node_it->first.resource_id);
        if (lock_dep_it == lock_dep_map.end())
            continue;
        
        // Add valid candidates to the neigbour list of dep
        for (auto cand : lock_dep_it->second)
            if (node_it->first.is_valid_neigh_cand(cand->first))
                graph_view.graph.neigh_list[node_it].push_back(cand);
    }

    // Sort the neighbour list of each node, this will be needed later
    for (auto&[dep, neigh_list] : graph_view.graph.neigh_list){
        std::sort(neigh_list.begin(), neigh_list.end(), NodeItLess(graph_view.get_nodes_end()));
    }

    graph_view.init_start_structs();
}

void Predictor::print_abs_deps() const{
    Logger::print(LogType::INFO, "ABSTRACT DEPENDENCIES");
    Logger::print(LogType::INFO, "------------------------------------");

    for (const auto& [dep, timestamps] : graph_view.graph.abs_deps_map){
        Logger::print(LogType::DBG, "{}: {}", dep ,timestamps.size());
    }

    Logger::print(LogType::INFO, "Num deps: {}", graph_view.graph.abs_deps_map.size());
    Logger::print(LogType::INFO, "------------------------------------");
}

void Predictor::print_lock_deps_map() const{
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

void Predictor::print_neigh_list() const{
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

void Predictor::print_abs_deps(std::FILE* out_file) const{
    Logger::print(out_file, "ABSTRACT DEPENDENCIES");
    Logger::print(out_file, "------------------------------------");

    for (const auto& [dep, timestamps] : graph_view.graph.abs_deps_map){
        Logger::print(out_file, "{}: {}", dep ,timestamps.size());
    }

    Logger::print(out_file, "Num deps: {}", graph_view.graph.abs_deps_map.size());
    Logger::print(out_file, "------------------------------------");
}

void Predictor::print_neigh_list(std::FILE* out_file) const{
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