# Batteries C++ Coding Style Guide

This document describes the coding conventions to be followed in this library.

## File Names

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

```c++
// DO NOT write:
//
class MyClass {
 public:
  void my_method()
  {
    internal_method();
  }
 
 private:
  void internal_method() 
  {
    ++counter_;
  }
  
  int counter_;
};

// Instead, DO write:
//
class MyClass {
 public:
  void my_method()
  {
    this->internal_method();
  }
 
 private:
  void internal_method() 
  {
    ++this->counter_;
  }
  
  int counter_;
};
```
