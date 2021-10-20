#pragma once

#include <batteries/int_types.hpp>
#include <batteries/suppress.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/io/ios_state.hpp>

#include <iomanip>
#include <optional>
#include <ostream>
#include <sstream>

namespace std {

// =============================================================================
// Support printing for std::optional<T>.
//
template <typename T>
inline ostream& operator<<(ostream& out, const optional<T>& t)
{
    if (t) {
        return out << *t;
    }
    return out << "{}";
}

// =============================================================================
// Support insertion of lambdas and other callables that take a std::ostream&.
//
template <typename Fn, typename = enable_if_t<::batt::IsCallable<Fn, ostream&>{}>>
inline ostream& operator<<(ostream& out, Fn&& fn)
{
    fn(out);
    return out;
}

}  // namespace std

namespace batt {

// =============================================================================
// print_all - insert all arguments to the stream.
//
inline std::ostream& print_all(std::ostream& out)
{
    return out;
}
template <typename First, typename... Rest>
std::ostream& print_all(std::ostream& out, First&& first, Rest&&... rest)
{
    return print_all(out << BATT_FORWARD(first), BATT_FORWARD(rest)...);
}

// =============================================================================
// extract_all - extract all arguments to the stream.
//
inline std::istream& extract_all(std::istream& in)
{
    return in;
}
template <typename First, typename... Rest>
std::istream& extract_all(std::istream& in, First&& first, Rest&&... rest)
{
    return extract_all(in >> BATT_FORWARD(first), BATT_FORWARD(rest)...);
}

// =============================================================================
// to_string - use ostream insertion to convert any object to a string.
//
template <typename... Args>
std::string to_string(Args&&... args)
{
    std::ostringstream oss;
    print_all(oss, BATT_FORWARD(args)...);
    return std::move(oss).str();
}

#if defined(__GNUC__) && !defined(__clang__)
BATT_SUPPRESS("-Wmaybe-uninitialized")
#endif

// =============================================================================
// from_string - use istream extraction to parse any object from a string.
//

namespace detail {

// General case.
//
template <typename T, typename... FormatArgs>
std::optional<T> from_string_impl(StaticType<T>, const std::string& str, FormatArgs&&... format_args)
{
    T val;
    std::istringstream iss{str};
    extract_all(iss, BATT_FORWARD(format_args)..., val);
    if (iss.good() || iss.eof()) {
        return val;
    }
    return std::nullopt;
}

// Special case for bool.
//
template <typename... FormatArgs>
std::optional<bool> from_string_impl(StaticType<bool>, const std::string& str,
                                     FormatArgs&&... /*format_args*/)
{
    return boost::algorithm::to_lower_copy(str) == "true" || from_string_impl(StaticType<int>{}, str) != 0;
}

}  // namespace detail

template <typename T, typename... FormatArgs>
std::optional<T> from_string(const std::string& str, FormatArgs&&... format_args)
{
    return detail::from_string_impl(StaticType<T>{}, str, BATT_FORWARD(format_args)...);
}

#if defined(__GNUC__) && !defined(__clang__)
BATT_UNSUPPRESS()
#endif

// =============================================================================
// c_str_literal(str) - escape a C string.
//
struct EscapedStringLiteral {
    std::string_view str;
};

inline EscapedStringLiteral c_str_literal(const std::string_view& str)
{
    return EscapedStringLiteral{str};
}

inline std::ostream& operator<<(std::ostream& out, const EscapedStringLiteral& t)
{
    static const char xdigit[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    const auto emit_hex_ascii = [&](char ch) {
        out << "\\x" << xdigit[(ch & 0xf) >> 4] << xdigit[ch & 0xf];
    };

    out << '"';
    for (char ch : t.str) {
        if (ch & 0b10000000) {
            emit_hex_ascii(ch);
            continue;
        }
        switch (ch & 0b1110000) {
        case 0b0000000:
            switch (ch & 0b1111) {
            case 0b0000:
                out << "\\0";
                break;
            case 0b0111:
                out << "\\a";
                break;
            case 0b1000:
                out << "\\b";
                break;
            case 0b1001:
                out << "\\t";
                break;
            case 0b1010:
                out << "\\n";
                break;
            case 0b1011:
                out << "\\v";
                break;
            case 0b1100:
                out << "\\f";
                break;
            case 0b1101:
                out << "\\r";
                break;
            default:
                emit_hex_ascii(ch);
                break;
            }
            break;
        case 0b0010000:
            switch (ch & 0b1111) {
            case 0b1011:
                out << "\\e";
                break;
            default:
                emit_hex_ascii(ch);
                break;
            }
            break;
        case 0b0100000:
            switch (ch & 0b1111) {
            case 0b0010:
                out << "\\\"";
                break;
            default:
                out << ch;
                break;
            }
            break;
        case 0b1010000:
            switch (ch & 0b1111) {
            case 0b1100:
                out << "\\\\";
                break;
            default:
                out << ch;
                break;
            }
            break;
        case 0b1110000:
            switch (ch & 0b1111) {
            case 0b1111:
                out << "\\x7f";
                break;
            default:
                out << ch;
                break;
            }
            break;
        default:
            out << ch;
            break;
        }
    }
    return out << '"';
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
enum struct Pretty { True, False, Default };

template <typename T>
class RangeDumper;

template <typename T>
RangeDumper<const T&> dump_range(const T& value, Pretty pretty = Pretty::Default);

namespace detail {

inline Pretty& range_dump_pretty()
{
    thread_local Pretty p = Pretty::False;
    return p;
}

inline int& range_dump_depth()
{
    thread_local int depth = 0;
    return depth;
}

template <typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, u8>{}>,
          typename = std::enable_if_t<!IsRange<T>{}>>
inline std::ostream& dump_item(std::ostream& out, T&& item)
{
    return out << BATT_FORWARD(item);
}

inline std::ostream& dump_item(std::ostream& out, const std::string& s)
{
    return out << c_str_literal(s);
}

inline std::ostream& dump_item(std::ostream& out, const std::string_view& s)
{
    return out << c_str_literal(s);
}

template <typename T, typename = std::enable_if_t<IsRange<T>{}>>
inline std::ostream& dump_item(std::ostream& out, T&& item)
{
    return out << dump_range(item);
}

inline std::ostream& dump_item(std::ostream& out, u8 byte_val)
{
    return out << "0x" << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte_val);
}

template <typename FirstT, typename SecondT>
inline std::ostream& dump_item(std::ostream& out, const std::pair<FirstT, SecondT>& p)
{
    out << "{";
    dump_item(out, p.first);
    out << ", ";
    dump_item(out, p.second);
    return out << "}";
}

}  // namespace detail

template <typename T>
class RangeDumper
{
   private:
    T value_;
    Pretty pretty_;

   public:
    template <typename Arg>
    explicit RangeDumper(Arg&& arg, Pretty pretty) noexcept : value_{BATT_FORWARD(arg)}
                                                            , pretty_{pretty}
    {
    }

    friend inline std::ostream& operator<<(std::ostream& out, const RangeDumper& t) noexcept
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
        for (const auto& item : t.value_) {
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
RangeDumper<const T&> dump_range(const T& value, Pretty pretty)
{
    return RangeDumper<const T&>{value, pretty};
}

}  // namespace batt
