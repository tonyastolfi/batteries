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

}  // namespace
