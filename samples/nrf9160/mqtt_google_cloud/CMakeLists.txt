#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

cmake_minimum_required(VERSION 3.8.2)

include(../../../cmake/boilerplate.cmake)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(mqtt-simple)

if(NOT EXISTS src/private_info/key.c)
  message(FATAL_ERROR "!!!!!! Generate key file before continuing.  (Use ECDSA)")
endif()

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/private_info/key.c)
target_sources(app PRIVATE src/ntp.c)

set(bsec_dir   ${CMAKE_CURRENT_SOURCE_DIR}/src/private_info/bsec)
set(BSEC_LIB_DIR     ${bsec_dir}/lib)
set(BSEC_INCLUDE_DIR ${bsec_dir}/inc)

add_library(bsec_lib STATIC IMPORTED GLOBAL)

add_dependencies(bsec_lib math_lib bsec_target)

set_target_properties(bsec_lib PROPERTIES IMPORTED_LOCATION    "${BSEC_LIB_DIR}/libalgobsec.a")
set_target_properties(bsec_lib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES    ${BSEC_INCLUDE_DIR})
target_link_libraries(bsec_lib INTERFACE -L${LIBC_LIBRARY_DIR})

target_link_libraries(app PUBLIC bsec_lib)