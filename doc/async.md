# &lt;batteries/async/...&gt; : Async Tasks and I/O

## Quick Reference

| Name | Description |
| :--- | :---------- |
| [batt::Channel&lt;T&gt;](/_autogen/Classes/classbatt_1_1Channel) | Unbuffered single-producer/single-consumer (SPSC) communcation channel between Tasks |
| [batt::Future&lt;T&gt;](/_autogen/Classes/classbatt_1_1Future) | An asynchronously generated value of type T |
| [batt::Grant](/_autogen/Classes/classbatt_1_1Grant) | Counted resource allocation/sychronization (similar to counting semaphore) |
| [batt::Latch&lt;T&gt;](/_autogen/Classes/classbatt_1_1Latch) | A write-once, single-value synchronized container.  Similar to Future/Promise, but guaranteed not to alloc and has no defined copy/move semantics. |
| [batt::Mutex&lt;T&gt;](/_autogen/Classes/classbatt_1_1Mutex) | Mutual exclusion for use with batt::Task |
| [batt::Queue&lt;T&gt;](/_autogen/Classes/classbatt_1_1Queue) | Unbounded multi-producer/multi-consumer (MPMC) FIFO queue. |
| [batt::Task](/_autogen/Classes/classbatt_1_1Task) | Lightweight user-space thread for async I/O and high-concurrency programs. |
| [batt::Watch&lt;T&gt;](/_autogen/Classes/classbatt_1_1Watch) | Atomic variable with synchronous and asynchronous change notification. |
| <nobr>[&lt;batteries/async/handler.hpp&gt;](/_autogen/Files/handler_8hpp)</nobr> | Utilities for managing asynchronous callback handlers |

## batt::Task

[API Reference](/_autogen/Classes/classbatt_1_1Task)

A `batt::Task` is similar to `std::thread`, but much lighter weight.  Like `std::thread`, each `batt::Task` has an independent call stack.  Unlike `std::thread`, however, `batt::Task` is implemented 100% in user space, and does not support preemption.  This makes the context swap overhead of `batt::Task` very low in comparison to a `std::thread`, which means it is possible to have many more `batt::Task` instances without losing efficiency.

### Asynchronous I/O

The primary use case for `batt::Task` is to support asynchronous I/O for efficiency, while retaining the programming model of traditional threads.  This makes asynchronous code much easier to write, read, debug, and maintain than equivalent code using asynchronous continuation handlers (in the style of Boost Asio or Node.js).

