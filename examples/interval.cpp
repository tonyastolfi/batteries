#include <batteries/assert.hpp>
#include <batteries/interval.hpp>

int main()
{
    batt::Interval<int> i{3, 7};

    BATT_CHECK_EQ(i.size(), 4);
    BATT_CHECK(i.contains(3));
    BATT_CHECK(i.contains(6));
    BATT_CHECK(!i.contains(2));
    BATT_CHECK(!i.contains(7));
    BATT_CHECK((batt::Interval<int>{5, 5}).empty());

    return 0;
}
