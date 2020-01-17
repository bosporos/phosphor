#
# project venice-build
# author Maximilien M. Cura
#

## monk-mark! documented

#
# NOTE: THIS FILE EXPECTS TO BE CALLED FROM THE CMakeLists.txt IN THE PARENT DIRECTORY.
# RUNNING IT INDEPENDENTLY WILL LEAD TO PROBLEMS.
#

# Again, I don't actually know what the minimum required version to run this would
# be, so I just went for what I saw in the docs and what I had
cmake_minimum_required(VERSION 3.13)

# project declaration
# still needs the description to be fleshed out, a versioning policy, and url-fixes
# before release
# monk-todo! flesh out description
# monk-todo! update-on 'versioning policy'
# monk-todo![release:public] figure out the url situation
project (VENICE
    VERSION 0.0.0
    DESCRIPTION "The Venice project"
    HOMEPAGE_URL "https://dawn-project.sh/venice"
    LANGUAGES C CXX ASM
)

# diagnostic messages
message(STATUS "Venice using phosphor root: ${phos_root}")
message(STATUS "Building Venice version ${PROJECT_VERSION}")

# options.cmake declares the different options that Venice takes
include (cmake/venice-build/options.cmake)
# Declares various things that are necessary for Venice to be built
include (cmake/venice-build/venice.cmake)
# Setting up the relevant flags, etc.
include (cmake/venice-build/config.cmake)

#
# SOURCE FILES
#

# vnz_file will check the existence of the files before putting them on the compile
# list, so not-yet-existant files may be present in the following list:

vnz_file("venice/venice.cc")

vnz_file("venice/alloc/alloc.cc")

vnz_file("venice/sys/pthread-base/sync/mutex.cc")
vnz_file("venice/sys/pthread-base/sync/condvar.cc")
vnz_file("venice/sys/pthread-base/sync/rwlock.cc")
vnz_file("venice/sys/pthread-base/thread/id.cc")

vnz_file("venice/math/integers.cc")
vnz_file("venice/math/floating.cc")

vnz_file("venice/string/string.cc")
vnz_file("venice/arch/x86-64/string/utility.s")

vnz_file("venice/range/range.cc")

vnz_file("venice/alloc/alloc_routines.s")

# LIBRARIES

# a temporary measure, until we can get to freestanding
list(APPEND vnz_libraries stdc++)

# LIBRARY CONSTRUCTION

# config.cmake already put together the flags for the compilers, but it leaves the
# caller to tell CMake to use them, in case the caller wants to stick something in,
# so we take care of that here
string(APPEND CMAKE_ASM_FLAGS "${vnz_flags_asm}")
string(APPEND CMAKE_C_FLAGS "${vnz_flags_c}")
string(APPEND CMAKE_CXX_FLAGS "${vnz_flags_cxx}")

# at the moment, static builds aren't supported
if(VENICE_BUILD_SHARED)
    add_library(venice SHARED)
else()
    message(STATUS "Static libraries are not supported")
endif()

# prettify for diagnostics
string(REPLACE "${vnz_root}" "\n\t@" vnz_sources_pretty ${vnz_sources})
message(STATUS "Sources: ${vnz_sources_pretty}")

target_sources(venice PRIVATE ${vnz_sources})
set_property(TARGET venice PROPERTY
    CXX_STANDARD_20
)
target_include_directories(venice PUBLIC "${vnz_root}")
# For templated files (i.e. Venice/Venice)
target_include_directories(venice PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(venice PRIVATE)
target_link_libraries(venice PRIVATE ${vnz_libraries})
