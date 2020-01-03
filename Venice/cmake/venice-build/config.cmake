#
# project venice-build
# author Maximilien M. Cura
#

# PLATFORM DEPENDENCIES

if(APPLE)
    set(vnz_platform_apple ON)
    set(vnz_platform_posix ON)
endif()

find_library(_vnz_pthread pthread)
if(NOT _vnz_pthread AND NOT PHOSPHOR_TARGET_OS_UNKOWN_SIENNA)
    message(FATAL_ERROR "Couldn't find libpthread")
else()
    list(APPEND vnz_libraries ${_vnz_pthread})
endif()

# COMPILER FLAGS

set(vnz_flags_cxx_optfile "${vnz_clangfile_dir}/${phos_build_type}/flags-cxx.txt")
set(vnz_flags_c_optfile "${vnz_clangfile_dir}/${phos_build_type}/flags-c.txt")
set(vnz_flags_asm_optfile "${vnz_clangfile_dir}/${phos_build_type}/flags-asm.txt")

set(_vnz_internal_diagnostic_supporting_compilers "Clang" "AppleClang" "GNU")

if(${CMAKE_ASM_COMPILER_ID} STREQUAL "GNU")
    set(_vnz_color_diagnostics_flag "-fdiagnostics-color=always")
else()
    set(_vnz_color_diagnostics_flag "-fcolor-diagnostics")
endif()

set(vnz_flags_global_cxx "")
set(vnz_flags_global_c "")
set(vnz_flags_global_asm "")

if(${phos_build_type} STREQUAL Debug)
    if(${CMAKE_ASM_COMPILER_ID} IN_LIST _vnz_internal_diagnostic_supporting_compilers)
        string(APPEND vnz_flags_global_asm ${_vnz_color_diagnostics_flag})
    endif()
    if(${CMAKE_C_COMPILER_ID} IN_LIST _vnz_internal_diagnostic_supporting_compilers)
        string(APPEND vnz_flags_global_c ${_vnz_color_diagnostics_flag})
    endif()
    if(${CMAKE_CXX_COMPILER_ID} IN_LIST
    _vnz_internal_diagnostic_supporting_compilers)
        string(APPEND vnz_flags_global_cxx ${_vnz_color_diagnostics_flag})
    endif()
endif()

set(vnz_flags_cxx "")
set(vnz_flags_c "")
set(vnz_flags_asm "")

if(${CMAKE_ASM_COMPILER_ID} STREQUAL "GNU")
    # GCC takes @arguments.txt
    list(APPEND vnz_flags_cxx "@${vnz_flags_cxx_optfile} @${vnz_flags_c_optfile} @${vnz_flags_asm_optfile} ${vnz_flags_global_cxx}")
    list(APPEND vnz_flags_c "@${vnz_flags_c_optfile} @${vnz_flags_asm_optfile} ${vnz_flags_global_c}")
    list(APPEND vnz_flags_asm "@${vnz_flags_asm_optfile} ${vnz_flags_global_asm}")
else()
    # Clang takes --config arguments.txt
    list(APPEND vnz_flags_cxx "--config ${vnz_flags_cxx_optfile} ${vnz_flags_global_cxx}")
    list(APPEND vnz_flags_c "--config ${vnz_flags_c_optfile} ${vnz_flags_global_c}")
    list(APPEND vnz_flags_asm "--config ${vnz_flags_asm_optfile} ${vnz_flags_global_asm}")
endif()
