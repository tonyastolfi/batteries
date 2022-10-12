# &lt;batteries/async/...&gt; : Async Tasks and I/O

| Quick Reference         |                                     |                                     |
| :---------------------- | :---------------------------------- | :---------------------------------- |
| [batt::Task](#batttask) | [batt::Watch&lt;T&gt;](#battwatcht) | [batt::Mutex&lt;T&gt;](#battmutext) |

## batt::Task

### Summary

```c++
#include <batteries/async/task.hpp>
```

| Constructors |
| :----------- |
| [Task(executor, stack_size, body_fn)](#batttasktaskexecutor-stack_size-body_fn) |
| [Task(executor, body_fn, name, stack_size, stack_type, priority)](#batttasktaskexecutor-body_fn-name-stack_size-stack_type-priority) |


| Static Methods                                  |                                                       |                                               |
| :---------------------------------------------- | :---------------------------------------------------- | :-------------------------------------------- |
| [current](#batttaskcurrent)                     | [current_name](#batttaskcurrent_name)                 | [current_priority](#batttaskcurrent_priority) | 
| [current_stack_pos](#batttaskcurrent_stack_pos) | [current_stack_pos_of](#batttaskcurrent_stack_pos_of) | [default_name](#batttaskdefault_name)         | 
| [yield](#batttaskyield)                         | [sleep](#batttasksleep)                               | [await](#batttaskawait)                       |
| [backtrace_all](batttaskbacktrace_all)          |                                                       |                                               |


| Getters                               |                                       | Modifiers                             | Synchronization                           |
| :------------------------------------ | :------------------------------------ | :------------------------------------ | :---------------------------------------- |
| [id](#batttaskid)                     | [name](#batttaskname)                 | [try_join](#batttasktry_join)         | [join](#batttaskjoin)                     |
| [get_executor](#batttaskget_executor) | [get_priority](#batttaskget_priority) | [set_priority](#batttaskset_priority) | [call_when_done](#batttaskcall_when_done) |
| [stack_pos](#batttaskstack_pos)       | [stack_pos_of](#batttaskstack_pos_of) | [wake](#batttaskwake)                 |                                           |


### Description

A `batt::Task` is similar to `std::thread`, but much lighter weight.  Like `std::thread`, each `batt::Task` has an independent call stack.  Unlike `std::thread`, however, `batt::Task` is implemented 100% in user space, and does not support preemption.  This makes the context swap overhead of `batt::Task` very low in comparison to a `std::thread`, which means it is possible to have many more `batt::Task` instances without losing efficiency.

#### Asynchronous I/O

The primary use case for `batt::Task` is to support asynchronous I/O for efficiency, while retaining the programming model of traditional threads.  This makes asynchronous code much easier to write, read, debug, and maintain than equivalent code using asynchronous continuation handlers (in the style of Boost Asio or Node.js).

An important feature of `batt::Task` to highlight is the static method [batt::Task::await](#batttaskawait).  This method allows the use of asynchronous continuation handler-based APIs (e.g., Boost Asio) in a "blocking" style.  Example:

```c++
#include <batteries/async/task.hpp>
#include <batteries/async/io_result.hpp>

#include <batteries/assert.hpp>
#include <batteries/utility.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

// Some function to get a server endpoint to which to connect.
//
extern boost::asio::ip::tcp::endpoint get_server_endpoint();

int main() {
  // Create an io_context to schedule our Task and manage all asynchronous I/O.
  //
  boost::asio::io_context io;
  
  // Create a TCP/IP socket; we will use this to connect to the server endpoint.
  //
  boost::asio::ip::tcp::socket s{io};
  
  // Launch a task to act as our client.
  //
  batt::Task client_task{io.get_executor(), 
    /*body_fn=*/[&] 
    {
      // Connect to the server.  batt::Task::await will not return until the handler passed
      // to `async_connect` has been invoked.
      //
      boost::system::error_code ec = batt::Task::await<boost::system::error_code>([&](auto&& handler){
        s.async_connect(get_server_endpoint(), BATT_FORWARD(handler));
      });
      
      BATT_CHECK(!ec);
      
      // Interact with the server via the connected socket...
    }};

  // VERY IMPORTANT: without this line, nothing will happen!
  //
  io.run();

  return 0;
}
```

All continuation handler based async APIs require a callback (the continuation handler).  In order to simplify the code, we want to "pause" our code until the I/O is finished, but the async API, `async_connect` in this case, will return immediately.  [batt::Task::await](#batttaskawait) gives us access to the "continuation" of the task, in this case everything that happens after `await` returns, as a handler that can be passed directly to `async_connect`.  All the context swapping, scheduling, memory managment, and synchronization is handled automatically by `batt::Task`, allowing the programmer to focus on the application's natural flow of control, and not the mechanics used to implement this flow.

#### Task Scheduling and Priorities

When a `batt::Task` is created, it is passed a Boost Asio executor object.  All execution of task code on the task's stack will happen via `boost::asio::dispatch` or `boost::asio::post` using this executor.  NOTE: this means if, for example, you use the executor from a `boost::asio::io_context` to create a task, that task will not run unless you call `io_context::run()`!

A running `batt::Task` is never preempted by another task.  Instead it must yield control of the current thread to allow another task to run.  There are four ways to do this:

* [batt::Task::join()](#batttaskjoin)
* [batt::Task::await()](#batttaskawait)
* [batt::Task::yield()](#batttaskyield)
* [batt::Task::sleep()](#batttasksleep)

WARNING: if you use a kernel or standard library synchronization mechanism or blocking call, for example `std::mutex`, from inside a `batt::Task`, that task WILL NOT YIELD!  This is why the Batteries Async library contains its own synchronization primitives, like `batt::Watch`, `batt::Queue`, and `batt::Mutex`.  All of these primitives are implemented on top one or more of the four methods enumerated above.

Each `batt::Task` is assigned a scheduling priority, which is a signed 32-bit integer.  Greater values of the priority int mean more urgent priority; lesser values mean less urgency.  Even though tasks don't interrupt each other (i.e. preemption), they sometimes perform actions that cause another task to move from a "waiting" state to a "ready to run" state.  For example, one task may be blocked inside a call to `batt::Watch<T>::await_equal`, and another task (let's call it the "notifier") may call `batt::Watch<T>::set_value`, activating the first task (let's call it the "listener").  As noted above, the newly activated ("listener") task will be run via `boost::asio::dispatch` or `boost::asio::post`.  Which mechanism is used depends on the relative priority of the two tasks:

* If the "notifier" has a higher (numerically greater) priority value than the "listener", the "listener" is scheduled via `boost::asio::post`.
* Otherwise, the "notifier" is immediately suspended and re-scheduled via `boost::asio::post`; then the "listener" is scheduled via `boost::asio::dispatch`.

In any case, activating another task will _not_ cause a running task to go from a "running" state to a "waiting" state.  It may however "bounce" it to another thread, or to be pushed to the back of an execution queue.  This only matters when there are more tasks ready to run than there are available threads for a given ExecutionContext: higher priority tasks are scheduled before lower priority ones in general.

NOTE: you may still end up with a priority inversion situation when multiple tasks with different priorities are `boost::asio::post`-ed to the same queue.  In this case, there is no mechanism currently for re-ordering the tasks to give preference based on priority.

Overall, it is best to consider priority "best-effort" rather than a guarantee of scheduling order.  It should be used for performance tuning, not to control execution semantics in a way that affects the functional behavior of a program.

### Methods

#### [batt::Task](#batttask)::Task(executor, stack_size, body_fn)

Create a new Task with a custom stack size.

```c++
template <typename BodyFn = void()>
explicit Task(const boost::asio::any_io_executor& ex, 
              batt::StackSize stack_size, 
              BodyFn&& body_fn) noexcept;
```

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::Task(executor, body_fn, name, stack_size, stack_type, priority)

Create a new Task, optionally setting name, stack size, stack type, and priority.

```c++
template <typename BodyFn = void()>
explicit Task(const boost::asio::any_io_executor& ex, 
              BodyFn&& body_fn,
              std::string&& name = Task::default_name(), 
              StackSize stack_size = batt::StackSize{512 * 1024},
              batt::StackType stack_type = batt::StackType::kFixedSize, 
              batt::Optional<Priority> priority = None) noexcept;
```

The default priority for a Task is the current task priority plus 100; this means that a new Task by default will always "soft-preempt" the currently running task.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::await

Block the current thread until some asynchronous operation completes.

```c++
template <typename R, typename Fn=void(Handler)>
static R await(Fn&& fn);                                               // (1)

template <typename R, typename Fn=void(Handler)>
static R await(batt::StaticType<R>, Fn&& fn);                          // (2)

template <typename T>
static batt::StatusOr<T> await(const batt::Future<T>& future_result);  // (3)
```

Overloads (1) &amp; (2) take a function whose job is to initiate an asynchronous operation.  Since async ops require a callback (which is invoked when the op completes), the function `fn` is passed a `Handler` which serves to wake up the task and resume its execution.  Example:

```c++
boost::asio::ip::tcp::socket s;

using ReadResult = std::pair<boost::system::error_code, std::size_t>;

ReadResult r = batt::Task::await<ReadResult>([&](auto&& handler) {
    s.async_read_some(buffers, BATT_FORWARD(handler));
  });

if (r.first) {
  std::cout << "Error! ec=" << r.first;
} else {
  std::cout << r.second << " bytes were read.";
}
```

The template parameter `R` is the return type of the `await` operation.  `R` can be any type that is movable and constructible from the arguments passed to the callback `Handler`.  In the example above, we define `R` to be a `std::pair` of the error code and the count of bytes read (size_t).  This is the exact signature required by the async op initiated inside the lambda expression passed to `await`.  When the socket implementation causes `handler(ec, n_bytes_read)` to be invoked, the `Handler` provided by `batt::Task::await` constructs an instance of `R` by forwarding the handler arguments (`auto retval = std::make_pair(ec, n_bytes_read);`), and resumes the task that was paused inside `await`, returning `retval`.

##### Return Value

* (1) &amp; (2): the instance of `R` constructed from callback arguments.
* (3): the value of the passed `batt::Future<T>` if successful; non-ok status if the future completed with an error.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::backtrace_all

Dumps stack trace and debug information for all active `batt::Task`s to stderr.

```c++
static i32 backtrace_all(bool force);
```

If `force` is true, then this function will attempt to dump debug information for running tasks, even though this may cause data races (if you're debugging a tricky threading issue, sometimes the risk of a crash is outweighed by the benefit of some additional clues about what's going on!).

##### Return Value

The number of tasks dumped.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::call_when_done
    
Attaches a listener callback to the task; this callback will be invoked when the task completes execution.

```c++
template <typename F = void()>
void call_when_done(F&& handler);
```

This method can be thought of as an asynchronous version of [join()](#batttaskjoin).

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::current

Returns a reference to the currently running Task, if there is one.

```c++
static batt::Task& current();
```

WARNING: if this method is called outside of any `batt::Task`, behavior is undefined.

##### Return Value

The current task.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::current_name

Returns the current task name, or "" if there is no current task.

```c++
static std::string_view current_name();
```

Unlike [batt::Task::current()](#batttaskcurrent), this method is safe to call outside a task.

##### Return Value

The current task name.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::current_priority

```c++
static batt::Task::Priority current_priority();
```

NOTE: this function is safe to call outside of a task; in this case, the default priority (0) is returned.

See [Task Scheduling and Priorities](#task-scheduling-and-priorities).

##### Return Value

The priority of the current task.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::current_stack_pos

Returns the offset of the current locus of control relative to the base of the stack.

```c++
static batt::Optional<usize> current_stack_pos();
```

##### Return Value

* If called from inside a task, the current stack position in bytes
* Else `batt::None`

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::current_stack_pos_of

Returns the offset of some address relative to the base of the current task stack.

```c++
static batt::Optional<usize> current_stack_pos_of(const volatile void* ptr);
```

NOTE: If `ptr` isn't actually on the current task's stack, then this function will still return a number, but it will be essentially a garbage value.  It's up to the caller to make sure that `ptr` points at something on the task stack.

##### Return Value

* If called from inside a task, the stack offset in bytes of `ptr`
* Else `batt::None`

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::default_name

```c++
static std::string default_name()
```

##### Return Value

The name given to a `batt::Task` if none is passed into the constructor.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::sleep

Puts a `batt::Task` to sleep for the specified real-time duration.

```c++
template <typename Duration = boost::posix_time::ptime>
static batt::ErrorCode sleep(const Duration& duration);
```

This operation can be interrupted by a [batt::Task::wake()](#batttaskwake), in which case a "cancelled" error code is returned instead of success (no error).

This method is safe to call outside a task; in this case, it is implemented via `std::this_task::sleep_for`.

##### Return Value

* `batt::ErrorCode{}` (no error) if the specified duration passed
* A value equal to `boost::asio::error::operation_aborted` if `batt::Task::wake()` was called on the given task

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::get_executor

Returns a copy of the executor passed to the given task at construction time.

```c++
batt::Task::executor_type get_executor() const;
```

##### Return Value

The executor for this task.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::get_priority

Get the priority of this task.

```c++
batt::Task::Priority get_priority() const;
```

See [Task Scheduling and Priorities](#task-scheduling-and-priorities).

##### Return Value

The priority of the task.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::id

Get the process-unique serial number of this task.

```c++
i32 id() const;
```

##### Return Value

The serial number of the task.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::join

Block until the task has finished.

```c++
void join();
```

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::name

Get the human-friendly name of this task.

```c++
std::string_view name() const;
```

This string is not necessarily unique to this task.  It is optionally passed in when the task is constructed.  The purpose is to identify the task for debugging purposes.

##### Return Value

The name of the task.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::set_priority

Set the scheduling priority of this task.

```c++
void set_priority(batt::Task::Priority new_priority);
```

See [Task Scheduling and Priorities](#task-scheduling-and-priorities).

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::stack_pos

Get the byte offset between the _current_ stack position and the base of this task's stack.  This value is only meaningful if this method is called while on the current task.  Usually you should just call [batt::Task::current_stack_pos()](#batttaskcurrent_stack_pos) instead.

```c+++
usize stack_pos() const;
```

##### Return Value

The byte offset between the current stack position and the base of this task's stack.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::stack_pos_of

Get the byte offset between the given pointer and the base of this task's stack.  It is up to the caller to make sure that `ptr` is the address of something on the task's stack.  Usually you should just call [batt::Task::current_stack_pos_of](#batttaskcurrent_stack_pos_of) instead.

```c++
usize stack_pos_of(const volatile void* ptr) const;;
```

##### Return Value

The byte offset between the given pointer and the base of this task's stack.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::try_join

Test whether this task has finished.

```c++
bool try_join();
```

This function is guaranteed never to block.

See [batt::Task::join()](#batttaskjoin).

##### Return Value

`true` iff the task has finished executing.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::wake

Interrupts a call to `batt::Task::sleep` on the given task.

```c++
bool wake();
```

NOTE: if the given task is suspended for some other reason (i.e., it is not inside a call to `batt::Task::sleep`), this method will **not** wake the task (in this case it will return `false`).

##### Return Value

* `true` iff the task was inside a call to `sleep` and was successfully activated

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Task](#batttask)::yield

Suspend the current task and immediately schedule it to resume via `boost::asio::post`.

```c++
static void yield();
```

<hr><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->
<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

## batt::Watch&lt;T&gt;

### Summary

```c++
#include <batteries/async/watch.hpp>
```

| Constructors                            |
| :-------------------------------------- |
| [Watch()](#battwatchwatch)              |
| [Watch(T)](#battwatchwatcht-init_value) |

| Getters                           | Modifiers                                                            || Synchronization                               || 
| :-------------------------------- | :-------------------------------- | :-------------------------------- | :--------------------------------------------- ||
| [is\_closed](#battwatchis_closed) | [close](#battwatchclose)          | [fetch\_add](#battwatchfetch_add) | [async_wait](#battwatchasync_wait) | [await\_not\_equal](#battwatchawait_not_equal)|
| [get\_value](#battwatchget_value) | [set_value](#battwatchset_value)  | [fetch\_sub](#battwatchfetch_sub) || [await\_equal](#battwatchawait_equal)  | 
|                                   | [modify](#battwatchmodify)        | [fetch\_or](#battwatchfetch_or)   || [await\_true](#battwatchawait_true)            |
|                                   | [modify\_if](#battwatchmodify_if) | [fetch\_and](#battwatchfetch_sub) || [await\_modify](#battwatchawait_modify)        |

### Description

A `batt::Watch` is like a `std::atomic` that you can block on, synchronously and asynchronously.  Like `std::atomic`, it has methods to atomically get/set/increment/etc.  But unlike `std::atomic`, you can also block a task waiting for some condition to be true.  Example:

```c++
#include <batteries/async/watch.hpp>
#include <batteries/assert.hpp>  // for BATT_CHECK_OK
#include <batteries/status.hpp>  // for batt::Status

int main() {
  batt::Watch<bool> done{false};
  
  // Launch some background task that will do stuff, then set `done` 
  // to `true` when it is finished.
  //
  launch_background_task(&done);

  batt::Status status = done.await_equal(true);
  BATT_CHECK_OK(status);
  
  return 0;
}
```

### Methods

#### [batt::Watch](#battwatcht)::Watch()

Constructs a `batt::Watch` object with a default-initialized value of `T`.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Watch](#battwatcht)::Watch(T init_value)

Constructs a `batt::Watch` object with the given initial value.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Watch](#battwatcht)::async_wait

Invokes the passed handler `fn` with the described value as soon as one of the following conditions is true:
 * When the Watch value is _not_ equal to the passed value `last_seen`, invoke `fn` with the current value of the Watch.
 * When the Watch is closed, invoke `fn` with `batt::StatusCode::kClosed`.

```c++
template <typename Handler = void(batt::StatusOr<T> new_value)>
void async_wait(T last_seen, Handler&& fn) const;
```

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Watch](#battwatcht)::await_equal

Blocks the current task/thread until the Watch contains the specified value.

```c++
batt::Status await_equal(T val) const;
```

##### Return Value

* `batt::OkStatus()` if the Watch value was observed to be `val`
* `batt::StatusCode::kClosed` if the Watch was closed before `val` was observed

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->


#### [batt::Watch](#battwatcht)::await_modify

Retries `fn` on the Watch value until it succeeds or the Watch is closed.

```c++
template <typename Fn = batt::Optional<T>(T)>
batt::StatusOr<T> await_modify(Fn&& fn);
```

If `fn` returns `batt::None`, this indicates `fn` should not be called again until a new value is available.

`fn` **MUST** be safe to call multiple times within a single call to `await_modify`.  This is because `await_modify` may be implemented via an atomic compare-and-swap loop.

##### Return Value

* If successful, the old (pre-modify) value on which `fn` finally succeeded
* `batt::StatusCode::kClosed` if the Watch was closed before `fn` was successful

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->


#### [batt::Watch](#battwatcht)::await_not_equal

Blocks the current task/thread until the Watch value is _not_ equal to `last_seen`.

```c++
batt::StatusOr<T> await_not_equal(T last_seen);
```

##### Return Value

* On success, the current value of the Watch, which is guaranteed to _not_ equal `last_seen`
* `batt::StatusCode::kClosed` if the Watch was closed before a satisfactory value was observed

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->


#### [batt::Watch](#battwatcht)::await_true

Blocks the current task/thread until the passed predicate function returns `true` for the current value of the Watch.

```c++
template <typename Pred = bool(T)>
batt::StatusOr<T> await_true(Pred&& pred);
```

This is the most general of Watch's blocking getter methods.

##### Return Value

* On success, the Watch value for which `pred` returned `true`
* `batt::StatusCode::kClosed` if the Watch was closed before a satisfactory value was observed

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->


#### [batt::Watch](#battwatcht)::close

Set the Watch to the "closed" state, which disables all blocking/async synchronization on the Watch, immediately unblocking any currently waiting tasks/threads.

```c++
void close();
```

This method is safe to call multiple times.  The Watch value can still be modified and retrieved after it is closed; this only disables the methods in the "Synchronization" category (see Summary section above).

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->


#### [batt::Watch](#battwatcht)::fetch_add

Atomically adds the specified amount to the Watch value, returning the previous value.

```c++
T fetch_add(T arg);
```

_NOTE: This method is only available if T is a primitive integer type._

##### Return Value

The prior value of the Watch.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->


#### [batt::Watch](#battwatcht)::fetch_and

Atomically sets the Watch value to the bitwise-and of the current value and the passed `arg`, returning the previous value.

```c++
T fetch_and(T arg);
```

_NOTE: This method is only available if T is a primitive integer type._

##### Return Value

The prior value of the Watch.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->


#### [batt::Watch](#battwatcht)::fetch_or

Atomically sets the Watch value to the bitwise-and of the current value and the passed `arg`, returning the previous value.

```c++
T fetch_or(T arg);
```

_NOTE: This method is only available if T is a primitive integer type._

##### Return Value

The prior value of the Watch.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->


#### [batt::Watch](#battwatcht)::fetch_sub

Atomically subtracts the specified amount to the Watch value, returning the previous value.

```c++
T fetch_sub(T arg);
```

_NOTE: This method is only available if T is a primitive integer type._

##### Return Value

The prior value of the Watch.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->


#### [batt::Watch](#battwatcht)::get_value

Returns the current value of the Watch to the caller.

```c++
T get_value() const;
```

##### Return Value

The current Watch value.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->


#### [batt::Watch](#battwatcht)::is_closed

Returns whether the Watch is in a "closed" state.

```c++
bool is_closed() const;
```

##### Return Value

* `true` if the Watch is closed
* `false` otherwise

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

#### [batt::Watch](#battwatcht)::modify

Atomically modifies the Watch value by applying the passed transform `fn`.

```c++
template <typename Fn = T(T)>
T modify(Fn&& fn);
```

`fn` **MUST** be safe to call multiple times within a single call to `modify`.  This is because `modify` may be implemented via an atomic compare-and-swap loop.

##### Return Value

* if `T` is a primitive integer type (including `bool`), the new value of the Watch
* else, the old value of the Watch

***NOTE: This behavior is acknowledged to be less than ideal and will be fixed in the future to be consistent, regardless of `T`***

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->


#### [batt::Watch](#battwatcht)::modify_if

Retries calling `fn` on the Watch value until EITHER of: 
  * `fn` returns `batt::None`
  * BOTH of: 
    * `fn` returns a non-`batt::None` value
    * the Watch value is atomically updated via compare-and-swap

```c++
template <typename Fn = batt::Optional<T>(T)>
batt::Optional<T> modify_if(Fn&& fn);
```

`fn` **MUST** be safe to call multiple times within a single call to `modify_if`.  This is because `modify_if` may be implemented via an atomic compare-and-swap loop.

Unlike [await\_modify](#battwatchawait_modify), this method never puts the current task/thread to sleep; it keeps _actively_ polling the Watch value until it reaches one of the exit criteria described above.

##### Return Value

The final value returned by `fn`, which is either `batt::None` or the new Watch value.

<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->


#### [batt::Watch](#battwatcht)::set_value

Atomically set the value of the Watch.

```c++
void set_value(T new_value);
```

<hr><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->
<br><!-- ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   - -->

## batt::Mutex&lt;T&gt;

### Summary

```c++
#include <batteries/async/mutex.hpp>
```

| Constructors | 
| :- |
| [Mutex()](#battmutexmutex) |
| [Mutex(args...)](#battmutexmutexargs) |

| Methods |||
| :- | :- | :- |
| [lock](#battmutexlock) | [lock (const)](#battmutexlock-const) | [with_lock](#battmutexwith_lock) |

| Operators |
| :- |
| [operator-&gt;](#battmutexoperator-&gt;) |

| Static Methods |
| :- |
| [thread_safe_base](#battmutexthreadsafebase) |

### Description

Provides mutually-exclusive access to an instance of type `T`.  This class has two advantages over `std::mutex`:

1. It will yield the current `batt::Task` (if there is one) when blocking to acquire a lock, allowing the current thread to be used by other tasks
2. By embedding the protected type `T` within the object, there is a much lower chance that state which should be accessed via a mutex will accidentally be accessed directly

This mutex implementation is mostly fair because it uses [Lamport's Bakery Algorithm](https://en.wikipedia.org/wiki/Lamport's_bakery_algorithm).  It is non-recursive, so threads/tasks that attempt to acquire a lock that they already have will deadlock.  Also, an attempt to acquire a lock on a `batt::Mutex` can't be cancelled, so it is not possible to set a timeout on lock acquisition.

An instance of `batt::Mutex<T>` may be locked in a few different ways:

#### Lock via guard class (similar to std::unique_lock)

```c++
batt::Mutex<std::string> s;
{
  batt::Mutex<std::string>::Lock lock{s};
  
  // Once the lock is acquired, you can access the protected object via pointer...
  //
  std::string* ptr = lock.get();

  // ... or by reference ...
  //
  std::string& ref = lock.value();
  
  // ... by operator* like a smart pointer...
  //
  std::string& ref2 = *lock;
  
  // ... or you can access its members via operator->:
  //
  const char* cs = lock->c_str();
}
// The lock is released when the guard class goes out of scope.
```

Equivalently, an instance of `batt::Mutex<T>::Lock` can be created via the `lock()` method:

```c++
batt::Mutex<std::string> s;
{
  auto locked = s.lock();
  
  static_assert(std::is_same_v<decltype(locked), batt::Mutex<std::string>::Lock>, 
                "It is nice to use auto in this case!");
}
```

As the second example implies, `batt::Mutex<T>::Lock` is a movable type (however it is non-copyable).

#### Run function with lock acquired

```c++
batt::Mutex<std::string> s{"Some string"};

s.with_lock([](std::string& locked_s) {
    locked_s += " and then some!";
});
```

#### Lock-free access to T

Even though access to the protected object of type `T` mostly happens via a lock, `batt::Mutex` supports types with a partial interface that is thread-safe without locking.  Example:

```c++
struct MyStateBase {
  explicit MyStateBase(std::string&& init_val) 
    : initial_value{std::move(init_val)}
  {}
 
  const std::string initial_value;
};

struct MyState : MyStateBase {
  // IMPORTANT: this member type alias tells batt::Mutex to enable the `no_lock()` method/feature.
  //
  using ThreadSafeBase = MyStateBase;

  explicit MyState(std::string&& init_val)
    : MyStateBase{init_val}
    , current_value{init_val}
  {}
};

batt::Mutex<MyState> state{"initial"};

// No lock needed to read a const value.
//
std::cout << "initial value = " << state.no_lock().initial_value << std::endl;

// We still need to acquire the lock to access the derived class.
//
state.with_lock([](MyState& s) {
  std::cout << "current value = " << s.current_value << std::endl;
  s.current_value = "changed";
});

// batt::Mutex::nolock returns a reference to the ThreadSafeBase type declared in MyState.
//
MyStateBase& base = state.no_lock();
```
