// TODO: Let sorted vector unsafely return a non-const reference to the internal objects

// GRAPH OBSERVATIONS:

// CURRENTLY USING ORDER GIVEN BY NORMAL COMPARISON BETWEEN THE VALUES THE ITERATORS POINT TO
// THIS IS NOT A NECESSITY, WE COULD HAVE SOMETHING LIKE A MAP THAT MAPS THE ITERATORS TO NUMBERS
// THIS WOULD SAVE SOME TIME AS IT AVOIDS COMPARING THE NODES EVERYTIME WHICH CAN GET HEAVY 
// ESPECIALLY IF LOCKSETS ARE BIG
// ADDING SUCH A MAPPING WILL INCREASE THE MEMORY USAGE AND SLIGHTLY (HOPEFULLY) DECREASE RUNTIME
// THE COMPARISON IS NEEDED BECAUSE WE NEED TO KNOW WHAT NODES NOT TO CHECK AGAIN WHEN DOING CYCLE ENUMERATION
// JOHNSON'S ALGORITHM EFFECTIVELY "KILLS" NODES AT EACH ITERATION, DECIDING THAT THEY CAN NEVER
// CREATE A SCC IN THE FUTURE. THIS KILLING NEEDS THE CONCEPT OF ORDERING BETWEEN NODES

// TODO: WHY DO WE NEED SORTED STUFF FOR GRAPH BASED COMPUTATIONS?
// TODO: WHY DOES DERBY2 FAIL WHEN MOVING THE GRAPH VIEW IN THE CYCLE ENUMERATOR?

// LOGICAL BUG FOUND BY RUNNING src_code_loc_test1/src_code_loc_bug1_dlf

// OPTIMIZATION: Make a normal VectorClock class and a OwnedVectorClock class(fighting agains branch pred?)

// TODO: See why the big runtime for sor

// OPTIMIZATIONS
// TODO: IGNORE VARIABLES THAT ARE NEVER READ 
// Note: Using thread ids and resource ids as indexes in vectors can be quite fragile so be careful
// Note: No more thread clean-up is done which might not be desirable!

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

//TODO: ERR REPORT FILE FOR BAD TRACES
//TODO: Rename the comparison operators as they are actually biased toward the first argument
//TODO: How does this handle nested cycles?
//TODO: Add automatic formatting for your code
//TODO: Think about the sentinel pattern
//TODO: Template formater for vectors(YOU HAVE IT, USE IT)
//TODO: Pack the comparison operators of VectorClock together in one
//TODO: Timer function
//TODO: Resources would benefit from a enum
//TODO: Differentiating between the locksets that contains cond_vars and locks and those that only locks
// would be useful

// TODO: Bensalem asserts!
// Graph info for bensalem: 12 nodes, only 3 with outgoing neighbours, graph on the second to last page of your notebook


// BIG QUESTION: Shouldn't the nodes(deps) be sorted based on when they appear in the trace?
// As keeping them in a mere map does not guarantee that ordering.
// ANSWER: NOP, dependencies can't really use the trace order, that's for events

// OPTIMIZATION:
// Currently the dep map and CSHist keep events but in most of the situations they refer to the same
// thing, using shared_ptr would probably be the go here or a master vector of pointers
// RESULT OF MASTER VECTOR OF POINTERS: PERFORMANCE WAS NOT IMPROVED SIGNIFICANTLY

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

#include "../include/meta_info.hpp"
#include "../include/bin_trace_parser.hpp"
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
    fflush(log_file);

    end = std::chrono::steady_clock::now();
    auto millis_passed_handle_events = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Logger::print(LogType::INFO, "Finished event handling");
    end = std::chrono::steady_clock::now();
    auto millis_passed_build_neigh_list = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    OrdDepGraphView graph_view(std::move(event_handler.abs_deps), event_handler.lock_dep_map);
    CycleEnumerator cycle_enumerator(graph_view);
    cycle_enumerator.enum_cycles();
    
    Logger::print(log_file, "num cycles: {}", cycle_enumerator.res_cycles.size());
    fflush(log_file);

    end = std::chrono::steady_clock::now();
    auto millis_passed_cycle_enum = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    DeadlockChecker dlk_checker(std::move(event_handler.cs_hist), std::move(event_handler.dep_loc_ev_map));

    // Each cycle might have multiple abstract deadlock patterns
    std::vector<AbsDlkPattern> all_abs_dlk_patterns;
    size_t abs_dlk_pattern_count = 0;

    for (const auto& cycle : cycle_enumerator.res_cycles){
        std::vector<AbsDlkPattern> abs_dlk_patterns = dlk_checker.get_abs_dlk_patterns(cycle);
        if (abs_dlk_patterns.size() > 0){
            abs_dlk_pattern_count += abs_dlk_patterns.size();
            all_abs_dlk_patterns.insert(all_abs_dlk_patterns.end(), abs_dlk_patterns.begin(), abs_dlk_patterns.end());
        }
    }

    Logger::print(log_file, "num abstract: {}", abs_dlk_pattern_count);
    Logger::print(log_file, "num concrete: -1\n"); // Just to match the format
    fflush(log_file);
    
    end = std::chrono::steady_clock::now();
    auto millis_passed_abs_dlk_check = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    uint32_t real_dlk_count = 0;
    std::vector<int> real_dlk_ind;

    std::set<std::vector<SimpleNode>> verified_patterns;
    for (auto& abs_dlk_pattern : all_abs_dlk_patterns){
        if (verified_patterns.find(abs_dlk_pattern.nodes) != verified_patterns.end()){
            continue;
        }

        if (dlk_checker.is_sync_preserving_dlk(abs_dlk_pattern)){
            verified_patterns.insert(abs_dlk_pattern.nodes);
            real_dlk_count += 1;
            Logger::print(log_file, "Deadlock found on cycle: {}", abs_dlk_pattern.nodes);
        }
    }

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
