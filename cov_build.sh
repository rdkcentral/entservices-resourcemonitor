#!/bin/bash
set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
############################
# Build entservices-resourcemonitor
echo "building entservices-resourcemonitor"

cd ${GITHUB_WORKSPACE}
cmake -G Ninja -S "$GITHUB_WORKSPACE" -B build/entservices-resourcemonitor \
-DUSE_THUNDER_R4=ON \
-DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
-DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
-DCMAKE_VERBOSE_MAKEFILE=ON \
-DRDK_SERVICES_COVERITY=ON \
-DCMAKE_DISABLE_FIND_PACKAGE_IARMBus=ON \
-DCMAKE_DISABLE_FIND_PACKAGE_DS=ON \
-DCMAKE_DISABLE_FIND_PACKAGE_RFC=ON \
-DCOMCAST_CONFIG=OFF \
-DPLUGIN_RESOURCEMONITOR=ON \
-DRDK_SERVICES_L1_TEST=OFF \
-DRDK_SERVICE_L2_TEST=OFF \
-Wall -Werror -Wno-error=format \
-Wl,-wrap,system -Wl,-wrap,popen -Wl,-wrap,syslog \
-DDISABLE_SECURITY_TOKEN -DUSE_THUNDER_R4=ON -DTHUNDER_VERSION=4 -DTHUNDER_VERSION_MAJOR=4 -DTHUNDER_VERSION_MINOR=4" \

cmake --build build/entservices-resourcemonitor --target install
echo "======================================================================================"
exit 0