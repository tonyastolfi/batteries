#include <batteries/seq/boxed.hpp>
//
#include <batteries/seq/boxed.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/seq/empty.hpp>

#include <string>

namespace {

TEST(SeqBoxedTest, Empty)
{
    batt::BoxedSeq<std::string> boxed_seq = batt::seq::Empty<std::string>{} | batt::seq::boxed();

    EXPECT_EQ(boxed_seq.next(), batt::None);
}

}  // namespace
