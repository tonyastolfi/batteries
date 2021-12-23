// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATUS_HPP
#define BATTERIES_STATUS_HPP

#include <batteries/assert.hpp>
#include <batteries/int_types.hpp>
#include <batteries/stream_util.hpp>
#include <batteries/strong_typedef.hpp>
#include <batteries/utility.hpp>

#include <boost/preprocessor/cat.hpp>

#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <vector>

#ifdef BATT_GLOG_AVAILABLE
#include <glog/logging.h>
#endif  // BATT_GLOG_AVAILABLE

namespace batt {

#ifdef BATT_STATUS_CUSTOM_MESSSAGES
#error This feature is not ready yet!
#endif

class StatusBase
{
   public:
    StatusBase() noexcept;
};

// Intentionally value-compatible with Abseil's StatusCode.
//
enum class StatusCode : int {
    kOk = 0,
    kCancelled = 1,
    kUnknown = 2,
    kInvalidArgument = 3,
    kDeadlineExceeded = 4,
    kNotFound = 5,
    kAlreadyExists = 6,
    kPermissionDenied = 7,
    kResourceExhausted = 8,
    kFailedPrecondition = 9,
    kAborted = 10,
    kOutOfRange = 11,
    kUnimplemented = 12,
    kInternal = 13,
    kUnavailable = 14,
    kDataLoss = 15,
    kUnauthenticated = 16,
    // ...
    // This range reserved for future allocation of Abseil status codes.
    // ...
    kClosed = 100,
    kGrantUnavailable = 101,
};

enum ErrnoValue {};

class BATT_WARN_UNUSED_RESULT Status;

class Status : private StatusBase
{
   public:
    BATT_STRONG_TYPEDEF(usize, PinGroup);

    using value_type = i32;

    static constexpr i32 kMaxCodeNumericRange = 0xffff;
    static constexpr i32 kGroupSizeBits = 12 /*-> 4096*/;
    static constexpr i32 kGroupSize = i32{1} << kGroupSizeBits;
    static constexpr i32 kLocalMask = (i32{1} << kGroupSizeBits) - 1;
    static constexpr i32 kGroupMask = ~kLocalMask;
    static constexpr i32 kMaxGroups = 0x7fffff00l - kGroupSize;

    struct CodeEntry {
        value_type code;
        int enum_value;
        std::string message;
    };

    struct CodeGroup {
        std::type_index enum_type_index{typeid(int)};
        usize index;
        int min_enum_value;
        std::vector<usize> enum_value_to_code;
        std::vector<CodeEntry> entries;
    };

    static const std::string& unknown_enum_value_message()
    {
        static const std::string s = "(Unknown enum value; not registered via batt::Status::register_codes)";
        return s;
    }

    template <typename EnumT>
    static const CodeGroup& code_group_for_type()
    {
        return code_group_for_type_internal<EnumT>();
    }

    template <typename EnumT>
    static bool register_codes(const std::vector<std::pair<EnumT, std::string>>& codes);

