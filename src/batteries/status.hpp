//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATUS_HPP
#define BATTERIES_STATUS_HPP

#include <batteries/config.hpp>
//
#include <batteries/assert.hpp>
#include <batteries/int_types.hpp>
#include <batteries/logging.hpp>
#include <batteries/stream_util.hpp>
#include <batteries/strong_typedef.hpp>
#include <batteries/utility.hpp>

#include <boost/preprocessor/cat.hpp>
#include <boost/system/error_code.hpp>

#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <vector>

namespace batt {

#ifdef BATT_STATUS_CUSTOM_MESSSAGES
#error This feature is not ready yet!
#endif

namespace detail {

class StatusBase
{
   public:
    StatusBase() noexcept;
};

}  // namespace detail

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
    kLoopBreak = 102,
    kEndOfStream = 103,
    kClosedBeforeEndOfStream = 104,
    kGrantRevoked = 105,
};

enum ErrnoValue {};

class BATT_WARN_UNUSED_RESULT Status;

class Status : private detail::StatusBase
{
   public:
    using value_type = i32;

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    static constexpr i32 kGroupSizeBits = 12 /*-> 4096*/;
    static constexpr i32 kGroupSize = i32{1} << kGroupSizeBits;
    static constexpr i32 kMaxGroups = 0x7fffff00l - kGroupSize;
    //+++++++++++-+-+--+----- --- -- -  -  -   -

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

