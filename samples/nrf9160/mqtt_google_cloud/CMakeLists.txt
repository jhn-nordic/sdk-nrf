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
