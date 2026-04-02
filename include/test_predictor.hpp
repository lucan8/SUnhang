#pragma once

struct TestPredictor{
    void static test();
  
    void static _test_read_event();
    void static _test_write_event();

    void static _test_acquire_event();
    void static _test_release_event();
};