#pragma once

struct TestVectorClock{
    static void test();

    // Tests increment, set and decrement
    static void _test_setters();

    // Test <= and ==
    static void _test_comp();

    // Tests merge and merge_into
    static void _test_merge();
};