An important feature of `batt::Task` to highlight is the static method [batt::Task::await](#batttaskawait).  This method allows the use of asynchronous continuation handler-based APIs (e.g., Boost Asio) in a "blocking" style.  Example:

```c++
#include <batteries/async/task.hpp>
#include <batteries/async/io_result.hpp>

#include <batteries/assert.hpp>
#include <batteries/utility.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

// Some function to get a server endpoint to which 
// to connect.
//
extern boost::asio::ip::tcp::endpoint get_server_endpoint();

int main() {
  // Create an io_context to schedule our Task and manage 
  // all asynchronous I/O.
  //
  boost::asio::io_context io;
  
  // Create a TCP/IP socket; we will use this to connect 
  // to the server endpoint.
  //
  boost::asio::ip::tcp::socket s{io};
  
  // Launch a task to act as our client.
  //
  batt::Task client_task{io.get_executor(), 
    /*body_fn=*/[&] 
    {
      // Connect to the server.  batt::Task::await will not 
      // return until the handler passed to `async_connect`
      // has been invoked.
      //
      boost::system::error_code ec = 
        batt::Task::await<boost::system::error_code>(
          [&](auto&& handler){
            s.async_connect(get_server_endpoint(), 
                            BATT_FORWARD(handler));
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

### Task Scheduling and Priorities

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

## Synchronization Primitives

### batt::Watch

[API Reference](/_autogen/Classes/classbatt_1_1Watch)

Watch is the simplest of Batteries' synchronization mechanisms.  It is like an atomic variable with the ability to wait on changes.  This is similar to a [Futex](https://en.wikipedia.org/wiki/Futex), and is used to implement the other synchronization primitives at a low level.

### batt::Mutex

[API Reference](/_autogen/Classes/classbatt_1_1Mutex)

Mutex provides mutual exclusion to an object.  It is based on batt::Watch, so it is fair, but it will not scale well as the number of tasks/threads attempting to acquire a Mutex concurrently.  It is up to the application programmer to avoid high levels of contention for a single Mutex.

An instance of `batt::Mutex<T>` may be locked in a few different ways:

#### Lock via guard class (similar to std::unique_lock)

```c++
batt::Mutex<std::string> s;
{
  batt::Mutex<std::string>::Lock lock{s};
  
  // Once the lock is acquired, you can access the protected object
  // via pointer...
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

#### Lock via lock() method

Equivalently, an instance of `batt::Mutex<T>::Lock` can be created via the `lock()` method:

```c++
batt::Mutex<std::string> s;
{
  auto locked = s.lock();
  
  static_assert(
    std::is_same_v<decltype(locked), 
                   batt::Mutex<std::string>::Lock>, 
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
// Since lock-free access to a type is by definition
// a subset of all access to that type, we define
// a base class containing all lock-free fields and
// member functions.
//
struct MyStateBase {
  explicit MyStateBase(std::string&& init_val) 
    : initial_value{std::move(init_val)}
  {}
 
  // This is safe to access concurrently because it is
  // const.
  //
  const std::string initial_value;
};

struct MyState : MyStateBase {
  // IMPORTANT: this member type alias tells batt::Mutex to
  // enable the `no_lock()` method/feature.
  //
  using ThreadSafeBase = MyStateBase;

  explicit MyState(std::string&& init_val)
    : MyStateBase{init_val}
    , current_value{init_val}
  {}
  
  // Because this field is non-const, it must be accessed
  // while holding a lock.
  //
  std::string current_value;
};

batt::Mutex<MyState> state{"initial"};

// batt::Mutex::nolock returns a reference to the 
// ThreadSafeBase type declared in MyState.
//
MyStateBase& base = state.no_lock();

// No lock needed to read a const value.
//
std::cout << "initial value = " 
          << base.initial_value << std::endl;

// We still need to acquire the lock to access the derived
// class.
//
state.with_lock([](MyState& s) {
  std::cout << "current value = " 
            << s.current_value << std::endl;
  s.current_value = "changed";
});
```

### batt::Channel

[API Reference](/_autogen/Classes/classbatt_1_1Channel)

Channel is a simple unbuffered SPSC primitive for transferring information between two tasks.  It can be used to implement zero-copy communication between tasks.

Example:

```c++
#include <batteries/async/channel.hpp>
#include <batteries/async/task.hpp>
#include <batteries/assert.hpp>

#include <boost/asio/io_context.hpp>

#include <iostream>
#include <string>

int main() {
  boost::asio::io_context io;
  
  // Create a channel to pass strings between tasks.
  //
  batt::Channel<std::string> channel;
  
  // The producer task reads lines from stdin until it gets an empty line.
  //
  batt::Task producer{io.get_executor(), [&channel]{
    std::string s;
    
    // Keep reading lines until an empty line is read.
    //
    for (;;) {
      std::getline(std::cin, s);

      if (s.empty()) {
        break;
      }

      batt::Status write_status = channel.write(s);
      BATT_CHECK_OK(write_status);
    }
    
    // Let the consumer task know we are done.
    //
    channel.close_for_write();
  }};

  // The consumer tasks reads strings from the channel, printing them to stdout
  // until the channel is closed.
  //
  batt::Task consumer{io.get_executor(), [&channel]{
    for (;;) {
      batt::StatusOr<std::string&> line = channel.read();
      if (!line.ok()) {
        break;
      }
      
      std::cout << *line << std::endl;
      
      // Important: signal to the producer that we are done with the string!
      //
      channel.consume();
    }
    
    channel.close_for_read();
  }};

  io.run();

  producer.join();
  consumer.join();

  return 0;
}
```

### batt::Future

[API Reference](/_autogen/Classes/classbatt_1_1Future)

### batt::Grant

[API Reference](/_autogen/Classes/classbatt_1_1Grant)

### batt::Latch

[API Reference](/_autogen/Classes/classbatt_1_1Latch)

If you don't need to move the Latch, then this mechanism is strictly more efficient than Future/Promise or Mutex.

### batt::Queue

[API Reference](/_autogen/Classes/classbatt_1_1Queue)

Note: This is an _unbounded_ FIFO implementation, so a back-pressure or rate-limiting mechanism is needed to prevent buffer bloat.
