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

message(STATUS "Pewter using phosphor root: ${phos_root}")
message(STATUS "Building Stormbreak version ${PROJECT_VERSION}")

# NO OPTIONS (yet) -- stormbreak/cmake/stormbreak-build/options.cmake

# STORMBREAK -- stormbreak/cmake/stormbreak-build/stormbreak.cmake

set(stb_root "${phos_root}/Stormbreak")
message(STATUS "Stormbreak root: ${stb_root}")
set(stb_clangfile_dir "${stb_root}/clang")
# set(stb_templatefile_dir "${pwt_root}/templates")

set(stb_sources "")
set(stb_libraries "")

function(stb_file file)
    if(EXISTS "${stb_root}/${file}")
        list(APPEND stb_sources "${stb_root}/${file}")
        set(stb_sources ${stb_sources} PARENT_SCOPE)
    else()
        message(NOTICE "Could not find file: ${file}")
    endif()
endfunction()

# TEMPLATES

# configure_file(
#     "${stb_templatefile_dir}/Stormbreak.h.in"
#     "Stb/Stb"
# )

# CONFIG -- stormbreak/cmake/stormbreak-build/config.cmake

set(stb_flags_cxx_optfile "${stb_clangfile_dir}/${phos_build_type}/flags-cxx.txt")

set(_stb_internal_diagnostic_supporting_compilers "Clang" "AppleClang" "GNU")

set(stb_flags_global_cxx "")

if(${phos_build_type} STREQUAL Debug)
    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        string(APPEND stb_flags_global_cxx "-fdiagnostics-color=always")
    else()
        string(APPEND stb_flags_global_cxx "-fcolor-diagnostics")
    endif()
endif()

set(stb_flags_cxx "")
