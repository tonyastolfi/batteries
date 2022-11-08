# &lt;batteries/seq/...&gt;: Fast, Ergonomic Sequence Processing

The Batteries Seq abstraction builds on top of STL iterator ranges, seeking to offer more readable and maintainable code without sacrificing efficiency.

## Intro Example

```c++
#include <batteries/seq.hpp>

#include <iostream>
#include <utility>
#include <vector>

int main() {
  std::vector<int> nums = {2, 1, -3, 4, 5, -2};
  
  int answer = batt::as_seq(nums)

         // Take only non-negative items.
         //
         | batt::seq::filter([](int n) {
             return n >= 0;
           })

         // Calculate a running total plus count.
         //
         | batt::seq::reduce(
             // Initial state ({total, count}):
             //
             std::make_pair(0, 0),

             // Reduce function:
             //
             [](std::pair<int, int> totals, int n) {
               return std::make_pair(totals.first + n, totals.second + 1);
             })

         // Divide total by count to get the final answer.
         //
         | batt::seq::apply([](std::pair<int, int> final_totals) {
             return final_totals.first / final_totals.second;
           });
  
  std::cout << "The average value of non-negative elements is: " 
            << answer << std::endl;

  return 0;
}
```

Output:

```
The average value of non-negative elements is: 3
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

Sequences are composed by applying one or more "operators" to transform the sequence or aggregate its values.  A given sequence operator `#!cpp Op()` is applied to an input sequence `#!cpp seq` by using the expression: `#!cpp seq | Op()`.  The resulting expression is usually another sequence, but it may be a single value (e.g., [count()][function-count], [collect()][function-collect], etc.).  The set of operators defined by batteries is shown the table below.  It is possible for your application code to define additional operators by composing the ones included in Batteries, or by following the conventions used to define the included operators.

## Sequence Operator Reference

_** NOTE: ** All of the functions below are defined in the namespace `batt::seq::`, which has been omitted for brevity._

