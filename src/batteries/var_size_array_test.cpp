#include <batteries/var_size_array.hpp>
//
#include <batteries/var_size_array.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

TEST(Var_Size_Array, Test)
{
    for (batt::usize n = 1; n < 10; ++n) {
        batt::VarSizeArray<std::string, 4> strs(n, "apple");
        EXPECT_EQ(strs.is_dynamic(), (n > 4));
        for (const std::string& s : strs) {
            EXPECT_THAT(s, ::testing::StrEq("apple"));
        }
    }
}

}  // namespace
