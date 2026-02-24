#include "../include/test_vectorclock.hpp"
#include "../include/vectorclock.hpp"
#include "../include/logger.hpp"
#include <cassert>

void TestVectorClock::test(){
    Logger::print(LogType::INFO, "Running tests on vector clocks...");
    _test_setters();

    _test_comp();

    _test_merge();

    Logger::print(LogType::INFO, "All vector clock tests passed!");
}

// Tests increment, set and decrement
void TestVectorClock::_test_setters(){
    VectorClock vc1;

    vc1.increment(1);
    assert(vc1._vector_clock[1] == 1);
    
    vc1.set(6, 3);
    assert(vc1._vector_clock[6] == 3);

    vc1.decrement(1);
    vc1.decrement(1);
    assert(vc1._vector_clock[1] == 0);

    Logger::print(LogType::INFO, "_test_setters passed");
}

// Test <= and ==
void TestVectorClock::_test_comp(){
    VectorClock vc1, vc2;
    vc1.set(3, 5);
    vc1.set(6, 3);

    vc2.set(3, 5);
    vc2.set(6, 3);
    vc2.set(5, 1);

    assert ((vc1 <= vc2) == true);
    assert ((vc2 <= vc1) == false);

    vc2.decrement(5);
    assert ((vc2 <= vc1) == true);
    assert ((vc2 == vc1) == true);

    Logger::print(LogType::INFO, "_test_comp passed");
}

// Tests merge and merge_into
void TestVectorClock::_test_merge(){
    VectorClock vc1, vc2, gt;

    // Set-up vcs
    vc1.set(1, 1);
    vc1.set(3, 5);
    vc2.set(2, 2);
    vc2.set(3, 6);
    
    // Set-up merged vcs
    VectorClock merge_res = vc1.merge(vc2);
    vc1.merge_into(vc2);

    // Merge operations are the same
    assert(merge_res == vc1);

    // Set-up gt
    gt.set(1, 1);
    gt.set(2, 2);
    gt.set(3, 6);

    // Make sure merge results match gt
    assert (merge_res == gt);

    Logger::print(LogType::INFO, "_test_merge passed");
}