    static std::string_view message_from_code(value_type value)
    {
        const usize index_of_group = get_index_of_group(value);
        const usize index_within_group = get_index_within_group(value);
        {
            std::unique_lock<std::mutex> lock{global_mutex()};

            const auto& all_groups = registered_groups();

            BATT_ASSERT_LT(index_of_group, all_groups.size());
            BATT_ASSERT_LT(index_within_group, all_groups[index_of_group]->entries.size());

            return all_groups[index_of_group]->entries[index_within_group].message;
        }
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    // Construct a no-error status object.
    //
    Status() : Status(StatusCode::kOk)
    {
    }

    // This is a regular copyable value type.
    //
    Status(const Status&) = default;
    Status& operator=(const Status&) = default;

    // Implicitly convert enumerated types to Status.  The given type `EnumT` must have been registered
    // via `Status::register_codes` prior to invoking this constructor.
    //
    template <typename EnumT, typename = std::enable_if_t<std::is_enum_v<EnumT>>>
    /*implicit*/ Status(EnumT enum_value) noexcept
    {
        const CodeGroup& group = code_group_for_type<EnumT>();
        BATT_ASSERT_GE(static_cast<int>(enum_value), group.min_enum_value);

        const int index_within_enum = static_cast<int>(enum_value) - group.min_enum_value;
        BATT_ASSERT_LT(index_within_enum, static_cast<int>(group.enum_value_to_code.size()))
            << BATT_INSPECT(group.index) << BATT_INSPECT(group.enum_type_index.name());

        this->value_ = group.enum_value_to_code[index_within_enum];

#ifdef BATT_STATUS_CUSTOM_MESSSAGES
        const usize index_within_group = get_index_within_group(this->value_);

        this->message_ = group.entries[index_within_group].message;
#endif
    }

#ifdef BATT_STATUS_CUSTOM_MESSSAGES
    template <typename EnumT, typename = std::enable_if_t<std::is_enum_v<EnumT>>>
    explicit Status(EnumT enum_value, const std::string_view& custom_message) noexcept : Status{enum_value}
    {
        this->message_ = custom_message;
    }
#endif

    bool ok() const noexcept BATT_WARN_UNUSED_RESULT
    {
        return (this->value_ & kLocalMask) == 0;
    }

    value_type code() const noexcept
    {
        return this->value_;
    }

    std::string_view message() const noexcept
    {
#ifdef BATT_STATUS_CUSTOM_MESSSAGES
        return this->message_;
#else
        return message_from_code(this->value_);
#endif
    }

    const CodeGroup& group() const
    {
        const usize index_of_group = get_index_of_group(this->value_);
        {
            std::unique_lock<std::mutex> lock{global_mutex()};

            const auto& all_groups = registered_groups();

            BATT_ASSERT_LT(index_of_group, all_groups.size());
            BATT_ASSERT_NOT_NULLPTR(all_groups[index_of_group]);

            return *all_groups[index_of_group];
        }
    }

    void IgnoreError() const noexcept
    {
        // do nothing
    }

    void Update(const Status& new_status)
    {
        if (this->ok()) {
            *this = new_status;
        }
    }

   private:
    friend class StatusBase;

    static usize next_group_index()
    {
        static std::atomic<i32> next_index{0};
        return next_index.fetch_add(1);
    }

    static std::mutex& global_mutex()
    {
        static std::mutex m;
        return m;
    }

    static std::vector<CodeGroup*>& registered_groups()
    {
        static std::vector<CodeGroup*> all_groups;
        return all_groups;
    }

    template <typename EnumT>
    static CodeGroup& code_group_for_type_internal()
    {
        static CodeGroup group;
        return group;
    }

    static usize get_index_of_group(value_type value)
    {
        return (value & kGroupMask) >> kGroupSizeBits;
    }

    static usize get_index_within_group(value_type value)
    {
        return value & kLocalMask;
    }

    template <typename EnumT>
    static bool register_codes_internal(const std::vector<std::pair<EnumT, std::string>>& codes);

    value_type value_;
#ifdef BATT_STATUS_CUSTOM_MESSSAGES
    std::string_view message_;
#endif
};

static_assert(sizeof(Status) <= sizeof(void*), "");

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

inline std::ostream& operator<<(std::ostream& out, const Status& t)
{
    return out << t.code() << ":" << t.message();
}

inline bool operator==(const Status& l, const Status& r)
{
    return l.code() == r.code() || (l.ok() && r.ok());
}

inline bool operator!=(const Status& l, const Status& r)
{
    return !(l == r);
}

inline Status OkStatus()
{
    return Status{};
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename T>
class BATT_WARN_UNUSED_RESULT StatusOr;

template <typename T>
class StatusOr
{
    template <typename U>
    friend class StatusOr;

   public:
    using value_type = T;

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
    // Constructors

    explicit StatusOr() noexcept : status_{StatusCode::kUnknown}
    {
        BATT_ASSERT(!this->ok());
    }

    /*implicit*/ StatusOr(const Status& s) : status_{s}
    {
        BATT_CHECK(!this->ok()) << "StatusOr must not be constructed with an Ok Status value.";
    }

    StatusOr(StatusOr&& that) : status_{StatusCode::kUnknown}
    {
        if (that.ok()) {
            new (&this->storage_) T(std::move(that.value()));
            this->status_ = OkStatus();

            that.value().~T();
            that.status_ = StatusCode::kUnknown;
        } else {
            this->status_ = std::move(that.status_);
        }
    }

    StatusOr(const StatusOr& that) : status_{that.status_}
    {
        if (this->ok()) {
            new (&this->storage_) T(that.value());
        }
    }

    /*implicit*/ StatusOr(const T& obj) noexcept(noexcept(T(std::declval<const T&>()))) : status_{OkStatus()}
    {
        new (&this->storage_) T(obj);
    }

    /*implicit*/ StatusOr(T&& obj) noexcept(noexcept(T(std::declval<T&&>()))) : status_{OkStatus()}
    {
        new (&this->storage_) T(std::move(obj));
    }

    template <
        typename U, typename = EnableIfNoShadow<StatusOr, U&&>,
        typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, T> && std::is_constructible_v<T, U&&>>,
        typename = void>
    /*implicit*/ StatusOr(U&& obj) noexcept(noexcept(T(std::declval<U&&>()))) : status_{OkStatus()}
    {
        new (&this->storage_) T(BATT_FORWARD(obj));
    }

    template <typename U, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, T> &&
                                                      std::is_constructible_v<T, U&&>>>
    /*implicit*/ StatusOr(StatusOr<U>&& that) noexcept(noexcept(T(std::declval<U&&>())))
        : status_{StatusCode::kUnknown}
    {
        if (that.status_.ok()) {
            new (&this->storage_) T(std::move(that.value()));
            this->status_ = OkStatus();

            that.value().~U();
            that.status_ = StatusCode::kUnknown;
        } else {
            this->status_ = std::move(that).status();
        }
    }

    template <typename U, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, T> &&
                                                      std::is_constructible_v<T, const U&>>>
    /*implicit*/ StatusOr(const StatusOr<U>& that) noexcept(noexcept(T(std::declval<const U&>())))
        : status_{StatusCode::kUnknown}
    {
        if (that.status_.ok()) {
            new (&this->storage_) T(that.value());
            this->status_ = OkStatus();
        } else {
            this->status_ = that.status_;
        }
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
    // Destructor

    ~StatusOr()
    {
        if (this->ok()) {
            this->value().~T();
        }
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
    // Assignment operator overloads

    StatusOr& operator=(T&& obj)
    {
        if (this->ok()) {
            this->value().~T();
        }
        this->status_ = OkStatus();
        new (&this->storage_) T(std::move(obj));

        return *this;
    }

    StatusOr& operator=(const T& obj)
    {
        if (this->ok()) {
            this->value().~T();
        }
        this->status_ = OkStatus();
        new (&this->storage_) T(obj);

        return *this;
    }

    template <typename U, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, T> &&
                                                      std::is_constructible_v<T, U&&>>>
    StatusOr& operator=(U&& obj) noexcept(noexcept(T(std::declval<U&&>())))
    {
        if (this->ok()) {
            this->value().~T();
        }
        this->status_ = OkStatus();
        new (&this->storage_) T(BATT_FORWARD(obj));

        return *this;
    }

    StatusOr& operator=(const StatusOr& that) noexcept(
        noexcept(T(std::declval<const T&>())) && noexcept(std::declval<T&>() = std::declval<const T&>()))
    {
        if (BATT_HINT_TRUE(this != &that)) {
            if (this->ok()) {
                if (that.ok()) {
                    this->value() = that.value();
                } else {
                    this->value().~T();
                }
            } else {
                if (that.ok()) {
                    new (&this->storage_) T(that.value());
                }
            }
            this->status_ = that.status_;
        }
        return *this;
    }

    StatusOr& operator=(StatusOr&& that) noexcept(
        noexcept(T(std::declval<T&&>())) && noexcept(std::declval<T&>() = std::move(std::declval<T&>())))
    {
        if (BATT_HINT_TRUE(this != &that)) {
            if (this->ok()) {
                if (that.ok()) {
                    this->value() = std::move(that.value());
                } else {
                    this->value().~T();
                }
            } else {
                if (that.ok()) {
                    new (&this->storage_) T(std::move(that.value()));
                }
            }
            this->status_ = that.status_;
        }
        return *this;
    }

    StatusOr& operator=(const Status& new_status) noexcept
    {
        BATT_CHECK(!new_status.ok()) << "StatusOr must not be constructed with an Ok Status value.";
        if (this->ok()) {
            this->value().~T();
        }
        this->status_ = new_status;

        return *this;
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    void IgnoreError() const noexcept
    {
        // do nothing
    }

    bool ok() const noexcept
    {
        return this->status_.ok();
    }

    const Status& status() const&
    {
        return this->status_;
    }

    T& value() noexcept
    {
        BATT_ASSERT(this->status_.ok()) << BATT_INSPECT(this->status_);
        return *reinterpret_cast<T*>(&this->storage_);
    }

    const T& value() const noexcept
    {
        BATT_ASSERT(this->status_.ok()) << BATT_INSPECT(this->status_);
        return *reinterpret_cast<const T*>(&this->storage_);
    }

    T& operator*() & noexcept
    {
        return this->value();
    }

    const T& operator*() const& noexcept
    {
        return this->value();
    }

    T operator*() && noexcept
    {
        return std::move(this->value());
    }

    const T* operator->() const noexcept
    {
        return &(this->value());
    }

    T* operator->() noexcept
    {
        return &(this->value());
    }

   private:
    Status status_;
    std::aligned_storage_t<sizeof(T), alignof(T)> storage_;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// StatusOr<Status> == Status.
//
template <>
class StatusOr<Status> : public Status
{
   public:
    using Status::Status;

    /*implicit*/ StatusOr(const Status& status) : Status{status}
    {
    }

    /*implicit*/ StatusOr(Status&& status) : Status{std::move(status)}
    {
    }
};

static_assert(sizeof(Status) == sizeof(StatusOr<Status>), "");

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// StatusOr<StatusOr<T>> == StatusOr<T>
//
template <typename T>
class StatusOr<StatusOr<T>> : public StatusOr<T>
{
   public:
    using StatusOr<T>::StatusOr;

    /*implicit*/ StatusOr(const StatusOr<T>& status_or) : StatusOr<T>{status_or}
    {
    }

    /*implicit*/ StatusOr(StatusOr<T>&& status_or) : StatusOr<T>{std::move(status_or)}
    {
    }
};

static_assert(sizeof(StatusOr<StatusOr<int>>) == sizeof(StatusOr<int>), "");

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename T, typename U, typename = std::enable_if_t<CanBeEqCompared<T, U>{}>>
inline bool operator==(const StatusOr<T>& l, const StatusOr<U>& r)
{
    return (l.ok() && r.ok() && *l == *r) || (l.status() == r.status());
}

// If `T` (and `U`) can't be equality-compared, then we define StatusOr<T> to be equal iff the non-ok status
// values are equal.
//
template <typename T, typename U, typename = std::enable_if_t<!CanBeEqCompared<T, U>{}>, typename = void>
inline bool operator==(const StatusOr<T>& l, const StatusOr<U>& r)
{
    return (!l.ok() && !r.ok() && l.status() == r.status());
}

template <typename T, typename U>
inline bool operator!=(const StatusOr<T>& l, const StatusOr<U>& r)
{
    return !(l == r);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename T>
struct IsStatusOr_Impl : std::false_type {
};

template <typename T>
struct IsStatusOr_Impl<StatusOr<T>> : std::true_type {
};

template <typename T>
using IsStatusOr = IsStatusOr_Impl<std::decay_t<T>>;

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T>
struct RemoveStatusOrImpl;

template <typename T>
struct RemoveStatusOrImpl<StatusOr<T>> : batt::StaticType<T> {
};

template <typename T>
using RemoveStatusOr = typename RemoveStatusOrImpl<T>::type;

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

inline bool is_ok_status(const std::error_code& ec)
{
    return !ec;
}

template <typename T>
bool is_ok_status(const T& val)
{
    return val.ok();
}

enum struct LogLevel { kFatal, kError, kWarning, kInfo, kDebug, kVerbose };

class NotOkStatusWrapper
{
   public:
    explicit NotOkStatusWrapper(const char* file, int line, Status&& status) noexcept
        : file_{file}
        , line_{line}
        , status_(std::move(status))
    {
        *this << this->status_ << "; ";
    }

    explicit NotOkStatusWrapper(const char* file, int line, const Status& status) noexcept
        : file_{file}
        , line_{line}
        , status_(status)
    {
#ifndef BATT_GLOG_AVAILABLE
        *this << "(" << this->file_ << ":" << this->line_ << ") ";
#endif  // BATT_GLOG_AVAILALBE
        *this << this->status_ << "; ";
    }

    ~NotOkStatusWrapper() noexcept
    {
#ifdef BATT_GLOG_AVAILABLE
        switch (this->level_) {
        case LogLevel::kFatal:
            ::google::LogMessage(this->file_, this->line_, google::GLOG_FATAL).stream()
                << this->output_.str();
            break;
        case LogLevel::kError:
            ::google::LogMessage(this->file_, this->line_, google::GLOG_ERROR).stream()
                << this->output_.str();
            break;
        case LogLevel::kWarning:
            ::google::LogMessage(this->file_, this->line_, google::GLOG_WARNING).stream()
                << this->output_.str();
            break;
        case LogLevel::kInfo:
            ::google::LogMessage(this->file_, this->line_, google::GLOG_INFO).stream() << this->output_.str();
            break;
        case LogLevel::kDebug:
            DLOG(INFO) << " [" << this->file_ << ":" << this->line_ << "] " << this->output_.str();
            break;
        case LogLevel::kVerbose:
            VLOG(1) << " [" << this->file_ << ":" << this->line_ << "] " << this->output_.str();
            break;
        }
#endif  // BATT_GLOG_AVAILABLE
    }

    operator Status() noexcept
    {
        return std::move(this->status_);
    }

    template <typename T>
    operator StatusOr<T>() noexcept
    {
        return StatusOr<T>{std::move(this->status_)};
    }

    NotOkStatusWrapper& operator<<(LogLevel new_level)
    {
        this->level_ = new_level;
        return *this;
    }

    template <typename T>
    NotOkStatusWrapper& operator<<(T&& val)
    {
        this->output_ << BATT_FORWARD(val);
        return *this;
    }

   private:
    const char* file_;
    int line_;
    Status status_;
    LogLevel level_{LogLevel::kVerbose};
    std::ostringstream output_;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

inline Status to_status(const std::error_code& ec)
{
    // TODO [tastolfi 2021-10-13] support these so we don't lose information.
    (void)ec;
    return Status{StatusCode::kInternal};
}

template <typename T, typename = std::enable_if_t<IsStatusOr<T>{}>>
inline decltype(auto) to_status(T&& v)
{
    return BATT_FORWARD(v).status();
}

template <typename T, typename = std::enable_if_t<std::is_same_v<std::decay_t<T>, Status>>, typename = void>
inline decltype(auto) to_status(T&& s)
{
    return BATT_FORWARD(s);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

#define BATT_REQUIRE_OK(expr)                                                                                \
    if (!::batt::is_ok_status(expr))                                                                         \
        return ::batt::NotOkStatusWrapper                                                                    \
        {                                                                                                    \
            __FILE__, __LINE__, ::batt::to_status(BATT_FORWARD(expr))                                        \
        }

#define BATT_ASSIGN_OK_RESULT(lvalue_expr, statusor_expr)                                                    \
    auto BOOST_PP_CAT(BATTERIES_temp_StatusOr_result_, __LINE__) = statusor_expr;                            \
    BATT_REQUIRE_OK(BOOST_PP_CAT(BATTERIES_temp_StatusOr_result_, __LINE__));                                \
    lvalue_expr = std::move(*BOOST_PP_CAT(BATTERIES_temp_StatusOr_result_, __LINE__))

#define BATT_OK_RESULT_OR_PANIC(expr)                                                                        \
    [&](auto&& status_or) {                                                                                  \
        BATT_CHECK(status_or.ok()) << BATT_INSPECT(status_or.status());                                      \
        return std::move(*BATT_FORWARD(status_or));                                                          \
    }(expr)

inline Status status_from_errno(int code)
{
    return static_cast<ErrnoValue>(code);
}

template <typename T>
inline Status status_from_retval(T retval)
{
    if (retval >= 0) {
        return OkStatus();
    }
    return status_from_errno(errno);
}

template <typename T>
inline T&& ok_result_or_panic(StatusOr<T>&& result)
{
    BATT_CHECK(result.ok()) << result.status();

    return std::move(*result);
}

template <typename T, typename = std::enable_if_t<IsStatusOr<std::decay_t<T>>{} &&
                                                  !std::is_same_v<std::decay_t<T>, StatusOr<Status>>>>
std::ostream& operator<<(std::ostream& out, T&& status_or)
{
    if (!status_or.ok()) {
        return out << "Status{" << status_or.status() << "}";
    }
    return out << "Ok{" << make_printable(*status_or) << "}";
}

inline bool status_is_retryable(const Status& s)
{
    return s == StatusCode::kUnavailable            //
           || s == static_cast<ErrnoValue>(EAGAIN)  //
           || s == static_cast<ErrnoValue>(EINTR)   //
        ;
}

//#=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++

inline StatusBase::StatusBase() noexcept
{
    static bool initialized = [] {
        Status::register_codes_internal<StatusCode>({
            {StatusCode::kOk, "Ok"},
            {StatusCode::kCancelled, "Cancelled"},
            {StatusCode::kUnknown, "Unknown"},
            {StatusCode::kInvalidArgument, "Invalid Argument"},
            {StatusCode::kDeadlineExceeded, "Deadline Exceeded"},
            {StatusCode::kNotFound, "Not Found"},
            {StatusCode::kAlreadyExists, "Already Exists"},
            {StatusCode::kPermissionDenied, "Permission Denied"},
            {StatusCode::kResourceExhausted, "Resource Exhausted"},
            {StatusCode::kFailedPrecondition, "Failed Precondition"},
            {StatusCode::kAborted, "Aborted"},
            {StatusCode::kOutOfRange, "Out of Range"},
            {StatusCode::kUnimplemented, "Unimplemented"},
            {StatusCode::kInternal, "Internal"},
            {StatusCode::kUnavailable, "Unavailable"},
            {StatusCode::kDataLoss, "Data Loss"},
            {StatusCode::kUnauthenticated, "Unauthenticated"},
            {StatusCode::kClosed, "Closed"},
            {StatusCode::kGrantUnavailable, "The requested grant count exceeds available count"},

        });

        std::vector<std::pair<ErrnoValue, std::string>> errno_codes;
        for (int code = 0; code < Status::kGroupSize; ++code) {
            const char* msg = std::strerror(code);
            if (msg) {
                errno_codes.emplace_back(static_cast<ErrnoValue>(code), std::string(msg));
            }
        }
        return Status::register_codes_internal<ErrnoValue>(errno_codes);
    }();
    BATT_ASSERT(initialized);
}

template <typename EnumT>
inline bool Status::register_codes(const std::vector<std::pair<EnumT, std::string>>& codes)
{
    static StatusBase base;

    return register_codes_internal<EnumT>(codes);
}

template <typename EnumT>
inline bool Status::register_codes_internal(const std::vector<std::pair<EnumT, std::string>>& codes)
{
    static bool exactly_once = [&]() -> bool {
        [&] {
            CodeGroup group;

            group.enum_type_index = std::type_index{typeid(EnumT)};
            group.index = next_group_index();
            BATT_CHECK_LT(group.index, kMaxGroups) << "Status::register_codes called too many times!";

            if (codes.empty()) {
                return;
            }

            int min_enum_value = std::numeric_limits<int>::max();
            int max_enum_value = std::numeric_limits<int>::min();
            Status::value_type next_code = group.index * kGroupSize;

            for (auto& [value, message] : codes) {
                const int enum_value = static_cast<int>(value);

                min_enum_value = std::min(min_enum_value, enum_value);
                max_enum_value = std::max(max_enum_value, enum_value);

                group.entries.emplace_back(CodeEntry{
                    next_code,
                    enum_value,
                    std::move(message),
                });
                next_code += 1;
            }

            BATT_CHECK_LE(max_enum_value - min_enum_value, kMaxCodeNumericRange)
                << "The maximum numeric range of codes was exceeded.  min_enum_value=" << min_enum_value
                << " max_enum_value=" << max_enum_value;

            group.min_enum_value = min_enum_value;

            group.enum_value_to_code.resize(max_enum_value - min_enum_value + 1);
            std::fill(group.enum_value_to_code.begin(), group.enum_value_to_code.end(), next_code);

            for (const CodeEntry& e : group.entries) {
                group.enum_value_to_code[e.enum_value - group.min_enum_value] = e.code;
            }

            // Insert an entry at the end of the group for all unknown values.
            //
            group.entries.emplace_back(
                CodeEntry{next_code, max_enum_value + 1, unknown_enum_value_message()});

            // Atomically insert the new code group.
            {
                std::unique_lock<std::mutex> lock{Status::global_mutex()};

                CodeGroup& global_group = Status::code_group_for_type_internal<EnumT>();
                BATT_CHECK(global_group.entries.empty())
                    << "A status code group may only be registered once!";
                global_group = std::move(group);

                std::vector<CodeGroup*>& all_groups = Status::registered_groups();
                if (all_groups.size() < global_group.index + 1) {
                    all_groups.resize(global_group.index + 1);
                }
                all_groups[global_group.index] = &global_group;
            }

            // Done!
        }();
        return true;
    }();

    return exactly_once;
}

}  // namespace batt

#endif  // BATTERIES_STATUS_HPP
