#------------------------------------------------------------------------------
# Copyright (c) 2026 Ainekko, Co.
# SPDX-License-Identifier: Apache-2.0
#------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.21)

project(soc1sim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

# Pick up Find*.cmake modules installed alongside the platform
list(APPEND CMAKE_MODULE_PATH "${CMAKE_PREFIX_PATH}/lib/cmake")

find_package(deviceLayer REQUIRED)
find_package(runtime REQUIRED)
find_package(hostUtils REQUIRED)
find_package(sw-sysemu REQUIRED)

add_executable(erbium_soc1sim src/main.cpp)

target_link_libraries(erbium_soc1sim
    PRIVATE
        deviceLayer::deviceLayer
        runtime::etrt
        hostUtils::logging
        sw-sysemu::sw-sysemu
)

install(TARGETS erbium_soc1sim
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(PROGRAMS scripts/erbium-run
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
