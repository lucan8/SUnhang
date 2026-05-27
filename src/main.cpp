//TODO: WHEN PRINTING THE NUMBER OF EVENTS TO THE META FILE IGNORE THE INVALID EVENTS
//TODO: LOAD THE IGNORED EVENTS DIRECTLY FROM A FILE 
// you even have simple serialization/deserialization for dynamic_bitset
// TODO: DEFINE SOME KIND OF EVENT NAMESPACE
// TODO: LOOK INTO ENDIANESS
// TODO: See if you can use SortedVector anywhere else
// OPTIMIZATION: Make a normal VectorClock class and a OwnedVectorClock class(fighting agains branch pred?)

// MISUNDERSTANDINGS:

// DEAD THREADS DON'T ACTUALLY CAUSE ISSUES FOR SPDOFFLINE
// ABSTRACT DEPENDENCIES MIGHT INCLUDE SOURCE CODE LOCATIONS, THAT'S WHY THEY GROW
// THE ABSTRACT DEP MAP IS ACTUALLY A MAP THAT HAS THE FIRST KEY THE ABS DEP
// AND THE SECOND KEY THE LOCATION
// THEN THAT POINTS TO A LIST OF EVENT IDS

// OBSERVATIONS FOR ORIGINAL
// STRINGBUFFER PRINTS THE SAME DEADLOCK TWICE FOR SPD
// DBCP1 PRINTS THE SAME DEADLOCK TWICE FOR SPD
// MYSQL4: SAME
// HASHMAP: SAME
// WEAKHASHMAP: SAME
// LINKEDHASHMAP: SAME
// TREEMAP: SAME

// JIGSAW PRINTS THE SAME DEADLOCK MULTIPLE TIMES
// WORSE: JIGSAW DOESN'T PRINT ANYTHING
// WEIRD, WHY: sharedLocks = new boolean[numEvents]; ?

// OPTIMIZATIONS
// TODO: IGNORE VARIABLES THAT ARE NEVER READ 
// Note: Using thread ids and resource ids as indexes in vectors can be quite fragile so be careful
// Note: No more thread clean-up is done which might not be desirable!
// Change LocksetT to be a sorted vector

// COMPARE:
// NOTIFY AND NOTIFYALL MERGE THEIR TS INTO ALL SLEEPING THREADS (AS ANYONE COULD WAKE UP)
// THE QUEUE APROACH

// TODO JACONTEBE:
// TEST WITH TIMEOUT 60 OR MORE, ADD MEMORY ACCESSES BACK, CHECK PROGRAM EXITS AS WELL
// WEIRD LUCENE BEHAVIOUR
//1. IF WE RUN WITHOUT MEMORY ACCESSES WE FIND CYCLES, PATTERNS AND EVEN DEADLOCK
//2. OTHERWISE NOTHING IS FOUND! THAT'S WRONG

//COND VAR OBSERVATIONS:

// AUTHOR IMPLEMENTATION QUESTIONS:

//1.
// RECENT STATUS MIGHT CONTAIN A COND VAR AND THEN THE SAME THREAD MIGHT CALL SIGNAL ON IT
// THAT MIGHT CREATE THE DEPENDENCY COND_VAR -> COND_VAR WHICH IS PLAIN STUPID

//2. 
// THE DEPENDENCY ASSOC_LOCK -> COND_VAR ALSO EXISTS AND DOESN'T MAKE MUCH SENSE

// WAIT EXTRA BEHAVIOUR(OF RELEASING THE LOCK BEFORE SLEEP AND ACQUIRING IT UPON WAKING)
//      ARE HANDLED in convert_2_std from their artifact
// COND_VAR_ID = -LOCK_ID (the conversion is done here, not in the trace)
// wait/join if it blocks should update their timestamps
// to the current state of the system upon waking up
// if t1 waits and let's say another 2 threads keep executing stuff, upon waking t1 should
// be aware that the other 2 threads executed before it to avoid creating fake deadlocks

