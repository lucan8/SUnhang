//FAILED TESTS:
// DBCP 1: expected cycles 2 found 0, none can be deduced from the graph though
// DBCP 2: same problems, though 200 deadlocks looks sus
// ECLIPSE : cycle enumerator find extra cycle
// HASHMAP????? : impossible, they are wrong!
// IDENTITYHASHMAP????? : impossible, they are wrong!

// Small mistake on their side, fork/join actually create fake thread entries in the maps
// For example fork(18) will create an entry 18 : id, and T18 will create another one which is wrong!

//IMPORTANT: Generic formatter for iterators
//TODO: Rename the comparison operators as they are actually biased toward the first argument
//TODO: How does this handle nested cycles?
//TODO: Add automatic formatting for your code
//TODO: Functions for stats and tests for graph construction (dependencies, locks, variables, events)
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

// BIG QUESTION: Shouldn't the nodes(deps) be sorted based on when they appear in the trace?
// As keeping them in a mere map does not guarantee that ordering.
// ANSWER: NOP, dependencies can't really use the trace order, that's for events

// TEST IDEEA: Run this on all the benchmarks, save information to a csv file and then check the csvs
// against the author's
 
//SIMPLE OPTIMIZATION: IGNORE FIRST LEVEL LOCK ACQUISITIONS!

//OPTIMIZATION:
// Instead of recomputing the SCCs everytime on the subgraph, take only the SCC from which the node was removed
// And run the algorithm only on that subgraph

// OPTIMIZATION:
// We could prune paths that can't be abstract deadlock patterns when we do cycle enumeration
// For example if we have the chain (t1, l2, {l1}) -> (t2, l3, {l2}) -> (t1, l1, {l3})
// We could stop looking at the path instantly as we are sure nothing will come out of it

// OPTIMIZATION:
// We could use vectors instead of maps for threads and resources as their ids are consecutive integers
// TODO: Bensalem asserts!
// Graph info for bensalem: 12 nodes, only 3 with outgoing neighbours, graph on the second to last page of your notebook

// ENCHANCEMENT:
// Don't stop at the first deadlock instance you find

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

#include "../include/predictor.hpp"
#include "../include/logger.hpp"
#include "../include/util.hpp"
#include "../include/test_vectorclock.hpp"
#include "../include/test_predictor.hpp"
#include "../include/scc_enumerator.hpp"
#include "../include/cycle_enumerator.hpp"
#include "../include/deadlock_checker.hpp"

// Maps for converting from std format
size_t std_lock_id_counter = 0;
std::unordered_map<std::string, int> std_lock_id_map = {};
size_t std_thread_counter = 0;
std::unordered_map<std::string, int> std_thread_map = {};
size_t std_var_counter = 0;
std::unordered_map<std::string, int> std_var_map = {};

size_t std_evt_counter = 0;

void reset_cnt_map() {
 std_lock_id_counter = 0;
 std_thread_counter = 0;
 std_var_counter = 0;
 std_evt_counter = 0;

 std_lock_id_map.clear();
 std_thread_map.clear();
 std_var_map.clear();
}

// Returns the corresponding id for std_id from std_id_map
// Updates the map and counter if not found
int get_id(std::unordered_map<std::string, int>& std_id_map, size_t& id_counter, const std::string& std_id){
  auto map_entry = std_id_map.find(std_id);
  int result_id;

  if(map_entry == std_id_map.end()) {
      std_id_map[std_id] = id_counter;
      result_id = id_counter;
      id_counter += 1;
    } else {
      result_id = map_entry->second;
    }

    return result_id;
}

// Map the std formated result to our custom result using the std_* maps
// std format ex: T1|acq(l1)|25 might turn into 1, 1, 1 uisng the std_* maps
EventInfo from_std(const std::vector<std::string>& current_result) {
    EventInfo result = {};

    // Set thread id
    result.thread_id = get_id(std_thread_map, std_thread_counter, current_result.at(0));

    // Set event type
    auto event_len = current_result.at(1).find("(");
    auto event_type = std_event_map.find(current_result.at(1).substr(0, event_len));

    // Event found
    if(event_type != std_event_map.end()) {
        result.event_type = event_type->second;
        // The event target(the variable on which the event is happening)
        auto target = current_result.at(1).substr(event_len + 1, current_result.at(1).length() - event_len - 2);

        // Update maps depending on operation
        if(result.event_type == EventsT::FORK || result.event_type == EventsT::JOIN) {
          result.target = get_id(std_thread_map, std_thread_counter, "T" + target);
        } 
        else if(result.event_type == EventsT::LK || result.event_type == EventsT::UK) {  
          result.target = get_id(std_lock_id_map, std_lock_id_counter, target);
        }      
	    else{
          result.target = get_id(std_var_map, std_var_counter, target);
	    }
      
      // Set src code location
      result.src_loc = std::stoi(current_result.at(2));
    } else { // Event not found
      result.event_type = EventsT::NONE;
    }

    std_evt_counter += 1;

    return result;
}

