#pragma once

#include <iomanip>
#include <optional>
#include <ostream>
#include <sstream>

#include <boost/io/ios_state.hpp>

#include <batteries/int_types.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

namespace std {

// =============================================================================
// Support printing for std::optional<T>.
//
template <typename T>
inline ostream &
operator<<(ostream &out, const optional<T> &t)
{
    if (t) {
        return out << *t;
    }
    return out << "{}";
}

// =============================================================================
// Support insertion of lambdas and other callables that take a std::ostream&.
//
template <typename Fn, typename = enable_if_t<::batt::IsCallable<Fn, ostream &>{}>>
inline ostream &
operator<<(ostream &out, Fn &&fn)
{
    fn(out);
    return out;
}

} // namespace std

namespace batt {

// =============================================================================
// to_string - use ostream insertion to convert any object to a string.
//
template <typename T>
std::string
to_string(const T &obj)
{
    std::ostringstream oss;
    oss << obj;
    return std::move(oss).str();
}

// =============================================================================
// dump_range(x) - make range `x` printable to std::ostream.  Will also print any nested ranges
// (e.g., `std::vector<std::vector<int>>`).
//
// Example:
//
// ```
// std::vector<int> nums;
//
// std::cout << batt::dump_range(nums);
// ```
//
// Example (Pretty-printing):
// ```
// std::vector<int> nums;
//
// std::cout << batt::dump_range(nums, batt::Pretty::True);
// ```
//
enum struct Pretty
{
    True,
    False,
    Default
};

template <typename T>
class RangeDumper;

template <typename T>
RangeDumper<const T &>
dump_range(const T &value, Pretty pretty = Pretty::Default);

namespace detail {

    inline Pretty &range_dump_pretty()
    {
        thread_local Pretty p = Pretty::False;
        return p;
    }

    inline int &range_dump_depth()
    {
        thread_local int depth = 0;
        return depth;
    }

    template <typename T,
              typename = std::enable_if_t<!std::is_same<std::decay_t<T>, u8>{}>,
              typename = std::enable_if_t<!IsRange<T>{}>>
    inline std::ostream &dump_item(std::ostream &out, T &&item)
    {
        return out << BATT_FORWARD(item);
    }

    template <typename T, typename = std::enable_if_t<IsRange<T>{}>>
    inline std::ostream &dump_item(std::ostream &out, T &&item)
    {
        return out << dump_range(item);
    }

    inline std::ostream &dump_item(std::ostream &out, u8 byte_val)
    {
        return out << "0x" << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte_val);
    }

} // namespace detail

template <typename T>
class RangeDumper
{
  private:
    T value_;
    Pretty pretty_;

  public:
    template <typename Arg>
    explicit RangeDumper(Arg &&arg, Pretty pretty) noexcept
      : value_{ BATT_FORWARD(arg) }
      , pretty_{ pretty }
    {}

    friend inline std::ostream &operator<<(std::ostream &out, const RangeDumper &t) noexcept
    {
        boost::io::ios_flags_saver flags_saver(out);
        const Pretty save_pretty = detail::range_dump_pretty();
        bool pretty = [&] {
            if (t.pretty_ == Pretty::Default) {
                return save_pretty == Pretty::True;
            }
            return t.pretty_ == Pretty::True;
        }();
        detail::range_dump_pretty() = pretty ? Pretty::True : Pretty::False;

        std::string indent(detail::range_dump_depth() * 2, ' ');
        ++detail::range_dump_depth();
        out << "{ ";
        if (pretty) {
            out << std::endl << indent;
        }
        for (const auto &item : t.value_) {
            if (pretty) {
                out << "  ";
            }
            detail::dump_item(out, item) << ", ";
            if (pretty) {
                out << std::endl << indent;
            }
        }

        --detail::range_dump_depth();
        detail::range_dump_pretty() = save_pretty;

        return out << "}";
    }
};

template <typename T>
RangeDumper<const T &>
dump_range(const T &value, Pretty pretty)
{
    return RangeDumper<const T &>{ value, pretty };
}

} // namespace batt
