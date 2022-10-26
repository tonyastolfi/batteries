---
author: "Tony"
title: "<batteries/stream_util.hpp>"
tags: 
    - collections
    - diagnostics
    - string processing
---
Enables insertion of lambda expressions into `std::ostream`, provides {{< doxfn "batt::to_string" >}} and {{< doxfn "batt::from_string" >}} functions, C-string literal escaping ({{< doxfn "batt::c_str_literal" >}}), and dumping range values with and without pretty printing ({{< doxfn "batt::dump_range" >}}).

[File Reference](reference/files/stream__util_8hpp)
<!--more-->

## Inserting Lambdas into std::ostream

Example:

```c++
std::cout << "Here is some output: " << [](std::ostream& out) {
    for (int i=0; i<10; ++i) {
       out << i << ", ";
    }
  };
```

Output:

```
Here is some output: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
```
