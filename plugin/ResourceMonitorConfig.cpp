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


#include "ResourceMonitorConfig.h"
#include "UtilsRFCConfig.h"
#include "UtilsLogging.h"
#include <tracing/Logging.h>
#ifdef WITH_RFC
#include <rfcapi.h>
#endif

namespace WPEFramework {

ENUM_CONVERSION_BEGIN(PsiMetric)
    { PsiMetric::SOME_AVG10,  _TXT("some_avg10") },
    { PsiMetric::SOME_AVG60,  _TXT("some_avg60") },
    { PsiMetric::SOME_AVG300, _TXT("some_avg300") },
    { PsiMetric::FULL_AVG10,  _TXT("full_avg10") },
    { PsiMetric::FULL_AVG60,  _TXT("full_avg60") },
    { PsiMetric::FULL_AVG300, _TXT("full_avg300") },
ENUM_CONVERSION_END(PsiMetric)

namespace Plugin {
constexpr char ResourceMonitorConfig::DEFAULT_PSI_PATH[];
constexpr char ResourceMonitorConfig::DEFAULT_FLASH_MOUNT_POINT[];

ResourceMonitorConfig::ResourceMonitorConfig()
{
    SetDefaults();
}

void ResourceMonitorConfig::SetDefaults()
{
    _psiCheckingEnabled = true;
    _zramCheckingEnabled = true;
    _lowMemoryWarningsEnabled = true;
    _psiPath = ResourceMonitorConfig::DEFAULT_PSI_PATH;
    _psiMetric = PsiMetric::FULL_AVG60;
    _psiExcessDuration = ResourceMonitorConfig::DEFAULT_PSI_EXCESS_DURATION;
    _psiPostActionDelay = ResourceMonitorConfig::DEFAULT_PSI_POST_ACTION_DELAY;
    _flashSpaceForHibernatedAppsSize = ResourceMonitorConfig::DEFAULT_FLASH_SIZE;
    _flashSpaceForHibernatedAppsMountPoint = ResourceMonitorConfig::DEFAULT_FLASH_MOUNT_POINT;
    _pollingInterval = ResourceMonitorConfig::DEFAULT_POLLING_INTERVAL;
    _warningThresholdMinMem = ResourceMonitorConfig::DEFAULT_WARNING_MIN_MEM;
    _warningThresholdMaxSwapPercentage = ResourceMonitorConfig::DEFAULT_WARNING_MAX_SWAP;
    _warningThresholdMaxPsi = ResourceMonitorConfig::DEFAULT_WARNING_MAX_PSI;
    _minPostWarningDelay = ResourceMonitorConfig::DEFAULT_MIN_POST_WARNING_DELAY;
    _softThresholdDeltaPercentage = ResourceMonitorConfig::DEFAULT_SOFT_THRESHOLD_DELTA;
    _hardThresholdDeltaPercentage = ResourceMonitorConfig::DEFAULT_HARD_THRESHOLD_DELTA;
    _pollingIntervalRfcValue = ResourceMonitorConfig::RFC_POLLING_INTERVAL;
    CalculateMemoryThresholds();
    CalculateSwapThresholds();
    CalculatePsiThresholds();
}

bool ResourceMonitorConfig::IsValidPsiPath(const std::string& value) const
{
    return !value.empty();
}

bool ResourceMonitorConfig::IsValidPollingInterval(uint32_t value) const
{
    return value >= ResourceMonitorConfig::MIN_POLLING_INTERVAL;
}

bool ResourceMonitorConfig::IsValidPsiExcessDuration(uint32_t value) const
{
    return value > 0;
}


bool ResourceMonitorConfig::IsValidPsiPostActionDelay(uint32_t value) const
{
    return value > 0;
}

bool ResourceMonitorConfig::IsValidMemoryWarningThreshold(uint64_t value) const
{
    return value >= ResourceMonitorConfig::MIN_WARNING_MEMORY;
}

bool ResourceMonitorConfig::IsValidPercentage(uint32_t value) const
{
    return value > 0 && value < ResourceMonitorConfig::MAX_PERCENTAGE;
}

bool ResourceMonitorConfig::CalculateMemoryThresholds()
{
    const uint64_t softDelta = (_warningThresholdMinMem * _softThresholdDeltaPercentage) / 100ULL;
    const uint64_t hardDelta = (_warningThresholdMinMem * _hardThresholdDeltaPercentage) / 100ULL;

    if (softDelta >= _warningThresholdMinMem || hardDelta >= _warningThresholdMinMem || hardDelta <= softDelta) {
        return false;
    }

    _softThresholdMinMem = _warningThresholdMinMem - softDelta;
    _hardThresholdMinMem = _warningThresholdMinMem - hardDelta;
    return true;
}

bool ResourceMonitorConfig::CalculateSwapThresholds()
{
    const uint64_t soft = static_cast<uint64_t>(_warningThresholdMaxSwapPercentage) + (static_cast<uint64_t>(_warningThresholdMaxSwapPercentage) * _softThresholdDeltaPercentage) / 100ULL;
    const uint64_t hard = static_cast<uint64_t>(_warningThresholdMaxSwapPercentage) + (static_cast<uint64_t>(_warningThresholdMaxSwapPercentage) * _hardThresholdDeltaPercentage) / 100ULL;

    if (soft > ResourceMonitorConfig::MAX_PERCENTAGE || hard > ResourceMonitorConfig::MAX_PERCENTAGE) {
        return false;
    }

    if (!(soft > _warningThresholdMaxSwapPercentage && hard > soft)) {
        return false;
    }

    _softThresholdMaxSwapPercentage = static_cast<uint32_t>(soft);
    _hardThresholdMaxSwapPercentage = static_cast<uint32_t>(hard);
    return true;
}

bool ResourceMonitorConfig::CalculatePsiThresholds()
{
    double warning = _warningThresholdMaxPsi;
    double soft = warning + (warning * _softThresholdDeltaPercentage / 100.0);
    double hard = warning + (warning * _hardThresholdDeltaPercentage / 100.0);
    if (soft > ResourceMonitorConfig::MAX_PERCENTAGE || hard > ResourceMonitorConfig::MAX_PERCENTAGE) {
        return false;
    }

    if (!(soft > warning && hard > soft)) {
        return false;
    }

    _softThresholdMaxPsi = soft;
    _hardThresholdMaxPsi = hard;
    return true;
}

bool ResourceMonitorConfig::Load(const ResourceMonitorConfigJSON& json)
{
    SetDefaults();
    const ResourceMonitorConfigJSON::RootConfig& root = json.Root;

    _psiCheckingEnabled = root.PsiCheckingEnabled.Value();
    _zramCheckingEnabled = root.ZramCheckingEnabled.Value();
    _lowMemoryWarningsEnabled = root.LowMemoryWarningsEnabled.Value();

    // PSI path.
    if (IsValidPsiPath(root.PsiPath.Value())) {
        _psiPath = root.PsiPath.Value();
    }
    else {
        LOGERR("Invalid psi_path. Using default: %s", ResourceMonitorConfig::DEFAULT_PSI_PATH);
    }

    // PSI metric.
    _psiMetric = root.PsiMetrics.Value();
    switch (_psiMetric) {
        case PsiMetric::SOME_AVG10:
        case PsiMetric::SOME_AVG60:
        case PsiMetric::SOME_AVG300:
        case PsiMetric::FULL_AVG10:
        case PsiMetric::FULL_AVG60:
        case PsiMetric::FULL_AVG300:
            break;
        default:
            LOGWARN("Invalid psi_metrics. Using default");
            _psiMetric = PsiMetric::FULL_AVG60;
            break;
    }

    // PSI excess duration.
    if (IsValidPsiExcessDuration(root.PsiExcessDuration.Value())) {
        _psiExcessDuration = root.PsiExcessDuration.Value();
    }
    else {
        LOGWARN("Invalid psi_excess_duration. Using default: %u", ResourceMonitorConfig::DEFAULT_PSI_EXCESS_DURATION);
    }

    // PSI post-action delay.
    if (IsValidPsiPostActionDelay(root.PsiPostActionDelay.Value())) {
        _psiPostActionDelay = root.PsiPostActionDelay.Value();
    }
    else {
        LOGWARN("Invalid psi_post_action_delay. Using default: %u", ResourceMonitorConfig::DEFAULT_PSI_POST_ACTION_DELAY);
    }

    // Flash configuration.
    if (!root.FlashSpaceForHibernatedAppsMountPoint.Value().empty()) {
        _flashSpaceForHibernatedAppsMountPoint = root.FlashSpaceForHibernatedAppsMountPoint.Value();
    }
    else {
        LOGWARN("Invalid flash mount point. Using default: %s", ResourceMonitorConfig::DEFAULT_FLASH_MOUNT_POINT);
    }

    if (root.FlashSpaceForHibernatedAppsSize.Value() > 0) {
        _flashSpaceForHibernatedAppsSize = root.FlashSpaceForHibernatedAppsSize.Value();
    }
    else {
        LOGWARN("Invalid flash size. Using default value: %u", ResourceMonitorConfig::DEFAULT_FLASH_SIZE);
    }

    // Polling interval.
    if (IsValidPollingInterval(root.PollingInterval.Value())) {
        _pollingInterval = root.PollingInterval.Value();
    }
    else {
        LOGWARN("Invalid polling_interval. Using default: %u", ResourceMonitorConfig::DEFAULT_POLLING_INTERVAL);
    }

    _minPostWarningDelay = root.MinPostWarningDelay.Value();
    if (_minPostWarningDelay == 0) {
        _minPostWarningDelay = ResourceMonitorConfig::DEFAULT_MIN_POST_WARNING_DELAY;
    }

    // Warning memory threshold.
    if (IsValidMemoryWarningThreshold(root.WarningThresholdMinMem.Value())) {
        _warningThresholdMinMem = root.WarningThresholdMinMem.Value();
    }
    else {
        LOGWARN("Invalid warning_threshold_min_mem. Using default: %llu", static_cast<unsigned long long>(ResourceMonitorConfig::DEFAULT_WARNING_MIN_MEM));
    }

    // Warning swap threshold.
    if (IsValidPercentage(root.WarningThresholdMaxSwapPercentage.Value())) {
        _warningThresholdMaxSwapPercentage = root.WarningThresholdMaxSwapPercentage.Value();
    }
    else {
        LOGWARN("Invalid warning_threshold_max_swap_percentage.Using default: %u", ResourceMonitorConfig::DEFAULT_WARNING_MAX_SWAP);
    }

    // Warning PSI threshold.
    if (IsValidPercentage(root.WarningThresholdMaxPsi.Value())) {
        _warningThresholdMaxPsi = root.WarningThresholdMaxPsi.Value();
    }
    else {
        LOGWARN("Invalid warning_threshold_max_psi. Using default: %u", ResourceMonitorConfig::DEFAULT_WARNING_MAX_PSI);
    }

    // Delta values
    if (IsValidPercentage(root.SoftThresholdDeltaPercentage.Value())) {
        _softThresholdDeltaPercentage = root.SoftThresholdDeltaPercentage.Value();
    }
    else {
        LOGWARN("Invalid soft_threshold_delta_percentage.Using default: %u", ResourceMonitorConfig::DEFAULT_SOFT_THRESHOLD_DELTA);
        _softThresholdDeltaPercentage = ResourceMonitorConfig::DEFAULT_SOFT_THRESHOLD_DELTA;
    }

    if (IsValidPercentage(root.HardThresholdDeltaPercentage.Value())) {
        _hardThresholdDeltaPercentage = root.HardThresholdDeltaPercentage.Value();
    }
    else {
        LOGWARN("Invalid hard_threshold_delta_percentage. Using default: %u", ResourceMonitorConfig::DEFAULT_HARD_THRESHOLD_DELTA);
        _hardThresholdDeltaPercentage = ResourceMonitorConfig::DEFAULT_HARD_THRESHOLD_DELTA;
    }

    #ifdef WITH_RFC
        if (Utils::ApplyRFCInt(ResourceMonitorConfig::RFC_POLLING_INTERVAL, _pollingInterval)){
            LOGINFO("RFC PollingInterval applied: %u", _pollingInterval);
        } else {
            LOGWARN("No RFC PollingInterval override, using %u", _pollingInterval);
        }
        if (Utils::ApplyRFCBool(ResourceMonitorConfig::RFC_LOW_MEMORY_WARNING, _lowMemoryWarningsEnabled)) {
            LOGINFO("RFC LOW MEMORY WARNING Applied : %u", _lowMemoryWarningsEnabled);
        } else {
            LOGWARN("No RFC low memory warning override, using %u", _lowMemoryWarningsEnabled);
        }
        if (Utils::ApplyRFCInt64(ResourceMonitorConfig::RFC_WARNING_MIN_MEM, _warningThresholdMinMem)) {
            LOGINFO("RFC WARNING MIN MEM applied: %llu", static_cast<unsigned long long>(_warningThresholdMinMem));
        } else {
            LOGWARN("No RFC warning threshold memory override, using %llu", static_cast<unsigned long long>(_warningThresholdMinMem));
        }
        if (Utils::ApplyRFCInt(ResourceMonitorConfig::RFC_WARNING_MAX_SWAP, _warningThresholdMaxSwapPercentage)) {
            LOGINFO("RFC WARNING MAX SWAP applied : %u", _warningThresholdMaxSwapPercentage);
        } else {
            LOGWARN("No RFC WARNING MAX SWAP override, using : %u", _warningThresholdMaxSwapPercentage);
        }
        if (Utils::ApplyRFCIntdouble(ResourceMonitorConfig::RFC_WARNING_MAX_PSI, _warningThresholdMaxPsi)) {
            LOGINFO("RFC WARNING MAX PSI applied: %.2f", _warningThresholdMaxPsi);
        } else {
            LOGWARN("No RFC WARNING MAX PSI override, using: %.2f", _warningThresholdMaxPsi);
        }
        if (Utils::ApplyRFCInt(ResourceMonitorConfig::RFC_MIN_POST_WARNING_DELAY, _minPostWarningDelay)) {
            LOGINFO("RFC MIN POST WARNING DELAY applied : %u", _minPostWarningDelay);
        } else {
            LOGWARN("No RFC MIN POST WARNING DELAY override, using : %u", _minPostWarningDelay);
        }
        if (Utils::ApplyRFCInt(ResourceMonitorConfig::RFC_SOFT_DELTA, _softThresholdDeltaPercentage)) {
            LOGINFO("RFC SOFT DELTA applied: %u", _softThresholdDeltaPercentage);
        } else {
            LOGWARN("No RFC SOFT DELTA override, using: %u", _softThresholdDeltaPercentage);
        }
        if(Utils::ApplyRFCInt(ResourceMonitorConfig::RFC_HARD_DELTA, _hardThresholdDeltaPercentage)) {
            LOGINFO("RFC HARD DELTA applied: %u", _hardThresholdDeltaPercentage);
        } else {
            LOGWARN("No RFC HARD DELTA override, using: %u", _hardThresholdDeltaPercentage);
        }
    #endif

    // Calculate derived thresholds.
    if (!CalculateMemoryThresholds()) {
        LOGWARN("Invalid calculated memory thresholds.Reverting warning threshold to default.");
        _softThresholdDeltaPercentage = ResourceMonitorConfig::DEFAULT_SOFT_THRESHOLD_DELTA;
        _hardThresholdDeltaPercentage = ResourceMonitorConfig::DEFAULT_HARD_THRESHOLD_DELTA;
        CalculateMemoryThresholds();
    }

    if (!CalculateSwapThresholds()) {
        LOGWARN("Invalid calculated swap thresholds. Reverting warning threshold to default.");
        _softThresholdDeltaPercentage = ResourceMonitorConfig::DEFAULT_SOFT_THRESHOLD_DELTA;
        _hardThresholdDeltaPercentage = ResourceMonitorConfig::DEFAULT_HARD_THRESHOLD_DELTA;
        CalculateSwapThresholds();
    }

    if (!CalculatePsiThresholds()) {
        LOGWARN("Invalid calculated PSI thresholds. Reverting warning threshold to default.");
        _softThresholdDeltaPercentage = ResourceMonitorConfig::DEFAULT_SOFT_THRESHOLD_DELTA;
        _hardThresholdDeltaPercentage = ResourceMonitorConfig::DEFAULT_HARD_THRESHOLD_DELTA;
        CalculatePsiThresholds();
    }

    LOGINFO("ResourceMonitor configuration loaded");
    LOGINFO("psi_metrics=%d", static_cast<int>(_psiMetric));
    LOGINFO("polling_interval=%u", _pollingInterval);
    LOGINFO("Low_memory_warning_enabled=%u", _lowMemoryWarningsEnabled);
    LOGINFO("warning_threshold_min_mem=%llu", static_cast<unsigned long long>(_warningThresholdMinMem));
    LOGINFO("soft_threshold_min_mem=%llu", static_cast<unsigned long long>(_softThresholdMinMem));
    LOGINFO("hard_threshold_min_mem=%llu", static_cast<unsigned long long>(_hardThresholdMinMem));
    LOGINFO("warning_threshold_max_swap_percentage=%u", _warningThresholdMaxSwapPercentage);
    LOGINFO("soft_threshold_max_swap_percentage=%u", _softThresholdMaxSwapPercentage);
    LOGINFO("hard_threshold_max_swap_percentage=%u", _hardThresholdMaxSwapPercentage);
    LOGINFO("warning_threshold_max_psi=%.2f", _warningThresholdMaxPsi);
    LOGINFO("soft_threshold_max_psi=%.2f", _softThresholdMaxPsi);
    LOGINFO("hard_threshold_max_psi=%.2f", _hardThresholdMaxPsi);
    LOGINFO("soft_threshold_delta_percentage=%u", _softThresholdDeltaPercentage);
    LOGINFO("hard_threshold_delta_percentage=%u", _hardThresholdDeltaPercentage);
    return true;
}
} // namespace Plugin
} // namespace WPEFramework