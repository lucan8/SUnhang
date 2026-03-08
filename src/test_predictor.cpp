#include "../include/test_predictor.hpp"
#include "../include/predictor.hpp"
#include "../include/logger.hpp"
#include <cassert>

void TestPredictor::test(){
    Logger::print(LogType::INFO, "Running tests on predictor...");

    _test_read_event();
    _test_write_event();

    _test_acquire_event();
    _test_release_event();

    Logger::print(LogType::INFO, "All predictor tests passed!");
}

void TestPredictor::_test_read_event(){
    Predictor pred, dummy_pred;
    EventInfo ev(1, EventsT::RD, 1, 1, 1);

    // Test time always moves forward
    pred.handle_event(ev);
    assert (dummy_pred.thread_map[1].vec_clock <= pred.thread_map[1].vec_clock);

    Logger::print(LogType::INFO, "test_read_event passed");
}

void TestPredictor::_test_write_event(){
    Predictor pred, dummy_pred;
    EventInfo ev(1, EventsT::WR, 1, 1, 1);

    // Test time always moves forward
    pred.handle_event(ev);
    assert (dummy_pred.thread_map[1].vec_clock <= pred.thread_map[1].vec_clock);

    // Test that another write moves the timestamp
    dummy_pred = pred;
    ev.thread_id = 2;
    pred.handle_event(ev);

    assert (pred.last_write[1] <= dummy_pred.last_write[1]);

    Logger::print(LogType::INFO, "test_write_event passed");
}

void TestPredictor::_test_acquire_event(){
    Predictor pred, dummy_pred;
    EventInfo ev(1, EventsT::LK, 1, 1, 1);

    // Test time always moves forward
    pred.handle_event(ev);
    assert (dummy_pred.thread_map[1].vec_clock <= pred.thread_map[1].vec_clock);
    
    // Lockset grows
    assert (pred.thread_map[1].lockset.find(1) != pred.thread_map[1].lockset.end());
    
    // CSHist chnages
    auto cs_opt = pred.cs_hist.get_back(1, 1);
    assert (cs_opt.has_value()); // Added critical section
    const CSInfo& cs_1 = *cs_opt.value();
    assert (cs_1.unlock_ev.vc.empty()); // Without the unlock
    
    // Dependency gets created
    auto dep = pred.graph_view.graph.abs_deps_map.begin();
    assert (dep != pred.graph_view.graph.abs_deps_map.end());

    // Dependency is the one we expect
    assert (dep->first.thread_id == 1 && dep->first.resource_id == 1 && dep->first.lockset.empty());
    
    // First level lock acq are ignored by lock_dep_map
    assert (pred.lock_dep_map.empty() == true);

    // Remove lock from lockset and add it back to recreate the dependency
    ev.event_type = EventsT::UK;
    ev.line = 2;
    pred.handle_event(ev);

    // Check that the critical section was completed
    assert (!cs_1.unlock_ev.vc.empty());
    assert (cs_1.lock_ev <= cs_1.unlock_ev);

    ev.event_type = EventsT::LK;
    ev.line = 3;
    pred.handle_event(ev);

    // CSHist last element different from prior and prior's lock vc is smaller
    auto cs_2 = *pred.cs_hist.get_back(1, 1).value();
    assert (cs_1.less_than_tr(cs_2));

    // Check that no new dep was added by it's corresponding list of vcs grew
    assert (pred.graph_view.graph.abs_deps_map.size() == 1);
    assert (dep->second.size() == 2);

    // Check that lock_dep_map remained unchanged
    assert (pred.lock_dep_map.empty() == true);

    // Create new dependency
    ev.target = 2;
    ev.line = 4;
    pred.handle_event(ev);
    
    // Make sure the entry for the old lock(1) was created
    auto lock_it = pred.lock_dep_map.find(1);
    assert (lock_it != pred.lock_dep_map.end());

    // Make sure the corr dep has old lock in it's lockset
    auto& ls = lock_it->second.at(0)->first.lockset;
    assert (ls.find(lock_it->first) != ls.end());
    Logger::print(LogType::INFO, "test_acquire_event passed");
}

void TestPredictor::_test_release_event(){
    Predictor pred, dummy_pred;
    EventInfo ev(1, EventsT::LK, 1, 1, 1);

    // Firs add lock then remove it
    pred.handle_event(ev);
    ev.event_type = EventsT::UK;
    pred.handle_event(ev);

    // Time always moves forward
    assert (dummy_pred.thread_map[1].vec_clock <= pred.thread_map[1].vec_clock);
    
    // Lockset shrinks
    assert (pred.thread_map[1].lockset.find(1) == pred.thread_map[1].lockset.end());

    Logger::print(LogType::INFO, "test_release_event passed");
}