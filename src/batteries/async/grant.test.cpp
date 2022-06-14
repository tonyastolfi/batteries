//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#include <batteries/async/grant.hpp>
//
#include <batteries/async/grant.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/async/fake_execution_context.hpp>

namespace {

using namespace batt::int_types;

// Test Plan:
//  1. Grant is empty, size is 0, is not valid, is not revoked.
//  2. Issue a grant
//     a. wait = false, pool is large enough
//     b. wait = false, pool is not large enough
//     c. wait = true, pool is large enough
//     d. wait = true, pool is not large enough, resolve by destructing a previous Grant
//     e. wait = true, pool is not large enough, resolve by increasing the pool via recycle
//     f. wait = true, pool is not large enough, resolve by closing the Issuer
//  3. partially spend a grant, with subcases from (2), with d,e combined as described:
//     a. --
//     b. --
//     c. --
//     d,e. resolve by subsuming another Grant
//     f. resolve by revoking the Grant
//  4. Grant move test
//  5. Grant swap test
//  6. Grant spend_all
//  7. subsume invalid grant
//
// Death Tests:
//  10. Issuer destroyed with active Grant still alive
//  11. subsume a grant from a different issuer
//      a. size={0,0}
//      b. size={0,1}
//      c. size={1,0}
//      d. size={1,1}
//  12. subsume into an invalid grant
//

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

//  1. Grant is empty, size is 0, is not valid, is not revoked.
//
TEST(AsyncGrantTest, EmptyGrant)
{
    batt::Grant::Issuer issuer{0};
    batt::StatusOr<batt::Grant> grant = issuer.issue_grant(0, batt::WaitForResource::kFalse);

    ASSERT_TRUE(grant.ok());
    EXPECT_EQ(grant->get_issuer(), &issuer);
    EXPECT_TRUE(grant->empty());
    EXPECT_FALSE(grant->is_valid());
    EXPECT_FALSE(*grant);
    EXPECT_FALSE(grant->is_revoked());
    EXPECT_EQ(grant->size(), 0u);
}

//  2. Issue a grant
//     a. wait = false, pool is large enough
//     c. wait = true, pool is large enough
//
TEST(AsyncGrantTest, IssueGrantSuccess)
{
    batt::Grant::Issuer issuer{10};
    EXPECT_EQ(issuer.available(), 10u);

    for (batt::WaitForResource wait_for_resource :
         {batt::WaitForResource::kFalse, batt::WaitForResource::kTrue}) {
        for (usize count = 0; count <= 10; ++count) {
            {
                batt::StatusOr<batt::Grant> grant = issuer.issue_grant(count, wait_for_resource);
                EXPECT_EQ(issuer.available(), 10u - count);

                ASSERT_TRUE(grant.ok());
                EXPECT_EQ(grant->size(), count);
            }
            EXPECT_EQ(issuer.available(), 10u);
        }
    }
}

//  2. Issue a grant
//     b. wait = false, pool is not large enough
//
TEST(AsyncGrantTest, IssueGrantNoWaitNotAvailable)
{
    batt::Grant::Issuer issuer{10};
    batt::StatusOr<batt::Grant> grant = issuer.issue_grant(11, batt::WaitForResource::kFalse);

    EXPECT_FALSE(grant.ok());
    EXPECT_EQ(grant.status(), batt::StatusCode::kGrantUnavailable);
}

// Define a test fixture for all the blocking test cases, to make wrangling Tasks and executors easier.
//
class AsyncGrantWaitTest : public ::testing::Test
{
   public:
    void start_blocking_operation(const std::function<void()>& fn)
    {
        BATT_CHECK(!this->blocking_operation_);

        this->blocking_operation_.emplace(this->execution_context_.get_executor(), fn,
                                          /*name=*/"AsyncGrantWaitTest::blocking_operation");

        // Allow the task to run up to the point where it blocks.
        //
        EXPECT_GT(this->execution_context_.poll(), 0u)
            << "There should have been some activity on the fake executor!";

        EXPECT_FALSE(this->blocking_operation_->try_join())
            << "The operation should have blocked, preventing the task from finishing!";
    }

    bool resume_blocking_operation()
    {
        BATT_CHECK(this->blocking_operation_);
        return this->execution_context_.poll() != 0u;
    }

