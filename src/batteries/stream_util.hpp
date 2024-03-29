//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once

#include <batteries/config.hpp>
//
#include <batteries/constants.hpp>
#include <batteries/finally.hpp>
#include <batteries/int_types.hpp>
#include <batteries/optional.hpp>
#include <batteries/suppress.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/io/ios_state.hpp>
#include <boost/preprocessor/seq/for_each.hpp>

#include <atomic>
#include <charconv>
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
    static std::atomic<usize>& max_show_length()
    {
        static std::atomic<usize> len{std::numeric_limits<usize>::max()};
        return len;
    }

    std::string_view str;
};

inline EscapedStringLiteral c_str_literal(const std::string_view& str)
{
    return EscapedStringLiteral{str};
}

template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, std::string_view>>>
inline Optional<EscapedStringLiteral> c_str_literal(const Optional<T>& maybe_str)
{
    if (maybe_str) {
        return {c_str_literal(*maybe_str)};
    }
    return None;
}

inline Optional<EscapedStringLiteral> c_str_literal(const NoneType&)
{
    return None;
}

#define BATT_DETAIL_OVERLOAD_STRING_PRINTABLE(type)                                                          \
    inline decltype(auto) make_printable(type str)                                                           \
    {                                                                                                        \
        return c_str_literal(str);                                                                           \
    }

#define BATT_DETAIL_SPECIALIZE_STRING_PRINTABLE(type)                                                        \
    BATT_DETAIL_OVERLOAD_STRING_PRINTABLE(type&)                                                             \
    BATT_DETAIL_OVERLOAD_STRING_PRINTABLE(type&&)                                                            \
    BATT_DETAIL_OVERLOAD_STRING_PRINTABLE(const type&)                                                       \
    BATT_DETAIL_OVERLOAD_STRING_PRINTABLE(const type&&)

BATT_DETAIL_SPECIALIZE_STRING_PRINTABLE(std::string)
BATT_DETAIL_SPECIALIZE_STRING_PRINTABLE(std::string_view)

#undef BATT_DETAIL_SPECIALIZE_STRING_PRINTABLE
#undef BATT_DETAIL_OVERLOAD_STRING_PRINTABLE

#define BATT_INSPECT_STR(expr) " " << #expr << " == " << ::batt::c_str_literal((expr))

inline const std::string_view& StringUpperBound()
{
    static const std::string_view s = [] {
        static std::array<char, 4096> s;
        s.fill(0xff);
        return std::string_view{s.data(), s.size()};
    }();
    return s;
}

