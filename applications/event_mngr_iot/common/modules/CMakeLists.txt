#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

target_sources_ifdef(CONFIG_DESKTOP_LED_ENABLE
		     app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/led_state.c)

target_sources_ifdef(CONFIG_DESKTOP_POWER_MANAGER_ENABLE
		     app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/power_manager.c)

target_sources_ifdef(CONFIG_DESKTOP_WATCHDOG_ENABLE
		     app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/watchdog.c)

target_sources_ifdef(CONFIG_LTE_LC_MODULE_ENABLE
		     app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lte_lc_module.c)

target_sources_ifdef(CONFIG_CLOUDAPI_MODULE_ENABLE
		     app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cloudapi_module.c)
