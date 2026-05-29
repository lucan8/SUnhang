#include <cstring>

#include "../include/std_trace_parser.hpp"
#include "../include/meta_info.hpp"
#include "../include/lockset.hpp"

const std::unordered_map<std::string, EventsT> StdParser::std_event_map = {
    {"r", EventsT::RD}, {"w", EventsT::WR},
    {"fork", EventsT::FORK}, {"join", EventsT::JOIN},
    {"acq", EventsT::LK}, {"rel", EventsT::UK},
    {"wait", EventsT::WAIT}, {"notify", EventsT::NOTIFY}, 
    {"notifyAll", EventsT::NOTIFYALL}, {"broadcast", EventsT::NOTIFYALL}
};

const char StdParser::trace_sep[2] = "|";
const uint8_t StdParser::exp_trace_token_cnt = 4;

StdIdMap::StdIdMap() : id_counter(0){}

void StdIdMap::reset(){
    id_counter = 0;
    _map.clear();
}

// Returns the corresponding id for std_id from _map
// Updates the _map and counter if not found
int StdIdMap::get(const std::string& std_id){
    auto map_entry = _map.find(std_id);
    int result_id;

    if(map_entry == _map.end()) {
        _map[std_id] = id_counter;
        result_id = id_counter;
        id_counter += 1;
        } else {
        result_id = map_entry->second;
        }

        return result_id;
}

StdParser::StdParser(std::FILE* trace_file) : trace_file(trace_file), line_index(0){}

bool StdParser::events_remaining() const{
    return !feof(trace_file);
}

EventInfo StdParser::_from_std(const std::string& tid, EventsT ev_type, const std::string& target,
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
    }
    else if(is_cv_type(ev_type)) {  
        result.target = get_ass_sync_obj(lock_id_map.get(target));
    }
    else{
        result.target = var_id_map.get(target);
    }
    
    // Set src code location and the trace id
    result.src_loc = std::stoi(src_loc);
    result.line = line_index;

    return result;
}

std::optional<EventInfo> StdParser::get_next_event() {
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
    line_index++;
    EventInfo event = _from_std(tid, ev_type_it->second, target, src_loc);

    return event;
}

void StdParser::to_bin_fmt(FILE* out_bin_file){
    std::vector<BinEvT> events;

    // Parse the trace, create events and convert them to binary format
    while (events_remaining()){
        auto event_opt = get_next_event();

        if (event_opt.has_value()){
            events.push_back(TraceBinFormatter::ev_to_bin(event_opt.value()));
        }

    }

    // Print metadata
    print_metadata(out_bin_file);

    // Print binary events
    fwrite(events.data(), sizeof(events[0]), events.size(), out_bin_file);
}

void StdParser::print_metadata(FILE* out_bin_file) const{
    meta tmp_meta_info(TraceBinFormatter::mask_th_count(th_id_map._map.size()),
                        TraceBinFormatter::mask_ev_count(line_index),
                        TraceBinFormatter::mask_var_count(var_id_map._map.size()),
                        TraceBinFormatter::mask_lock_count(lock_id_map._map.size())
                        );
    fwrite(&tmp_meta_info, tmp_meta_info.load_sizeof(), 1 , out_bin_file);
}