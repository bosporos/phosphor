#
# project phosphor-central-build
# author Maximilien M. Cura
#

cmake_minimum_required(VERSION 3.13)

project (PHOSPHOR
    VERSION 0.0.0
    DESCRIPTION "Phosphorescent"
    HOMEPAGE_URL "https://dawn-project.sh/phosphor"
    LANGUAGES C CXX ASM
)

include (cmake/phosphor-central-build/options.cmake)
include (cmake/phosphor-central-build/central.cmake)

phos_add_proj("Venice")         # VNZ
phos_add_proj("Sepia")          # SEP
phos_add_proj("Cairo")          # QHR
phos_add_proj("Fez")            # FEZ
phos_add_proj("Paris")          # PRS

phos_add_proj("Petra")          # PTR
phos_add_proj("Qaravan")        # QRV

phos_add_proj("Mediterranean")  # MDTR
phos_add_proj("Baltik")         # BLT

phos_add_proj("Indigo")         # IND
phos_add_proj("Silk")           # SLX
phos_add_proj("Lithograf")      # LITH

phos_add_proj("Okeanos")        # OKNS
phos_add_proj("Meltem")         # MLM
phos_add_proj("Sienna")         # SNN

phos_build_projects()
