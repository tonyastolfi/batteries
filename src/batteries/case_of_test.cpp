#include <batteries/case_of.hpp>
//
#include <batteries/case_of.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/assert.hpp>

namespace {

TEST(CaseOf, BasicTest)
{
    std::variant<int, std::string, void *> v;

    auto apply_case_of = [&]() -> decltype(auto) {
        return batt::case_of(
          v,
          [](int) {
              return (char)1;
          },
          [](std::string) {
              return 2;
          },
          [](void *) {
              return (long)3;
          });
    };

    v = 4;
    EXPECT_EQ(1, apply_case_of());

    v = std::string("foo");
    EXPECT_EQ(2, apply_case_of());

    v = nullptr;
    EXPECT_EQ(3, apply_case_of());
}

struct Foo
{};
struct Bar
{};

TEST(CaseOf, DocExample)
{
    std::variant<Foo, Bar> var = Bar{};

    int result = batt::case_of(
      var,
      [](const Foo &) {
          return 1;
      },
      [](const Bar &) {
          return 2;
      });

    BATT_CHECK_EQ(result, 2);
}

} // namespace
