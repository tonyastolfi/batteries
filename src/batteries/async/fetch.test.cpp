//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/async/fetch.hpp>
//
#include <batteries/async/fetch.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/buffer.hpp>
#include <batteries/small_vec.hpp>

#include <boost/asio/io_context.hpp>

#include <functional>

namespace {

using namespace batt::int_types;

const std::string kHelloStr{"hello"};
const std::string kWorldStr{", world"};

const boost::asio::const_buffer kHelloChunk{kHelloStr.data(), kHelloStr.size()};
const boost::asio::const_buffer kWorldChunk{kWorldStr.data(), kWorldStr.size()};

class MockAsyncFetchStream
{
   public:
    using Self = MockAsyncFetchStream;

    using const_buffers_type = batt::SmallVec<batt::ConstBuffer, 2>;

    using Handler = std::function<void(boost::system::error_code const&, Self::const_buffers_type const&)>;

    MOCK_METHOD(void, async_fetch, (usize min_size, Handler const& handler), ());

    MOCK_METHOD(void, consume, (usize count), ());
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
// Test Plan:
//   1. fetch -> error
//   2. fetch -> one chunk, bigger than the minimum; OK
//   3. fetch -> one chunk, smaller than the minimum; DEATH
//   4. fetch -> many chunks, first is bigger than the minimum; OK
//   5. fetch -> many chunks, first is smaller than the minimum, but sequence is big enough; OK (copied)
//
//   a. {2, 4, 5} chunk fully consumed
//   b. {2, 4} chunk back up partially - consume remaining
//   c. {2, 5} chunk back up fully - consume none
//
//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

class AsyncFetchTest : public ::testing::Test
{
   public:
    using ScopedChunk = batt::BasicScopedChunk<::testing::StrictMock<MockAsyncFetchStream>>;

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    template <typename Fn = void()>
    void run_test(Fn&& test_fn)
    {
        batt::Task task{this->io_context.get_executor(), BATT_FORWARD(test_fn)};

        ASSERT_NO_FATAL_FAILURE(this->io_context.run());

        task.join();
    }

    void respond_to_fetch_with_error(boost::system::error_code ec)
    {
        EXPECT_CALL(this->mock_stream, async_fetch(::testing::_, ::testing::_))
            .WillOnce(::testing::InvokeArgument<1>(ec, MockAsyncFetchStream::const_buffers_type{}));
    }

