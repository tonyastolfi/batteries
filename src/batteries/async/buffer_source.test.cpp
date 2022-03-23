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

            auto sb_prefix = sb | batt::seq::take_n(prefix_size);

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

}  // namespace
