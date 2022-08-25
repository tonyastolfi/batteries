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

using namespace batt::int_types;

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

template <usize kExtraBytes>
class ValueWrapper
{
   public:
    //+++++++++++-+-+--+----- --- -- -  -  -   -

    Val val;
    Gen gen{0};
    std::array<char, kExtraBytes + 1> extra_;

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
class MockValueImpl : public batt::AbstractValueImpl<AbstractMockValue, MockValueImpl, T>
{
   public:
    using Super = batt::AbstractValueImpl<AbstractMockValue, MockValueImpl, T>;

    explicit MockValueImpl(T&& obj) : Super{std::move(obj)}
    {
    }

    Val get_val() const override
    {
        return this->obj_.val;
    }

    Gen get_gen() const override
    {
        return this->obj_.gen;
    }
};

template <usize kExtraBytes>
void run_basic_tests()
{
    using Storage = batt::TypeErasedStorage<AbstractMockValue, MockValueImpl>;

    ::testing::StrictMock<MockValue> mock;
    ::testing::Sequence s;
    {
        EXPECT_CALL(mock, default_construct(Val{0}))  //
            .InSequence(s)
            .WillOnce(::testing::Return());

        ValueWrapper<kExtraBytes> value{Val{0}, mock};
        static_assert(sizeof(value) > kExtraBytes, "");

        const bool is_small_object = sizeof(value) <= Storage::reserved_size;

        EXPECT_EQ(value.val, Val{0});
        EXPECT_EQ(value.gen, Gen{0});
        {
            Storage storage5;

            EXPECT_FALSE(storage5.is_valid());
            EXPECT_EQ(storage5.get(), nullptr);

            storage5 = [&] {
                EXPECT_CALL(mock, copy_construct(Val{0}, Gen{1}))  // The temporary arg
                    .InSequence(s)
                    .WillOnce(::testing::Return())
                    .RetiresOnSaturation();

                EXPECT_CALL(mock, move_construct(Val{0}, Gen{2}))  // The copy in `storage1`
                    .InSequence(s)
                    .WillOnce(::testing::Return())
                    .RetiresOnSaturation();

                EXPECT_CALL(mock, destruct(Val{0}, Gen{1}))  // The temporary arg
                    .InSequence(s)
                    .WillOnce(::testing::Return())
                    .RetiresOnSaturation();
                {
                    Storage storage2{batt::StaticType<ValueWrapper<kExtraBytes>>{}, batt::make_copy(value)};
                    {
                        EXPECT_EQ(storage2->get_val(), Val{0});
                        EXPECT_EQ(storage2->get_gen(), Gen{2});

                        if (is_small_object) {
                            EXPECT_CALL(mock, move_construct(Val{0}, Gen{3}))  // The copy in `storage3`
                                .InSequence(s)
                                .WillOnce(::testing::Return())
                                .RetiresOnSaturation();

                            EXPECT_CALL(mock, destruct(Val{0}, Gen{2}))  // The copy in `storage2`
                                .InSequence(s)
                                .WillOnce(::testing::Return())
                                .RetiresOnSaturation();
                        }

                        Storage storage3 = std::move(storage2);

                        EXPECT_EQ(storage3->get_val(), Val{0});

                        if (is_small_object) {
                            EXPECT_EQ(storage3->get_gen(), Gen{3});

                            EXPECT_CALL(mock, move_construct(Val{0}, Gen{4}))  // The returned value
                                .InSequence(s)
                                .WillOnce(::testing::Return())
                                .RetiresOnSaturation();

                            EXPECT_CALL(mock, destruct(Val{0}, Gen{3}))  // The copy in `storage3`
                                .InSequence(s)
                                .WillOnce(::testing::Return())
                                .RetiresOnSaturation();

                            EXPECT_CALL(mock, move_construct(Val{0}, Gen{5}))  // The copy in storage5
                                .InSequence(s)
                                .WillOnce(::testing::Return())
                                .RetiresOnSaturation();

                            EXPECT_CALL(mock, destruct(Val{0}, Gen{4}))  // The returned value
                                .InSequence(s)
                                .WillOnce(::testing::Return())
                                .RetiresOnSaturation();

                        } else {
                            EXPECT_EQ(storage3->get_gen(), Gen{2});
                        }

                        // The parens suppress copy elision.
                        //
                        return (storage3);
                    }
                }
            }();

            EXPECT_TRUE(storage5.is_valid());

            Gen expected_gen{is_small_object ? 5 : 2};

            EXPECT_EQ(storage5->get_val(), Val{0});
            EXPECT_EQ(storage5->get_gen(), expected_gen);

            EXPECT_CALL(mock, destruct(Val{0}, expected_gen))  //
                .InSequence(s)
                .WillOnce(::testing::Return());
        }

        EXPECT_CALL(mock, destruct(Val{0}, Gen{0}))  //
            .InSequence(s)
            .WillOnce(::testing::Return());
    }
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(TypeErasureTest, Basic)
{
    run_basic_tests<0>();
    run_basic_tests<39>();
    run_basic_tests<40>();
    run_basic_tests<2000>();
}

}  // namespace