    template <typename... Ts>
    void respond_to_fetch_with_chunks(Ts&&... chunks)
    {
        EXPECT_CALL(this->mock_stream, async_fetch(::testing::_, ::testing::_))
            .WillOnce(::testing::InvokeArgument<1>(
                boost::system::error_code{},
                MockAsyncFetchStream::const_buffers_type{BATT_FORWARD(chunks)...}));
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    boost::asio::io_context io_context;
    ::testing::StrictMock<MockAsyncFetchStream> mock_stream;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//  1. fetch -> error
//
TEST_F(AsyncFetchTest, FetchError)
{
    this->run_test([this] {
        this->respond_to_fetch_with_error(boost::asio::error::access_denied);

        batt::StatusOr<ScopedChunk> chunk = batt::fetch_chunk(this->mock_stream, /*min_size=*/1);

        EXPECT_FALSE(chunk.ok());
        EXPECT_EQ(chunk.status(), batt::StatusCode::kInternal);
    });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//  2a. fetch -> ok, single chunk; consume all
//
TEST_F(AsyncFetchTest, FetchSingleChunkConsumeAll)
{
    this->run_test([this] {
        this->respond_to_fetch_with_chunks(kHelloChunk);

        batt::StatusOr<ScopedChunk> chunk = batt::fetch_chunk(this->mock_stream, /*min_size=*/1);

        EXPECT_TRUE(chunk.ok());
        EXPECT_EQ(chunk->size(), 5u);
        EXPECT_THAT((const char*)chunk->data(), ::testing::StrEq(kHelloStr));

        EXPECT_CALL(this->mock_stream, consume(5u))  //
            .WillOnce(::testing::Return());
    });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//  2b. fetch -> ok, single chunk; consume part
//
TEST_F(AsyncFetchTest, FetchSingleChunkConsumePart)
{
    this->run_test([this] {
        this->respond_to_fetch_with_chunks(kHelloChunk);

        batt::StatusOr<ScopedChunk> chunk = batt::fetch_chunk(this->mock_stream, /*min_size=*/1);

        EXPECT_TRUE(chunk.ok());
        EXPECT_EQ(chunk->size(), 5u);
        EXPECT_THAT((const char*)chunk->data(), ::testing::StrEq(kHelloStr));

        chunk->back_up(2);

        EXPECT_EQ(chunk->size(), 3u);
        EXPECT_THAT(std::string((const char*)chunk->data(), chunk->size()),
                    ::testing::StrEq(kHelloStr.substr(0, 3)));

        EXPECT_CALL(this->mock_stream, consume(3u))  //
            .WillOnce(::testing::Return());
    });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//  2c. fetch -> ok, single chunk; consume none
//
TEST_F(AsyncFetchTest, FetchSingleChunkConsumeNone)
{
    this->run_test([this] {
        this->respond_to_fetch_with_chunks(kHelloChunk);

        batt::StatusOr<ScopedChunk> chunk = batt::fetch_chunk(this->mock_stream, /*min_size=*/1);

        EXPECT_TRUE(chunk.ok());
        EXPECT_EQ(chunk->size(), 5u);
        EXPECT_THAT((const char*)chunk->data(), ::testing::StrEq(kHelloStr));

        chunk->back_up();

        EXPECT_TRUE(chunk->empty());
        EXPECT_EQ(chunk->size(), 0u);

        EXPECT_CALL(this->mock_stream, consume(0u))  //
            .WillOnce(::testing::Return());
    });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// 3. fetch -> one chunk, smaller than the minimum; DEATH
//
TEST_F(AsyncFetchTest, FetchSingleChunkTooSmallDeath)
{
    EXPECT_DEATH(this->run_test([this] {
        // The expected panic prevents the mock object from being freed, so we have to use AllowLeak to
        // suppress a spurious error message from gmock.  Since the process dies with a fatal error, it
        // doesn't really matter that the mock leaks.
        //
        testing::Mock::AllowLeak(&this->mock_stream);

        ON_CALL(this->mock_stream, async_fetch(::testing::_, ::testing::_))
            .WillByDefault(::testing::InvokeArgument<1>(
                boost::system::error_code{}, MockAsyncFetchStream::const_buffers_type{kHelloChunk}));

        batt::StatusOr<ScopedChunk> chunk =
            batt::fetch_chunk(this->mock_stream, /*min_size=*/kHelloStr.size() + 1);

        chunk.IgnoreError();
    }),
                 "The stream returned less than the minimum fetch size!");
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// 4a. fetch -> many chunks, first is bigger than the minimum; OK; consume all
//
TEST_F(AsyncFetchTest, FetchManyChunksFirstBigEnoughConsumeAll)
{
    this->run_test([this] {
        this->respond_to_fetch_with_chunks(kHelloChunk, kWorldChunk);

        batt::StatusOr<ScopedChunk> chunk = batt::fetch_chunk(this->mock_stream, /*min_size=*/5);

        EXPECT_TRUE(chunk.ok());
        EXPECT_EQ(chunk->size(), 5u);
        EXPECT_THAT((const char*)chunk->data(), ::testing::StrEq(kHelloStr));

        EXPECT_CALL(this->mock_stream, consume(5u))  //
            .WillOnce(::testing::Return());
    });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// 4b. fetch -> many chunks, first is bigger than the minimum; OK; consume part
//
TEST_F(AsyncFetchTest, FetchManyChunksFirstBigEnoughConsumePart)
{
    this->run_test([this] {
        this->respond_to_fetch_with_chunks(kHelloChunk, kWorldChunk);

        batt::StatusOr<ScopedChunk> chunk = batt::fetch_chunk(this->mock_stream, /*min_size=*/3);

        EXPECT_TRUE(chunk.ok());
        EXPECT_EQ(chunk->size(), 5u);
        EXPECT_THAT((const char*)chunk->data(), ::testing::StrEq(kHelloStr));

        chunk->back_up(4);

        EXPECT_EQ(chunk->size(), 1u);
        EXPECT_THAT(std::string((const char*)chunk->data(), chunk->size()),
                    ::testing::StrEq(kHelloStr.substr(0, 1)));

        EXPECT_CALL(this->mock_stream, consume(1u))  //
            .WillOnce(::testing::Return());
    });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
TEST_F(AsyncFetchTest, FetchManyChunksGatherConsumeAll)
{
    this->run_test([this] {
        this->respond_to_fetch_with_chunks(kHelloChunk, kWorldChunk);

        batt::StatusOr<ScopedChunk> chunk = batt::fetch_chunk(this->mock_stream, /*min_size=*/10);

        EXPECT_TRUE(chunk.ok());
        EXPECT_EQ(chunk->size(), 10u);
        EXPECT_THAT(std::string((const char*)chunk->data(), chunk->size()), ::testing::StrEq("hello, wor"));

        EXPECT_CALL(this->mock_stream, consume(10u))  //
            .WillOnce(::testing::Return());
    });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
TEST_F(AsyncFetchTest, FetchManyChunksGatherConsumeNone)
{
    this->run_test([this] {
        this->respond_to_fetch_with_chunks(kHelloChunk, kWorldChunk);

        batt::StatusOr<ScopedChunk> result = batt::fetch_chunk(this->mock_stream, /*min_size=*/10);

        ASSERT_TRUE(result.ok());

        ScopedChunk tmp1 = std::move(*result);
        ScopedChunk tmp2;
        tmp2 = std::move(tmp1);
        tmp2 = std::move(tmp2);

        ScopedChunk chunk = std::move(tmp2);

        EXPECT_EQ(chunk.size(), 10u);
        EXPECT_THAT(std::string((const char*)chunk.data(), chunk.size()), ::testing::StrEq("hello, wor"));

        chunk.back_up();

        EXPECT_EQ(chunk.size(), 0u);
        EXPECT_TRUE(chunk.empty());
        EXPECT_THAT(std::string((const char*)chunk.data(), chunk.size()), ::testing::StrEq(""));

        EXPECT_CALL(this->mock_stream, consume(0u))  //
            .WillOnce(::testing::Return());
    });
}

}  // namespace
