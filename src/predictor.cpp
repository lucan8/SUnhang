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
    Logger::print(LogType::DBG, "Read event");
    thread_map[evt.thread_id].vec_clock.merge_into(last_write[evt.target]);
}

void Predictor::write_event(const EventInfo& evt) {
    Logger::print(LogType::DBG, "Write event");
    last_write[evt.target] = thread_map[evt.thread_id].vec_clock;
}

void Predictor::acquire_event(const EventInfo& evt) {
    Logger::print(LogType::DBG, "Acquire event");  
    
    ThreadInfo& th_info = thread_map[evt.thread_id];

    // Create abstract dependency and add it's instance's timestamp to the vector
    AbsDependency dep(evt.thread_id, evt.target, th_info.lockset);
    auto [it, inserted] = graph_view.graph.abs_deps_map.try_emplace(std::move(dep), std::vector<VectorClock>{});
    it->second.push_back(th_info.vec_clock);

    // Locks from lockset should point to this dependency
    if (inserted)
        for (const auto lock : th_info.lockset)
            lock_dep_map[lock].push_back(&(it->first));

    // Add lock to lockset
    th_info.lockset.insert(evt.target);
}

void Predictor::release_event(const EventInfo& evt) {
    Logger::print(LogType::DBG, "Release event");

    thread_map[evt.thread_id].lockset.erase(evt.target);
}

void Predictor::fork_event(const EventInfo& evt) {
    Logger::print(LogType::DBG, "Fork event");
}

void Predictor::join_event(const EventInfo& evt) {
    Logger::print(LogType::DBG, "Join event");
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

void Predictor::build_neigh_list() {
    for (const auto&[dep, evt] : graph_view.graph.abs_deps_map){
        // Get candidate neighbours
        auto lock_dep_it = lock_dep_map.find(dep.resource_id);
        if (lock_dep_it == lock_dep_map.end())
            continue;
        
        // Add valid candidates to the neigbour list of dep
        for (const auto& cand : lock_dep_it->second)
            if (dep.is_valid_neigh_cand_opt(*cand))
                graph_view.graph.neigh_list[&dep].push_back(cand);
    }

    // Sort the neighbour list of each node, this will be needed later
    for (auto&[dep, neigh_list] : graph_view.graph.neigh_list){
        std::sort(neigh_list.begin(), neigh_list.end(), PtrLess());
    }

    graph_view.init_start_structs();
}

void Predictor::print_lock_deps_map() const{
    Logger::print(LogType::INFO, "LOCK DEPENDENCIES MAP");
    Logger::print(LogType::INFO, "------------------------------------");

    for (const auto& [lock, dep_vec] : lock_dep_map){
        Logger::print(LogType::DBG, "(Lock){}: {}(Dep count)", lock, dep_vec.size());
        for (const auto dep : dep_vec)
            Logger::print(LogType::DBG, "{}", *dep);
    }

    Logger::print(LogType::INFO, "Num locks: {}", lock_dep_map.size());
    Logger::print(LogType::INFO, "------------------------------------");
}

void Predictor::print_neigh_list() const{
    Logger::print(LogType::INFO, "NEIGHBOUR LIST");
    Logger::print(LogType::INFO, "------------------------------------");

    for (const auto& [dep, dep_vec] : graph_view.graph.neigh_list){
        Logger::print(LogType::DBG, "{}(dep): {}(neigh count)", *dep, dep_vec.size());
        for (const auto dep : dep_vec)
            Logger::print(LogType::DBG, "{}", *dep);
    }

    Logger::print(LogType::INFO, "Num deps that have neigh: {}", graph_view.graph.neigh_list.size());
    Logger::print(LogType::INFO, "------------------------------------");
}