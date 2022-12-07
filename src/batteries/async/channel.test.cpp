//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/async/channel.hpp>
//
#include <batteries/async/channel.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/asio/io_context.hpp>

#include <sstream>
#include <string>

namespace {

TEST(AsyncChannelTest, Example)
{
    boost::asio::io_context io;

    // Create a channel to pass strings between tasks.
    //
    batt::Channel<std::string> channel;

    std::stringstream iss, oss;

    iss << "hello, world" << std::endl << "a second line" << std::endl << std::endl;

    // The producer task reads lines from stdin until it gets an empty line.
    //
    batt::Task producer{io.get_executor(), [&channel, &iss] {
                            std::string s;

                            // Keep reading lines until an empty line is read.
                            //
                            for (;;) {
                                std::getline(iss, s);

                                if (s.empty()) {
                                    break;
                                }

                                batt::Status write_status = channel.write(s);
                                BATT_CHECK_OK(write_status);
                            }

                            // Let the consumer task know we are done.
                            //
                            channel.close_for_write();
                        }};

    // The consumer tasks reads strings from the channel, printing them to stdout
    // until the channel is closed.
    //
    batt::Task consumer{io.get_executor(), [&channel, &oss] {
                            for (;;) {
                                batt::StatusOr<std::string&> line = channel.read();
                                if (!line.ok()) {
                                    break;
                                }

                                oss << "READ: " << *line << std::endl;

                                // Important: signal to the producer that we are done with the string!
                                //
                                channel.consume();
                            }

                            channel.close_for_read();
                        }};

    io.run();

    producer.join();
    consumer.join();

    EXPECT_THAT(oss.str(), ::testing::StrEq("READ: hello, world\nREAD: a second line\n"));
}

template <typename T>
class MockReadHandler
{
   public:
    MOCK_METHOD(void, invoke, (batt::StatusOr<T&>), ());

    void operator()(const batt::StatusOr<T&>& result)
    {
        return this->invoke(result);
    }
};

class MockWriteHandler
{
   public:
    MOCK_METHOD(void, invoke, (batt::Status), ());

