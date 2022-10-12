# &lt;batteries/cpu\_align.hpp&gt;: Prevent false sharing by isolating a type in its own cache line(s).

[File Reference](/_autogen/Files/cpu__align_8hpp/)

## Features

- Class template [batt::CpuCacheLineIsolated](/_autogen/Classes/classbatt_1_1CpuCacheLineIsolated/#battcpucachelineisolated) for guaranteed cache-line isolation of an object.

## Details

Sometimes when objects modified by different threads are packed too
closely in memory, performance can degrade significantly due to a
"ping-pong" effect where each CPU/core that modifies an object in the
same cache line (the minimum-sized block of memory fetched from main
memory into cache) must wait to read changes made to the other objects
in that cache line.  This problem can be solved by padding the memory
into which the objects are placed to ensure each resides within its
own cache line.  `batt::CpuCacheLineIsolated<T>` makes this very
simple and easy to do.

```c++
// More compact, but slower due to false sharing between CPUs:
//
std::array<int, 100> slow_ints;

// Each int is padded and aligned so it gets its own cache line:
//
std::array<batt::CpuCacheLineIsolated<int>, 100> fast_ints;

// The underlying `T` (int in this case) is accessed via `*` and `->`
// operators, like a pointer or `std::optional`.
//
int& i_ref = *fast_int[0];
```

<!--more-->
