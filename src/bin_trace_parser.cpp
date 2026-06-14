#include "../include/bin_trace_parser.hpp"
#include "../include/meta_info.hpp"

SharedObjTracker::SharedObjTracker(size_t res_count) 
    : _shared_count(0), _shared_cand_map(res_count, INVALID){}

void  SharedObjTracker::reset(size_t res_count){
    _shared_count = 0;
    _shared_cand_map = std::vector<ResourceIdT>(res_count, INVALID);
}

void SharedObjTracker::update(ThreadIdT tid, ResourceIdT res_id){
    if (is_shared(res_id)){
        return;
    }

    if (_shared_cand_map[res_id] == INVALID){
        _shared_cand_map[res_id] = tid;
    }
    else if (_shared_cand_map[res_id] != tid){
        _shared_count += 1;
        set_shared(res_id);
    }
}

void SharedObjTracker::set_shared(ResourceIdT res_id){
    _shared_cand_map[res_id] = IS_SHARED;
}

void SharedObjTracker::ignore_unshared_objects(){
    // It's important that the counter starts from 1 and not 0
    // for locks as cond vars identify using -lock_id, which might result in them having the same id
    ResourceIdT id_cnt = 1;
    
    for (ResourceIdT id = 0; id < _shared_cand_map.size(); ++id){
        if (is_shared(id)){
            _shared_cand_map[id] = id_cnt;
            id_cnt++;
        }
        else{
            _shared_cand_map[id] = INVALID;
        }

        _shared_count = id_cnt;
    }
}

bool SharedObjTracker::is_shared(ResourceIdT res_id) const{
    return _shared_cand_map[res_id] == IS_SHARED;
}

std::optional<ResourceIdT> SharedObjTracker::get_new_id(ResourceIdT res_id) const{
    ResourceIdT new_res_id = _shared_cand_map[res_id];
    if (new_res_id == INVALID) return {};

    return new_res_id;
}

size_t SharedObjTracker::get_unshared_count() const{
    return _shared_cand_map.size() - _shared_count;
}

BinParser::BinParser(std::FILE* trace_file) : trace_file(trace_file){
    meta_info.load_header(trace_file);

    meta_info.header.THREAD_COUNT = TraceBinFormatter::mask_th_count(meta_info.header.THREAD_COUNT);
    meta_info.header.LOCK_COUNT = TraceBinFormatter::mask_lock_count(meta_info.header.LOCK_COUNT);
    meta_info.header.VAR_COUNT = TraceBinFormatter::mask_var_count(meta_info.header.VAR_COUNT);
    meta_info.header.EVENT_COUNT = TraceBinFormatter::mask_ev_count(meta_info.header.EVENT_COUNT);

    shared_locks.reset(meta_info.header.LOCK_COUNT);
    shared_vars.reset(meta_info.header.VAR_COUNT);
    meta_info.resize_notif_threads();
}


void BinParser::preprocess_trace() {
    // Account for metadata loading
    long start_file_pos = std::ftell(trace_file);

    std::array<BinEvT, EV_BLOCK_CNT> bin_ev_block;

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
            } else if (is_notif_type(event_type)) {
                meta_info.NOTIF_THREADS[thread_id] = 1;
                // Calling wait/notify on an object makes the object shared
                // This is conservative as this is not necessarily true, but rare to be false
                shared_locks.set_shared(get_ass_sync_obj(target));
            }
        }
    } while(read_ev_cnt == EV_BLOCK_CNT);

    ignore_unshared_objects();

    // Restore the file's position
    std::fseek(trace_file, start_file_pos, SEEK_SET);
}

void BinParser::parse_and_handle_trace(EventHandler& event_handler) {
    // There is no deadlock if we have less than 2 locks   
    if (meta_info.header.LOCK_COUNT <= 1){
        return;
    } 
    
    std::array<BinEvT, EV_BLOCK_CNT> bin_ev_block;
    size_t read_ev_cnt = 0;
    EventIdT curr_ev_id = -1;

    do {
        read_ev_cnt = std::fread(bin_ev_block.data(), sizeof(BinEvT), EV_BLOCK_CNT, trace_file);

        for (size_t i = 0; i < read_ev_cnt; ++i) {
            curr_ev_id++;
            BinEvT bin_ev = bin_ev_block[i];
            
            // Only extract what's necessary to check the conditions
            EventsT event_type = TraceBinFormatter::unsafe_extract_ev_type(bin_ev); 
            ResourceIdT target = TraceBinFormatter::extract_target(bin_ev);
            
            // Get the new id of the target
            std::optional<ResourceIdT> target_new_id_opt = target;

            if (is_lock_type(event_type)) {
                target_new_id_opt = shared_locks.get_new_id(target);
            } else if (is_access_type(event_type)) {
                target_new_id_opt = shared_vars.get_new_id(target);
            } else if (is_cv_type(event_type)){
                // Transform from cv id to associated lock id, map it to it's new id and get
                // the associated cv of that
                target_new_id_opt = get_ass_sync_obj(shared_locks.get_new_id(get_ass_sync_obj(target)).value());
            }

            // Invalid id? Unshared lock/variable, ignore
            if (!target_new_id_opt.has_value()){
                continue;
            }

            // Fetch the rest afterwards
            EventInfo ev_info(TraceBinFormatter::extract_tid(bin_ev),
                                event_type,
                                target_new_id_opt.value(),
                                TraceBinFormatter::extract_src_loc(bin_ev),
                                curr_ev_id
                                );

            event_handler.handle_event(ev_info);
        }
    } while(read_ev_cnt == EV_BLOCK_CNT);
}

void BinParser::ignore_unshared_objects() {
    shared_locks.ignore_unshared_objects();
    shared_vars.ignore_unshared_objects();

    meta_info.header.VAR_COUNT = shared_vars._shared_count;
    meta_info.header.LOCK_COUNT = shared_locks._shared_count;
}

void BinParser::print_summary(FILE* log_file) const {
    Logger::print(log_file, "num threads: {}", meta_info.header.THREAD_COUNT);
    Logger::print(log_file, "num events: {}", meta_info.header.EVENT_COUNT);
    Logger::print(log_file, "num locations: {}", meta_info.header.VAR_COUNT);
    Logger::print(log_file, "num locks: {}", meta_info.header.LOCK_COUNT);

    // Logger::print(LogType::DBG, "unshared lock count: {}", shared_locks.get_unshared_count());
    // Logger::print(LogType::DBG, "unshared var count: {}", shared_vars.get_unshared_count());
}