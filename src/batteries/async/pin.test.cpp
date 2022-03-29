//+++++++++++-+-+--+----- --- -- -  -  -   -
// Case IV (see below)
//
static void pin_object(int* i)
{
    ++*i;
}

static void unpin_object(int* i)
{
    --*i;
}

#include <batteries/async/pin.hpp>
//
#include <batteries/async/pin.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

// Test Plan:
//  For each of:
//   (I) batt::Pinnable
//   (II) User-defined type that derives from batt::Pinnable
//   (III) User-defined type that implements namespace-local pin_object and unpin_object functions
//   (IV) Built-in type that implements pin_object and unpin_object functions
//  (for each):
//
//  1. (CreateDestroy) Create/Destroy (no pins acquired or released) - DONE
//  2. (AcquireRelease) Acquire a pin and verify the pointer is correct - DONE
//  3. Launch Tasks each of which holds a Pin to the object - verify the object isn't destroyed until the last
//     pin is released.
//    a. 1 Task (+the owner task)
//    b. 10 Tasks (+the owner)
//  4. Launch background Task that tries to destroy the pinnable object
//    a. With no prior pins - exits immediately
//    b. With one prior pin - exits once the pin is released
//    c. With many prior pins - swap and move them around - exits once the last pin is released, not before
//       - Release by all these mechanisms:
//         i. assign to an empty (default-constructed) Pin
//         ii. call `release()` explicitly
//         iii. Destroy the Pin<T> object.
//

namespace foo {

//+++++++++++-+-+--+----- --- -- -  -  -   -
// Case II
//
class InternalPinCount : public batt::Pinnable
{
};

//+++++++++++-+-+--+----- --- -- -  -  -   -
// Case III
//
class ExternalPinCount
{
   public:
    int my_count = 1;
};

void pin_object(ExternalPinCount* obj)
{
    obj->my_count += 1;
}

void unpin_object(ExternalPinCount* obj)
{
    obj->my_count -= 1;
}

}  // namespace foo
}  // namespace

namespace {
//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

TEST(AsyncPinTest, CreateDestroyI)
{
    batt::Pinnable obj;
    (void)obj;
}

TEST(AsyncPinTest, CreateDestroyII)
{
    foo::InternalPinCount obj;
    (void)obj;
}

TEST(AsyncPinTest, CreateDestroyIII)
{
    foo::ExternalPinCount obj;
    (void)obj;
}

TEST(AsyncPinTest, AcquireReleaseI)
{
    batt::Pinnable obj;
    {
        batt::Pin<batt::Pinnable> pin = batt::make_pin(&obj);

        EXPECT_TRUE(pin);
        EXPECT_EQ(&*pin, &obj);
        EXPECT_EQ(pin.get(), &obj);
    }
}

TEST(AsyncPinTest, AcquireReleaseII)
{
    foo::InternalPinCount obj;
    batt::Pin<foo::InternalPinCount> pin = batt::make_pin(&obj);

    EXPECT_TRUE(pin);
}

TEST(AsyncPinTest, AcquireReleaseIII)
{
    foo::ExternalPinCount obj;
    {
        batt::Pin<foo::ExternalPinCount> pin = batt::make_pin(&obj);

        EXPECT_TRUE(pin);
        EXPECT_EQ(pin->my_count, 2);
        EXPECT_EQ(&*pin, &obj);
        EXPECT_EQ(pin.get(), &obj);
    }
    EXPECT_EQ(obj.my_count, 1);
}

TEST(AsyncPinTest, AcquireReleaseIV)
{
    int obj = 1;
    {
        batt::Pin<int> pin = batt::make_pin(&obj);

        EXPECT_TRUE(pin);
        EXPECT_EQ(obj, 2);
        EXPECT_EQ(&*pin, &obj);
        EXPECT_EQ(pin.get(), &obj);
    }
    EXPECT_EQ(obj, 1);
}

}  // namespace