inline std::ostream& operator<<(std::ostream& out, const EscapedStringLiteral& t)
{
    if (t.str.data() == StringUpperBound().data()) {
        return out << "\"\\xff\"...";
    }

    static const char xdigit[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    const auto emit_hex_ascii = [&](char ch) {
        out << "\\x" << xdigit[(ch >> 4) & 0xf] << xdigit[ch & 0xf];
    };

    out << '"';
    usize i = 0;
    for (char ch : t.str) {
        if (i > EscapedStringLiteral::max_show_length()) {
            return out << "\"...(" << t.str.length() - i << " skipped chars)";
        }
        ++i;
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

struct HexByteDumper {
    std::string_view bytes;
};

inline std::ostream& operator<<(std::ostream& out, const HexByteDumper& t)
{
    out << std::endl;
    boost::io::ios_flags_saver saver{out};

    const char* const bytes = t.bytes.data();
    const usize len = t.bytes.size();
    for (usize i = 0; i < len; ++i) {
        if (i % 16 == 0) {
            out << std::hex << std::setw(8) << std::setfill('0') << i << ": ";
        }
        out << std::hex << std::setw(2) << std::setfill('0') << (((unsigned)bytes[i]) & 0xfful);
        if (i % 16 == 15) {
            out << std::endl;
        } else if (i % 2 == 1) {
            out << " ";
        }
    }
    return out;
}

inline HexByteDumper dump_hex(const void* ptr, usize size)
{
    return HexByteDumper{std::string_view{static_cast<const char*>(ptr), size}};
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

/*! \brief Wrapper around `usize` (`std::size_t`) that prints as human-readable sizes.
 */
struct SizeDumper {
    enum struct UnitBase {
        kLog2 = 2,
        kLog10 = 10,
    };

    std::string format(UnitBase base, i32& parts, i32& ord, usize& error) const
    {
        std::ostringstream oss;

        parts = 0;
        ord = 0;
        error = 0;

        bool first_part = true;
        usize n = this->value;
        usize unit_ord = 7;

        auto format_impl = [&](const std::array<std::pair<u64, const char*>, 7>& units) {
            for (auto [div, unit_str] : units) {
                const usize q = n / div;
                const usize r = n % div;
                if (q != 0 || (div == 1 && first_part)) {
                    if (first_part) {
                        ord = unit_ord;
                    }
                    parts += 1;
                    oss << (first_part ? "" : "+") << q << unit_str;
                    if (!this->exact) {
                        error = r;
                        break;
                    }
                    first_part = false;
                }
                n = r;
                unit_ord -= 1;
            }
        };

        switch (base) {
        case UnitBase::kLog2:
            format_impl({
                std::make_pair(kEiB, "EiB"),
                std::make_pair(kPiB, "PiB"),
                std::make_pair(kTiB, "TiB"),
                std::make_pair(kGiB, "GiB"),
                std::make_pair(kMiB, "MiB"),
                std::make_pair(kKiB, "KiB"),
                std::make_pair(u64{1}, "B"),
            });
            break;

        case UnitBase::kLog10:
            format_impl({
                std::make_pair(kEB, "EB"),
                std::make_pair(kPB, "PB"),
                std::make_pair(kTB, "TB"),
                std::make_pair(kGB, "GB"),
                std::make_pair(kMB, "MB"),
                std::make_pair(kKB, "KB"),
                std::make_pair(u64{1}, "B"),
            });
            break;

        default:
            BATT_PANIC() << "Invalid value for `base`: " << int(base);
        }

        return std::move(oss).str();
    }

    /*! \brief The size value to be printed.
     */
    usize value;

    /*! \brief If false, then `value` will be printed rounded off to the nearest human-friendly unit (mb,
     * kb, etc.); if true, then a longer string will be printed to capture the exact value.
     */
    bool exact;
};

inline std::ostream& operator<<(std::ostream& out, SizeDumper t)
{
    i32 base2_parts = 0, base10_parts = 0, base2_ord = 0, base10_ord = 0;
    usize base2_error = 0, base10_error = 0;
    const std::string base2_str = t.format(SizeDumper::UnitBase::kLog2, base2_parts, base2_ord, base2_error);
    const std::string base10_str =
        t.format(SizeDumper::UnitBase::kLog10, base10_parts, base10_ord, base10_error);

    if (base10_parts < base2_parts           //
        || (base10_parts == base2_parts      //
            && (base2_ord < base10_ord       //
                || (base2_ord == base10_ord  //
                    && (base2_error != 0 && base10_error == 0))))) {
        return out << ((base10_error == 0) ? "" : "~") << base10_str;
    } else {
        return out << ((base2_error == 0) ? "" : "~") << base2_str;
    }
}

inline SizeDumper dump_size(usize n)
{
    return SizeDumper{
        .value = n,
        .exact = false,
    };
}

inline SizeDumper dump_size_exact(usize n)
{
    return SizeDumper{
        .value = n,
        .exact = true,
    };
}

/*! \brief Parse a byte size string with optional units.
 *
 * Units can be any of:
 *  - 'b': bytes
 *  - 'kb': kilobytes (=1024 bytes)
 *  - 'mb': megabytes (=1024 kilobytes)
 *  - 'gb': gigabytes (=1024 megabytes)
 *  - 'tb': terabytes (=1024 gigabytes)
 *  - 'pb': petabytes (=1024 terabtyes)
 *  - 'eb': eitabytes (=1024 petabytes)
 *
 * The unit string may be upper or lower case, and only the first character is considered, so the string "45k"
 * will parse the same as "45KILOBYTES" or "45Kb", etc.
 *
 * Simple addition and subtraction will also be evaluated, so for example, the string "16mb-1" will parse to
 * the value `16 * 1024 * 1024 - 1`.  No space is allowed between additive terms when using this notation.
 *
 * \return The parse size in bytes if successful, None otherwise.
 */
inline Optional<usize> parse_byte_size(std::string_view s)
{
    if (s.empty()) {
        return None;
    }

    usize result_value = 0;
    bool have_result = false;

    while (!s.empty()) {
        if (s.front() == '+') {
            s.remove_prefix(1);
            continue;
        }
        if (s.front() == '-') {
            if (!have_result) {
                return None;
            }
            s.remove_prefix(1);
            Optional<usize> delta = parse_byte_size(s);
            if (!delta) {
                return None;
            }
            result_value -= *delta;
            break;
        }
        std::string_view num_chars = s;
        while (!s.empty() && s.front() >= '0' && s.front() <= '9') {
            s.remove_prefix(1);
        }
        num_chars.remove_suffix(s.size());

        usize num_value = 0;
        std::from_chars_result int_parse =
            std::from_chars(num_chars.data(), num_chars.data() + num_chars.size(), num_value);

        if (int_parse.ec != std::errc{}) {
            return None;
        }

        const usize unit_value = [&]() -> usize {
            if (s.empty()) {
                return 1ull;
            }
            switch (std::tolower(s.front())) {
            case 'b':
                return 1ull;
            case 'k':
                return kKiB;
            case 'm':
                return kMiB;
            case 'g':
                return kGiB;
            case 't':
                return kTiB;
            case 'p':
                return kPiB;
            case 'e':
                return kEiB;
            default:
                return 1ull;
            }
        }();
        while (!s.empty() &&
               ((s.front() >= 'a' && s.front() <= 'z') || (s.front() >= 'A' && s.front() <= 'Z'))) {
            s.remove_prefix(1);
        }

        have_result = true;
        result_value += num_value * unit_value;

        // After the unit string, only '-' or '+' are allowed.
        //
        if (!s.empty() && s.front() != '+' && s.front() != '-') {
            return None;
        }
    }

    return result_value;
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

inline auto pretty_print_indent()
{
    return std::string(detail::range_dump_depth() * 2, ' ');
}

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

        std::string indent = pretty_print_indent();
        ++detail::range_dump_depth();
        const auto leave_indent_level = finally([&] {
            --detail::range_dump_depth();
            detail::range_dump_pretty() = save_pretty;
        });

        out << "{ ";
        if (pretty && std::begin(t.value_) != std::end(t.value_)) {
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

        return out << "}";
    }
};

template <typename T>
RangeDumper<const T&> dump_range(const T& value, Pretty pretty)
{
    return RangeDumper<const T&>{value, pretty};
}

#define BATT_INSPECT_RANGE(expr) " " << #expr << " == " << ::batt::dump_range((expr))
#define BATT_INSPECT_RANGE_PRETTY(expr)                                                                      \
    " " << #expr << " == " << ::batt::dump_range((expr), ::batt::Pretty::True)

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

#define BATT_PRINT_OBJECT_FIELD(r, obj, fieldname)                                                           \
    << " ." << BOOST_PP_STRINGIZE(fieldname) << "=" << ::batt::make_printable(obj.fieldname) << ","

#define BATT_PRINT_OBJECT_IMPL(type, fields_seq)                                                             \
    std::ostream& operator<<(std::ostream& out, const type& t)                                               \
    {                                                                                                        \
        return out << ::batt::name_of<type>()                                                                \
                   << "{" BOOST_PP_SEQ_FOR_EACH(BATT_PRINT_OBJECT_FIELD, (t), fields_seq) << "}";            \
    }

}  // namespace batt
