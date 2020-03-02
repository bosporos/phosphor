#
# project stormbreak
# author Maximilien M. Cura

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

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    list(APPEND stb_flags_cxx "@${stb_flags_cxx_optfile} ${stb_flags_global_cxx}")
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang")
    list(APPEND stb_flags_cxx "--config ${stb_flags_cxx_optfile} ${stb_flags_global_cxx}")
endif()
