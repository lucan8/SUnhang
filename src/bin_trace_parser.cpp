#include "../include/bin_trace_parser.hpp"
#include "../include/meta_info.hpp"

SharedObjTracker::SharedObjTracker(size_t obj_count) 
    : _is_shared(obj_count), _shared_count(0), _shared_cand_map(obj_count, INVALID_TID){}

void  SharedObjTracker::reset(size_t obj_count){
    _is_shared = std::vector<uint8_t>(obj_count, 0);
    _shared_count = 0;
    _shared_cand_map = std::vector<ThreadIdT>(obj_count, INVALID_TID);
}

void SharedObjTracker::update(ThreadIdT tid, ResourceIdT obj_id){
    if (_is_shared[obj_id]){
        return;
    }

    if (_shared_cand_map[obj_id] == INVALID_TID){
        _shared_cand_map[obj_id] = tid;
    }
    else if (_shared_cand_map[obj_id] != tid){
        _shared_count += 1;
        _is_shared[obj_id] = 1;
    }
}

bool SharedObjTracker::is_shared(ResourceIdT res_id) const{
    return _is_shared[res_id];
}

size_t SharedObjTracker::get_unshared_count() const{
    return _is_shared.size() - _shared_count;
}

BinParser::BinParser(std::FILE* trace_file) : trace_file(trace_file){
    std::fread(&meta_info, meta_info.load_sizeof(), 1, trace_file);

    meta_info.THREAD_COUNT = TraceBinFormatter::mask_th_count(meta_info.THREAD_COUNT);
    meta_info.LOCK_COUNT = TraceBinFormatter::mask_lock_count(meta_info.LOCK_COUNT);
    meta_info.VAR_COUNT = TraceBinFormatter::mask_var_count(meta_info.VAR_COUNT);
    meta_info.EVENT_COUNT = TraceBinFormatter::mask_ev_count(meta_info.EVENT_COUNT);

    shared_locks.reset(meta_info.LOCK_COUNT);
    shared_vars.reset(meta_info.VAR_COUNT);
    meta_info.resize_notif_threads();
    ignored_events.resize(meta_info.EVENT_COUNT, 0);
}


// Helper function for parse_full_trace. Should not be used anywhere else
void BinParser::_update_last_write(std::vector<uint8_t>& ignored_events, EventIdT event_idx, EventsT event_type,
                                ResourceIdT target, std::vector<EventIdT>& last_write) const{
    if (event_type == EventsT::WR){
        // Ignore the write before this and update
        int last_write_ev_id = last_write[target];
        if (last_write_ev_id != -1){
            ignored_events[last_write_ev_id] = true;
        }
        last_write[target] = event_idx;
    }
    // Acknowledge the last write(make it so it is not ignored)
    else if (event_type == EventsT::RD){
        last_write[target] = -1;
    }
}

void BinParser::preprocess_trace() {
    // Account for metadata loading
    long start_file_pos = std::ftell(trace_file);

    std::array<BinEvT, EV_BLOCK_CNT> bin_ev_block;
    std::vector<EventIdT> last_write(meta_info.VAR_COUNT, -1);

    size_t read_ev_cnt = 0;
    EventIdT curr_ev_id = -1;

    do {
        read_ev_cnt = std::fread(bin_ev_block.data(), sizeof(BinEvT), EV_BLOCK_CNT, trace_file);

        for (size_t i = 0; i < read_ev_cnt; ++i) {
            curr_ev_id++;
            BinEvT bin_ev = bin_ev_block[i];

            // Not very safe, but surely faster. The trace should be correct
            EventsT event_type = TraceBinFormatter::unsafe_extract_ev_type(bin_ev);
            ThreadIdT thread_id = TraceBinFormatter::extract_tid(bin_ev);
            ResourceIdT target = TraceBinFormatter::extract_target(bin_ev);

            if (is_lock_type(event_type)) {
                shared_locks.update(thread_id, target);
            } else if (is_access_type(event_type)) {
                shared_vars.update(thread_id, target);
                _update_last_write(ignored_events, curr_ev_id, event_type, target, last_write);
            } else if (is_notif_type(event_type)) {
                meta_info.NOTIF_THREADS[thread_id] = 1;
            }
        }
    } while(read_ev_cnt == EV_BLOCK_CNT);

    // Restore the file's position
    std::fseek(trace_file, start_file_pos, SEEK_SET);
}


void BinParser::parse_and_handle_trace(EventHandler& event_handler) {       
    std::array<BinEvT, EV_BLOCK_CNT> bin_ev_block;
    size_t read_ev_cnt = 0;
    EventIdT curr_ev_id = -1;

    do {
        read_ev_cnt = std::fread(bin_ev_block.data(), sizeof(BinEvT), EV_BLOCK_CNT, trace_file);

        for (size_t i = 0; i < read_ev_cnt; ++i) {
            curr_ev_id++;

            // Don't process events that should be ignored
            if (ignored_events[curr_ev_id]) {
                continue;
            }

            BinEvT bin_ev = bin_ev_block[i];
            
            // Only extract what's necessary to check the conditions
            EventsT event_type = TraceBinFormatter::unsafe_extract_ev_type(bin_ev); 
            ResourceIdT target = TraceBinFormatter::extract_target(bin_ev);

            if (is_lock_type(event_type) && !shared_locks.is_shared(target)) {
                continue;
            }
            if (is_access_type(event_type) && !shared_vars.is_shared(target)) {
                continue;
            }

            // Fetch the rest afterwards
            EventInfo ev_info(TraceBinFormatter::extract_tid(bin_ev),
                                event_type,
                                target,
                                TraceBinFormatter::extract_src_loc(bin_ev),
                                curr_ev_id
                                );

            event_handler.handle_event(ev_info);
        }
    } while(read_ev_cnt == EV_BLOCK_CNT);
}

void BinParser::print_summary(FILE* log_file) const {
    Logger::print(log_file, "num threads: {}", meta_info.THREAD_COUNT);
    Logger::print(log_file, "num events: {}", meta_info.EVENT_COUNT);
    Logger::print(log_file, "num locations: {}", meta_info.VAR_COUNT);
    Logger::print(log_file, "num locks: {}", meta_info.LOCK_COUNT);

    // Logger::print(LogType::DBG, "unshared lock count: {}", shared_locks.get_unshared_count());
    // Logger::print(LogType::DBG, "unshared var count: {}", shared_vars.get_unshared_count());
}