    void SetUp() override
    {
        this->first_.emplace(this->issuer_.issue_grant(3, batt::WaitForResource::kTrue));
        ASSERT_TRUE(this->first_.ok());

        this->start_blocking_operation([this] {
            this->second_.emplace(this->issuer_.issue_grant(2, batt::WaitForResource::kTrue));
        });

        EXPECT_FALSE(this->resume_blocking_operation());
    }

    void TearDown() override
    {
        if (this->blocking_operation_) {
            ASSERT_TRUE(this->blocking_operation_->try_join());
            this->blocking_operation_ = batt::None;
        }
    }

    batt::FakeExecutionContext execution_context_;
    batt::Optional<batt::Task> blocking_operation_;
    batt::Grant::Issuer issuer_{4};
    batt::StatusOr<batt::Grant> first_;
    batt::StatusOr<batt::Grant> second_;
};

//  2. Issue a grant
//     d. wait = true, pool is not large enough, resolve by destructing a previous Grant
//
TEST_F(AsyncGrantWaitTest, IssueGrantWaitForOldGrantDestruct)
{
    // This will cause the pending `issue_grant` to resolve.
    //
    this->first_ = batt::Status{batt::StatusCode::kUnknown};

    // Unblock the operation.
    //
    ASSERT_TRUE(this->resume_blocking_operation());

    // Verify success.
    //
    ASSERT_TRUE(this->second_.ok());
    EXPECT_EQ(this->second_->size(), 2u);
}

//  2. Issue a grant
//     e. wait = true, pool is not large enough, resolve by increasing the pool via recycle
//
TEST_F(AsyncGrantWaitTest, IssueGrantWaitForPoolIncrease)
{
    // This will cause the pending `issue_grant` to resolve.
    //
    this->issuer_.grow(1);

    // Unblock the operation.
    //
    ASSERT_TRUE(this->resume_blocking_operation());

    // Verify success.
    //
    ASSERT_TRUE(this->first_.ok());
    EXPECT_EQ(this->first_->size(), 3u);

    ASSERT_TRUE(this->second_.ok());
    EXPECT_EQ(this->second_->size(), 2u);
}

//  2. Issue a grant
//     f. wait = true, pool is not large enough, resolve by closing the Issuer
//
TEST_F(AsyncGrantWaitTest, IssueGrantWaitForClose)
{
    // This will cause the pending `issue_grant` to resolve.
    //
    this->issuer_.close();

    // Unblock the operation.
    //
    ASSERT_TRUE(this->resume_blocking_operation());

    // Verify result.
    //
    ASSERT_TRUE(this->first_.ok());
    EXPECT_EQ(this->first_->size(), 3u);

    ASSERT_FALSE(this->second_.ok());
    EXPECT_EQ(this->second_.status(), batt::StatusCode::kClosed);
}

//  3. partially spend a grant, with subcases from (2), with d,e combined as described:
//     a. wait = false, pool is large enough
//     b. wait = false, pool not large enough
//     c. wait = true, pool is large enough
//
TEST(AsyncGrantTest, PartialSpendSuccess)
{
    batt::Grant::Issuer issuer{10};

    for (batt::WaitForResource wait_for_resource :
         {batt::WaitForResource::kFalse, batt::WaitForResource::kTrue}) {
        batt::StatusOr<batt::Grant> grant = issuer.issue_grant(10, batt::WaitForResource::kFalse);
        ASSERT_TRUE(grant.ok());
        EXPECT_EQ(grant->size(), 10u);

        // 3b. expect failure because count too big
        {
            batt::StatusOr<batt::Grant> subgrant = grant->spend(11, batt::WaitForResource::kFalse);
            EXPECT_FALSE(subgrant.ok());
            EXPECT_EQ(subgrant.status(), batt::StatusCode::kGrantUnavailable);
        }

        for (usize count = 0; count < 10; ++count) {
            batt::StatusOr<batt::Grant> subgrant = grant->spend(1, wait_for_resource);

            ASSERT_TRUE(subgrant.ok());
            EXPECT_EQ(subgrant->size(), 1u);
            EXPECT_EQ(grant->size(), 9u - count);
        }

        // 3b. expect failure because grant too small
        {
            batt::StatusOr<batt::Grant> subgrant = grant->spend(1, batt::WaitForResource::kFalse);
            EXPECT_FALSE(subgrant.ok());
            EXPECT_EQ(subgrant.status(), batt::StatusCode::kGrantUnavailable);
        }
    }
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

class AsyncGrantWaitSpendTest : public AsyncGrantWaitTest
{
   public:
    void SetUp() override
    {
        this->first_.emplace(this->issuer_.issue_grant(3, batt::WaitForResource::kTrue));
        ASSERT_TRUE(this->first_.ok());

        this->first_subgrant_.emplace(this->first_->spend(2, batt::WaitForResource::kTrue));
        ASSERT_TRUE(this->first_subgrant_.ok());

        this->start_blocking_operation([this] {
            this->second_subgrant_.emplace(this->first_->spend(2, batt::WaitForResource::kTrue));
        });

        EXPECT_FALSE(this->resume_blocking_operation());
    }

    batt::StatusOr<batt::Grant> first_subgrant_;
    batt::StatusOr<batt::Grant> second_subgrant_;
};

//  3. partially spend a grant, with subcases from (2), with d,e combined as described:
//       d. subsume a new grant from the issuer
//
TEST_F(AsyncGrantWaitSpendTest, ResolveBySubsumeNewGrant)
{
    batt::StatusOr<batt::Grant> extra = this->issuer_.issue_grant(1, batt::WaitForResource::kFalse);
    ASSERT_TRUE(extra.ok());

    batt::Grant& retval = this->first_->subsume(std::move(*extra));
    EXPECT_EQ(&retval, &*this->first_);

    EXPECT_TRUE(this->resume_blocking_operation());
    ASSERT_TRUE(this->second_subgrant_.ok());
    EXPECT_EQ(this->second_subgrant_->size(), 2u);
}

//  3. partially spend a grant, with subcases from (2), with d,e combined as described:
//       e. subsume a new grant from the issuer
//
TEST_F(AsyncGrantWaitSpendTest, ResolveBySubsumeFirstSubgrant)
{
    batt::Grant& retval = this->first_->subsume(std::move(*this->first_subgrant_));
    EXPECT_EQ(&retval, &*this->first_);

    EXPECT_TRUE(this->resume_blocking_operation());
    ASSERT_TRUE(this->second_subgrant_.ok());
    EXPECT_EQ(this->second_subgrant_->size(), 2u);
}

//  3. partially spend a grant, with subcases from (2), with d,e combined as described:
//     f. resolve by revoking the Grant
//
TEST_F(AsyncGrantWaitSpendTest, ResolveByRevoke)
{
    EXPECT_FALSE(this->first_->is_revoked());

    this->first_->revoke();
    EXPECT_TRUE(this->first_->is_revoked());

    EXPECT_TRUE(this->resume_blocking_operation());
    EXPECT_FALSE(this->second_subgrant_.ok());
    EXPECT_EQ(this->second_subgrant_.status(), batt::StatusCode::kClosed);
}

//  4. Grant move test
//
TEST(AsyncGrantTest, Move)
{
    batt::Grant::Issuer issuer{3};
    EXPECT_EQ(issuer.available(), 3u);
    {
        batt::StatusOr<batt::Grant> first = issuer.issue_grant(2, batt::WaitForResource::kFalse);
        ASSERT_TRUE(first.ok());
        EXPECT_EQ(issuer.available(), 1u);
        EXPECT_EQ(first->size(), 2u);
        EXPECT_TRUE(first->is_valid());
        EXPECT_EQ(first->get_issuer(), &issuer);
        EXPECT_FALSE(first->is_revoked());

        batt::Grant second = std::move(*first);
        EXPECT_EQ(issuer.available(), 1u);
        EXPECT_EQ(first->size(), 0u);
        EXPECT_EQ(second.size(), 2u);
        EXPECT_FALSE(first->is_valid());
        EXPECT_TRUE(second.is_valid());
        EXPECT_EQ(first->get_issuer(), nullptr);
        EXPECT_EQ(second.get_issuer(), &issuer);
        EXPECT_FALSE(first->is_revoked());
        EXPECT_FALSE(second.is_revoked());

        batt::StatusOr<batt::Grant> subgrant = first->spend(1, batt::WaitForResource::kFalse);
        EXPECT_FALSE(subgrant.ok());
        EXPECT_EQ(subgrant.status(), batt::StatusCode::kFailedPrecondition);
    }
    EXPECT_EQ(issuer.available(), 3u);
}

//  5. Grant swap test
//     a. same issuer
//
TEST(AsyncGrantTest, SwapSameIssuer)
{
    for (bool swap_order : {false, true}) {
        batt::Grant::Issuer issuer{3};
        EXPECT_EQ(issuer.available(), 3u);
        {
            batt::StatusOr<batt::Grant> first = issuer.issue_grant(2, batt::WaitForResource::kFalse);
            EXPECT_EQ(issuer.available(), 1u);
            {
                batt::StatusOr<batt::Grant> second = issuer.issue_grant(1, batt::WaitForResource::kFalse);
                EXPECT_EQ(issuer.available(), 0u);

                ASSERT_TRUE(first.ok());
                ASSERT_TRUE(second.ok());
                EXPECT_EQ(first->size(), 2u);
                EXPECT_EQ(second->size(), 1u);
                EXPECT_EQ(first->get_issuer(), &issuer);
                EXPECT_EQ(second->get_issuer(), &issuer);

                if (swap_order) {
                    first->swap(*second);
                } else {
                    second->swap(*first);
                }

                EXPECT_EQ(first->size(), 1u);
                EXPECT_EQ(second->size(), 2u);
                EXPECT_EQ(first->get_issuer(), &issuer);
                EXPECT_EQ(second->get_issuer(), &issuer);
                EXPECT_EQ(issuer.available(), 0u);
            }
            EXPECT_EQ(issuer.available(), 2u);
        }
        EXPECT_EQ(issuer.available(), 3u);
    }
}

//  5. Grant swap test
//     b. different issuer
//
TEST(AsyncGrantTest, DifferentIssuer)
{
    for (bool swap_order : {false, true}) {
        batt::Grant::Issuer issuer1{3};
        EXPECT_EQ(issuer1.available(), 3u);

        batt::Grant::Issuer issuer2{3};
        EXPECT_EQ(issuer2.available(), 3u);
        {
            batt::StatusOr<batt::Grant> first = issuer1.issue_grant(2, batt::WaitForResource::kFalse);
            EXPECT_EQ(issuer1.available(), 1u);
            EXPECT_EQ(issuer2.available(), 3u);
            {
                batt::StatusOr<batt::Grant> second = issuer2.issue_grant(1, batt::WaitForResource::kFalse);
                EXPECT_EQ(issuer1.available(), 1u);
                EXPECT_EQ(issuer2.available(), 2u);

                ASSERT_TRUE(first.ok());
                ASSERT_TRUE(second.ok());
                EXPECT_EQ(first->size(), 2u);
                EXPECT_EQ(second->size(), 1u);
                EXPECT_EQ(first->get_issuer(), &issuer1);
                EXPECT_EQ(second->get_issuer(), &issuer2);

                if (swap_order) {
                    first->swap(*second);
                } else {
                    second->swap(*first);
                }

                EXPECT_EQ(first->size(), 1u);
                EXPECT_EQ(second->size(), 2u);
                EXPECT_EQ(first->get_issuer(), &issuer2);
                EXPECT_EQ(second->get_issuer(), &issuer1);
                EXPECT_EQ(issuer1.available(), 1u);
                EXPECT_EQ(issuer2.available(), 2u);
            }
            EXPECT_EQ(issuer1.available(), 3u);
            EXPECT_EQ(issuer2.available(), 2u);
        }
        EXPECT_EQ(issuer1.available(), 3u);
        EXPECT_EQ(issuer2.available(), 3u);
    }
}

//  5. Grant swap test
//     c. one invalid
//
TEST(AsyncGrantTest, OneInvalid)
{
    for (bool swap_order : {false, true}) {
        batt::Grant::Issuer issuer{3};
        EXPECT_EQ(issuer.available(), 3u);
        {
            batt::StatusOr<batt::Grant> first = issuer.issue_grant(2, batt::WaitForResource::kFalse);
            EXPECT_EQ(issuer.available(), 1u);
            ASSERT_TRUE(first.ok());
            EXPECT_TRUE(first->is_valid());
            {
                batt::Grant second = std::move(*first);
                EXPECT_EQ(second.size(), 2u);
                EXPECT_FALSE(first->is_valid());
                EXPECT_TRUE(second.is_valid());
                {
                    batt::StatusOr<batt::Grant> third = issuer.issue_grant(1, batt::WaitForResource::kFalse);
                    ASSERT_TRUE(third.ok());
                    EXPECT_EQ(issuer.available(), 0u);

                    if (swap_order) {
                        first->swap(*third);
                    } else {
                        third->swap(*first);
                    }

                    EXPECT_FALSE(third->is_valid());
                    EXPECT_TRUE(first->is_valid());
                    EXPECT_EQ(first->size(), 1u);
                    EXPECT_EQ(issuer.available(), 0u);
                }
                EXPECT_EQ(issuer.available(), 0u);
            }
            EXPECT_EQ(issuer.available(), 2u);
        }
        EXPECT_EQ(issuer.available(), 3u);
    }
}

//  6. Grant spend_all
//
TEST(AsyncGrantTest, SpendAll)
{
    batt::Grant::Issuer issuer{10};
    EXPECT_EQ(issuer.available(), 10u);

    for (batt::WaitForResource wait_for_resource :
         {batt::WaitForResource::kFalse, batt::WaitForResource::kTrue}) {
        for (usize count = 0; count <= 10; ++count) {
            batt::StatusOr<batt::Grant> grant = issuer.issue_grant(count, wait_for_resource);
            EXPECT_EQ(issuer.available(), 10u - count);

            ASSERT_TRUE(grant.ok());
            EXPECT_EQ(grant->size(), count);

            const u64 spent_count = grant->spend_all();
            EXPECT_EQ(count, spent_count);
            EXPECT_EQ(issuer.available(), 10u);
        }
    }
}

//  7. subsume invalid grant
//
TEST(AsyncGrantTest, SubsumeInvalid)
{
    batt::Grant::Issuer issuer{10};
    batt::StatusOr<batt::Grant> first = issuer.issue_grant(3, batt::WaitForResource::kFalse);
    batt::StatusOr<batt::Grant> second = issuer.issue_grant(2, batt::WaitForResource::kFalse);

    ASSERT_TRUE(first.ok());
    ASSERT_TRUE(second.ok());

    batt::Grant first_copy = std::move(*first);

    EXPECT_EQ(second->size(), 2u);

    second->subsume(std::move(*first));
    EXPECT_EQ(second->size(), 2u);

    second->subsume(std::move(first_copy));
    EXPECT_EQ(second->size(), 5u);
}

// Death Tests:
//  10. Issuer destroyed with active Grant still alive
//
TEST(AsyncGrantTest, DeathIssuerDestroyedTooSoon)
{
    batt::Optional<batt::Grant::Issuer> issuer{10};
    batt::StatusOr<batt::Grant> grant = issuer->issue_grant(3, batt::WaitForResource::kFalse);
    ASSERT_TRUE(grant.ok());

    EXPECT_DEATH(issuer = batt::None, "[As]sert.*[Ff]ail.*This may indicate a Grant is still active");
}

// Death Tests:
//  11. subsume a grant from a different issuer
//      a. size={0,0}
//      b. size={0,1}
//      c. size={1,0}
//      d. size={1,1}
//
TEST(AsyncGrantTest, DeathSubsumeDifferentIssuer)
{
    batt::Grant::Issuer issuer1{3};
    batt::Grant::Issuer issuer2{3};

    for (usize grant1_size : {0, 1}) {
        for (usize grant2_size : {0, 1}) {
            batt::StatusOr<batt::Grant> grant1 =
                issuer1.issue_grant(grant1_size, batt::WaitForResource::kFalse);
            ASSERT_TRUE(grant1.ok());

            batt::StatusOr<batt::Grant> grant2 =
                issuer2.issue_grant(grant2_size, batt::WaitForResource::kFalse);
            ASSERT_TRUE(grant2.ok());

            EXPECT_DEATH(grant1->subsume(std::move(*grant2)), "[As]sert.*[Ff]ail.*issuer");
        }
    }
}

// Death Tests:
//  12. subsume into an invalid grant
//
TEST(AsyncGrantTest, DeathInvalidSubsumesValid)
{
    batt::Grant::Issuer issuer{10};
    batt::StatusOr<batt::Grant> first = issuer.issue_grant(3, batt::WaitForResource::kFalse);
    batt::StatusOr<batt::Grant> second = issuer.issue_grant(2, batt::WaitForResource::kFalse);

    ASSERT_TRUE(first.ok());
    ASSERT_TRUE(second.ok());

    batt::Grant first_copy = std::move(*first);

    EXPECT_EQ(second->size(), 2u);
    EXPECT_DEATH(first->subsume(std::move(*second)),
                 "[As]sert.*[Ff]ail.*It is NOT legal to subsume a Grant into an invalidated Grant.");
}

}  // namespace
