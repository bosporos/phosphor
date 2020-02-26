#
# project phosphor-central-build
# author Maximilien M. Cura
#

## monk-mark! documented

# I don't actually know what the minimum required version to run this would be,
# so I just went for what I saw in the docs and what I had
cmake_minimum_required(VERSION 3.13)

# project declaration
project (PHOSPHOR
    # monk-todo! figure out phosphor versioning
    VERSION 0.0.0
    # punny dummy text for now
    DESCRIPTION "Phosphorescent"
    # don't have a url yet, so it's just pointed at a possible url for the moment
    # monk-todo![release:public] figure out the url situation
    HOMEPAGE_URL "https://dawn-project.sh/phosphor"
    # we need C, C++, and assembly support from CMake
    LANGUAGES C CXX ASM
)

# takes care of phosphor's global options, etc.
include (cmake/phosphor-central-build/options.cmake)
# takes care of the various subroutines and systems that need to be set up so that
# we can build the subsystems
include (cmake/phosphor-central-build/central.cmake)

#
# Declaration of subprojects
#

phos_add_proj("Pewter")

# Venice, abbr. vnz
phos_add_proj("Venice")
# Sepia, abbr. sep
phos_add_proj("Sepia")
# Cairo, abbr. qhr
phos_add_proj("Cairo")
# Fez, no abbr.
phos_add_proj("Fez")
# Paris, no abbr.
phos_add_proj("Paris")

# Petra, abbr. ptr
phos_add_proj("Petra")
# Qaravan, abbr. qrv? or no abbr.?
phos_add_proj("Qaravan")

# Mediterranean, abbr. mdtr
phos_add_proj("Mediterranean")
# Baltik, abbr. blt
phos_add_proj("Baltik")

# Indigo, abbr. ind
phos_add_proj("Indigo")
# Silk, abbr. slx? or no abbr.?
phos_add_proj("Silk")
# Lithograf, abbr. lith
phos_add_proj("Lithograf")

# Okeanos, abbr. okns
phos_add_proj("Okeanos")
# Meltem/Etesian abbr. mlm/ets
phos_add_proj("Etesian")
# Sienna abbr. snn
phos_add_proj("Sienna")

#
# At this point, phosphor-build gives control over to the subprojects
#

phos_build_projects()
