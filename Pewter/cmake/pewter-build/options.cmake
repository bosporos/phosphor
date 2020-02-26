#
# project pewter-build
# author Maximilien M. Cura
#

# BUILD OPTIONS

set(PEWTER_ENABLE_PLATFORM_OPTS ON CACHE BOOL "Allow pewter to build with platform-specific optimizations.")

if(PHOSPHOR_BUILD_SHARED)
    set(PEWTER_BUILD_SHARED ON CACHE BOOL "Build shared library.")
else()
    set(PEWTER_BUILD_SHARED OFF CACHE BOOL "Build shared library.")
endif()

set(PEWTER_BUILD_TESTS $<IF:$<CONFIG:Debug>,ON,OFF> CACHE BOOL "Build pewter with test suite.")
