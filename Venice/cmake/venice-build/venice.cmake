#
# project venice-build
# author Maximilien M. Cura
#

set(vnz_root "${phos_root}/Venice")
message(STATUS "Venice root: ${venice_root}")
set(vnz_clangfile_dir "${vnz_root}/clang")
set(vnz_templatefile_dir "${vnz_root}/templates")

set(vnz_sources "")
set(vnz_libraries "")

function(vnz_file file)
    if(EXISTS "${vnz_root}/${file}")
        list(APPEND vnz_sources "${vnz_root}/${file}")
        set(vnz_sources ${vnz_sources} PARENT_SCOPE)
    else()
        message(NOTICE "Could not find file: ${file}")
    endif()
endfunction()

# TEMPLATES

configure_file(
    "${vnz_templatefile_dir}/Venice.h.in"
    "Venice/Venice"
    @ONLY
)
