- 0.17.1 (2022/12/02)
    - Added `batt::Runtime::reset()` and `batt::Runtime::is_halted()` so that downstream libraries that use a Google Test global environment to start/stop batt::Runtime can restart the default thread pool, for GTEST_REPEAT/--gtest_repeat testing.

- 0.16.2 (2022/11/30)
    - Fixed a broken link in the release notes.

- 0.16.1 (2022/11/30)
    - Added release notes (this document) to the doc pages.

- 0.16.0 (2022/11/30)
    - Added clamp min/max functions to [batt::Watch][battwatch], to atomically enforce upper/lower bounds on the watched value.