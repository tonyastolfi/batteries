cmake_minimum_required(VERSION 2.8)
project(Batteries CXX)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

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

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Thank you to Victoria Rudakova for this Doxygen/CMake integration code!
#  (https://vicrucann.github.io/tutorials/quick-cmake-doxygen/)
#
option(BUILD_DOC "Build documentation" OFF)
if (BUILD_DOC)
  find_package(Doxygen)
  if (DOXYGEN_FOUND)
    # set input and output files
#    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/../doc/Doxyfile.in)
#    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/doc_doxygen/Doxyfile)

    # request to configure the file
#    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
#    message("Doxygen build started")

    # note the option ALL which allows to build the docs together with the application
#    add_custom_target( doxygen ALL
#        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
#        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc_doxygen
#        COMMENT "Generating API documentation with Doxygen"
#        VERBATIM )
#    message("Doxygen target added")
      
  else (DOXYGEN_FOUND)
    message("Doxygen need to be installed to generate the doxygen documentation")
  endif (DOXYGEN_FOUND)
endif (BUILD_DOC)
#
#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

target_link_libraries(BattTest ${CONAN_LIBS_GTEST} boost_context dl ${BACKTRACE_DEPENDENCY})

enable_testing()

add_test(NAME BattTest
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
         COMMAND BattTest)