//IMPORTANT: Generic formatter for iterators
//TODO: Cleanup the parsers
//TODO: Events don't need the src_loc, only lock events do
//TODO: Should we actually print cycles that only differ by source code location?
//TODO: Reentrant locks for cshist
//TODO: Look into implementing the binary trace
//TODO: Remove notifyAll(leave only broadcast)
//TODO: Add the hand-made tests for multi-notif situations
//TODO: Change LRU to be normal instead of circular
//TODO: COMPARE THE RUNTIME OF THE SECOND RELEASE WHEN USING VECTOR CLOCKS TO THE UNORDERED_MAP VERSION
//TODO: ERR REPORT FILE FOR BAD TRACES
//TODO: Rename the comparison operators as they are actually biased toward the first argument
//TODO: How does this handle nested cycles?
//TODO: Add automatic formatting for your code
//TODO: Create namespace for util
//TODO: Think where to put your typedefs
//TODO: Rethink the graph situation
//TODO: Renames dependencies to nodes
//TODO: Think about the sentinel pattern
//TODO: Remove all asserts in release
//TODO: Use ranges instead of start and end iterators
//TODO: Template formater for vectors(YOU HAVE IT, USE IT)
//TODO: Pack the comparison operators of VectorClock together in one
//TODO: Timer function
//TODO: Circular array range based for loop
//TODO: Resources would benefit from a enum
//TODO: Differentiating between the locksets that contains cond_vars and locks and those that only locks
// would be useful

// TODO: Bensalem asserts!
// Graph info for bensalem: 12 nodes, only 3 with outgoing neighbours, graph on the second to last page of your notebook

// ENCHANCEMENT:
// Don't stop at the first deadlock instance you find

// BIG QUESTION: Shouldn't the nodes(deps) be sorted based on when they appear in the trace?
// As keeping them in a mere map does not guarantee that ordering.
// ANSWER: NOP, dependencies can't really use the trace order, that's for events

// OPTIMIZATION:
// Currently the dep map and CSHist keep events but in most of the situations they refer to the same
// thing, using shared_ptr would probably be the go here or a master vector of pointers

//OPTIMIZATION:
// Instead of recomputing the SCCs everytime on the subgraph, take only the SCC from which the node was removed
// And run the algorithm only on that subgraph

// OPTIMIZATION:
// We could prune paths that can't be abstract deadlock patterns when we do cycle enumeration
// For example if we have the chain (t1, l2, {l1}) -> (t2, l3, {l2}) -> (t1, l1, {l3})
// We could stop looking at the path instantly as we are sure nothing will come out of it

#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include <array>
#include <unordered_map>
#include <set>
#include <optional>
#include <future>
#include <cassert>
#include <filesystem>

using namespace std::chrono_literals;
namespace fs = std::filesystem;

#include "../include/trace_parser.hpp"
#include "../include/event_handler.hpp"
#include "../include/logger.hpp"
#include "../include/util.hpp"
#include "../include/test_vectorclock.hpp"
#include "../include/test_predictor.hpp"
#include "../include/scc_enumerator.hpp"
#include "../include/cycle_enumerator.hpp"
#include "../include/deadlock_checker.hpp"

