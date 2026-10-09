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
#include <rfcapi.h>


namespace Utils {
inline bool ApplyRFCInt(const char* rfcName, uint32_t& target) {
    RFC_ParamData_t param{};
    WDMP_STATUS status = getRFCParameter(nullptr, rfcName, &param);
    if (status == WDMP_SUCCESS && param.value[0] != '\0') {
        target = std::stoull(param.value);
        return true;
    }
    return false;
}

inline bool ApplyRFCInt64(const char* rfcName, uint64_t& target) {
    RFC_ParamData_t param{};
    WDMP_STATUS status = getRFCParameter(nullptr, rfcName, &param);
    if (status == WDMP_SUCCESS && param.value[0] != '\0') {
        target = std::stoull(param.value);
        return true;
    }
    return false;
}

inline bool ApplyRFCIntdouble(const char* rfcName, double& target) {
    RFC_ParamData_t param{};
    WDMP_STATUS status = getRFCParameter(nullptr, rfcName, &param);
    if (status == WDMP_SUCCESS && param.value[0] != '\0') {
        target = std::stod(param.value);
        return true;
    }
    return false;
}

inline bool ApplyRFCBool(const char* rfcName, bool& target) {
    RFC_ParamData_t param{};
    WDMP_STATUS status = getRFCParameter(nullptr, rfcName, &param);
    if (status == WDMP_SUCCESS && param.value[0] != '\0') {
        target = (std::string(param.value) == "true" || std::string(param.value) == "1");
        return true;
    }
    return false;
}
}
