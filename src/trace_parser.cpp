#include <algorithm>
#include <cstring>

#include "../include/trace_parser.hpp"
#include "../include/logger.hpp"

const std::unordered_map<std::string, EventsT> TraceParser::std_event_map = {
    {"r", EventsT::RD}, {"w", EventsT::WR},
    {"fork", EventsT::FORK}, {"join", EventsT::JOIN},
    {"acq", EventsT::LK}, {"rel", EventsT::UK},
    {"wait", EventsT::WAIT}, {"notify", EventsT::NOTIFY}, 
    {"notifyAll", EventsT::NOTIFYALL}, {"broadcast", EventsT::NOTIFYALL}
};

const char TraceParser::trace_sep[2] = "|";
const uint8_t TraceParser::exp_trace_token_cnt = 4;

bool TraceParser::events_remaining(){
    return !feof(trace_file);
}

// Map the std formated result to our custom result using the std_* maps
// std format ex: T1|acq(l1)|25 might turn into 1, 1, 1 uisng the std_* maps
EventInfo TraceParser::from_std(const std::string& tid, EventsT ev_type, const std::string& target,
                                const std::string& src_loc) {
    EventInfo result = {};

    // Set event type and thread id
    result.event_type = ev_type;
    result.thread_id = th_id_map.get(tid);

    // Update maps depending on operation
    if (is_th_type(ev_type)) {
        result.target = th_id_map.get(target[0] == 'T' ? target : "T" + target);
    }
    else if(is_lock_type(ev_type)) {
        result.target = lock_id_map.get(target);
        shared_locks.update(result.thread_id, result.target);
    }
    else if(is_cv_type(ev_type)) {  
        result.target = get_ass_sync_obj(lock_id_map.get(target));
    }
    else{
        result.target = var_id_map.get(target);
        shared_vars.update(result.thread_id, result.target);
    }
    
    // Set src code location and the trace id
    result.src_loc = std::stoi(src_loc);
    result.line = line_index;

    return result;
}

std::optional<EventInfo> TraceParser::get_next_event() {
    line_index++;

    char buffer[64];
    fgets(buffer, sizeof(buffer), trace_file);
    
    char* tid_c = std::strtok(buffer, trace_sep);
    if (!tid_c) return {};
    
    char* ev_type_c = std::strtok(nullptr, "(");
    if (!ev_type_c) return {};
    
    // Check that the event is valid before proceeding
    auto ev_type_it = std_event_map.find(ev_type_c);
    if (ev_type_it == std_event_map.end()) return {};

    char* target_c = std::strtok(nullptr, ")");
    if (!target_c) return {};

    char* src_loc_c = std::strtok(nullptr, "\n");
    if (!src_loc_c) return {};

    // Ignore the trailing "|"
    src_loc_c += 1; 

    std::string tid(tid_c), target(target_c), src_loc(src_loc_c);

    // Convert the strings to our mapping
    EventInfo event = from_std(tid, ev_type_it->second, target, src_loc);

    return event;
}

// Helper function for parse_full_trace. Should not be used anywhere else
void TraceParser::_update_last_write(std::vector<EventInfo>& events, size_t ev_count, 
                                     std::vector<int>& last_write) const{
    int last_ev_idx = ev_count - 1;
    EventInfo& last_ev = events[last_ev_idx];

    if (last_ev.event_type == EventsT::WR){
        // Ignore the write before this and update
        int last_write_ev_id = last_write[last_ev.target];
        if (last_write_ev_id != -1){
            events[last_write_ev_id].ignored = true;
        }
        last_write[last_ev.target] = last_ev_idx;
    }
    // Acknowledge the last write(make it so it is not ignored)
    else if (last_ev.event_type == EventsT::RD){
        last_write[last_ev.target] = -1;
    }
}

std::vector<EventInfo> TraceParser::parse_full_trace(){
    std::vector<EventInfo> events(meta::EVENT_COUNT);
    size_t ev_count = 0;
    std::vector<int> last_write(meta::VAR_COUNT, -1);

    size_t invalid_ev_cnt = 0;
    while (events_remaining()){
        auto event_opt = get_next_event();

        if (event_opt.has_value()){
            EventInfo& ev_info = event_opt.value();
            events[ev_count++] = std::move(ev_info);
            _update_last_write(events, ev_count, last_write);
        }
        else{
            invalid_ev_cnt++;
        }

        // if (ev_count % 100000 == 0){
        //     Logger::print(LogType::DBG, "Parsed {} events", ev_count);
        // }
    }

    // Logger::print(LogType::DBG, "Parsed {} events", ev_count);
    // Logger::print(LogType::DBG, "Invalid {} events", invalid_ev_cnt);

    return events;
}

void TraceParser::print_summary(FILE* log_file) const{
    Logger::print(log_file, "num threads: {}", th_id_map.id_counter);
    Logger::print(log_file, "num events: {}", line_index);
    Logger::print(log_file, "num locations: {}", var_id_map.id_counter);
    Logger::print(log_file, "num locks: {}", lock_id_map.id_counter);

    // Logger::print(LogType::DBG, "unshared lock count: {}", shared_locks.get_unshared_count());
    // Logger::print(LogType::DBG, "unshared var count: {}", shared_vars.get_unshared_count());
}