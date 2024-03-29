- 0.23.1 (2023/02/06)
    - Fixed bug in script/run-with-docker.sh
    - Added xxd to docker images

- 0.22.1 (2023/01/27)
    - Added `batt::DefaultInitialized`, a type that implicitly converts to any default-constructible type.
    - Added a `step` parameter (type `batt::TransferStep &`, batteries/async/fetch.hpp) that allows callers to determine which step failed when `batt::transfer_chunked_data` returns an error.

- 0.21.0 (2023/01/25)
    - Added `batt::register_error_category` to allow applications to specify how `error_code` values with custom `error_category` objects should be converted to `batt::Status`.
    - Upgraded boost to 1.81.0
    - Upgraded gtest to 1.13.0

- 0.20.1 (2023/01/18)
    - Added `batt::transfer_chunked_data` (in batteries/async/fetch.hpp).

- 0.19.8 (2023/01/11)
    - Upgrade to Conan 1.56 (from 1.53).

- 0.19.6 (2022/12/19)
    - Fixed build error with `batt::to_status(std::error_code)`

- 0.19.5 (2022/12/16)
    - Fixed crash bug caused by accidental implicit conversion from error_code enum values to batt::Status.

- 0.19.2 (2022/12/16)
    - Fixed test regression by updating the expected error status to match the boost::system::error_code value.

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
