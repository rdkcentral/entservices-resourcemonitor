# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0

set(PLUGIN_NAME L2TestsIN)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(THUNDER_PORT 9998)

find_package(${NAMESPACE}Plugins REQUIRED)

################################################################################
# Source Files
################################################################################

if(PLUGIN_RESOURCEMONITOR)
    set(SRC_FILES
        ${SRC_FILES}
        tests/ResourceMonitor_L2Test.cpp
    )
endif()

################################################################################

add_library(${MODULE_NAME}
    SHARED
    ${SRC_FILES}
)

set_target_properties(${MODULE_NAME}
    PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES
)

target_compile_definitions(${MODULE_NAME}
    PRIVATE
        MODULE_NAME=Plugin_${PLUGIN_NAME}
        THUNDER_PORT="${THUNDER_PORT}"
)

target_compile_options(${MODULE_NAME}
    PRIVATE
    -Wno-error
)

target_link_libraries(${MODULE_NAME}
    PRIVATE
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
)

################################################################################
# Mock Libraries
################################################################################

if(NOT L2_TEST_OOP_RPC)

    find_library(TESTMOCKLIB_LIBRARIES
        NAMES TestMocklib
    )

    if(TESTMOCKLIB_LIBRARIES)

        message("Found mock library - ${TESTMOCKLIB_LIBRARIES}")

        target_link_libraries(${MODULE_NAME}
            PRIVATE
            ${TESTMOCKLIB_LIBRARIES}
        )

    else()

        message("Require TestMocklib")

    endif()

endif()

################################################################################

find_library(MOCKACCESSOR_LIBRARIES
    NAMES MockAccessor
)

if(MOCKACCESSOR_LIBRARIES)

    message("Found MockAccessor library - ${MOCKACCESSOR_LIBRARIES}")

    target_link_libraries(${MODULE_NAME}
        PRIVATE
        ${MOCKACCESSOR_LIBRARIES}
    )

else()

    message("Require MockAccessor")

endif()

################################################################################

target_include_directories(${MODULE_NAME}
    PRIVATE

    ./

    ../../../entservices-testframework/Tests/mocks

    ../../../entservices-testframework/Tests/mocks/thunder

    ../../../entservices-testframework/Tests/mocks/devicesettings

    ../../../entservices-testframework/Tests/mocks/MockPlugin

    ../../../entservices-testframework/Tests/L2Tests/L2TestsPlugin

    ${CMAKE_INSTALL_PREFIX}/include
)

################################################################################

install(TARGETS ${MODULE_NAME}
    DESTINATION lib
)