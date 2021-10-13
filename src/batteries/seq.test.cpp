#include <batteries/seq.hpp>
//
#include <batteries/seq.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/range/irange.hpp>

#include <array>
#include <random>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using batt::as_seq;
using batt::make_optional;
using batt::None;
namespace seq = batt::seq;

template <typename T>
std::decay_t<T> force_copy(T&& src)
{
    std::array<char, sizeof(std::decay_t<T>)> buffer1;
    std::array<char, sizeof(std::decay_t<T>)> buffer2;

    auto* tmp1 = new (buffer1.data()) std::decay_t<T>(BATT_FORWARD(src));
    auto* tmp2 = new (buffer2.data()) std::decay_t<T>(std::move(*tmp1));

    tmp1->~T();
    buffer1.fill(0xff);

    std::decay_t<T> dst = *tmp2;

    tmp2->~T();
    buffer2.fill(0xff);

    return dst;
}

TEST(SeqTest, CollectVecTest)
{
    EXPECT_EQ((std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8}), as_seq(boost::irange(1, 9)) | seq::collect_vec());
}

TEST(SeqTest, TakeWhileTest)
{
    EXPECT_EQ((std::vector<int>{1, 2, 3}), as_seq(boost::irange(1, 9))  //
                                               | seq::take_while([](int i) {
                                                     return i < 4;
                                                 })  //
                                               | seq::collect_vec());

    EXPECT_EQ((std::vector<int>{}), as_seq(boost::irange(1, 9))  //
                                        | seq::take_while([](int i) {
                                              return i % 2 == 0;
                                          })  //
                                        | seq::collect_vec());

    EXPECT_EQ((std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8}), as_seq(boost::irange(1, 9))  //
                                                              | seq::take_while([](int i) {
                                                                    return i == i;
                                                                })  //
                                                              | seq::collect_vec());
}

TEST(SeqTest, FilterTest)
{
    EXPECT_EQ((std::vector<int>{1, 5, 7}), as_seq(boost::irange(0, 10))  //
                                               | seq::filter([](int i) {
                                                     return i % 2 != 0;
                                                 })  //
                                               | seq::filter([](int i) {
                                                     return i % 3 != 0;
                                                 })  //
                                               | seq::collect_vec());
}

TEST(SeqTest, MapTest)
{
    EXPECT_EQ((std::vector<int>{0, -2, -4, -6}), as_seq(boost::irange(0, 4))  //
                                                     | seq::map([](int i) {
                                                           return i * -2;
                                                       })  //
                                                     | seq::collect_vec());

    auto map_fn = [](int i) {
        return i * -2;
    };
    auto nums = as_seq(boost::irange(0, 4));

    EXPECT_EQ((std::vector<int>{0, -2, -4, -6}),
              batt::make_copy(nums)                    //
                  | seq::map(batt::make_copy(map_fn))  //
                  | seq::collect_vec());
}

TEST(SeqTest, ConcatTest)
{
    EXPECT_EQ((std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7}), as_seq(boost::irange(0, 3))                    //
                                                              | seq::chain(as_seq(boost::irange(3, 7)))  //
                                                              | seq::chain(as_seq(boost::irange(7, 8)))  //
                                                              | seq::collect_vec());

    EXPECT_EQ((std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7}), as_seq(boost::irange(0, 0))                    //
                                                              | seq::chain(as_seq(boost::irange(0, 7)))  //
                                                              | seq::chain(as_seq(boost::irange(7, 8)))  //
                                                              | seq::collect_vec());

    EXPECT_EQ((std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7}), as_seq(boost::irange(0, 0))                    //
                                                              | seq::chain(as_seq(boost::irange(0, 3)))  //
                                                              | seq::chain(as_seq(boost::irange(3, 3)))  //
                                                              | seq::chain(as_seq(boost::irange(3, 7)))  //
                                                              | seq::chain(as_seq(boost::irange(7, 8)))  //
                                                              | seq::collect_vec());

    EXPECT_EQ((std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7}), as_seq(boost::irange(0, 1))                    //
                                                              | seq::chain(as_seq(boost::irange(1, 3)))  //
                                                              | seq::chain(as_seq(boost::irange(3, 5)))  //
                                                              | seq::chain(as_seq(boost::irange(5, 8)))  //
                                                              | seq::chain(as_seq(boost::irange(8, 8)))  //
                                                              | seq::collect_vec());
}

