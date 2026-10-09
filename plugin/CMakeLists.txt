# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(PLUGIN_NAME ResourceMonitor)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_IMPLEMENTATION ${MODULE_NAME}Implementation)

set(PLUGIN_RESOURCEMONITOR_AUTOSTART True CACHE STRING "Automatically start ResourceMonitor plugin")
set(PLUGIN_RESOURCEMONITOR_STARTMODE "on" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")
set(PLUGIN_RESOURCEMONITOR_STARTUPORDER "" CACHE STRING "To configure startup order of ResourceManager plugin")


if(BUILD_REFERENCE)
    add_definitions(-DBUILD_REFERENCE=${BUILD_REFERENCE})
endif()

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(${NAMESPACE}Messaging REQUIRED)
find_package(${NAMESPACE}Core REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)
find_package(WPEFrameworkDefinitions REQUIRED)

add_library(${MODULE_NAME} SHARED
    ResourceMonitor.cpp
    ResourceMonitorConfig.cpp
    Module.cpp
)

include_directories(
   ../helpers)


set_target_properties(${MODULE_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES)

set_source_files_properties(ResourceMonitor.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
        ${NAMESPACE}Messaging::${NAMESPACE}Messaging
        ${NAMESPACE}Core::${NAMESPACE}Core
        WPEFrameworkDefinitions::WPEFrameworkDefinitions)

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

add_library(${PLUGIN_IMPLEMENTATION} SHARED
    ResourceMonitorImplementation.cpp
    ResourceMonitorConfig.cpp
    Module.cpp
)

include_directories(
   ../helpers)

set_target_properties(${PLUGIN_IMPLEMENTATION} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES)

set(RESOURCE_MONITOR_INCLUDES $ENV{RESOURCE_MONITOR_INCLUDES})
separate_arguments(RESOURCE_MONITOR_INCLUDES)
include_directories(BEFORE ${RESOURCE_MONITOR_INCLUDES})

if (RDK_SERVICES_L1_TEST)
    include_directories(${CMAKE_SOURCE_DIR}/Tests/L1Tests)
    add_definitions(-DENABLE_L1TEST)
endif(RDK_SERVICES_L1_TEST)

if (RDK_SERVICE_L2_TEST)
   find_library(TESTMOCKLIB_LIBRARIES NAMES TestMocklib)
   if (TESTMOCKLIB_LIBRARIES)
       message ("linking mock libraries ${TESTMOCKLIB_LIBRARIES} library")
       target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${TESTMOCKLIB_LIBRARIES})
   else (TESTMOCKLIB_LIBRARIES)
       message ("Require ${TESTMOCKLIB_LIBRARIES} library")
   endif (TESTMOCKLIB_LIBRARIES)
   add_definitions(-DENABLE_ERM)
endif (RDK_SERVICE_L2_TEST)

if (RDK_SERVICES_L1_TEST OR RDK_SERVICE_L2_TEST)
    target_link_libraries(${PLUGIN_IMPLEMENTATION}
        PRIVATE
            CompileSettingsDebug::CompileSettingsDebug
            ${NAMESPACE}Plugins::${NAMESPACE}Plugins
            ${PLUGIN_RESOURCE_MANAGER_EXTRA_LIBRARIES})
else()
    target_link_libraries(${PLUGIN_IMPLEMENTATION}
        PRIVATE
            CompileSettingsDebug::CompileSettingsDebug
            ${NAMESPACE}Plugins::${NAMESPACE}Plugins
            ${PLUGIN_RESOURCE_MANAGER_EXTRA_LIBRARIES})
endif()

find_library(RFC_LIBRARIES NAMES rfcapi)
message(STATUS "RFC_LIBRARIES = ${RFC_LIBRARIES}")
if (RFC_LIBRARIES)
    find_path(RFC_INCLUDE_DIRS NAMES rfcapi.h REQUIRED)
    target_include_directories(${MODULE_NAME} PRIVATE ${RFC_INCLUDE_DIRS})
    target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ${RFC_INCLUDE_DIRS})
    target_link_libraries(${MODULE_NAME} PRIVATE ${RFC_LIBRARIES})
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${RFC_LIBRARIES})
    target_compile_definitions(${MODULE_NAME} PRIVATE WITH_RFC)
    target_compile_definitions(${PLUGIN_IMPLEMENTATION} PRIVATE WITH_RFC)
endif ()

target_include_directories(${MODULE_NAME}
    PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    ${CMAKE_SYSROOT}/usr/include/WPEFramework)

target_include_directories(${PLUGIN_IMPLEMENTATION}
    PRIVATE
    ${CMAKE_SYSROOT}/usr/include/WPEFramework)


install(TARGETS ${PLUGIN_IMPLEMENTATION}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)


write_config(${PLUGIN_NAME})