        const char* name() const noexcept
        {
            return name_of(this->enum_type_index);
        }
    };

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    template <typename EnumT>
    static const CodeGroup& code_group_for_type()
    {
        return code_group_for_type_internal<EnumT>();
    }

    static const std::string& unknown_enum_value_message()
    {
        static const std::string s = "(Unknown enum value; not registered via batt::Status::register_codes)";
        return s;
    }

    template <typename EnumT>
    static bool register_codes(const std::vector<std::pair<EnumT, std::string>>& codes);

    static const CodeEntry& get_entry_from_code(value_type value)
    {
        const usize index_of_group = Status::get_index_of_group(value);
        const usize index_within_group = Status::get_index_within_group(value);
        const auto& all_groups = Status::registered_groups();

        BATT_CHECK_LT(index_of_group, all_groups.size());
        BATT_CHECK_LT(index_within_group, all_groups[index_of_group]->entries.size())
            << BATT_INSPECT(index_of_group) << BATT_INSPECT(value);

        return all_groups[index_of_group]->entries[index_within_group];
    }

    static std::string_view message_from_code(value_type value)
    {
        return Status::get_entry_from_code(value).message;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

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

        BATT_ASSERT_NOT_NULLPTR(message_from_code(this->value_).data());

#ifdef BATT_ASSERT_CUSTOM_MESSSAGES
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

    value_type code_index_within_group() const noexcept
    {
        return Status::get_index_within_group(this->value_);
    }

    const CodeEntry& code_entry() const noexcept
    {
        return Status::get_entry_from_code(this->value_);
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
        const auto& all_groups = registered_groups();

        BATT_ASSERT_LT(index_of_group, all_groups.size());
        BATT_ASSERT_NOT_NULLPTR(all_groups[index_of_group]);

        return *all_groups[index_of_group];
    }

    void IgnoreError() const noexcept
    {
        // do nothing
    }

    void Update(const Status& new_status);

   private:
    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
    friend class detail::StatusBase;

    static constexpr usize kMaxGroupCount = 256;
    static constexpr i32 kMaxCodeNumericRange = 0xffff;
    static constexpr i32 kLocalMask = (i32{1} << kGroupSizeBits) - 1;
    static constexpr i32 kGroupMask = ~kLocalMask;

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    static usize next_group_index()
    {
        static std::atomic<i32> next_index{0};
        const usize i = next_index.fetch_add(1);
        BATT_CHECK_LT(i, kMaxGroupCount);
        return i;
    }

    static std::array<CodeGroup*, kMaxGroupCount>& registered_groups()
    {
        static std::array<CodeGroup*, kMaxGroupCount> all_groups;
        [[maybe_unused]] static bool initialized = [] {
            all_groups.fill(nullptr);
            return true;
        }();

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

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    // Unique error code;
    //
    value_type value_;
#ifdef BATT_STATUS_CUSTOM_MESSSAGES
    std::string_view message_;
#endif
};

static_assert(sizeof(Status) <= sizeof(void*), "");

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

// Print human-friendly representation of a Status.
//
std::ostream& operator<<(std::ostream& out, const Status& t);

// Equality comparison of `Status` values.
//
bool operator==(const Status& l, const Status& r);
bool operator!=(const Status& l, const Status& r);

// Returns a Status value `s` for which `s.ok() == true`.
//
Status OkStatus();

inline void Status::Update(const Status& new_status)
{
    if (this->ok() || *this == Status{StatusCode::kUnknown}) {
        *this = new_status;
    }
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename T>
class BATT_WARN_UNUSED_RESULT StatusOr;

namespace detail {

template <typename T>
class StatusOrValueContainer
{
   public:
    using Self = StatusOrValueContainer;

    template <typename... Args>
    void construct(Args&&... args)
    {
        new (&this->storage_) T(BATT_FORWARD(args)...);
    }

    void construct(const Self& that)
    {
        this->construct(that.reference());
    }

    void construct(Self&& that)
    {
        this->construct(std::move(that.reference()));
    }

    T* pointer() noexcept
    {
        return reinterpret_cast<T*>(&this->storage_);
    }

    const T* pointer() const noexcept
    {
        return reinterpret_cast<const T*>(&this->storage_);
    }

    T& reference() noexcept
    {
        return *this->pointer();
    }

    const T& reference() const noexcept
    {
        return *this->pointer();
    }

    void destroy()
    {
        this->reference().~T();
    }

   private:
    std::aligned_storage_t<sizeof(T), alignof(T)> storage_;
};

template <typename T>
class StatusOrValueContainer<T&>
{
   public:
    using Self = StatusOrValueContainer<T&>;

    void construct(T& value)
    {
        this->ptr_ = &value;
    }

    void construct(const Self& that)
    {
        this->ptr_ = that.ptr_;
    }

    void construct(Self&& that)
    {
        this->ptr_ = that.ptr_;
    }

    T* pointer() const noexcept
    {
        return this->ptr_;
    }

    T& reference() const noexcept
    {
        return *this->pointer();
    }

    void destroy() noexcept
    {
        this->ptr_ = nullptr;
    }

   private:
    T* ptr_ = nullptr;
};

}  // namespace detail

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
            this->value_.construct(std::move(that.value_));
            this->status_ = OkStatus();

            that.value_.destroy();
            that.status_ = StatusCode::kUnknown;
        } else {
            this->status_ = std::move(that.status_);
        }
    }

    StatusOr(const StatusOr& that) : status_{that.status_}
    {
        if (this->ok()) {
            this->value_.construct(that.value_);
        }
    }

    /*implicit*/ StatusOr(const std::decay_t<T>& obj) noexcept(
        noexcept(T(std::declval<const std::decay_t<T>&>())))
        : status_{OkStatus()}
    {
        this->value_.construct(obj);
    }

    /*implicit*/ StatusOr(std::decay_t<T>& obj) noexcept(noexcept(T(std::declval<std::decay_t<T>&>())))
        : status_{OkStatus()}
    {
        this->value_.construct(obj);
    }

    /*implicit*/ StatusOr(std::decay_t<T>&& obj) noexcept(noexcept(T(std::declval<std::decay_t<T>&&>())))
        : status_{OkStatus()}
    {
        this->value_.construct(std::move(obj));
    }

    template <
        typename U, typename = EnableIfNoShadow<StatusOr, U&&>,
        typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, T> && std::is_constructible_v<T, U&&>>,
        typename = void>
    /*implicit*/ StatusOr(U&& obj) noexcept(noexcept(T(std::declval<U&&>()))) : status_{OkStatus()}
    {
        this->value_.construct(BATT_FORWARD(obj));
    }

    template <typename U, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, T> &&
                                                      std::is_constructible_v<T, U&&>>>
    /*implicit*/ StatusOr(StatusOr<U>&& that) noexcept(noexcept(T(std::declval<U&&>())))
        : status_{StatusCode::kUnknown}
    {
        if (that.status_.ok()) {
            this->value_.construct(std::move(that.value()));
            this->status_ = OkStatus();

            that.value_.destroy();
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
            this->value_.destroy();
        }
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
    // Assignment operator overloads

    StatusOr& operator=(std::decay_t<T>&& obj)
    {
        static_assert(std::is_same_v<T, std::decay_t<T>>, "");
        if (this->ok()) {
            this->value_.destroy();
        }
        this->status_ = OkStatus();
        this->value_.construct(std::move(obj));

        return *this;
    }

    StatusOr& operator=(const T& obj)
    {
        if (this->ok()) {
            this->value_.destroy();
        }
        this->status_ = OkStatus();
        this->value_.construct(obj);

        return *this;
    }

    template <typename U, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, T> &&
                                                      std::is_constructible_v<T, U&&>>>
    StatusOr& operator=(U&& obj) noexcept(noexcept(T(std::declval<U&&>())))
    {
        if (this->ok()) {
            this->value_.destroy();
        }
        this->status_ = OkStatus();
        this->value_.construct(BATT_FORWARD(obj));

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
                    this->value_.destroy();
                }
            } else {
                if (that.ok()) {
                    this->value_.construct(that.value());
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
                    this->value_.destroy();
                }
            } else {
                if (that.ok()) {
                    this->value_.construct(std::move(that.value()));
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
            this->value_.destroy();
        }
        this->status_ = new_status;

        return *this;
    }

    template <typename... Args>
    void emplace(Args&&... args)
    {
        if (this->ok()) {
            this->value_.destroy();
        }
        this->value_.construct(BATT_FORWARD(args)...);
        this->status_ = OkStatus();
    }

    template <typename U>
    void emplace(StatusOr<U>&& that)
    {
        if (this->ok()) {
            this->value_.destroy();
            this->status_ = StatusCode::kUnknown;
        }
        if (that.ok()) {
            this->value_.construct(std::move(*that));
        }
        this->status_ = that.status();
    }

    template <typename U>
    void emplace(const StatusOr<U>& that)
    {
        if (this->ok()) {
            this->value_.destroy();
            this->status_ = StatusCode::kUnknown;
        }
        if (that.ok()) {
            this->value_.construct(*that);
        }
        this->status_ = that.status();
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
        return this->value_.reference();
    }

    const T& value() const noexcept
    {
        BATT_ASSERT(this->status_.ok()) << BATT_INSPECT(this->status_);
        return this->value_.reference();
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

    std::add_const_t<std::remove_reference_t<T>>* operator->() const noexcept
    {
        return &(this->value());
    }

    std::remove_reference_t<T>* operator->() noexcept
    {
        return &(this->value());
    }

   private:
    Status status_;
    detail::StatusOrValueContainer<T> value_;
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
    return (l.ok() && r.ok() && l.value() == r.value()) || (!l.ok() && !r.ok() && l.status() == r.status());
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

namespace detail {

template <typename T>
struct IsStatusOrImpl : std::false_type {
};

template <typename T>
struct IsStatusOrImpl<StatusOr<T>> : std::true_type {
};

}  // namespace detail

template <typename T>
using IsStatusOr = detail::IsStatusOrImpl<std::decay_t<T>>;

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

inline std::atomic<LogLevel>& require_fail_global_default_log_level()
{
    static std::atomic<LogLevel> global_default_{LogLevel::kVerbose};
    return global_default_;
}

inline LogLevel& require_fail_thread_default_log_level()
{
    thread_local LogLevel per_thread_default_ = require_fail_global_default_log_level();
    return per_thread_default_;
}

#ifdef BATT_GLOG_AVAILABLE
#define BATT_VLOG_IS_ON(level) VLOG_IS_ON(level)
#else
#define BATT_VLOG_IS_ON(level) false
#endif

namespace detail {

class NotOkStatusWrapper
{
   public:
    explicit NotOkStatusWrapper(const char* file, int line, Status&& status, bool vlog_is_on) noexcept
        : file_{file}
        , line_{line}
        , status_(std::move(status))
        , vlog_is_on_{vlog_is_on}
    {
        *this << this->status_ << "; ";
    }

    explicit NotOkStatusWrapper(const char* file, int line, const Status& status, bool vlog_is_on) noexcept
        : file_{file}
        , line_{line}
        , status_(status)
        , vlog_is_on_{vlog_is_on}
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
            if (this->vlog_is_on_) {
                ::google::LogMessage(this->file_, this->line_, google::GLOG_INFO).stream()
                    << this->output_.str();
            }
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
    bool vlog_is_on_;
    LogLevel level_{require_fail_thread_default_log_level()};
    std::ostringstream output_;
};

}  // namespace detail

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T,
          typename = std::enable_if_t<IsStatusOr<T>{} && !std::is_same_v<std::decay_t<T>, StatusOr<Status>>>>
inline decltype(auto) to_status(T&& v)
{
    return BATT_FORWARD(v).status();
}

template <typename T,
          typename = std::enable_if_t<std::is_same_v<std::decay_t<T>, Status> ||
                                      std::is_same_v<std::decay_t<T>, StatusOr<Status>>>,
          typename = void>
inline decltype(auto) to_status(T&& s)
{
    return BATT_FORWARD(s);
}

template <typename T,
          typename = std::enable_if_t<std::is_same_v<std::decay_t<T>, boost::system::error_code> ||
                                      std::is_same_v<std::decay_t<T>, std::error_code>>,
          typename = void, typename = void>
inline Status to_status(const T& ec)
{
    // TODO [tastolfi 2021-10-13] support these so we don't lose information.
    if (!ec) {
        return OkStatus();
    }
    return Status{StatusCode::kInternal};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

#define BATT_REQUIRE_OK(expr)                                                                                \
    for (decltype(auto) BOOST_PP_CAT(BATTERIES_temp_status_result_, __LINE__) = (expr);                      \
         !::batt::is_ok_status(BOOST_PP_CAT(BATTERIES_temp_status_result_, __LINE__));)                      \
        return ::batt::detail::NotOkStatusWrapper                                                            \
        {                                                                                                    \
            __FILE__, __LINE__,                                                                              \
                ::batt::to_status(BATT_FORWARD(BOOST_PP_CAT(BATTERIES_temp_status_result_, __LINE__))),      \
                BATT_VLOG_IS_ON(1)                                                                           \
        }

#define BATT_ASSIGN_OK_RESULT(lvalue_expr, statusor_expr)                                                    \
    auto BOOST_PP_CAT(BATTERIES_temp_StatusOr_result_, __LINE__) = statusor_expr;                            \
    BATT_REQUIRE_OK(BOOST_PP_CAT(BATTERIES_temp_StatusOr_result_, __LINE__));                                \
    lvalue_expr = std::move(*BOOST_PP_CAT(BATTERIES_temp_StatusOr_result_, __LINE__))

#define BATT_OK_RESULT_OR_PANIC(expr)                                                                        \
    [&](auto&& expr_value) {                                                                                 \
        BATT_CHECK(::batt::is_ok_status(expr_value))                                                         \
            << BOOST_PP_STRINGIZE(expr) << ".status == " << ::batt::to_status(expr_value);                   \
        return std::move(*BATT_FORWARD(expr_value));                                                         \
    }((expr))

#define BATT_CHECK_OK(expr)                                                                                  \
    if (bool BOOST_PP_CAT(BATTERIES_check_ok_flag_, __LINE__) = true)                                        \
        for (decltype(auto) BOOST_PP_CAT(BATTERIES_check_expr_value_, __LINE__) = (expr);                    \
             BOOST_PP_CAT(BATTERIES_check_ok_flag_, __LINE__);                                               \
             BOOST_PP_CAT(BATTERIES_check_ok_flag_, __LINE__) = false)                                       \
            for (; !BATT_HINT_TRUE(                                                                          \
                       ::batt::is_ok_status(BOOST_PP_CAT(BATTERIES_check_expr_value_, __LINE__))) &&         \
                   BATT_HINT_TRUE(::batt::lock_fail_check_mutex());                                          \
                 ::batt::fail_check_exit())                                                                  \
    BATT_FAIL_CHECK_MESSAGE("batt::to_status(" BOOST_PP_STRINGIZE(expr) ")", ::batt::to_status(BOOST_PP_CAT(BATTERIES_check_expr_value_, __LINE__)),                   \
        "==", "batt::OkStatus()", ::batt::OkStatus(), __FILE__, __LINE__, __PRETTY_FUNCTION__)

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

template <typename EnumT>
inline bool Status::register_codes(const std::vector<std::pair<EnumT, std::string>>& codes)
{
    static detail::StatusBase base;

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
            //
            CodeGroup& global_group = Status::code_group_for_type_internal<EnumT>();
            BATT_CHECK(global_group.entries.empty()) << "A status code group may only be registered once!";
            global_group = std::move(group);

            /*std::array<CodeGroup*, ...>*/ auto& all_groups = Status::registered_groups();
            BATT_CHECK_LT(global_group.index, all_groups.size());

            all_groups[global_group.index] = &global_group;

            // Done!
        }();
        return true;
    }();

    return exactly_once;
}

}  // namespace batt

#endif  // BATTERIES_STATUS_HPP

#if BATT_HEADER_ONLY
#include <batteries/status_impl.hpp>
#endif  // BATT_HEADER_ONLY