TEST(SeqTest, FlattenTest)
{
    EXPECT_EQ((std::vector<int>{0, 1, 2,  //
                                1, 2, 3,  //
                                2, 3, 4,  //
                                3, 4, 5}  //
               ),
              as_seq(boost::irange(0, 4))  //
                  | seq::map([](int n) {
                        return as_seq(boost::irange(n, n + 3));
                    })              //
                  | seq::flatten()  //
                  | seq::collect_vec());

    EXPECT_EQ((std::vector<int>{0,           //
                                0, 1, 2, 3,  //
                                0, 1, 2,     //
                                0, 1}        //
               ),
              as_seq(std::vector<int>{0, 1, 4, 0, 0, 3, 2, 0}) | seq::map([](int n) {
                  return as_seq(boost::irange(0, n));
              })                    //
                  | seq::flatten()  //
                  | seq::collect_vec());
}

TEST(SeqTest, MergeTest)
{
    std::vector<int> nums{0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7};

    std::default_random_engine rng{0};
    std::uniform_int_distribution<int> pick_side(0, 1);

    std::set<std::pair<std::size_t, std::size_t>> sizes_tested;

    for (int i = 0; i < 1 * 1000; ++i) {
        std::vector<int> left, right;

        for (int n : nums) {
            if (pick_side(rng)) {
                left.emplace_back(n);
            } else {
                right.emplace_back(n);
            }
        }

        sizes_tested.emplace(left.size(), right.size());

        EXPECT_EQ(nums, as_seq(left)                     //
                            | seq::merge(as_seq(right))  //
                            | seq::decayed()             //
                            | seq::collect_vec());
    }

    // EXPECT_THAT(sizes_tested, ::testing::SizeIs(nums.size() + 1));
}

TEST(SeqTest, KMergeTest)
{
    std::vector<int> nums;
    for (int n = 0; n < 7; ++n) {
        for (int k = 0; k < 10; ++k) {
            nums.emplace_back(n);
        }
    }

    std::default_random_engine rng{0};
    std::uniform_int_distribution<int> pick_side(0, 9);

    std::set<std::vector<std::size_t>> sizes_tested;

    for (int i = 0; i < 1 * 1000; ++i) {
        std::vector<std::vector<int>> kseqs(10);

        for (int n : nums) {
            kseqs[pick_side(rng)].emplace_back(n);
        }

        sizes_tested.emplace(as_seq(kseqs)  //
                             | seq::map([](auto&& s) {
                                   return s.size();
                               })  //
                             | seq::collect_vec());

        EXPECT_EQ(nums, as_seq(kseqs)  //
                            | seq::map([](auto&& vec) {
                                  return as_seq(vec);
                              })              //
                            | seq::kmerge()   //
                            | seq::decayed()  //
                            | seq::collect_vec());
    }

    // EXPECT_THAT(sizes_tested, ::testing::SizeIs(::testing::Gt(1000ul)));
}

TEST(SeqTest, MapPairwiseTest)
{
    std::vector<std::string> strs{{"apple"}, {"banana"}, {"carrot"}};

    EXPECT_THAT(as_seq(strs)  //
                    | seq::map_pairwise(as_seq(boost::irange(1, 10)),
                                        [](const std::string& s, int n) {
                                            return s + std::string(n, '.');
                                        })  //
                    | seq::collect_vec(),
                ::testing::ElementsAre("apple.", "banana..", "carrot..."));
}

TEST(SeqTest, OutputTest)
{
    {
        std::ostringstream oss;

        as_seq(boost::irange(1, 5)) | seq::print_out(oss);

        EXPECT_THAT(oss.str(), ::testing::StrEq("1 2 3 4 "));
    }
    {
        std::ostringstream oss;

        as_seq(boost::irange(1, 5)) | seq::print_out(oss, " mississippi, ");

        EXPECT_THAT(oss.str(),
                    ::testing::StrEq("1 mississippi, 2 mississippi, 3 mississippi, 4 mississippi, "));
    }
    {
        std::ostringstream oss;

        (void)(as_seq(boost::irange(10, 16)) | seq::debug_out(oss));

        EXPECT_TRUE(oss.str().empty());

        as_seq(boost::irange(10, 16)) | seq::debug_out(oss) | seq::consume();

        EXPECT_THAT(oss.str(), ::testing::StrEq("10 11 12 13 14 15 "));
    }
}

