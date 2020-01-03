#
# project phosphor-central-build
# author Maximilien M. Cura
#

# =============
# BUILD OPTIONS
# =============

set(PHOSPHOR_BUILD_SHARED OFF CACHE BOOL "Default to building shared libraries (may be overridden by individual projects)")
set(PHOSPHOR_ENABLE_CCACHE_BUILD OFF CACHE BOOL "Default to building with ccache (for faster iterative builds; may be overridden by individual projects)")

# ===============
# PROJECT OPTIONS
# ===============

set(PHOSPHOR_ENABLE_PROJECTS "Venice" CACHE STRING "Projects to build")

# ========
# DEFAULTS
# ========

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected; defaulting to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif()
