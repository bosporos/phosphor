#
# project phosphor-central-build
# author Maximilien M. Cura
#

# ===========
# GLOBAL VARS
# ===========

set (phos_root ${CMAKE_SOURCE_DIR})
set (phos_build_type ${CMAKE_BUILD_TYPE})

# ================
# PROJECT HANDLING
# ================

# string (TOLOWER ${PHOSPHOR_ENABLE_PROJECTS} _phos_userspec_projects)
set(_phos_userspec_projects ${PHOSPHOR_ENABLE_PROJECTS})
list(TRANSFORM _phos_userspec_projects TOLOWER)

set (_phos_enabled_projects "")
set (_phos_disabled_projects "")

function (phos_add_proj proj)
    string(TOLOWER ${proj} _proj)

    # message(STATUS "___TEST___ ${_proj} \t||\t ${_phos_userspec_projects} \t||\t ${PHOSPHOR_ENABLE_PROJECTS}")

    if(${_proj} IN_LIST _phos_userspec_projects)
        if(EXISTS "${phos_root}/${proj}" AND IS_DIRECTORY "${phos_root}/${proj}")
            add_subdirectory(${proj})
            list(APPEND _phos_enabled_projects ${proj})
            set(_phos_enabled_projects ${_phos_enabled_projects} PARENT_SCOPE)
        else()
            message(FATAL_ERROR "Project ${proj} is unimplemented!")
        endif()
    else()
        list(APPEND _phos_disabled_projects ${proj})
        set(_phos_disabled_projects ${_phos_disabled_projects} PARENT_SCOPE)
    endif()
endfunction()

function(phos_build_projects)
    message(STATUS "Enabled projects: ${_phos_enabled_projects}")
    message(STATUS "Disabled projects: ${_phos_disabled_projects}")
endfunction()

# ===============
# CCACHE HANDLING
# ===============

if(PHOSPHOR_ENABLE_CCACHE_BUILD)
    find_program(_phos_ccache ccache)
    if(_phos_ccache)
        set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${_phos_ccache})
        set_property (GLOBAL PROPERTY RULE_LAUNCH_LINK ${_phos_ccache})
    endif()
endif()
