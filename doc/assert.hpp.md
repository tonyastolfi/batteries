# &lt;batteries/assert.hpp&gt; : Fatal error check macros

[File Reference](../_autogen/Files/assert_8hpp/)

This header includes enhanced drop-in replacements for standard `assert()` statements.  All the supported assertion types have a version (`BATT_CHECK*`) which is always on, even in optimized/release builds, and a version (`BATT_ASSERT*`) that is automatically stripped out of non-Debug builds.

**NOTE:** Batteries assumes the build type is Release/Optimized if the macro `NDEBUG` is defined; in this case, all `BATT_ASSERT*` statements will be stripped out of the compilation. 

## Advantages

### Informative Messages

Using a more descriptive assertion macro allows your program to print a more informative error message if an assertion does fail.  For example, you might use the statement:

```c++
assert(x == 1);
```
But if this assertion fails, all you know is that x was not equal to 1.  What was it equal to?!  Batteries will answer this question automatically if you write:

```c++
BATT_ASSERT_EQ(x, 1);
```
  
You don't have to worry about making sure that the types you're comparing support `std::ostream` output to take advantage of this feature; Batteries will automatically do its best to print out something that might be useful, regardless of type.

If you want to take advantage of this feature explicitly (when writing some arbitrary type to a stream), you can use:

```c++
#include <batteries/assert.hpp>

struct MyCustomType {
  int x;
  int y;
};

int main() {
  MyCustomType x;
  
  // Works even though MyCustomType has no ostream operator<<!
  //
  std::cout << batt::make_printable(x);
  
  return 0;
}
```

### Stack Traces

Full stack traces, with source symbols if available, are automatically printed whenever an assertion failure happens.

### Always On Checks

`BATT_CHECK_*` allows you to write assertions that are guaranteed never to be compiled out of your program, even in optimized/release builds.
- All `BATT_CHECK_*`/`BATT_ASSERT_*` statements support `operator<<` like `std::ostream` objects, so that you can add more contextual information to help diagnose an assertion failure.  Example:

```c++
int y = get_y();
int z = get_z();
int x = (y + z) / 2;
BATT_ASSERT_EQ(x, 1) 
    << "y = " << y << ", z = " << z 
    << " (expected the average of y and z to be 1)";
```

### Low-Overhead

Diagnostic output expressions added via `<<` are never evaluated unless the assertion actually fails, so don't worry if they are somewhat expensive.

### Prevent Code Rot

Even on Release builds, all expressions that appear in a `BATT_ASSERT_*` statement _will_ be compiled, so you don't have to worry about breaking Debug builds when compiling primarily using optimization.

## Quick Reference

### Logical Assertions

| Debug-only                   | Always Enabled              | Description                      |
| :--------------------------- | :-------------------------- | :------------------------------- |
| [BATT_ASSERT(cond)][define-batt_assert] | [BATT_CHECK(cond)][define-batt_check] | Assert that `bool{cond} == true` |
| [BATT_ASSERT_IMPLES(p, q)][define-batt_assert_implies] | [BATT_CHECK_IMPLIES(p, q)][define-batt_check_implies] | Assert that if `(p)` is true, then so is `(q)` (i.e., `(!(p) || (q))`)|
| [BATT_ASSERT_NOT_NULLPTR(x)][define-batt_assert_not_nullptr] | [BATT_CHECK_NOT_NULLPTR(x)][define-batt_check_not_nullptr] | Assert that `(x) != nullptr` |

### Comparison Assertions

| Debug-only             | Always Enabled            | Description              |
| :--------------------- | :------------------------ | :----------------------- |
| [BATT_ASSERT_EQ(a, b)][define-batt_assert_eq] | [BATT_CHECK_EQ(a, b)][define-batt_check_eq] | Assert that `(a) == (b)` |
| [BATT_ASSERT_NE(a, b)][define-batt_assert_ne] | [BATT_CHECK_NE(a, b)][define-batt_check_ne] | Assert that `(a) != (b)` |
| [BATT_ASSERT_LT(a, b)][define-batt_assert_lt] | [BATT_CHECK_LT(a, b)][define-batt_check_lt] | Assert that `(a) < (b)`  |
| [BATT_ASSERT_GT(a, b)][define-batt_assert_gt] | [BATT_CHECK_GT(a, b)][define-batt_check_gt] | Assert that `(a) > (b)`  |
| [BATT_ASSERT_LE(a, b)][define-batt_assert_le] | [BATT_CHECK_LE(a, b)][define-batt_check_le] | Assert that `(a) <= (b)` |
| [BATT_ASSERT_GE(a, b)][define-batt_assert_ge] | [BATT_CHECK_GE(a, b)][define-batt_check_ge] | Assert that `(a) >= (b)` |

### Other/Advanced

| Name | Description |
| :--- | :---------- |
| [BATT_PANIC()][define-batt_panic]| Forces the program to exit immediately, printing a full stack trace and any message `<<`-inserted to the `BATT_PANIC()` statement. <br>Example:<br>`BATT_PANIC() << "Something has gone horribly wrong!  x = " << x;`  |
| [BATT_UNREACHABLE()][define-batt_unreachable]| Statement that tells the compiler this point in the code should be unreachable; for example, it is right after a call to `std::abort()` or `std::terminate()`.  Use this to silence spurious warnings about dead code. |
| [BATT_NORETURN][define-batt_noreturn]| When added to a function declaration (before the return type), tells the compiler that a function never returns.  Use this to silence spurious warnings. <br>Example:<br>  `BATT_NORETURN void print_stuff_and_exit();`  |
| [batt::make_printable(obj)][function-make_printable]| Makes any expression printable, even if it doesn't have an overloaded  `std::ostream& operator<<(std::ostream&, T)` .  If the type of `obj` does define such an operator, however, that will be invoked when using `batt::make_printable`.  `obj` is passed/forwarded by reference only; no copy of the original object/value is made. |
