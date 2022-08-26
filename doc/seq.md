# &lt;batteries/seq/...&gt;: Fast, Ergonomic Sequence Processing

The Batteries Seq abstraction builds on top of STL iterator ranges, seeking to offer more readable and maintainable code without sacrificing efficiency.

## Intro Example

```c++
#include <batteries/seq.hpp>

#include <iostream>
#include <vector>

int main() {
  std::vector<int> nums = {1, 2, 3, 4, 5};
  
  int sum_of_lens_under_5 = batt::as_seq(nums) 
    | batt::seq::map([](int n) {
      return batt::to_string(n);
    })
    | batt::seq::filter([](const std::string& s) {
      return s.length() < 5;
    })
    | batt::seq::map([](const std::string& s) {
      return s.length();
    }) 
    | batt::seq::sum();
    
  std::cout << "The total length of strings under 5 is: " << (3 + 3 + 4 + 4) << std::endl;

  return 0;
}
```

Output:

```
The total length of strings under 5 is: 14
```

## Seq&lt;T&gt; Concept

`Seq<T>` is a concept that represents an ordered sequence of objects of type `T`.  The class [batt::BoxedSeq<T>](/) is a type-erased container for an object that models this concept.  The type requirements/interface of `batt::BoxedSeq<T>` are:

 - `Seq<T>` must be copy-constructible, copy-assignable, and publically destructible
 - `Seq<T>` must have a public type member or type alias/typedef named `Item`, equivalent to `T`
 - Given an object `seq` of type `Seq<T>`:
   - `seq.peek()` must return a value of type `batt::Optional<T>`; if the sequence is empty or at its end, `batt::None` is returned, otherwise the next item in the sequence is returned
   - `seq.next()` is the same in terms of returned value, but it additionally has the side effect of consuming the returned item from the sequence

## STL Ranges to Sequences

The function `batt::as_seq` is used to create a `Seq` from STL containers and ranges in order to access the rest of the Seq API.  You can pass a range, a pair of iterators, or a starting iterator and a size to overloads of `batt::as_seq` to do this.

## Composing Sequences
