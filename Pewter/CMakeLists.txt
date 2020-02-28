#
# project pewter-build
# author Maximilien M. Cura
#

## monk-mark! documented

#
# NOTE: THIS FILE EXPECTS TO BE CALLED FROM THE CMakeLists.txt IN THE PARENT DIRECTORY.
# RUNNING IT INDEPENDENTLY MAY LEAD TO UNEXPECTED RESULTS.
#

# Again, I don't actually know what the minimum required version to run this would
# be, so I just went for what I saw in the docs and what I had
cmake_minimum_required(VERSION 3.13)

# project declaration
# needs:
#   - a versioning policy
#   - url-fixes
# monk-todo! update-on 'versioning policy'
# monk-todo![release:public] figure out the url situation
project(PEWTER
    VERSION 0.0.0
    DESCRIPTION "The Pewter RL UI engine."
    HOMEPAGE_URL "https://dawn-project.sh/pewter"
    # C is only there for GLAD
    LANGUAGES C CXX
)

message(STATUS "Pewter using phosphor root: ${phos_root}")
message(STATUS "Building Pewter version ${PROJECT_VERSION}")

include(cmake/pewter-build/options.cmake)
include(cmake/pewter-build/pewter.cmake)
include(cmake/pewter-build/config.cmake)

#
# SOURCE FILES
#

# pwt_file will check the existence of the files before putting them on the compile
# list, so not-yet-existant files may be present in the following list:

pwt_file("Pewter/Renderers/TTY/TTY.cc")
pwt_file("Pewter/Display.cc")
pwt_file("Pewter/Glyph.cc")

#
# LIBRARIES
#

list(APPEND pwt_libraries stdc++ venice)

# LIBRARY CONSTRUCTION

string(APPEND CMAKE_CXX_FLAGS "${pwt_flags_cxx}")

if(PEWTER_BUILD_SHARED)
    add_library(pewter SHARED)
else()
    message(STATUS "Static libraries are not supported")
endif()

string(REPLACE "${pwt_root}" "\n\t@" pwt_sources_pretty "${pwt_sources}")
message(STATUS "Sources: ${pwt_sources_pretty}")

target_sources(pewter PRIVATE ${pwt_sources})
set_property(TARGET pewter PROPERTY
    CXX_STANDARD_20
)
target_include_directories(pewter PUBLIC "${pwt_root}")
target_include_directories(pewter PUBLIC "${phos_root}/Venice")
target_include_directories(pewter PUBLIC "${pwt_root}/glad/include")
# For template files (i.e. Pewter/Pewter)
target_include_directories(pewter PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(pewter PRIVATE)
target_link_libraries(pewter PRIVATE ${pwt_libraries})

add_executable(pewter-tty-test "test/pewter-tty-test.cc")
set_property(TARGET pewter-tty-test PROPERTY CXX_STANDARD_20)
target_link_libraries(pewter-tty-test PUBLIC pewter)