void parse_trace(Predictor& predictor, std::ifstream& file) {
    std::string evt_str;
    int line_index = 0;

    while(std::getline(file, evt_str)) {
        line_index++;
        std::vector<std::string> evt_str_split = split(evt_str, trace_sep);

        if(evt_str_split.size() != exp_split_trace_size) {
            Logger::print(LogType::WARN, "Bad file format on line {}: {}", line_index, evt_str);
            continue;
        }
        
        // Convert the split std event with our mapping
        EventInfo event = from_std(evt_str_split);
        event.line = line_index;
        
        bool is_val_evt = predictor.handle_event(event);
        // if (!is_val_evt)
        //     Logger::print(LogType::WARN, "Invalid event on line {}: {}", line_index, evt_str);

        // Logger::print(LogType::DBG, "{}", event);
    }
    predictor.build_neigh_list();
}

void print_parse_summary(std::FILE* log_file, const Predictor& pred){
    Logger::print(log_file, "----Trace info----");
    Logger::print(log_file, "num threads: {}", std_thread_counter);
    Logger::print(log_file, "num events: {}", std_evt_counter);
    Logger::print(log_file, "num locations: {}", std_var_counter);
    Logger::print(log_file, "num locks: {}", std_lock_id_counter);
    Logger::print(log_file, "num acq/req: {}", pred.acq_count);
    Logger::print(log_file, "num deps: {}", pred.graph_view.graph.abs_deps_map.size());
}


// Arg1 : trace file
// Arg2 : name of test
// Arg1 + Arg2 is used to build the name of the output files

int main(int argc, char *argv[]) {
    const uint8_t exp_args = 3;
    if (argc != exp_args){
        Logger::print(LogType::ERR, "Usage: ./SUnhang.exe [input_file_path] [out_summary_file_path]");
        return 1; 
    }

    auto start = std::chrono::system_clock::now();

    std::string in_file_path = argv[1];
    std::string out_summ_path = argv[2];
    // std::string extra_log_path = argv[3];

    Logger::print(LogType::DBG, "Input path: {}", in_file_path);
    Logger::print(LogType::DBG, "Out summary path: {}", out_summ_path);
    // Logger::print(LogType::DBG, "Extra log path: {}", extra_log_path);

    std::ifstream in_file(in_file_path);
    if(!in_file.good()) {
        Logger::print(LogType::ERR, "In file not found: {}", in_file_path);
        return 1;
    }

    std::FILE* log_file(std::fopen(out_summ_path.c_str(), "w"));
    if (!log_file){
        Logger::print(LogType::ERR, "Log file not found: {}", out_summ_path);
        return 1;
    }

    // std::FILE* extra_log_file(std::fopen(extra_log_path.c_str(), "w"));
    // if (!extra_log_file){
    //     Logger::print(LogType::ERR, "Extra log file not found: {}", out_summ_path);
    //     return 1;
    // }

    // // Test stuff
    // TestVectorClock::test();
    // TestPredictor::test();

    reset_cnt_map();
    Predictor predictor;

    // Trace parsing and grpah construction
    parse_trace(predictor, in_file);
    auto help = std_thread_map;
    print_parse_summary(log_file, predictor);

    // predictor.print_abs_deps();
    // Logger::print_dash_line();
    // predictor.print_neigh_list();

    auto end = std::chrono::system_clock::now();
    auto millis_passed_parse_trace = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    CycleEnumerator cycle_enumerator(predictor.graph_view);
    cycle_enumerator.enum_cycles();
    Logger::print(log_file, "num cycles: {}", cycle_enumerator.res_cycles.size());

    end = std::chrono::system_clock::now();
    auto millis_passed_cycle_enum = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    DeadlockChecker dlk_checker(predictor.cs_hist);

    std::vector<int> abs_dlk_cycles_ind;
    abs_dlk_cycles_ind.reserve(32);
    for (int i = 0; i < cycle_enumerator.res_cycles.size(); ++i){
        bool is_abs_dlk = dlk_checker.is_abs_dlk_pattern(cycle_enumerator.res_cycles[i]);
        if (is_abs_dlk)
            abs_dlk_cycles_ind.push_back(i);
    }

    Logger::print(log_file, "num abstract: {}", abs_dlk_cycles_ind.size());
    Logger::print(log_file, "num concrete: -1\n"); // Just to match the format
    
    end = std::chrono::system_clock::now();
    auto millis_passed_abs_dlk_check = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    uint32_t real_dlk_count = 0;
    for (int i : abs_dlk_cycles_ind){
        auto dlk_info_opt = dlk_checker.get_sync_preserving_dlk(cycle_enumerator.res_cycles[i]);
        if (dlk_info_opt.has_value()){
            real_dlk_count += 1;
            auto dlk_info = dlk_info_opt.value();
            Logger::print(log_file, "Deadlock found on cycle: {}", dlk_info);
        }
    }

    end = std::chrono::system_clock::now();
    auto millis_passed_sync_pres_check = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    Logger::print(log_file, "\nnum deadlocks: {}", real_dlk_count);

    Logger::print(log_file, "Time for parsing and graph construction = {} milliseconds", millis_passed_parse_trace);
    Logger::print(log_file, "Time for cycle enumeration = {} milliseconds", millis_passed_cycle_enum - millis_passed_parse_trace);
    Logger::print(log_file, "Time for abs deadlock checks = {} milliseconds", millis_passed_abs_dlk_check - millis_passed_cycle_enum);
    Logger::print(log_file, "Time for sync pres check = {} milliseconds", millis_passed_sync_pres_check - millis_passed_abs_dlk_check);
    return 0;
}
