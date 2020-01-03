#
# project venice-build
# author Maximilien M. Cura
#

# BUILD OPTIONS

set(VENICE_ENABLE_ARCH_OPTS ON CACHE BOOL "Allow venice to build with architecture-specific optimizations. Recommended value: ON")
set(VENICE_ENABLE_PLATFORM_OPTS ON CACHE BOOL "Allow venice to build with platform-specific optimiztations. Recommended value: ON")

if(PHOSPHOR_BUILD_SHARED)
    set(VENICE_BUILD_SHARED ON CACHE BOOL "Build shared library.")
else()
    set(VENICE_BUILD_SHARED OFF CACHE BOOL "Build shared library.")
endif()

set(VENICE_BUILD_TESTS $<IF:$<CONFIG:Debug>,ON,OFF> CACHE BOOL "Build venice with test suite")
