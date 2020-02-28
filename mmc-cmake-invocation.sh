#!/bin/sh

cmake -G Ninja -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_INSTALL_PREFIX:STRING=/usr/local/ -DPHOSPHOR_BUILD_SHARED:BOOL=ON -DPHOSPHOR_ENABLE_CCACHE_BUILD=ON -DVENICE_ENABLE_PLATFORM_OPTS=ON -DVENICE_ENABLE_ARCH_OPTS=ON -DPEWTER_ENABLE_PLATFORM_OPTS=ON -DPHOSPHOR_ENABLE_PROJECTS:STRING="Venice;Pewter" ..
