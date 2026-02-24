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
    
    // Dependency gets created
    auto dep = pred.abs_deps_map.begin();
    assert (dep != pred.abs_deps_map.end());

    // Dependency is the one we expect
    assert (dep->first.thread_id == 1 && dep->first.resource_id == 1 && dep->first.lockset.empty());

    // Remove lock from lockset and add it back to recreate the dependency
    ev.event_type = EventsT::UK;
    pred.handle_event(ev);

    ev.event_type = EventsT::LK;
    pred.handle_event(ev);

    // Check that no new dep was added by it's corresponding list of vcs grew
    assert (pred.abs_deps_map.size() == 1);
    assert (dep->second.size() == 2);

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