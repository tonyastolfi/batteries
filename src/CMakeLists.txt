cmake_minimum_required(VERSION 3.16)
project(Batteries CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_definitions("-fno-diagnostics-color")

add_definitions("-std=c++17 -Wall -Wextra -Werror -DBOOST_ERROR_CODE_HEADER_ONLY -D_GNU_SOURCE -fno-omit-frame-pointer -g3")
if(NOT APPLE)
add_definitions("-DBOOST_STACKTRACE_USE_BACKTRACE")
endif()

include_directories(.)

remove_definitions(-DBOOST_STACKTRACE_USE_NOOP)

file(GLOB TestSources
     batteries/*_test.cpp
     batteries/*/*_test.cpp
     batteries/*.test.cpp
     batteries/*/*.test.cpp)

add_executable(BattTest ${TestSources})

set_target_properties(BattTest PROPERTIES LINK_FLAGS "")

if(APPLE)
  set(BACKTRACE_DEPENDENCY "")
else()
  set(BACKTRACE_DEPENDENCY "backtrace")
endif()

target_link_libraries(BattTest ${CONAN_LIBS_GTEST} boost_context dl ${BACKTRACE_DEPENDENCY})

enable_testing()

add_test(NAME BattTest
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
         COMMAND BattTest)

