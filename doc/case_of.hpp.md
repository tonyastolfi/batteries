# &lt;batteries/case_of.hpp&gt;: `switch`-like handling for `std::variant`

`batt::case_of` is a replacement for `std::visit`.

Example:

```c++
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
```
## `batt::case_of`

The form of `batt::case_of` is:

`batt::case_of(` _variant-object_ `, ` _handler-for-case-1_ `, ` _handler-for-case-2_ `, ...);`

The "handlers" can be any callable type that takes one argument whose
type matches one of the types in the variant passed as
_variant-object_.  The handlers do not have to return a value, but if
any of them do, they must all return the same type.  The handler that
will be invoked is the first one (from left to right) that is
invokable for a given variant case type.  This behavior allows you to
write things like:

```c++
std::variant<std::logic_error, std::runtime_error, std::string, int, double> v;

batt::case_of(
  v, 
  [](const std::exception& e) {
    // common code for all types deriving from std::exception
  },
  [](int i) {
    // a special case for integers
  },
  [](const auto& value) {
    // A default case for everything else
  });
```

## `batt::make_case_of_visitor`

You can construct an overloaded functor from a list of functions with
the same number of arguments using `batt::make_cose_of_visitor`.  This
functor behaves similarly to `batt::case_of` (and in fact is used to
implement `case_of`): the function that actually gets invoked on a
particular set of arguments is the first one in the list whose
paramaeters will bind to those arguments.

Example:

```c++
auto do_stuff = batt::make_case_of_visitor(
    [](int) {
      std::cout << "one int" << std::endl;
    },
    [](int, int) {
      std::cout << "two ints" << std::endl;
    },
    [](auto&&...) {
      std::cout << "something else!" << std::endl;
    });
    
do_stuff(5);
do_stuff(9, 4);
do_stuff();
do_stuff("hello, world!");
```

Output:

```shell
one int
two ints
something else!
something else!
```
