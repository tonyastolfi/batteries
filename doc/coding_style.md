# Batteries C++ Coding Style Guide

This document describes the coding conventions to be followed in this library.

## File Names

All source code files live under `batteries/src`.

- Source, header, inline/implementation, and test sources should be colocated within the same directory.
- Source files should use the `.cpp` extension.
- Header files should use the `.hpp` extension.
- Test files for a given source/header should use the `.test.cpp` suffix.

For example, if you have a header file: `src/some_namespace/myutils.hpp`, then you should also have:

- `src/some_namespace/myutils.cpp`
- `src/some_namespace/myutils.test.cpp`
- `src/some_namespace/myutils.ipp`

## Class Member Access

To enhance readability, all implicit uses of `this` within a class should be made explicit.  For example:

**DO NOT** write:

```c++
class MyClass {
 public:
  void my_method()
  {
    internal_method();  // BAD!
  }
 
 private:
  void internal_method() 
  {
    ++counter_;  // BAD!
  }
  
  int counter_;
};
```

Instead, **DO** write:

```c++
class MyClass {
 public:
  void my_method()
  {
    this->internal_method();  // GOOD!
  }
 
 private:
  void internal_method() 
  {
    ++this->counter_;  // GOOD!
  }
  
  int counter_;
};
```
