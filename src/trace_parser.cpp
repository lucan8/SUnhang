#include <algorithm>

#include "../include/trace_parser.hpp"
#include "../include/logger.hpp"

const std::unordered_map<std::string, EventsT> TraceParser::std_event_map = {
    {"r", EventsT::RD}, {"w", EventsT::WR},
    {"fork", EventsT::FORK}, {"join", EventsT::JOIN},
    {"acq", EventsT::LK}, {"rel", EventsT::UK},
    {"wait", EventsT::WAIT}, {"notify", EventsT::NOTIFY}, 
    {"notifyAll", EventsT::NOTIFYALL}, {"broadcast", EventsT::NOTIFYALL}
};

const char TraceParser::trace_sep = '|';
const uint8_t TraceParser::exp_split_trace_size = 3;

bool TraceParser::events_remaining(){
    return !trace_file.eof();
}

// Map the std formated result to our custom result using the std_* maps
// std format ex: T1|acq(l1)|25 might turn into 1, 1, 1 uisng the std_* maps
std::optional<EventInfo> TraceParser::from_std(const std::vector<std::string>& current_result) {
    // Set event type
    auto event_len = current_result[1].find("(");
    std::string event_str = current_result[1].substr(0, event_len);
    auto event_type = std_event_map.find(event_str);

    // Event not found
    if(event_type == std_event_map.end()) {
        // Logger::print(LogType::WARN, "Event not found: {}", event_str);
        return {};
    }

    EventInfo result = {};

    // Set event type and thread id
    result.event_type = event_type->second;
    result.thread_id = th_id_map.get(current_result[0]);
    
    // The event target(the variable on which the event is happening)
    auto target = current_result[1].substr(event_len + 1, current_result[1].length() - event_len - 2);

    // Update maps depending on operation
    if(result.event_type == EventsT::FORK || result.event_type == EventsT::JOIN) {
        result.target = th_id_map.get(target.contains('T') ? target : "T" + target);
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
    result.src_loc = std::stoi(current_result[2]);

    return result;
}

std::optional<EventInfo> TraceParser::get_next_event() {
    std::string evt_str;

    std::getline(trace_file, evt_str);
    
    line_index++;

    std::vector<std::string> evt_str_split = split(evt_str, trace_sep);

    if(evt_str_split.size() != exp_split_trace_size) {
        Logger::print(LogType::WARN, "Bad trace file format on line {}: {}", line_index, evt_str);
        return {};
    }
    
    // Convert the split std event with our mapping
    std::optional<EventInfo> event = from_std(evt_str_split);
    if (event.has_value())
        event.value().line = line_index;

    return event;
}

void TraceParser::print_summary(FILE* log_file) const{
    Logger::print(log_file, "num threads: {}", th_id_map.id_counter);
    Logger::print(log_file, "num events: {}", line_index);
    Logger::print(log_file, "num locations: {}", var_id_map.id_counter);
    Logger::print(log_file, "num locks: {}", lock_id_map.id_counter);
}