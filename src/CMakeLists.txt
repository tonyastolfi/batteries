cmake_minimum_required(VERSION 2.8)
project(Batteries CXX)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_definitions("-std=c++17 -Wall -Wextra -Werror -DBOOST_ERROR_CODE_HEADER_ONLY -D_GNU_SOURCE -fno-omit-frame-pointer")
if(NOT APPLE)
add_definitions("-DBOOST_STACKTRACE_USE_BACKTRACE")
endif()

include_directories(.)

file(GLOB TestSources batteries/*_test.cpp)

add_executable(BattTest ${TestSources})

set_target_properties(BattTest PROPERTIES LINK_FLAGS "")

if(APPLE)
  set(BACKTRACE_DEPENDENCY "")
else()
  set(BACKTRACE_DEPENDENCY "backtrace")
endif()

target_link_libraries(BattTest ${CONAN_LIBS_GTEST} dl ${BACKTRACE_DEPENDENCY})

enable_testing()

add_test(NAME BattTest
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
         COMMAND BattTest)