| Name | Description |
|:-----|:------------|
| <nobr>[all_true()][function-all_true]</nobr> | Returns `#!cpp true` iff all items in the input sequence are `#!cpp true` when evaluated in a boolean context.  This operator supports short-circuit; i.e., it stops as soon as it encounters the first `#!cpp false`-valued item.<br><br>`#!cpp #include <batteries/seq/all_true.hpp>` |
| <nobr>[any_true()][function-any_true]</nobr> | Returns `#!cpp true` iff any items in the input sequence are `#!cpp true` when evaluated in a boolean context.  This operator supports short-circuit; i.e., it stops as soon as it encounters the first `#!cpp true`-valued item.<br><br>`#!cpp #include <batteries/seq/any_true.hpp>` |
|<nobr>[apply(Fn)][function-apply]</nobr> | Applies the passed function to the _entire_ sequence as a single value, returning the result.  Not to be confused with [map(Fn)][function-map] which applies a function to each item in the sequence to produce a new sequence.<br><br>`#!cpp #include <batteries/seq/apply.hpp>` |
|<nobr>[attach(Data&& data)][function-attach]</nobr> | Produces a sequence identical to the input except that a copy of `#!cpp data` is stored in the sequence object.  Useful for extending the lifetimes of objects that a sequence may depend on, but doesn't explicitly capture.<br><br>`#!cpp #include <batteries/seq/attach.hpp>` |
|<nobr>[boxed()][function-boxed]</nobr> | Transforms a templated sequence type to a type-erased container of concrete type `#!cpp batt::BoxedSeq<T>`, hiding the details of how the sequence was composed.<br><br>`#!cpp #include <batteries/seq/boxed.hpp>` |
| <nobr>[cache_next()][function-cache_next]</nobr> | Lazily caches a copy of the next item in the input sequence, to speed up repeated calls to `peek()`.<br><br>`#!cpp #include <batteries/seq/cache_next.hpp>` |
| <nobr>[chain(seq2)][function-chain]</nobr> | Concatenates `seq2` onto the end of the input sequence.<br><br>`#!cpp #include <batteries/seq/chain.hpp>` |
| <nobr>[collect(batt::StaticType&lt;T&gt;)][function-collect]</nobr> | Collects all the items in the sequence by repeatedly calling `T::emplace_back` on a default-constructed instance of container type `T`. <br><br>`#!cpp #include <batteries/seq/collect.hpp>`|
| <nobr>[collect_vec()][function-collect_vec]</nobr> | Collects all the items in the input sequence into a `#!cpp std::vector<T>`, where `T` is the declared Item type of the sequence.<br><br>`#!cpp #include <batteries/seq/collect_vec.hpp>` |
| <nobr>[consume()][function-consume]</nobr> | Consumes the entire input sequence, throwing away all values and returning `void`.<br><br>`#!cpp #include <batteries/seq/consume.hpp>` |
| <nobr>[count()][function-count]</nobr> | Returns the number of items in the sequence, consuming all items in the process.<br><br>`#!cpp #include <batteries/seq/count.hpp>` |
| <nobr>[debug_out(out, sep=" ")][function-debug_out]</nobr> | Prints all items in the input sequence to the passed `#!cpp std::ostream` as a side-effect, leaving the input otherwise unmodified.<br><br>`#!cpp #include <batteries/seq/print_out.hpp>` |
| <nobr>[decayed()][function-decayed]</nobr> | Transforms a sequence comprised of value references into an equivalent sequence of value-copies, similar to `#!cpp std::decay<T>`.<br><br>`#!cpp #include <batteries/seq/decay.hpp>` |
| <nobr>[deref()][function-deref]</nobr> | Transforms a sequence by dereferencing all items, i.e. `#!cpp item` becomes `#!cpp *item`.  The Item type may be anything which defines `#!cpp operator*`, including all built-in pointers, smart pointers, optionals, iterators, etc.<br><br>`#!cpp #include <batteries/seq/deref.hpp>` |
| <nobr>[emplace_back(T& container)][function-emplace_back]</nobr> | Invokes `#!cpp container.emplace_back(item)` for each item in the input sequence, fully consuming the sequence.<br><br>`#!cpp #include <batteries/seq/emplace_back.hpp>` |
| <nobr>[filter(PredFn)][function-filter]</nobr> | Removes any items from the input sequence for which the passed `#!cpp PredFn` returns `#!cpp false`, producing a filtered sequence.<br><br>`#!cpp #include <batteries/seq/filter.hpp>` |
| <nobr>[filter_map(Fn)][function-filter_map]</nobr> | Combines the effect of [map][function-map] and [filter][function-filter] by invoking the passed `#!cpp Fn` on each item of the input sequence, producing a temporary sequence of `#!cpp Optional<Item>` values, from which all `#!cpp None` values are removed. <br><br>`#!cpp #include <batteries/seq/filter_map.hpp>` |
| <nobr>[first()][function-first]</nobr> | Returns the first item in the sequence; equivalent to calling `#!cpp seq.next()`.<br><br>`#!cpp #include <batteries/seq/first.hpp>` |
| <nobr>[flatten()][function-flatten]</nobr> | Transforms a sequence of sequences into a flat sequence of the nested items by concatenating all the sub-sequences together.<br><br>`#!cpp #include <batteries/seq/flatten.hpp>` |
| <nobr>[for_each(Fn)][function-for_each]</nobr> | Invokes the passed function on each item of the sequence, consuming the input.  `#!cpp Fn` may return type `#!cpp void` or value of type `#!cpp batt::seq::LoopControl` (i.e., `#!cpp LoopControl::kContinue` or `#!cpp LoopControl::kBreak`) to control how much of the input sequence to consume, similar to the C++ keywords `#!cpp continue` and `#!cpp break` within a built-in loop.<br><br>`#!cpp #include <batteries/seq/for_each.hpp>` |
| <nobr>[fuse()][function-fuse]</nobr> | Transforms a sequence with item type `#!cpp Optional<T>` into a sequence of `#!cpp T`, terminating the sequence when the first `#!cpp None` value is encountered.<br><br>`#!cpp #include <batteries/seq/fuse.hpp>` |
| <nobr>[group_by(BinaryPredFn)][function-group_by]</nobr> | Transforms a sequence into a sequence of sequences by invoking `BinaryPredFn` on each pair of adjacent items in the input, creating a split wherever `BinaryPredFn` returns false.<br><br>`#!cpp #include <batteries/seq/group_by.hpp>` |
| <nobr>[inner_reduce(Fn)][function-inner_reduce]</nobr> | Given input sequence `#!cpp xs = x0, x1, x2, x3, ..., xN`, the expression `#!cpp xs | inner_reduce(fn)` produces the return value of the expression `#!cpp fn(...fn(fn(fn(x0, x1), x2, x3)..., xN)`.<br><br>`#!cpp #include <batteries/seq/inner_reduce.hpp>` |
| <nobr>[inspect(Fn)][function-inspect]</nobr> | Calls the given `#!cpp Fn` on each item of the input sequence, producing a sequence equivalent to the original input.  This differs from [for_each][function-for_each], which also invokes a function for each input item, but does not produce a sequence.<br><br>`#!cpp #include <batteries/seq/inspect.hpp>` |
| <nobr>[inspect_adjacent(BinaryFn)][function-inspect_adjacent]</nobr> | Calls the given `#!cpp BinaryFn` on each sequentially adjacent pair of items of the input, producing a sequence equivalent to the original sequence (similar to [inspect][function-inspect]).<br><br>`#!cpp #include <batteries/seq/inspect_adjacent.hpp>` |
| <nobr>[is_sorted()][function-is_sorted]</nobr> | Returns `#!cpp true` iff the input sequence is sorted.  The item type of the input sequence must support `#!cpp operator<`.<br><br>`#!cpp #include <batteries/seq/is_sorted.hpp>` |
| <nobr>[is_sorted_by(OrderFn)][function-is_sorted_by]</nobr> | Returns `#!cpp true` iff the input sequence is sorted according to the passed `#!cpp OrderFn`, which should behave like the less-than operator.<br><br>`#!cpp #include <batteries/seq/is_sorted.hpp>` |
| <nobr>[kmerge()][function-kmerge]</nobr> | Performs a k-way merge of a sequence of sorted sequences, producing a single sorted sequence.  The built in comparison operators (e.g. `#!cpp <`, `#!cpp <=`, `#!cpp >`, etc.) are used to define the ordering of nested items.  Behavior is unspecified if the input sequences are not sorted!  The input sequences must be copyable, with each copy able to iterate through independent of the others.<br><br>`#!cpp #include <batteries/seq/kmerge.hpp>` |
| <nobr>[kmerge_by(OrderFn)][function-kmerge_by]</nobr> | Like [kmerge()][function-kmerge], except the ordering is defined by the passed binary predicate `OrderFn`, which should behave like the less-than operator (`#!cpp operator<`).<br><br>`#!cpp #include <batteries/seq/kmerge.hpp>` |
| <nobr>[last()][function-last]</nobr> | Returns the final item in the sequence if the input is non-empty, else `#!cpp batt::None`.<br><br>`#!cpp #include <batteries/seq/last.hpp>` |
| <nobr>[map(Fn)][function-map]</nobr> | Transforms the input sequence by applying the passed `Fn` to each item in the input to produce the output.<br><br>`#!cpp #include <batteries/seq/map.hpp>` |
| <nobr>[map_adjacent(Fn)][function-map_adjacent]</nobr> | Transforms `#!cpp x0, x1, x2, x3, ...` into `#!cpp fn(x0, x1), fn(x1, x2), fn(x2, x3), ...`<br><br>`#!cpp #include <batteries/seq/map_adjacent.hpp>` |
| <nobr>[map_fold(state0, fn)][function-map_fold]</nobr> | Similar to `#!cpp map(Fn: auto (T) -> U)`, except that the `fn` takes an additional first argument, the `state`, and returns a `#!cpp std::tuple<State, U>`.  This produces both a sequence of output values `#!cpp u0, u1, u2, ...` and also a series of state values, `#!cpp state1, state2, state3, ...`, with each item's state output passed into the next item's call to `fn`.  This operator is preferable to using `map` with an `Fn` that uses mutable state in its implementation.<br><br>`#!cpp #include <batteries/seq/map_fold.hpp>` |
| <nobr>[map_pairwise(SeqB, Fn)][function-map]</nobr> | Given two sequences `#!cpp seqA = a0, a1, a2, ...` and `#!cpp seqB = b0, b1, b2, ...`, the expression `#!cpp seqA | map_pairwise(seqB, fn)` produces a sequence with items `#!cpp fn(a0, b0), fn(a1, b1), fn(a2, b2), ...`<br><br>`#!cpp #include <batteries/seq/map_pairwise.hpp>` |
| <nobr>[prepend(seq0)][function-prepend]</nobr> | Forms a new sequence by concatenating the input sequence onto the end of `seq0`; this is the opposite of [chain][function-chain].<br><br>`#!cpp #include <batteries/seq/prepend.hpp>` |
| <nobr>[print_out(out, sep=" ")][function-print_out]</nobr> | Consumes all items in the input stream, printing them to the passed `#!cpp std::ostream out`, inserting the passed separator string `sep` in between each item.<br><br>`#!cpp #include <batteries/seq/print_out.hpp>` |
| <nobr>[printable()][function-printable]</nobr> | Turns the input sequence into a value that can be `#!cpp <<`-inserted to any `#!cpp std::ostream` to print out all items.<br><br>`#!cpp #include <batteries/seq/printable.hpp>` |
| <nobr>[product()][function-sum]</nobr> | Returns the arithmetic product of the items in the input sequence.  The item type `T` of the input sequence must support built-in multiplication (`#!cpp operator*`), and `T{1}` must be the multiplicative identity value of `T` (i.e., "one").<br><br>`#!cpp #include <batteries/seq/sum.hpp>` |
| <nobr>[reduce(state, fn)][function-reduce]</nobr> | Transforms a sequence of items `#!cpp item0, item1, item2, ..., itemN` into the result of calling: `#!cpp fn(...fn(fn(fn(state, item0), item1), item2)..., itemN)`; this operation is sometimes called _left-fold_ or _foldl_.<br><br>`#!cpp #include <batteries/seq/reduce.hpp>` |
| <nobr>[rolling(Fn, init = {})][function-rolling]</nobr> | Like [reduce][function-reduce], but instead of evaluating to the single final value, produces a sequence starting with `#!cpp init` followed by all intermediate values calculated by `#!cpp Fn` (Note: this means [rolling][function-rolling] produces a sequence one larger than the input sequence).  The type of the `#!cpp init` param should be the Item type `T` of the sequence; `#!cpp Fn` should have the signature: `#!cpp auto (T, T) -> T`.<br><br>`#!cpp #include <batteries/seq/rolling.hpp>` |
| <nobr>[rolling_sum(Fn)][function-rolling_sum]</nobr> | Produces a sequence that begins with a default-initialized item (i.e. "zero"), followed by a running total of all the items up to that point in the input.<br><br>`#!cpp #include <batteries/seq/rolling_sum.hpp>` |
| <nobr>[skip_n(n)][function-skip_n]</nobr> | Produces a new sequence that doesn't include the first `n` items of the input sequence.  This is the complement of [take_n(n)][function-take_n].<br><br>`#!cpp #include <batteries/seq/skip_n.hpp>` |
| <nobr>[splice(n, InnerSeq)][function-splice]</nobr> | Inserts the items of `#!cpp InnerSeq` into the input sequence at position `#!cpp n`.  This is a more general version of [chain][function-chain] and [prepend][function-prepend].<br><br>`#!cpp #include <batteries/seq/splice.hpp>` |
| <nobr>[status_ok()][function-status_ok]</nobr> | Transforms a sequence `a` with item type `#!cpp batt::StatusOr<T>` into a sequence `b` with item type `#!cpp T` by unwrapping each input item until one with non-ok status is encountered, at which point `b` is terminated.  If the `b` ends because of a non-ok item from `a`, then `b.status()` may be used to retrieve the error status value. <br><br>`#!cpp #include <batteries/seq/status_ok.hpp>` |
| <nobr>[sum()][function-sum]</nobr> | Returns the arithmetic sum of the items in the input sequence.  The item type `T` of the input sequence must support built-in addition (`#!cpp operator+`), and `T{}` must be the additive identity value of `T` (i.e., "zero").<br><br>`#!cpp #include <batteries/seq/sum.hpp>` |
| <nobr>[take_n(n)][function-take_n]</nobr> | Produces a new sequence comprised of only the first `n` items of the input sequence.  This is the complement of [skip_n(n)][function-skip_n].<br><br>`#!cpp #include <batteries/seq/take_n.hpp>` |
| <nobr>[take_while(PredFn)][function-take_while]</nobr> | Produces a sequence with the same items as the input up until the passed `PredFn` returns `#!cpp false`, at which point the sequence is terminated.<br><br>`#!cpp #include <batteries/seq/take_while.hpp>` |

## Other Functions/Classes

- `Addition`
- `Empty<T>`
- `lazy`
- `NaturalOrder`
- `NaturalEqual`
- `single_item`
