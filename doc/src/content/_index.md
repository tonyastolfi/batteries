---
title: "Overview"
type: "top"
menu: "main"
weight: 1
---
![](/images/BatteriesLogo.png)

Batteries is a Modern C++ library full of useful features to help make you more productive.

Some examples:

- [#include <batteries/segv.hpp>](/headers/segv) will install a signal handler that automatically prints a stack trace if your program crashes.
- [BATT_ASSERT](/headers/assert) and [BATT_CHECK](/headers/assert) are replacements for `assert` that allow you to stream-insert additional diagnostic information that is only evaluated/printed if the assertion fails: 
  ```c++
  #include <batteries/assert.hpp>
  #include <batteries/stream_util.hpp>
  
  #include <vector>
  #include <string>
  
  int main(int argc, char** argv) {
    BATT_ASSERT_LT(argc, 2) << "argc should have been less than 2!  argv=" 
        << batt::dump_range(std::vector<std::string>(argv, argv + argc), batt::Pretty::True);
    return 0;
  }
  ```
- {{< doxdefine file="utility.hpp" name="BATT_FORWARD" >}} implements "perfect" argument forwarding without having to write out the type of an expression every time:
  ```c++
  #include <batteries/utility.hpp>
  
  template <typename... Args>
  void call_foo(Args&&... args) 
  {
    // Nicer than having to write std::forward<Args>(args)...
    //
    foo(BATT_FORWARD(args)...);
  }
  ```
- {{< doxfn "batt::make_copy" >}} complements [std::move](https://en.cppreference.com/w/cpp/utility/move) by creating rvalues via copy (instead of move).
