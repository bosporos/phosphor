#
# project venice-build
# author Maximilien M. Cura
#

cmake_minimum_required(VERSION 3.13)

project (VENICE
    VERSION 0.0.0
    DESCRIPTION "The Venice project"
    HOMEPAGE_URL "https://dawn-project.sh/venice"
    LANGUAGES C CXX ASM
)

message(STATUS "Venice using phosphor root: ${phos_root}")
message(STATUS "Building Venice version ${PROJECT_VERSION}")

include (cmake/venice-build/options.cmake)
include (cmake/venice-build/venice.cmake)
include (cmake/venice-build/config.cmake)

# SOURCE FILES

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

# LIBRARIES

# temporary measure
list(APPEND vnz_libraries stdc++)

# LIBRARY CONSTRUCTION

string(APPEND CMAKE_ASM_FLAGS "${vnz_flags_asm}")
string(APPEND CMAKE_C_FLAGS "${vnz_flags_c}")
string(APPEND CMAKE_CXX_FLAGS "${vnz_flags_cxx}")

if(VENICE_BUILD_SHARED)
    add_library(venice SHARED)
else()
    message(STATUS "Static libraries are not supported")
endif()

string(REPLACE "${vnz_root}" "@" vnz_sources_pretty ${vnz_sources})
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
