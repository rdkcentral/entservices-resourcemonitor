# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0

message(STATUS "Entered L1Tests CMake")

cmake_minimum_required(VERSION 3.8)

set(PLUGIN_NAME L1TestsIN)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(${NAMESPACE}Plugins REQUIRED)

################################################################################
# Common Test Files
################################################################################

set(TEST_LIB
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
)

find_package(GTest REQUIRED)


################################################################################
# Helper macro
################################################################################

macro(add_plugin_test_ex plugin_opt plugin_test_sources_str plugin_includes_str plugin_libs_str)

    if(${plugin_opt})

        message(STATUS "${plugin_opt}=ON")

        string(REPLACE ";" ";" srclist "${plugin_test_sources_str}")
        string(REPLACE ";" ";" inclist "${plugin_includes_str}")
        string(REPLACE ";" ";" liblist "${plugin_libs_str}")

        foreach(item IN LISTS srclist)
            list(APPEND TEST_SRC ${item})
        endforeach()

        foreach(item IN LISTS inclist)
            list(APPEND TEST_INC ${item})
        endforeach()

        foreach(item IN LISTS liblist)
            list(APPEND TEST_LIB ${item})
        endforeach()

    else()

        message(STATUS "${plugin_opt}=OFF")

    endif()

endmacro()

################################################################################
# ResourceMonitor Tests
################################################################################

set(RESOURCEMONITOR_INC
    ${CMAKE_SOURCE_DIR}/plugin
)

set(RESOURCEMONITOR_LIBS
    ${NAMESPACE}ResourceMonitor
    ${NAMESPACE}ResourceMonitorImplementation
)

message(STATUS "PLUGIN_RESOURCEMONITOR=${PLUGIN_RESOURCEMONITOR}")

add_plugin_test_ex(
    PLUGIN_RESOURCEMONITOR
    tests/test_ResourceMonitor.cpp
    "${RESOURCEMONITOR_INC}"
    "${RESOURCEMONITOR_LIBS}"
)

################################################################################

message(STATUS "TEST_SRC=${TEST_SRC}")
add_library(${MODULE_NAME} SHARED ${TEST_SRC})

if(RDK_SERVICES_L1_TEST)
    target_link_options(${MODULE_NAME}
        PRIVATE
        "-Wl,-wrap,opendir")
endif()

include_directories(${TEST_INC})

target_link_directories(${MODULE_NAME}
    PUBLIC
    ${CMAKE_INSTALL_PREFIX}/lib
    ${CMAKE_INSTALL_PREFIX}/lib/wpeframework/plugins
)

target_link_libraries(${MODULE_NAME}
    ${TEST_LIB}
)

target_link_libraries(${MODULE_NAME}
    GTest::gtest
    GTest::gmock
)

target_include_directories(${MODULE_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>

    ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks
    ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks/devicesettings
    ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks/thunder
    ${CMAKE_SOURCE_DIR}/../Thunder/Source/plugins
)

target_compile_definitions(${MODULE_NAME}
    PRIVATE
    UNIT_TEST
)

install(TARGETS ${MODULE_NAME}
    LIBRARY DESTINATION lib
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_NAME}.conf.in")
    write_config(${PLUGIN_NAME})
endif()