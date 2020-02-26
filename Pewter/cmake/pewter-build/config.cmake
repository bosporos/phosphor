#
# project pewter-build
# author Maximilien M. Cura
#

# PLATFORM DEPENDENCIES

if(APPLE)
    set(pwt_platform_apple ON)
    set(pwt_platform_posix ON)
endif()

find_library(_pwt_lib_gl NAMES gl OpenGL)
if(NOT _pwt_lib_gl)
    message(FATAL_ERROR "Couldn't find libgl or libOpenGL.")
else()
    list(APPEND pwt_libraries ${_pwt_lib_gl})
endif()

find_library(_pwt_lib_glfw NAMES glfw)
if(NOT _pwt_lib_glfw)
    message(FATAL_ERROR "Couldn't find libglfw.")
else()
    list(APPEND pwt_libraries ${_pwt_lib_glfw})
endif()

add_library(glad SHARED)
target_include_directories(glad PUBLIC "${pwt_root}/glad/include")
target_sources(glad PRIVATE "${pwt_root}/glad/src/glad.c")
target_link_libraries(glad PUBLIC ${_pwt_lib_gl})

# COMPILER FLAGS

set(pwt_flags_cxx_optfile "${pwt_clangfile_dir}/${phos_build_type}/flags-cxx.txt")

set(_pwt_internal_diagnostic_supprting_compilers "Clang" "AppleClang" "GNU")

set(pwt_flags_global_cxx "")

if(${phos_build_type} STREQUAL Debug)
    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        string(APPEND pwt_flags_global_cxx "-fdiagnostics-color=always")
    else()
        string(APPEND pwt_flags_global_cxx "-fcolor-diagnostics")
    endif()
endif()

set(pwt_flags_cxx "")

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    list(APPEND pwt_flags_cxx "@${pwt_flags_cxx_optfile} ${pwt_flags_global_cxx}")
else()
    list(APPEND pwt_flags_cxx "--config ${pwt_flags_cxx_optfile} ${pwt_flags_global_cxx}")
endif()
