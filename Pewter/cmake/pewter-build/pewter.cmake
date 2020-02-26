#
# project pewter-build
# author Maximilien M. Cura
#

set(pwt_root "${phos_root}/Pewter")
message(STATUS "Pewter root: ${pwt_root}")
set(pwt_clangfile_dir "${pwt_root}/clang")
set(pwt_templatefile_dir "${pwt_root}/templates")

set(pwt_sources "")
set(pwt_libraries "")

function(pwt_file file)
    if(EXISTS "${pwt_root}/${file}")
        list(APPEND pwt_sources "${pwt_root}/${file}")
        set(pwt_sources ${pwt_sources} PARENT_SCOPE)
    else()
        message(NOTICE "Could not find file: ${file}")
    endif()
endfunction()

# TEMPLATES

configure_file(
    "${pwt_templatefile_dir}/Pewter.h.in"
    "Pewter/Pewter"
    @ONLY
)
