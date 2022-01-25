//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/shared_ptr.hpp>
//
#include <batteries/shared_ptr.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/static_assert.hpp>
#include <batteries/type_traits.hpp>

namespace {

struct NonRefCounted {
    int x;
    std::string s;

    NonRefCounted(int x, std::string s) : x{x}, s{s}
    {
    }
};

struct WithRefCounter : batt::RefCounted<WithRefCounter> {
    int y;
    std::string t;

    WithRefCounter(int y, std::string t) : y{y}, t{t}
    {
    }
};

static_assert(batt::IsRefCounted<WithRefCounter>{}, "");

template <typename T>
bool verify_is_ref_counted(batt::StaticType<T> = {})
{
    static_assert(batt::IsRefCounted<T>{}, "");
    static_assert(batt::IsRefCounted<const T>{}, "");
    static_assert(batt::IsRefCounted<volatile T>{}, "");
    static_assert(batt::IsRefCounted<const volatile T>{}, "");
    static_assert(batt::IsRefCounted<T&>{}, "");
    static_assert(batt::IsRefCounted<const T&>{}, "");
    static_assert(batt::IsRefCounted<volatile T&>{}, "");
    static_assert(batt::IsRefCounted<const volatile T&>{}, "");
    static_assert(batt::IsRefCounted<T&&>{}, "");
    static_assert(batt::IsRefCounted<const T&&>{}, "");
    static_assert(batt::IsRefCounted<volatile T&&>{}, "");
    static_assert(batt::IsRefCounted<const volatile T&&>{}, "");
    BATT_STATIC_ASSERT_TYPE_EQ(batt::SharedPtr<T>, boost::intrusive_ptr<T>);
    BATT_STATIC_ASSERT_TYPE_EQ(batt::SharedPtr<const T>, boost::intrusive_ptr<const T>);
    BATT_STATIC_ASSERT_TYPE_EQ(batt::SharedPtr<volatile T>, boost::intrusive_ptr<volatile T>);
    BATT_STATIC_ASSERT_TYPE_EQ(batt::SharedPtr<volatile const T>, boost::intrusive_ptr<volatile const T>);

    return true;
}

template <typename T>
bool verify_not_ref_counted(batt::StaticType<T> = {})
{
    static_assert(!batt::IsRefCounted<T>{}, "");
    static_assert(!batt::IsRefCounted<const T>{}, "");
    static_assert(!batt::IsRefCounted<volatile T>{}, "");
    static_assert(!batt::IsRefCounted<const volatile T>{}, "");
    static_assert(!batt::IsRefCounted<T&>{}, "");
    static_assert(!batt::IsRefCounted<const T&>{}, "");
    static_assert(!batt::IsRefCounted<volatile T&>{}, "");
    static_assert(!batt::IsRefCounted<const volatile T&>{}, "");
    static_assert(!batt::IsRefCounted<T&&>{}, "");
    static_assert(!batt::IsRefCounted<const T&&>{}, "");
    static_assert(!batt::IsRefCounted<volatile T&&>{}, "");
    static_assert(!batt::IsRefCounted<const volatile T&&>{}, "");
    BATT_STATIC_ASSERT_TYPE_EQ(batt::SharedPtr<T>, std::shared_ptr<T>);
    BATT_STATIC_ASSERT_TYPE_EQ(batt::SharedPtr<const T>, std::shared_ptr<const T>);
    BATT_STATIC_ASSERT_TYPE_EQ(batt::SharedPtr<volatile T>, std::shared_ptr<volatile T>);
    BATT_STATIC_ASSERT_TYPE_EQ(batt::SharedPtr<volatile const T>, std::shared_ptr<volatile const T>);

    return true;
}

TEST(SharedPtrTest, Test)
{
    EXPECT_TRUE(verify_is_ref_counted<WithRefCounter>());
    EXPECT_TRUE(verify_not_ref_counted<NonRefCounted>());
    EXPECT_TRUE(verify_not_ref_counted<int>());
    EXPECT_TRUE(verify_not_ref_counted<char>());
    EXPECT_TRUE(verify_not_ref_counted<void*>());
    EXPECT_TRUE(verify_not_ref_counted<std::string>());

    std::shared_ptr<std::string> s = batt::make_shared<std::string>("foo");
    EXPECT_EQ(*s, "foo");

    std::shared_ptr<NonRefCounted> n = batt::make_shared<NonRefCounted>(5, "bar");
    EXPECT_EQ(n->x, 5);
    EXPECT_EQ(n->s, "bar");

    boost::intrusive_ptr<WithRefCounter> w = batt::make_shared<WithRefCounter>(6, "grill");
    EXPECT_EQ(w->y, 6);
    EXPECT_EQ(w->t, "grill");

    std::unique_ptr<std::string> s2 = std::make_unique<std::string>("hello");
    std::string* s2_addr = s2.get();
    std::shared_ptr<std::string> s3 = batt::into_shared(std::move(s2));
    EXPECT_EQ(s2_addr, s3.get());

    std::unique_ptr<WithRefCounter> w2 = std::make_unique<WithRefCounter>(7, "world");
    WithRefCounter* w2_addr = w2.get();
    boost::intrusive_ptr<WithRefCounter> w3 = batt::into_shared(std::move(w2));
    EXPECT_EQ(w2_addr, w3.get());
}

}  // namespace
