#pragma once

#include "common_types.hpp"

struct meta{
    ThreadIdT THREAD_COUNT = 0;
    EventIdT EVENT_COUNT = 0;
    ResourceIdT VAR_COUNT = 0;
    ResourceIdT LOCK_COUNT = 0;
    uint8_t LOCK_DEPTH = 8;
    std::vector<uint8_t> NOTIF_THREADS;

    meta(ThreadIdT THREAD_COUNT, EventIdT EVENT_COUNT, ResourceIdT VAR_COUNT, ResourceIdT LOCK_COUNT)
        : THREAD_COUNT(THREAD_COUNT), EVENT_COUNT(EVENT_COUNT), VAR_COUNT(VAR_COUNT), LOCK_COUNT(LOCK_COUNT){}
    
    meta(){}

    // Resized NOTIF_THREADS to THREAD_COUNT
    void resize_notif_threads(){
        NOTIF_THREADS.resize(THREAD_COUNT);
    }

    // Returns the size of the partial object that will be loaded from the bin/meta file
    size_t load_sizeof() const{
        return sizeof(*this) - sizeof(this->LOCK_DEPTH) - sizeof(NOTIF_THREADS);
    }
};

inline meta meta_info;