#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/board.c)

target_sources_ifdef(CONFIG_DESKTOP_BUTTONS_ENABLE app
                     PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/buttons.c)

target_sources_ifdef(CONFIG_DESKTOP_CLICK_DETECTOR_ENABLE app
                     PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/click_detector.c)

#target_sources_ifdef(CONFIG_DESKTOP_SELECTOR_HW_ENABLE app
#                     PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/selector_hw.c)

target_sources_ifdef(CONFIG_DESKTOP_LED_ENABLE app
		     PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/leds.c)
