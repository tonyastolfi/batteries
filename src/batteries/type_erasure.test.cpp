//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/type_erasure.hpp>
//
#include <batteries/type_erasure.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/strong_typedef.hpp>

#include <atomic>
#include <memory>

namespace {

BATT_STRONG_TYPEDEF(int, Val);
BATT_STRONG_TYPEDEF(int, Gen);

struct MockValue {
    MOCK_METHOD(void, default_construct, (Val val), ());
    MOCK_METHOD(void, destruct, (Val val, Gen gen), ());
    MOCK_METHOD(void, copy_construct, (Val val, Gen gen), ());
    MOCK_METHOD(void, move_construct, (Val val, Gen gen), ());
    MOCK_METHOD(void, copy_assign, (Val old_val, Gen old_gen, Val new_val, Gen new_gen), ());
    MOCK_METHOD(void, move_assign, (Val old_val, Gen old_gen, Val new_val, Gen new_gen), ());
};

class ValueWrapper
{
   public:
    //+++++++++++-+-+--+----- --- -- -  -  -   -

    Val val;
    Gen gen{0};

    ValueWrapper(Val val, ::testing::StrictMock<MockValue>& mock) : val{val}, impl_{&mock}
    {
        this->impl_->default_construct(val);
    }

    ~ValueWrapper()
    {
        this->impl_->destruct(this->val, this->gen);
    }

    ValueWrapper(const ValueWrapper& other) : val{other.val}, gen{other.gen + 1}, impl_{other.impl_}
    {
        this->impl_->copy_construct(this->val, this->gen);
    }

    ValueWrapper(ValueWrapper&& other) : val{other.val}, gen{other.gen + 1}, impl_{other.impl_}
    {
        this->impl_->move_construct(this->val, this->gen);
    }

    ValueWrapper& operator=(const ValueWrapper& other)
    {
        this->impl_->copy_assign(this->val, this->gen, other.val, Gen{other.gen + 1});
        this->val = other.val;
        this->gen = Gen{other.gen + 1};
        return *this;
    }

    ValueWrapper& operator=(ValueWrapper&& other)
    {
        this->impl_->move_assign(this->val, this->gen, other.val, Gen{other.gen + 1});
        this->val = other.val;
        this->gen = Gen{other.gen + 1};
        return *this;
    }

   private:
    ::testing::StrictMock<MockValue>* impl_;
};

class AbstractMockValue : public batt::AbstractValue<AbstractMockValue>
{
   public:
    virtual Val get_val() const = 0;

    virtual Gen get_gen() const = 0;
};

template <typename T>
class MockValueImpl : public AbstractMockValue
{
   public:
    explicit MockValueImpl(T&& obj) : impl_{std::move(obj)}
    {
    }

    AbstractMockValue* copy_to(batt::MutableBuffer memory)
    {
        return new (memory.data()) MockValueImpl{batt::make_copy(this->impl_)};
    }

    AbstractMockValue* move_to(batt::MutableBuffer memory)
    {
        return new (memory.data()) MockValueImpl{std::move(this->impl_)};
    }

    Val get_val() const override
    {
        return this->impl_.val;
    }

    Gen get_gen() const override
    {
        return this->impl_.gen;
    }

   private:
    T impl_;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(TypeErasureTest, Basic)
{
    ::testing::StrictMock<MockValue> mock;
    ::testing::Sequence s;

    EXPECT_CALL(mock, default_construct(Val{0}))  //
        .InSequence(s)
        .WillOnce(::testing::Return());

    ValueWrapper value{Val{0}, mock};

    EXPECT_EQ(value.val, Val{0});
    EXPECT_EQ(value.gen, Gen{0});
    {
        batt::TypeErasedStorage<AbstractMockValue, MockValueImpl> storage5;

        EXPECT_FALSE(storage5.is_valid());
        EXPECT_EQ(storage5.get(), nullptr);

        storage5 = [&] {
            EXPECT_CALL(mock, copy_construct(Val{0}, Gen{1}))  // The temporary arg
                .InSequence(s)
                .WillOnce(::testing::Return());

            EXPECT_CALL(mock, move_construct(Val{0}, Gen{2}))  // The copy in `storage1`
                .InSequence(s)
                .WillOnce(::testing::Return());

            EXPECT_CALL(mock, destruct(Val{0}, Gen{1}))  // The temporary arg
                .InSequence(s)
                .WillOnce(::testing::Return());
            {
                batt::TypeErasedStorage<AbstractMockValue, MockValueImpl> storage2{
                    batt::StaticType<ValueWrapper>{}, batt::make_copy(value)};
                {
                    EXPECT_EQ(storage2->get_val(), Val{0});
                    EXPECT_EQ(storage2->get_gen(), Gen{2});

                    EXPECT_CALL(mock, move_construct(Val{0}, Gen{3}))  // The copy in `storage3`
                        .InSequence(s)
                        .WillOnce(::testing::Return());

                    batt::TypeErasedStorage<AbstractMockValue, MockValueImpl> storage3 = std::move(storage2);

                    EXPECT_EQ(storage3->get_val(), Val{0});
                    EXPECT_EQ(storage3->get_gen(), Gen{3});

                    EXPECT_CALL(mock, move_construct(Val{0}, Gen{4}))  // The returned value
                        .InSequence(s)
                        .WillOnce(::testing::Return());

                    EXPECT_CALL(mock, destruct(Val{0}, Gen{3}))  // The copy in `storage3`
                        .InSequence(s)
                        .WillOnce(::testing::Return());

                    EXPECT_CALL(mock, destruct(Val{0}, Gen{2}))  // The copy in `storage2`
                        .InSequence(s)
                        .WillOnce(::testing::Return());

                    EXPECT_CALL(mock, move_construct(Val{0}, Gen{5}))  // The copy in storage5
                        .InSequence(s)
                        .WillOnce(::testing::Return());

                    EXPECT_CALL(mock, destruct(Val{0}, Gen{4}))  // The returned value
                        .InSequence(s)
                        .WillOnce(::testing::Return());

                    // The parens suppress copy elision.
                    //
                    return (storage3);
                }
            }
        }();

        EXPECT_TRUE(storage5.is_valid());

        EXPECT_EQ(storage5->get_val(), Val{0});
        EXPECT_EQ(storage5->get_gen(), Gen{5});

        EXPECT_CALL(mock, destruct(Val{0}, Gen{5}))  //
            .InSequence(s)
            .WillOnce(::testing::Return());
    }

    EXPECT_CALL(mock, destruct(Val{0}, Gen{0}))  //
        .InSequence(s)
        .WillOnce(::testing::Return());
}

}  // namespace
