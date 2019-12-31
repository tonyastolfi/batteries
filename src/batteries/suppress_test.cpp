#include <batteries/suppress.hpp>
//
#include <batteries/suppress.hpp>

namespace {

BATT_SUPPRESS("-Wreturn-type")
BATT_SUPPRESS("-Wunused-function")

int
foo()
{}

BATT_UNSUPPRESS()
BATT_UNSUPPRESS()

} // namespace
