# &lt;batteries/status.hpp&gt;: Efficient, Ergonomic Error Handling

## Summary

```c++
#include <batteries/status.hpp>
```

| Types | Functions | Macros |
| :---- | :-------- | :----- |
| [batt::Status](#battstatus) | [batt::to_status](#battto_status) | [BATT_REQUIRE_OK](#batt_require_ok) |
| [batt::StatusCode](#battstatuscode) | [batt::OkStatus](#battokstatus) | [BATT_CHECK_OK](#batt_check_ok) |
| [batt::StatusOr&lt;T&gt;](#battstatusort) | [batt::status_from_retval](#battstatus_from_retval) | [BATT_OK_RESULT_OR_PANIC](#batt_ok_result_or_panic) |
| [batt::ErrnoValue](#batterrnovalue) | [batt::status_from_errno](#battstatus_from_errno) | [BATT_ASSIGN_OK_RESULT](#batt_assign_ok_result) |
| [batt::IsStatusOr&lt;T&gt;](#battisstatusort) | [batt::status_is_retryable](#battstatus_is_retryable) ||
| [batt::RemoveStatusOr&lt;T&gt;](#battremovestatusort) | [batt::is_ok_status](#battis_ok_status) ||
| [batt::LogLevel](#battloglevel) | [batt::ok_result_or_panic&lt;T&gt;](#battok_result_or_panict) ||

## Why not exceptions?

Batteries C++ discourages the use of exceptions as an error handling mechanism.  Exceptions tend to hurt the readability and maintainability of code because they introduce non-local control flow (throw statements can jump to catch statements arbitrarily far away with no obvious connection between the two points), break the separation of interface and implementation (because implementations can silently introduce new failure modes because of a deep change, even several layers down), and add to the complexity of syntax because when you program with exceptions now there are multiple ways to return a value from a function, each with its own caveats and idiosyncrasies.  The purported benefit of exceptions, that they clean up the code by hiding error handling code, does more harm than good because failure modes are an essential element to understanding any code.

## What instead of exceptions? (batt::Status)

In place of exceptions, Batteries provides `batt::Status` and many related constructs.

_DISCLAIMER/ACKNOWLEDGMENT: batt::Status is modelled very closely after [absl::Status](https://abseil.io/docs/cpp/guides/status) from Google's Abseil library._

A `batt::Status` is like an exception in that it represents a specific error condition, but it can also represent no error at all.  When a function that returns a value, say an `int`, can also fail, it is best to write it as:

```c++
batt::StatusOr<int> parse_int(std::string_view s);
```

In this example, we imagine a function that takes a string and parses it as an integer value.  Because this operation may fail (if for example it is handed an invalid string like "hello, world"), we declare the return type as `batt::StatusOr<int>`.

Functions returning no value that can fail should just return `batt::Status`.

To make the use of `Status` easier in practice, Batteries provides several macros to automatically unwrap a `Status` or propagate it up a call stack.  Example:

```c++
batt::StatusOr<Endpoint> resolve_url(std::string url);

batt::StatusOr<std::string> download_from_server(std::string url)
{
  // This will panic (crash) our program if the url fails to resolve.
  //
  Endpoint endpoint = BATT_OK_RESULT_OR_PANIC(resolve_url(url));
  
  // Connect to the server.
  //
  batt::StatusOr<Socket> maybe_socket = connect_to_server(endpoint);
  BATT_REQUIRE_OK(maybe_socket);
  //
  //   ^^^ This will cause the current function to return the non-ok status 
  //       code if there is an error, or continue otherwise.
  
  // We can unwrap the StatusOr like this.
  //
  Socket& socket = *maybe_socket;
  
  // This macro combines the two steps above, checking for errors 
  // and unwrapping the value.
  //
  BATT_ASSIGN_OK_RESULT(std::string data, socket.read_all());
  
  // Done!
  //
  return data;
}
```

## Standard Status Codes

Batteries provides the same standard status codes as Abseil Status, plus a few extras:

```c++
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
};
```

## Handling System Errors

In addition, `batt::Status` can accept standard `errno` values from system calls:

```c++
batt::Status example() 
{
  int fd = open("something.txt", 0);
  if (fd == -1) {
    return batt::status_from_errno(errno);
  }
  
  return batt::OkStatus();
}
```

In fact, because this pattern is so common when invoking system APIs, the above example can also be written as:

```c++
batt::Status even_better_example() 
{
  int fd = open("something.txt", 0);
  BATT_REQUIRE_OK(batt::status_from_retval(fd));
  
  return batt::OkStatus();
}
```

## batt::Status

### Summary

```c++
#include <batteries/status.hpp>
```

| Constructors                                          | Operators                           | Methods                       |                                       |
| :---------------------------------------------------- | :---------------------------------- | :---------------------------- | :------------------------------------ |
| [Status()](#battstatusstatus)                         | [operator=](#battstatusoperator=)   | [ok](#battstatusok)           | [group](#battstatusgroup)             |
| [Status(enum_value)](#battstatusstatusenum_value)     | [operator<<](#battstatusoperator<<) | [code](#battstatuscodemethod) | [IgnoreError](#battstatusignoreerror) |
| [Status(const Status&)](#battstatusstatusconststatus) | [operator==](#battstatusoperator==) | [message](#battstatusmessage) | [Update](#battstatusupdate)           |

| Static Methods                                                      | Types                               | Constants                                   |
| :------------------------------------------------------------------ | :---------------------------------- | :------------------------------------------ |
| [code_group_for_type](#battstatuscode_group_for_type)               | [value_type](#battstatusvalue_type) | [kGroupSizeBits](#battstatuskgroupsizebits) |
| [message_from_code](#battstatusmessage_from_code)                   | [CodeEntry](#battstatuscodeentry)   | [kGroupSize](#battstatuskgroupsize)         |
| [register_codes](#battstatusregister_codes)                         | [CodeGroup](#battstatuscodegroup)   | [kMaxGroups](#battstatuskmaxgroups)         |
| [unknown_enum_value_message](#battstatusunknown_enum_value_message) |                                     |                                             |

### Constructors

### Operators

### Methods

### Static Methods

### Types

### Constants
