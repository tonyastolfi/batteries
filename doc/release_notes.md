- 0.19.1 (2022/12/16)
    - Added accurate translation of more boost::system::error_code values into batt::Status.

- 0.19.0 (2022/12/14)
    - Added `batt::Latch<T>::set_error` to explicitly set the Latch to an error status for better readability.
    - Added new header batteries/async/fetch.hpp with:
        - `batt::fetch_chunk`: blocking API for no-copy input streams with an async_fetch method
        - `batt::BasicScopedChunk<Stream>`: automatically calls consume on fetched data at scope exit

- 0.18.1 (2022/12/09)
    - Added `batt::PinnablePtr<T>`.

- 0.18.0 (2022/12/07)
    - Added `batt::Channel<T>::async_read`, along with unit tests for the async methods of `batt::Channel<T>`.

- 0.17.1 (2022/12/02)
    - Added `batt::Runtime::reset()` and `batt::Runtime::is_halted()` so that downstream libraries that use a Google Test global environment to start/stop batt::Runtime can restart the default thread pool, for GTEST_REPEAT/--gtest_repeat testing.

- 0.16.2 (2022/11/30)
    - Fixed a broken link in the release notes.

- 0.16.1 (2022/11/30)
    - Added release notes (this document) to the doc pages.

- 0.16.0 (2022/11/30)
    - Added clamp min/max functions to [batt::Watch][battwatch], to atomically enforce upper/lower bounds on the watched value.
