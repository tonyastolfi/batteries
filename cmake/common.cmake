#
# Copyright 2021-2023 Anthony Paul Astolfi
#

# Enable link-time optimization. NOTE: currently breaks arm64 Linux (Apple Silicon)
#
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)


# The src dir should appear in the include path.
#
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

# Generate compile_commands.json.
#
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable ccache on systems where it is installed.
#
find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
  message("Enabled ccache builds")
endif ()


#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

macro (batt_add_library name)

  #+++++++++++-+-+--+----- --- -- -  -  -   -
  # Find source files.
  #
  file(GLOB_RECURSE ${name}_Sources
    LIST_DIRECTORIES false
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    )

  #+++++++++++-+-+--+----- --- -- -  -  -   -
  # Find test source files and remove them from the sources list.
  #
  file(GLOB_RECURSE ${name}_TestSources
    LIST_DIRECTORIES false
    ${CMAKE_CURRENT_SOURCE_DIR}/*.test.cpp
    )

  foreach (_file "FORCE_LIST_NOT_EMPTY;${${name}_TestSources}")
    list(REMOVE_ITEM ${name}_Sources ${_file})
  endforeach ()

  #+++++++++++-+-+--+----- --- -- -  -  -   -
  # Handle header-only library case.
  #
  if ("${${name}_Sources}" STREQUAL "")
    set(${name}_HeaderOnly TRUE)
  else ()
    set(${name}_HeaderOnly FALSE)
  endif ()

  if (${name}_HeaderOnly)
    add_library(${name} INTERFACE)
  else()
    add_library(${name} ${${name}_Sources})
  endif()

  #+++++++++++-+-+--+----- --- -- -  -  -   -
  # Add unit test executable.
  #
  if (NOT ("$ENV{${name}_BUILD_TESTS}" STREQUAL "0") AND
      NOT ("${${name}_TestSources}" STREQUAL ""))

    add_executable(${name}_Test ${${name}_TestSources})

    target_link_libraries(${name}_Test PRIVATE ${name})

  endif ()

endmacro ()
