# Copyright (c) 2023 honey the codewitch
#
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_HTCW_JSON)

zephyr_library()

#include_directories(${ZEPHYR_BASE}/lib/???/htcw_json)
zephyr_include_directories(../include/)
zephyr_library_include_directories(../include/)
zephyr_interface_library_named(HTCW_JSON)


#zephyr_library_sources_ifdef(CONFIG_PARAM_DEF, file.cpp)
zephyr_library_link_libraries(HTCW_JSON)
#target_link_libraries(HTCW_JSON INTERFACE zephyr_interface)

endif()