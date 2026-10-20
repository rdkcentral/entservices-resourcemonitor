/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/


#pragma once
#include <string>
#include <cstdint>
#include <limits>
#include <rfcapi.h>


namespace Utils {

inline bool ApplyRFCInt(const char* rfcName, uint32_t& target)
{
    RFC_ParamData_t param{};
    WDMP_STATUS status = getRFCParameter(nullptr, rfcName, &param);
    if (status != WDMP_SUCCESS || param.value[0] == '\0') {
        return false;
    }
    try {
        size_t pos = 0;
        unsigned long long value = std::stoull(param.value, &pos);
        if (param.value[pos] != '\0') {
            return false;
        }
        if (value > std::numeric_limits<uint32_t>::max()) {
            return false;
        }
        target = static_cast<uint32_t>(value);
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

inline bool ApplyRFCInt64(const char* rfcName, uint64_t& target)
{
    RFC_ParamData_t param{};
    WDMP_STATUS status = getRFCParameter(nullptr, rfcName, &param);
    if (status != WDMP_SUCCESS || param.value[0] == '\0') {
        return false;
    }
    try {
        size_t pos = 0;
        uint64_t value = std::stoull(param.value, &pos);
        if (param.value[pos] != '\0') {
            return false;
        }
        target = value;
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

inline bool ApplyRFCIntDouble(const char* rfcName, double& target)
{
    RFC_ParamData_t param{};
    WDMP_STATUS status = getRFCParameter(nullptr, rfcName, &param);
    if (status != WDMP_SUCCESS || param.value[0] == '\0') {
        return false;
    }
    try {
        size_t pos = 0;
        double value = std::stod(param.value, &pos);
        if (param.value[pos] != '\0') {
            return false;
        }
        target = value;
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

inline bool ApplyRFCBool(const char* rfcName, bool& target)
{
    RFC_ParamData_t param{};
    WDMP_STATUS status = getRFCParameter(nullptr, rfcName, &param);
    if (status != WDMP_SUCCESS || param.value[0] == '\0') {
        return false;
    }
    std::string value(param.value);
    if (value == "true" || value == "1") {
        target = true;
        return true;
    }
    if (value == "false" || value == "0") {
        target = false;
        return true;
    }
    return false;
}
} // namespace Utils
