#
# Copyright (c) 2018 - 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

cmake_minimum_required(VERSION 3.8.2)

if(CMAKE_BUILD_TYPE MATCHES "^ZRelease$")
  set(spm_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../common/configuration/spm_no_serial.conf)
endif()

set(NRF_SUPPORTED_BOARDS
  nrf9160_pca20035ns
  nrf9160_pca10090ns
  )

set(NRF_SUPPORTED_BUILD_TYPES
  ZDebug
  ZRelease
  )

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE ZDebug)
endif()

include($ENV{ZEPHYR_BASE}/../nrf/cmake/boilerplate.cmake)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/configuration/${BOARD}/dts.overlay")
  set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/configuration/${BOARD}/dts.overlay")
endif()

# Define configuration files.
set(CONF_FILE "configuration/${BOARD}/board.conf")
list(APPEND CONF_FILE "configuration/app.conf")
list(APPEND CONF_FILE "configuration/${CMAKE_BUILD_TYPE}.conf")

################################################################################


include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(asset_tracker)
zephyr_compile_definitions(PROJECT_NAME=${PROJECT_NAME})
zephyr_compile_definitions(_POSIX_C_SOURCE=200809L)

################################################################################

target_sources(app PRIVATE
  src/main.c
  src/modules/app_module.c
  )

# Include application events and configuration headers
zephyr_library_include_directories(
  ../common/events
  ../common/include
  configuration/
  configuration/${BOARD}
  )

# Application sources
add_subdirectory(../common/events common/events)
add_subdirectory(../common/hw_interface common/hw_interface)
add_subdirectory(../common/modules common/modules)