TEST(SeqTest, GroupByTest)
{
    EXPECT_EQ((std::vector<std::vector<int>>{{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {9, 10, 11}}),
              force_copy(as_seq(boost::irange(12))  //
                         | seq::group_by([](int i, int j) {
                               return i / 3 == j / 3;
                           })  //
                         | seq::map([](auto&& sub_group) {
                               auto v = BATT_FORWARD(sub_group) | seq::collect_vec();
                               // std::cout << "collect_vec: ";
                               for (const auto& i : v) {
                                   // std::cout << i << ", ";
                                   (void)i;
                               }
                               // std::cout << std::endl;
                               return v;
                           }))  //
                  | seq::collect_vec());
}

template <typename Seq>
struct CountCopies {
    using Item = batt::SeqItem<Seq>;

    template <typename... Args>
    explicit CountCopies(int* count, Args&&... args) : count_{count}
                                                     , seq_(BATT_FORWARD(args)...)
    {
        *count_ = 1;
    }

    CountCopies(const CountCopies& that) : count_{that.count_}, seq_(that.seq_)
    {
        if (count_) {
            *count_ += 1;
        }
    }

    CountCopies(CountCopies&& that) noexcept : count_{that.count_}, seq_(BATT_FORWARD(that.seq_))
    {
        that.count_ = nullptr;
    }

    ~CountCopies() noexcept
    {
        if (count_) {
            *count_ -= 1;
        }
    }

    CountCopies& operator=(const CountCopies& that)
    {
        if (this != &that) {
            if (count_) {
                *count_ -= 1;
            }
            this->count_ = that.count_;
            this->seq_ = that.seq_;
            if (count_) {
                *count_ += 1;
            }
        }
        return *this;
    }

    CountCopies& operator=(CountCopies&& that) noexcept
    {
        if (this != &that) {
            if (count_) {
                *count_ -= 1;
            }
            this->count_ = that.count_;
            this->seq_ = BATT_FORWARD(that.seq_);
            that.count_ = nullptr;
        }
        return *this;
    }

    decltype(auto) peek()
    {
        return seq_.peek();
    }

    decltype(auto) next()
    {
        return seq_.next();
    }

    int* count_;
    Seq seq_;
};

template <typename Seq>
CountCopies<Seq> count_copies(int* count, Seq&& seq)
{
    return CountCopies<Seq>{count, BATT_FORWARD(seq)};
}

TEST(SeqTest, GroupBy_MoveConstruct)
{
    int copies = 0;

    auto groups = count_copies(&copies, as_seq(boost::irange(12)))  //
                  | seq::group_by([](int i, int j) {
                        return i / 3 == j / 3;
                    });

    EXPECT_EQ(copies, 1);

    auto subgroup1 = *groups.next();

    EXPECT_EQ(copies, 1);

    auto groups2 = std::move(groups);

    EXPECT_EQ(copies, 1);
    EXPECT_EQ((std::vector<int>{0, 1, 2}), std::move(subgroup1) | seq::collect_vec());

    auto subgroup2 = *groups2.next();

    EXPECT_EQ(copies, 1);

    auto groups3 = std::move(groups2);

    EXPECT_EQ(copies, 1);
    EXPECT_EQ((std::vector<int>{3, 4, 5}), std::move(subgroup2) | seq::collect_vec());

    auto subgroup3 = *groups3.next();

    EXPECT_EQ(copies, 1);

    auto groups4 = std::move(groups3);

    EXPECT_EQ(copies, 1);
    EXPECT_EQ((std::vector<int>{6, 7, 8}), std::move(subgroup3) | seq::collect_vec());

    auto subgroup4 = *groups4.next();

    EXPECT_EQ(copies, 1);
    {
        auto groups5 = std::move(groups4);

        EXPECT_EQ(copies, 1);
        EXPECT_EQ((std::vector<int>{9, 10, 11}), std::move(subgroup4) | seq::collect_vec());
        EXPECT_FALSE(groups5.next());
        EXPECT_EQ(copies, 1);
    }
    EXPECT_EQ(copies, 0);
}

TEST(SeqTest, GroupBy_MoveAssign)
{
    auto groups_a = as_seq(boost::irange(12))  //
                    | seq::group_by([](int i, int j) {
                          return i / 3 == j / 3;
                      });

    auto subgroup1 = *groups_a.next();
    auto groups_b = std::move(groups_a);

    EXPECT_EQ((std::vector<int>{0, 1, 2}), std::move(subgroup1) | seq::collect_vec());

    auto subgroup2 = *groups_b.next();
    EXPECT_EQ(&(groups_a = std::move(groups_b)), &groups_a);

    EXPECT_EQ((std::vector<int>{3, 4, 5}), std::move(subgroup2) | seq::collect_vec());

    auto subgroup3 = *groups_a.next();
    EXPECT_EQ(&(groups_b = std::move(groups_a)), &groups_b);

    EXPECT_EQ((std::vector<int>{6, 7, 8}), std::move(subgroup3) | seq::collect_vec());

    auto subgroup4 = *groups_b.next();
    EXPECT_EQ(&(groups_a = std::move(groups_b)), &groups_a);

    EXPECT_EQ((std::vector<int>{9, 10, 11}), std::move(subgroup4) | seq::collect_vec());
    EXPECT_FALSE(groups_a.next());
}

TEST(SeqTest, GroupBy_CopyAssign)
{
    int copies = 0;
    {
        auto groups_a = count_copies(&copies, as_seq(boost::irange(12)))  //
                        | seq::group_by([](int i, int j) {
                              return i / 3 == j / 3;
                          });

        EXPECT_EQ(copies, 1);

        auto subgroup1 = *groups_a.next();

        EXPECT_EQ(copies, 1);
        {
            auto groups_b = groups_a;

            EXPECT_EQ(copies, 2);
            EXPECT_EQ((std::vector<int>{0, 1, 2}), std::move(subgroup1) | seq::collect_vec());

            auto subgroup2 = *groups_b.next();

            EXPECT_EQ(copies, 2);
            EXPECT_EQ(&(groups_a = groups_b), &groups_a);
            EXPECT_EQ(copies, 2);
            EXPECT_EQ((std::vector<int>{3, 4, 5}), std::move(subgroup2) | seq::collect_vec());

            auto subgroup3 = *groups_a.next();

            EXPECT_EQ(copies, 2);
            EXPECT_EQ(&(groups_b = groups_a), &groups_b);
            EXPECT_EQ(copies, 2);
            EXPECT_EQ((std::vector<int>{6, 7, 8}), std::move(subgroup3) | seq::collect_vec());

            auto subgroup4 = *groups_b.next();

            EXPECT_EQ(copies, 2);
            EXPECT_EQ(&(groups_a = groups_b), &groups_a);
            EXPECT_EQ(copies, 2);

            EXPECT_EQ((std::vector<int>{9, 10, 11}), std::move(subgroup4) | seq::collect_vec());
            EXPECT_FALSE(groups_a.next());
            EXPECT_EQ(copies, 2);

        }  // ~groups_b

        EXPECT_EQ(copies, 1);

    }  // ~groups_a

    EXPECT_EQ(copies, 0);
}

TEST(SeqTest, GroupBy_CopySubGroup)
{
    int copies = 0;
    {
        auto groups = count_copies(&copies, as_seq(boost::irange(12))) | seq::group_by([](int i, int j) {
                          return i / 3 == j / 3;
                      });

        EXPECT_EQ((std::vector<int>{0, 1, 2}), *groups.peek() | seq::collect_vec());
        EXPECT_EQ(copies, 1);

        auto subgroup1 = *groups.next();

        EXPECT_EQ((std::vector<int>{3, 4, 5}), *groups.peek() | seq::collect_vec());
        EXPECT_EQ(copies, 1);

        auto subgroup_copy1 = subgroup1;

        EXPECT_EQ((std::vector<int>{3, 4, 5}), *groups.peek() | seq::collect_vec());
        EXPECT_EQ(copies, 2);
        EXPECT_EQ(*subgroup1.next(), 0);

        auto subgroup_copy2 = subgroup1;

        EXPECT_EQ((std::vector<int>{3, 4, 5}), *groups.peek() | seq::collect_vec());
        EXPECT_EQ(copies, 3);
        EXPECT_EQ(*subgroup1.next(), 1);
        EXPECT_EQ(*subgroup1.peek(), 2);

        auto subgroup2 = *groups.next();

        EXPECT_EQ(*subgroup1.peek(), 2);
        EXPECT_EQ(copies, 4);

        auto subgroup_copy3 = subgroup1;

        EXPECT_EQ((std::vector<int>{6, 7, 8}), *groups.peek() | seq::collect_vec());
        EXPECT_EQ(copies, 5);
        EXPECT_EQ(*subgroup1.next(), 2);

        // `copies` drops because subgroup1 is consumed.
        //
        EXPECT_EQ(copies, 4);

        auto subgroup_copy4 = subgroup1;

        // Copying a consumed subgroup does not create a copy.
        //
        EXPECT_EQ(copies, 4);

        EXPECT_EQ((std::vector<int>{6, 7, 8}), *groups.peek() | seq::collect_vec());
        EXPECT_EQ(copies, 4);
        EXPECT_FALSE(subgroup1.next());
        EXPECT_EQ(copies, 4);

        EXPECT_EQ((std::vector<int>{6, 7, 8}), *groups.peek() | seq::collect_vec());

        auto subgroup3 = *groups.next();

        EXPECT_EQ(copies, 5);
        EXPECT_EQ((std::vector<int>{0, 1, 2}), std::move(subgroup_copy1) | seq::collect_vec());

        // `copies` drops because subgroup_copy1 is consumed
        //
        EXPECT_EQ(copies, 4);

        EXPECT_EQ((std::vector<int>{9, 10, 11}), *groups.peek() | seq::collect_vec());
        EXPECT_EQ((std::vector<int>{1, 2}), std::move(subgroup_copy2) | seq::collect_vec());

        // `copies` drops because subgroup_copy2 is consumed
        //
        EXPECT_EQ(copies, 3);

        EXPECT_EQ((std::vector<int>{9, 10, 11}), *groups.peek() | seq::collect_vec());
        EXPECT_EQ((std::vector<int>{2}), std::move(subgroup_copy3) | seq::collect_vec());

        // `copies` drops because subgroup_copy2 is consumed
        //
        EXPECT_EQ(copies, 2);

        auto subgroup4 = *groups.next();

        // subgroup4 is now current; copies of the seq are held by groups,
        // subgroup2, and subgroup3.
        //
        EXPECT_EQ(copies, 3);

        // Nothing comes after subgroup4, so peek should now return None.
        //
        EXPECT_FALSE(groups.peek());

        EXPECT_EQ((std::vector<int>{}), std::move(subgroup_copy4) | seq::collect_vec());
        EXPECT_EQ(copies, 3);

        EXPECT_EQ((std::vector<int>{6, 7, 8}), std::move(subgroup3) | seq::collect_vec());
        EXPECT_EQ(copies, 2);

        EXPECT_EQ((std::vector<int>{3, 4, 5}), std::move(subgroup2) | seq::collect_vec());
        EXPECT_EQ(copies, 1);

        EXPECT_EQ((std::vector<int>{9, 10, 11}), std::move(subgroup4) | seq::collect_vec());

        // `copies` doesn't drop, because subgroup4 wasn't detached when we consumed
        // it.
        //
        EXPECT_EQ(copies, 1);

    }  // ~groups

    EXPECT_EQ(copies, 0);
}

TEST(SeqTest, FirstTest)
{
    EXPECT_EQ(make_optional(0), as_seq(boost::irange(10)) | seq::first());
    EXPECT_EQ(make_optional(0), as_seq(boost::irange(1)) | seq::first());
    EXPECT_EQ(None, as_seq(boost::irange(0)) | seq::first());
}

TEST(SeqTest, LastTest)
{
    EXPECT_EQ(make_optional(9), as_seq(boost::irange(10)) | seq::last());
    EXPECT_EQ(make_optional(0), as_seq(boost::irange(1)) | seq::last());
    EXPECT_EQ(None, as_seq(boost::irange(0)) | seq::last());
}

TEST(SeqTest, ReduceTest)
{
    EXPECT_EQ(1 + 2 + 3 + 4 + 5, as_seq(boost::irange(6)) | seq::reduce(0, [](int a, int b) {
                                     return a + b;
                                 }));

    EXPECT_EQ(10 + 1 + 2 + 3 + 4 + 5, as_seq(boost::irange(6)) | seq::reduce(10, [](int a, int b) {
                                          return a + b;
                                      }));

    EXPECT_EQ(99, as_seq(boost::irange(0)) | seq::reduce(99, [](int a, int b) {
                      return a + b;
                  }));
}

TEST(SeqTest, ChainTest)
{
    // Empty -> NonEmpty
    //
    EXPECT_EQ((std::vector<int>{1, 2, 3, 4, 5}),
              as_seq(boost::irange(1, 1)) | seq::chain(as_seq(boost::irange(1, 6))) | seq::collect_vec());

    // Empty -> Empty
    //
    EXPECT_EQ((std::vector<int>{}),
              as_seq(boost::irange(1, 1)) | seq::chain(as_seq(boost::irange(5, 5))) | seq::collect_vec());

    // NonEmpty -> Empty
    //
    EXPECT_EQ((std::vector<int>{1, 2, 3, 4, 5}),
              as_seq(boost::irange(1, 6)) | seq::chain(as_seq(boost::irange(6, 6))) | seq::collect_vec());

    // NonEmpty -> NonEmpty
    //
    EXPECT_EQ((std::vector<int>{1, 2, 3, 4, 5}),
              as_seq(boost::irange(1, 3)) | seq::chain(as_seq(boost::irange(3, 6))) | seq::collect_vec());

    // Many nested chains.
    //
    EXPECT_EQ((std::vector<int>{1, 2, 3, 4, 5}),
              as_seq(boost::irange(1, 3)) | seq::chain(as_seq(boost::irange(3, 4))) |
                  seq::chain(as_seq(boost::irange(4, 5))) | seq::chain(as_seq(boost::irange(5, 6))) |
                  seq::collect_vec());
}

TEST(SeqTest, SpliceTest)
{
    // NonEmpty -> NonEmpty -> NonEmpty
    //
    EXPECT_EQ((std::vector<int>{2, 4, 1, 3, 5, 7, 9, 6, 8, 10}),
              as_seq(boost::irange(2, 12, 2)) | seq::splice(2, as_seq(boost::irange(1, 10, 2))) |
                  seq::collect_vec());

    // NonEmpty -> NonEmpty -> Empty
    //
    EXPECT_EQ((std::vector<int>{2, 4, 6, 8, 10, 1, 3, 5, 7, 9}),
              as_seq(boost::irange(2, 12, 2)) | seq::splice(5, as_seq(boost::irange(1, 10, 2))) |
                  seq::collect_vec());

    // NonEmpty -> Empty -> NonEmpty
    //
    EXPECT_EQ((std::vector<int>{2, 4, 6, 8, 10}), as_seq(boost::irange(2, 12, 2)) |
                                                      seq::splice(2, as_seq(boost::irange(10, 10, 2))) |
                                                      seq::collect_vec());

    // NonEmpty -> Empty -> Empty
    //
    EXPECT_EQ((std::vector<int>{2, 4, 6, 8, 10}), as_seq(boost::irange(2, 12, 2)) |
                                                      seq::splice(5, as_seq(boost::irange(10, 10, 2))) |
                                                      seq::collect_vec());

    // Empty -> NonEmpty -> NonEmpty
    //
    EXPECT_EQ((std::vector<int>{1, 3, 5, 7, 9, 2, 4, 6, 8, 10}),
              as_seq(boost::irange(2, 12, 2)) | seq::splice(0, as_seq(boost::irange(1, 10, 2))) |
                  seq::collect_vec());

    // Empty -> NonEmpty -> Empty
    //
    EXPECT_EQ((std::vector<int>{1, 3, 5, 7, 9}), as_seq(boost::irange(12, 12, 2)) |
                                                     seq::splice(0, as_seq(boost::irange(1, 10, 2))) |
                                                     seq::collect_vec());

    EXPECT_EQ((std::vector<int>{1, 3, 5, 7, 9}), as_seq(boost::irange(12, 12, 2)) |
                                                     seq::splice(5000, as_seq(boost::irange(1, 10, 2))) |
                                                     seq::collect_vec());

    // Empty -> Empty -> NonEmpty
    //
    EXPECT_EQ((std::vector<int>{2, 4, 6, 8, 10}), as_seq(boost::irange(2, 12, 2)) |
                                                      seq::splice(0, as_seq(boost::irange(10, 10, 2))) |
                                                      seq::collect_vec());

    // Empty -> Empty -> Empty
    //
    EXPECT_EQ((std::vector<int>{}), as_seq(boost::irange(12, 12, 2)) |
                                        seq::splice(0, as_seq(boost::irange(10, 10, 2))) |
                                        seq::collect_vec());

    EXPECT_EQ((std::vector<int>{}), as_seq(boost::irange(12, 12, 2)) |
                                        seq::splice(5000, as_seq(boost::irange(10, 10, 2))) |
                                        seq::collect_vec());
}

TEST(SeqTest, RunningTotalTest)
{
    EXPECT_EQ((std::vector<int>{0, 1, 3, 6, 10, 15, 21, 28}),
              as_seq(boost::irange(0, 8)) | seq::running_total() | seq::collect_vec());
}

}  // namespace
