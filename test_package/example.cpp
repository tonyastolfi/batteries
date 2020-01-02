#include <iostream>
#include <vector>

#include <batteries.hpp>

int
main()
{
    // Smoke test for <batteries/assert.hpp>
    //
    BATT_CHECK_EQ(1 + 1, 2);

    // Smoke test for <batteries/hint.hpp>
    //
    if (BATT_HINT_TRUE(2 * 2 == 4)) {
        std::cout << "Math is working." << std::endl;
    }

    // Smoke test for <batteries/int_types.hpp>
    //
    using namespace batt::int_types;

    u8 x_8 = 0xff;
    u16 x_16 = 0xffff;
    u32 x_32 = 0xffffffff;
    (void)x_8;
    (void)x_16;
    (void)x_32;

    // Smoke test for <batteries/int_types.hpp>
    //
    BATT_CHECK(batt::kSigSegvHandlerInstalled);

    // Smoke test for <batteries/stream_util.hpp>
    //
    std::vector<int> v;
    std::cout << batt::dump_range(v) << std::endl;

    // Smoke test for <batteries/suppress.hpp>
    //
    BATT_SUPPRESS("-Wunused-variable")
    int not_used = 4;
    BATT_UNSUPPRESS()

    // Smoke test for <batteries/type_traits.hpp>
    //
    static_assert(batt::IsRange<decltype(v)>{}, "std::vector is a range type!");

    // Smoke test for <batteries/utility.hpp>
    //
    std::vector<int> v_copy = batt::make_copy(v);

    return 0;
}
