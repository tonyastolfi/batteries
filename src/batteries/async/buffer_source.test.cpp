#include <batteries/async/buffer_source.hpp>
//
#include <batteries/async/buffer_source.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/async/stream_buffer.hpp>
#include <batteries/seq/boxed.hpp>

namespace {

using namespace batt::int_types;

TEST(BufferSourceTest, HasConstBufferSequenceRequirementsTest)
{
    EXPECT_TRUE((batt::HasConstBufferSequenceRequirements<std::vector<batt::ConstBuffer>>{}));
    EXPECT_TRUE((batt::HasConstBufferSequenceRequirements<std::vector<batt::MutableBuffer>>{}));
    EXPECT_TRUE((batt::HasConstBufferSequenceRequirements<std::array<batt::MutableBuffer, 2>>{}));
    EXPECT_FALSE((batt::HasConstBufferSequenceRequirements<int>{}));
    EXPECT_FALSE((batt::HasConstBufferSequenceRequirements<batt::BoxedSeq<std::pair<int, std::string>>>{}));
}

TEST(BufferSourceTest, HasBufferSourceRequirementsTest)
{
    EXPECT_TRUE((batt::HasBufferSourceRequirements<batt::StreamBuffer>{}));
    EXPECT_FALSE((batt::HasBufferSourceRequirements<batt::BoxedSeq<batt::ConstBuffer>>{}));
}

TEST(BufferSourceTest, TakePrefix)
{
    constexpr usize kBufferSize = 16;
    const std::string_view kTestData{"0123456789"};

    for (usize offset = 0; offset < kBufferSize; ++offset) {
        for (usize prefix_size = 0; prefix_size <= kTestData.size() + 2; ++prefix_size) {
            batt::StreamBuffer sb{kBufferSize};

            if (offset > 0) {
                ASSERT_TRUE(sb.prepare_exactly(offset).ok());
                sb.commit(offset);
                sb.consume(offset);
            }

            batt::BufferSource sb_prefix = sb | batt::seq::take_n(prefix_size);

            ASSERT_TRUE(sb.write_all(batt::ConstBuffer{kTestData.data(), kTestData.size()}).ok());

            sb.close_for_write();

            auto data = sb_prefix.fetch_at_least(1);

            ASSERT_TRUE(data.ok());
            EXPECT_EQ(boost::asio::buffer_size(*data), std::min(prefix_size, kTestData.size()));

            batt::StatusOr<std::vector<char>> bytes = sb_prefix | batt::seq::collect_vec();

            ASSERT_TRUE(bytes.ok()) << BATT_INSPECT(bytes.status());
            EXPECT_THAT((std::string_view{bytes->data(), bytes->size()}),
                        ::testing::StrEq(kTestData.substr(0, std::min(prefix_size, kTestData.size()))));
        }
    }
}

TEST(BufferSourceTest, PrependBuffers)
{
    batt::StreamBuffer rest{1024};

    const std::string first_str = "A penny saved... ";
    const std::string second_str = "is a penny earned.";

    {
        batt::Status s = rest.write_all(batt::ConstBuffer{second_str.data(), second_str.size()});
        ASSERT_TRUE(s.ok()) << s;

        rest.close_for_write();
    }

    batt::BufferSource src = rest | batt::seq::prepend(batt::ConstBuffer{first_str.data(), first_str.size()});
    batt::StatusOr<std::vector<char>> bytes = src | batt::seq::collect_vec();

    ASSERT_TRUE(bytes.ok()) << bytes.status();
    EXPECT_THAT((std::string_view{bytes->data(), bytes->size()}), ::testing::StrEq(first_str + second_str));
}

TEST(BufferSourceTest, PrependBuffersEndOfStream)
{
    batt::StreamBuffer empty_stream{64};
    empty_stream.close_for_write();

    auto src = empty_stream | batt::seq::prepend(batt::ConstBuffer{});
    auto fetched = src.fetch_at_least(1);

    EXPECT_EQ(fetched.status(), batt::StatusCode::kEndOfStream);
}

}  // namespace
