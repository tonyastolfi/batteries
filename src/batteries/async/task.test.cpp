// Copyright 2021 Anthony Paul Astolfi
//
#include <batteries/async/task.hpp>
//
#include <batteries/async/task.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/async/io_result.hpp>
#include <batteries/buffer.hpp>

#ifdef BATT_GLOG_AVAILABLE
#include <glog/logging.h>
#endif  // BATT_GLOG_AVAILABLE

#include <batteries/assert.hpp>
#include <batteries/segv.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#include <boost/asio/ip/tcp.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception_ptr.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <thread>

namespace {

using batt::ConstBuffer;
using batt::ErrorCode;
using batt::IOResult;
using batt::MutableBuffer;
using batt::Task;

namespace ip = boost::asio::ip;
using ip::tcp;

template <typename AsyncStream>
ErrorCode write_all(ConstBuffer data, AsyncStream& stream)
{
    ErrorCode ec;

    while (data.size() > 0) {
        auto result = Task::await<IOResult<std::size_t>>([&](auto&& handler) {
            stream.async_write_some(data, BATT_FORWARD(handler));
        });
        if (!result.ok()) {
            ec = result.error();
            break;
        }
        data += *result;
    }

    return ec;
}

template <typename AsyncStream>
IOResult<std::size_t> read_all(MutableBuffer buffer, AsyncStream& stream)
{
    ErrorCode ec;
    std::size_t bytes_read = 0;

    while (buffer.size() > 0) {
        auto result = Task::await<IOResult<std::size_t>>([&](auto&& handler) {
            stream.async_read_some(buffer, BATT_FORWARD(handler));
        });
        if (!result.ok()) {
            ec = result.error();
            break;
        }
        buffer += *result;
        bytes_read += *result;
    }

    return IOResult<std::size_t>{ec, bytes_read};
}

TEST(TaskTest, ClientServerAsio)
{
    constexpr std::size_t kNumIterations = 15;
    constexpr std::size_t kNumInstances = 200;
    constexpr std::size_t kNumThreads = 16;

    boost::asio::io_context io;

    for (std::size_t j = 0; j < kNumIterations; ++j) {
        std::atomic<std::size_t> client_connected = 0;
        std::atomic<std::size_t> client_sent_data = 0;
        std::atomic<std::size_t> server_accepted = 0;
        std::atomic<std::size_t> server_read_data = 0;

        std::vector<std::unique_ptr<Task>> instances;

        for (std::size_t i = 0; i < kNumInstances; ++i) {
            instances.emplace_back(std::make_unique<Task>(io.get_executor(), [&] {
                BATT_CHECK_NOT_NULLPTR(&Task::current());
                Task* self = &Task::current();

                tcp::acceptor listening{io, tcp::v4()};
                listening.set_option(tcp::no_delay{true});

                ErrorCode bind_ec;
                for (int retry = 0; retry < 100; ++retry) {
                    listening.bind(tcp::endpoint{ip::address_v4::loopback(), 0}, bind_ec);
                    if (!bind_ec) {
                        break;
                    }
                    std::cerr << "+" << std::flush;
                    Task::sleep(boost::posix_time::milliseconds(2500));
                }
                BATT_CHECK(!bind_ec) << bind_ec.message();

                ErrorCode listen_ec;
                listening.listen(boost::asio::socket_base::max_listen_connections, listen_ec);

                const std::string test_data = "testing 1, 2, 3...";

                BATT_CHECK_EQ((void*)self, (void*)&Task::current());

                Task client{
                    io.get_executor(), [&] {
                        tcp::socket stream{io, tcp::v4()};
                        stream.set_option(tcp::no_delay{true});

                        ErrorCode connect_ec;

                        for (int retry = 0; retry < 100; ++retry) {
                            if (!connect_ec) {
                                connect_ec = Task::await<boost::system::error_code>([&](auto&& handler) {
                                    stream.async_connect(listening.local_endpoint(), BATT_FORWARD(handler));
                                });
                            }
                            if (!connect_ec) {
                                break;
                            }
                            std::cerr << "." << std::flush;
                            stream.close(connect_ec);
                            Task::sleep(boost::posix_time::milliseconds(2500));

                            stream.open(tcp::v4(), connect_ec);
                            if (!connect_ec) {
                                stream.set_option(tcp::no_delay{true});
                            }
                        }
                        BATT_CHECK(!connect_ec) << connect_ec.message();

                        ++client_connected;

                        auto write_ec = write_all(boost::asio::buffer(test_data), stream);

                        BATT_CHECK(!write_ec) << write_ec.message();

                        ++client_sent_data;
                    }};

                BATT_CHECK_EQ((void*)self, (void*)&Task::current());

                Task server{io.get_executor(), [&] {
                                auto accept_result = Task::await<IOResult<tcp::socket>>([&](auto&& handler) {
                                    listening.async_accept(BATT_FORWARD(handler));
                                });

                                BATT_CHECK(accept_result.ok()) << accept_result.error().message();

                                tcp::socket& stream = *accept_result;
                                stream.set_option(tcp::no_delay{true});

                                ++server_accepted;

                                std::vector<char> buffer(test_data.length());

                                auto result = read_all(boost::asio::buffer(buffer), stream);

                                BATT_CHECK(result.ok()) << result.error().message();
                                BATT_CHECK_EQ(std::string_view(buffer.data(), buffer.size()), test_data);

                                ++server_read_data;
                            }};

                BATT_CHECK_EQ((void*)self, (void*)&Task::current());

                client.join();

                BATT_CHECK_EQ((void*)self, (void*)&Task::current());

                server.join();

                BATT_CHECK_EQ((void*)self, (void*)&Task::current());
            }));
        }

        io.restart();
        std::vector<std::thread> threads;
        for (std::size_t i = 0; i < kNumThreads; ++i) {
            threads.emplace_back([&io] {
                try {
                    io.run();
                } catch (...) {
#ifdef BATT_GLOG_AVAILABLE
                    LOG(ERROR)
#else
                    std::cerr
#endif
                        << "unhandled exception: "
                        << boost::diagnostic_information(boost::current_exception());
                }
            });
        }
        for (std::thread& t : threads) {
            t.join();
        }

        EXPECT_EQ(client_connected, kNumInstances);
        EXPECT_EQ(server_accepted, kNumInstances);
        EXPECT_EQ(client_sent_data, kNumInstances);
        EXPECT_EQ(server_read_data, kNumInstances);
    }
}

}  // namespace
