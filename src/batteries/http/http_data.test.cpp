#include <batteries/http/http_data.hpp>
//
#include <batteries/http/http_data.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/http/http_chunk_decoder.hpp>

#include <boost/asio/bind_executor.hpp>

namespace {

using namespace batt::int_types;

template <typename BufferSequence>
std::string buffers_to_string(const BufferSequence& buffers)
{
    const usize size = boost::asio::buffer_size(buffers);
    std::vector<char> tmp(size);
    boost::asio::buffer_copy(batt::MutableBuffer{tmp.data(), tmp.size()}, buffers);
    return std::string(tmp.data(), tmp.size());
}

TEST(HttpDataTest, Test)
{
    for (bool include_trailer : {false, true}) {
        batt::StreamBuffer raw_input{512}, chunked_output{512};
        batt::HttpChunkDecoder<batt::StreamBuffer&> decoder{chunked_output,
                                                            batt::IncludeHttpTrailer{include_trailer}};

        EXPECT_EQ(decoder.size(), 0u);

        std::string expect_decoded = "";

        batt::Status encoder_result;
        {
            boost::asio::io_context io;

            batt::Task encoder_task{io.get_executor(), [&] {
                                        encoder_result = batt::http_encode_chunked(
                                            raw_input, chunked_output,
                                            batt::IncludeHttpTrailer{include_trailer});
                                    }};

            auto on_scope_exit = batt::finally([&] {
                raw_input.close_for_write();
                io.run();
                encoder_task.join();
            });

            std::vector<std::string> chunks;
            chunks.push_back("The data will flow\n");
            chunks.push_back("From source to destination\n");
            chunks.push_back("One chunk at a time\n");

            for (const auto& chunk : chunks) {
                EXPECT_TRUE(raw_input.write_all(batt::ConstBuffer{chunk.data(), chunk.size()}).ok());
                expect_decoded += chunk;

                io.poll();
                io.reset();

                batt::StatusOr<batt::SmallVec<batt::ConstBuffer, 2>> decoded =
                    decoder.fetch_at_least(expect_decoded.size());

                ASSERT_TRUE(decoded.ok()) << BATT_INSPECT(decoded.status());
                EXPECT_THAT(buffers_to_string(*decoded), ::testing::StrEq(expect_decoded));
            }
        }

        EXPECT_TRUE(encoder_result.ok()) << BATT_INSPECT(encoder_result);

        batt::StatusOr<batt::SmallVec<batt::ConstBuffer, 2>> decoded =
            decoder.fetch_at_least(expect_decoded.size());

        ASSERT_TRUE(decoded.ok()) << BATT_INSPECT(decoded.status());
        EXPECT_THAT(buffers_to_string(*decoded), ::testing::StrEq(expect_decoded));

        // Consume all the encoded data;
        //
        decoder.consume(expect_decoded.size());
        decoded = decoder.fetch_at_least(1);

        EXPECT_EQ(decoded.status(), batt::StatusCode::kEndOfStream);
        EXPECT_EQ(chunked_output.size(), 0u) << BATT_INSPECT(include_trailer) << BATT_INSPECT(decoder.done());
    }
}

}  // namespace