int main(int argc, char *argv[]) {
    const uint8_t exp_args = 3;
    if (argc != exp_args){
        Logger::print(LogType::ERR, "Usage: ./SUnhang.exe [trace_path] [out_summary_path]");
        return 1; 
    }

    std::string trace_file_path = argv[1];
    std::string out_summ_path = argv[2];

    // Logger::print(LogType::DBG, "Input path: {}", trace_file_path);
    // Logger::print(LogType::DBG, "Out summary path: {}", out_summ_path);
    // Logger::print(LogType::DBG, "Trace meta path: {}", trace_meta_file_path);

    std::FILE* trace_file(std::fopen(trace_file_path.c_str(), "rb"));
    if(!trace_file) {
        Logger::print(LogType::ERR, "In file not found: {}", trace_file_path);
        return 1;
    }

    std::FILE* log_file(std::fopen(out_summ_path.c_str(), "w"));
    if (!log_file){
        Logger::print(LogType::ERR, "Log file not found: {}", out_summ_path);
        return 1;
    }

    // Test stuff
    // TestVectorClock::test();
    // TestPredictor::test();

    // THIS NEEDS TO BE FIRST
    // meta::init(trace_meta_file);
    auto start = std::chrono::steady_clock::now();
    
    // Preprocess trace file(fill metadata, determine events to be ignored etc...)
    BinParser trace_parser(trace_file);
    trace_parser.preprocess_trace();

    Logger::print(log_file, "----Trace info----");
    trace_parser.print_summary(log_file);

    auto end = std::chrono::steady_clock::now();
    auto millis_passed_parse_trace = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Read and handle events
    EventHandler event_handler(meta_info.THREAD_COUNT, meta_info.VAR_COUNT);
    trace_parser.parse_and_handle_trace(event_handler);

    // Print summary
    event_handler.print_summary(log_file);

    end = std::chrono::steady_clock::now();
    auto millis_passed_handle_events = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Logger::print(LogType::DBG, "skipped_ev_cnt: {}", skipped_ev_cnt);

    // Logger::print(LogType::INFO, "Finished event handling");
    event_handler.build_neigh_list();
    end = std::chrono::steady_clock::now();
    auto millis_passed_build_neigh_list = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    CycleEnumerator cycle_enumerator(event_handler.graph_view);
    cycle_enumerator.enum_cycles();
    
    Logger::print(log_file, "num cycles: {}", cycle_enumerator.res_cycles.size());

    end = std::chrono::steady_clock::now();
    auto millis_passed_cycle_enum = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    DeadlockChecker dlk_checker(event_handler.cs_hist);

    std::vector<int> abs_dlk_cycles_ind;
    size_t conc_count = 0;
    abs_dlk_cycles_ind.reserve(32);
    for (int i = 0; i < cycle_enumerator.res_cycles.size(); ++i){
        bool is_abs_dlk = dlk_checker.is_abs_dlk_pattern(cycle_enumerator.res_cycles[i]);
        if (is_abs_dlk){
            abs_dlk_cycles_ind.push_back(i);
        }
    }

    Logger::print(log_file, "num abstract: {}", abs_dlk_cycles_ind.size());
    Logger::print(log_file, "num concrete: -1\n"); // Just to match the format
    // fflush(log_file);
    
    end = std::chrono::steady_clock::now();
    auto millis_passed_abs_dlk_check = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    uint32_t real_dlk_count = 0;
    std::vector<int> real_dlk_ind;

    std::vector<std::pair<std::tuple<int, int, int>, int>> vals;
    for (int i : abs_dlk_cycles_ind){
        auto& cycle = cycle_enumerator.res_cycles[i];
        
        for (auto& node : cycle){
            for (auto& ev : node->second){
                std::tuple<int, int, int> tup(node->first.thread_id, node->first.resource_id, ev.src_loc);
                bool found = false;
                for (auto& [t, c] : vals){
                    if (t == tup){
                        found = true;
                        c += 1;
                        break;
                    }
                }
                if (!found){
                    vals.push_back({tup, 0});
                }
            }
        }

        auto dlk_info_opt = dlk_checker.get_sync_preserving_dlk(cycle_enumerator.res_cycles[i]);
        if (dlk_info_opt.has_value()){
            real_dlk_count += 1;
            auto dlk_info = dlk_info_opt.value();
            Logger::print(log_file, "Deadlock found on cycle: {}", dlk_info);
            // Logger::print(LogType::DBG, "{}", cycle_enumerator.res_cycles[i]);
            real_dlk_ind.push_back(i);
        }
    }

    // size_t prod = 1;
    // for (auto v : count){
    //     for (auto c : v){
    //         prod *= c; 
    //     }
    // }


    end = std::chrono::steady_clock::now();
    auto millis_passed_sync_pres_check = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    Logger::print(log_file, "\nnum deadlocks: {}", real_dlk_count);

    Logger::print(log_file, "Time for parsing = {} milliseconds", millis_passed_parse_trace);
    Logger::print(log_file, "Time for event handling = {} milliseconds", millis_passed_handle_events - millis_passed_parse_trace);
    Logger::print(log_file, "Time for neight list building = {} milliseconds", millis_passed_build_neigh_list - millis_passed_handle_events);
    Logger::print(log_file, "Time for cycle enumeration = {} milliseconds", millis_passed_cycle_enum - millis_passed_build_neigh_list);
    Logger::print(log_file, "Time for abs deadlock checks = {} milliseconds", millis_passed_abs_dlk_check - millis_passed_cycle_enum);
    Logger::print(log_file, "Time for sync pres check = {} milliseconds", millis_passed_sync_pres_check - millis_passed_abs_dlk_check);
    Logger::print(log_file, "Total Time = {} milliseconds", millis_passed_sync_pres_check);
    
    // Cleanup
    std::fclose(log_file);
    std::fclose(trace_file);

    return 0;
}