    void operator()(const batt::Status& result)
    {
        return this->invoke(result);
    }
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Case 1a: async_read, close_for_read, close_for_write
//
TEST(AsyncChannelTest, AsyncReadCloseReadCloseWrite)
{
    ::testing::StrictMock<MockReadHandler<std::string>> mock_read_handler;
    batt::Channel<std::string> channel;
    bool read_done = false;

    channel.async_read(std::ref(mock_read_handler));

    EXPECT_CALL(mock_read_handler, invoke(::testing::Truly([](const batt::StatusOr<std::string&>& result) {
                    return result.status() == batt::StatusCode::kClosed;
                })))
        .WillOnce(::testing::InvokeWithoutArgs([&read_done] {
            read_done = true;
        }));

    channel.close_for_read();

    EXPECT_FALSE(read_done);

    channel.close_for_write();

    EXPECT_TRUE(read_done);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Case 1b: async_read, close_for_write
//
TEST(AsyncChannelTest, AsyncReadCloseWrite)
{
    ::testing::StrictMock<MockReadHandler<std::string>> mock_read_handler;
    batt::Channel<std::string> channel;
    bool read_done = false;

    channel.async_read(std::ref(mock_read_handler));

    EXPECT_CALL(mock_read_handler, invoke(::testing::Truly([](const batt::StatusOr<std::string&>& result) {
                    return result.status() == batt::StatusCode::kClosed;
                })))
        .WillOnce(::testing::InvokeWithoutArgs([&read_done] {
            read_done = true;
        }));

    channel.close_for_write();

    EXPECT_TRUE(read_done);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Case 1c: async_read, close_for_read, (Channel destroyed)
//
TEST(AsyncChannelTest, AsyncReadCloseReadDestroy)
{
    ::testing::StrictMock<MockReadHandler<std::string>> mock_read_handler;
    bool read_done = false;
    {
        batt::Channel<std::string> channel;

        channel.async_read(std::ref(mock_read_handler));

        EXPECT_CALL(mock_read_handler,
                    invoke(::testing::Truly([](const batt::StatusOr<std::string&>& result) {
                        return result.status() == batt::StatusCode::kClosed;
                    })))
            .WillOnce(::testing::InvokeWithoutArgs([&read_done] {
                read_done = true;
            }));

        channel.close_for_read();

        EXPECT_FALSE(read_done);
    }
    EXPECT_TRUE(read_done);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Case 1d: async_read, (Channel destroyed)
//
TEST(AsyncChannelTest, AsyncReadDestroy)
{
    ::testing::StrictMock<MockReadHandler<std::string>> mock_read_handler;
    bool read_done = false;
    {
        batt::Channel<std::string> channel;

        channel.async_read(std::ref(mock_read_handler));

        EXPECT_CALL(mock_read_handler,
                    invoke(::testing::Truly([](const batt::StatusOr<std::string&>& result) {
                        return result.status() == batt::StatusCode::kClosed;
                    })))
            .WillOnce(::testing::InvokeWithoutArgs([&read_done] {
                read_done = true;
            }));

        EXPECT_FALSE(read_done);
    }
    EXPECT_TRUE(read_done);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Case 2: async_read, async_write
//
TEST(AsyncChannelTest, AsyncReadWrite)
{
    ::testing::StrictMock<MockReadHandler<std::string>> mock_read_handler;
    ::testing::StrictMock<MockWriteHandler> mock_write_handler;
    batt::Channel<std::string> channel;

    channel.async_read(std::ref(mock_read_handler));

    std::string the_string;

    EXPECT_CALL(mock_read_handler,
                invoke(::testing::Truly([&the_string](const batt::StatusOr<std::string&>& result) {
                    return result.ok() && &*result == &the_string;
                })))
        .WillOnce(::testing::Return());

    channel.async_write(the_string, std::ref(mock_write_handler));

    EXPECT_CALL(mock_write_handler, invoke(::batt::OkStatus()))  //
        .WillOnce(::testing::Return());

    channel.consume();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Case 3: async_write, async_read
//
TEST(AsyncChannelTest, AsyncWriteRead)
{
    ::testing::StrictMock<MockReadHandler<std::string>> mock_read_handler;
    ::testing::StrictMock<MockWriteHandler> mock_write_handler;
    batt::Channel<std::string> channel;

    std::string the_string;

    channel.async_write(the_string, std::ref(mock_write_handler));

    EXPECT_CALL(mock_read_handler,
                invoke(::testing::Truly([&the_string](const batt::StatusOr<std::string&>& result) {
                    return result.ok() && &*result == &the_string;
                })))
        .WillOnce(::testing::Return());

    channel.async_read(std::ref(mock_read_handler));

    EXPECT_CALL(mock_write_handler, invoke(::batt::OkStatus()))  //
        .WillOnce(::testing::Return());

    channel.consume();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Case 4a: close_for_read, async_read
//
TEST(AsyncChannelTest, CloseReadAsyncRead)
{
    ::testing::StrictMock<MockReadHandler<std::string>> mock_read_handler;
    batt::Channel<std::string> channel;

    channel.close_for_read();

    EXPECT_CALL(mock_read_handler, invoke(::testing::Truly([](const batt::StatusOr<std::string&>& result) {
                    return result.status() == batt::StatusCode::kClosed;
                })))
        .WillOnce(::testing::Return());

    channel.async_read(std::ref(mock_read_handler));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Case 4b: close_for_write, async_read
//
TEST(AsyncChannelTest, CloseWriteAsyncRead)
{
    ::testing::StrictMock<MockReadHandler<std::string>> mock_read_handler;
    batt::Channel<std::string> channel;

    channel.close_for_write();

    EXPECT_CALL(mock_read_handler, invoke(::testing::Truly([](const batt::StatusOr<std::string&>& result) {
                    return result.status() == batt::StatusCode::kClosed;
                })))
        .WillOnce(::testing::Return());

    channel.async_read(std::ref(mock_read_handler));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Case 4c: close_for_write, async_write
//
TEST(AsyncChannelTest, CloseWriteAsyncWrite)
{
    ::testing::StrictMock<MockWriteHandler> mock_write_handler;
    batt::Channel<std::string> channel;

    channel.close_for_write();

    EXPECT_CALL(mock_write_handler, invoke(::testing::Truly([](const batt::Status& result) {
                    return result == batt::StatusCode::kClosed;
                })))
        .WillOnce(::testing::Return());

    std::string the_string;

    channel.async_write(the_string, std::ref(mock_write_handler));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Case 4d: close_for_read, async_write
//
TEST(AsyncChannelTest, CloseReadAsyncWrite)
{
    ::testing::StrictMock<MockWriteHandler> mock_write_handler;
    batt::Channel<std::string> channel;

    channel.close_for_read();

    EXPECT_CALL(mock_write_handler, invoke(::testing::Truly([](const batt::Status& result) {
                    return result == batt::StatusCode::kClosed;
                })))
        .WillOnce(::testing::Return());

    std::string the_string;

    channel.async_write(the_string, std::ref(mock_write_handler));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Case 5: async_write, close_for_read
//
TEST(AsyncChannelTest, AsyncWriteCloseRead)
{
    ::testing::StrictMock<MockWriteHandler> mock_write_handler;
    batt::Channel<std::string> channel;

    std::string the_string;

    channel.async_write(the_string, std::ref(mock_write_handler));

    EXPECT_CALL(mock_write_handler, invoke(::testing::Truly([](const batt::Status& result) {
                    return result == batt::StatusCode::kClosed;
                })))
        .WillOnce(::testing::Return());

    channel.close_for_read();
}

}  // namespace
