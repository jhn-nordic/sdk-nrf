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
  thingy91_nrf9160ns
  nrf9160dk_nrf9160ns
  )

set(NRF_SUPPORTED_BUILD_TYPES
  ZDebug
  ZRelease
  )

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE ZDebug)
endif()


if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/configuration/${BOARD}/dts.overlay")
  set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/configuration/${BOARD}/dts.overlay")
endif()

# Define configuration files.
set(CONF_FILE "configuration/${BOARD}/board.conf")
list(APPEND CONF_FILE "configuration/app.conf")
list(APPEND CONF_FILE "configuration/${CMAKE_BUILD_TYPE}.conf")

################################################################################


find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cloud_and_gps_EM_edition)
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

