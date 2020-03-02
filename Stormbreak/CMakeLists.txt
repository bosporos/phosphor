#
# project stormbreak
# author Maximilien M. Cura
#

#
# NOTE: THIS FILE EXPECTS TO BE CALLED FROM THE CMakeLists.txt IN THE PARENT DIRECTORY.
# RUNNING IT INDEPENDENTLY MAY LEAD TO UNEXPECTED RESULTS.
#

cmake_minimum_required(VERSION 3.13)

project(STORMBREAK
    VERSION 0.0.0
    DESCRIPTION "The Stormbreak Roguelike"
    HOMEPAGE_URL "https://dawn-project.sh/stormbreak"
    LANGUAGES CXX
)

message(STATUS "Stormbreak using phosphor root: ${phos_root}")
message(STATUS "Building Stormbreak version ${PROJECT_VERSION}")

include (stb-preamble.cmake)

#
# SOURCE FILES
#

stb_file("STB/STB.cc")

#
# LIBRARIES
#

list(APPEND stb_libraries stdc++ venice pewter)

# EXECUTABLE

string(APPEND CMAKE_CXX_FLAGS "${stb_flags_cxx}")

add_executable(stormbreak)

string(REPLACE "${stb_root}" "\n\t@" stb_sources_pretty "${stb_sources}")
message(STATUS "Sources: ${stb_sources_pretty}")

target_sources(stormbreak PRIVATE ${stb_sources})
set_property(TARGET stormbreak PROPERTY CXX_STANDARD_20)

target_include_directories(stormbreak PUBLIC "${stb_root}")
target_include_directories(stormbreak PUBLIC "${phos_root}/Venice")
target_include_directories(stormbreak PUBLIC "${phos_root}/Pewter")
target_include_directories(stormbreak PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(stormbreak PRIVATE ${stb_libraries})
