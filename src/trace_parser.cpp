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
std::optional<EventInfo> TraceParser::from_std(const std::string& tid, const std::string& ev_type, const std::string& target,
                                               const std::string& src_loc) {
    auto ev_type_it = std_event_map.find(ev_type);

    // Event not found
    if(ev_type_it == std_event_map.end()) {
        // Logger::print(LogType::WARN, "Event not found: {}", ev_type);
        return {};
    }

    EventInfo result = {};

    // Set event type and thread id
    result.event_type = ev_type_it->second;
    result.thread_id = th_id_map.get(tid);

    // Update maps depending on operation
    if(result.event_type == EventsT::FORK || result.event_type == EventsT::JOIN) {
        result.target = th_id_map.get(target[0] == 'T' ? target : "T" + target);
    }
    else if(result.event_type == EventsT::LK || result.event_type == EventsT::UK) {  
        result.target = lock_id_map.get(target);
    }
    else if(result.event_type == EventsT::WAIT || result.event_type == EventsT::NOTIFY || result.event_type == EventsT::NOTIFYALL) {  
        result.target = get_ass_sync_obj(lock_id_map.get(target));
    }
    else{
        result.target = var_id_map.get(target);
    }
    
    // Set src code location
    result.src_loc = std::stoi(src_loc);

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

    char* target_c = std::strtok(nullptr, ")");
    if (!target_c) return {};

    char* src_loc_c = std::strtok(nullptr, "\n");
    if (!src_loc_c) return {};

    // Ignore the trailing "|"
    src_loc_c += 1; 

    std::string tid(tid_c), ev_type(ev_type_c), target(target_c), src_loc(src_loc_c);

    // Convert the strings to our mapping
    std::optional<EventInfo> event = from_std(tid, ev_type, target, src_loc);
    if (event.has_value())
        event.value().line = line_index;

    return event;
}

void TraceParser::print_summary(FILE* log_file) const{
    Logger::print(log_file, "num threads: {}", th_id_map.id_counter);
    Logger::print(log_file, "num events: {}", line_index - 1);
    Logger::print(log_file, "num locations: {}", var_id_map.id_counter);
    Logger::print(log_file, "num locks: {}", lock_id_map.id_counter);
}