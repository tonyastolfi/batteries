---
author: "Tony"
title: "<batteries/static_assert.hpp>"
tags: 
    - diagnostics
    - compile-time
    - type traits
---
Comparison based static assertion macros that, when failure occurs, cause compiler output to contain the values being compared.

| Macro Name | Assertion |
| ---------- | --------- |
| {{< doxdefine file="static_assert.hpp" name="BATT_STATIC_ASSERT_EQ" >}}(x,y) | x == y |
| {{< doxdefine file="static_assert.hpp" name="BATT_STATIC_ASSERT_NE" >}}(x,y) | x != y |
| {{< doxdefine file="static_assert.hpp" name="BATT_STATIC_ASSERT_LT" >}}(x,y) | x < y |
| {{< doxdefine file="static_assert.hpp" name="BATT_STATIC_ASSERT_GT" >}}(x,y) | x > y |
| {{< doxdefine file="static_assert.hpp" name="BATT_STATIC_ASSERT_LE" >}}(x,y) | x <= y |
| {{< doxdefine file="static_assert.hpp" name="BATT_STATIC_ASSERT_GE" >}}(x,y) | x >= y |


[File Reference](/reference/files/static__assert_8hpp)
<!--more-->
