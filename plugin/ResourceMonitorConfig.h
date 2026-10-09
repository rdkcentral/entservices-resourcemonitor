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

#include "Module.h"

#include <cstdint>
#include <string>

namespace WPEFramework {
enum class PsiMetric {
    SOME_AVG10,
    SOME_AVG60,
    SOME_AVG300,
    FULL_AVG10,
    FULL_AVG60,
    FULL_AVG300
};

namespace Plugin {
// Runtime configuration.
class ResourceMonitorConfigJSON;

class ResourceMonitorConfig {
public:
    static constexpr uint64_t DEFAULT_WARNING_MIN_MEM = 300000;
    static constexpr uint32_t DEFAULT_WARNING_MAX_SWAP = 40;
    static constexpr uint32_t DEFAULT_WARNING_MAX_PSI = 1;
    static constexpr uint32_t DEFAULT_POLLING_INTERVAL = 5000;
    static constexpr uint32_t DEFAULT_PSI_EXCESS_DURATION = 50000;
    static constexpr uint32_t DEFAULT_PSI_POST_ACTION_DELAY = 1000000;
    static constexpr char DEFAULT_PSI_PATH[] = "/proc/pressure/memory";
    static constexpr uint32_t DEFAULT_FLASH_SIZE = 300;
    static constexpr char DEFAULT_FLASH_MOUNT_POINT[] = "/mnt/media/apps/hibernated_apps";
    static constexpr uint64_t MIN_WARNING_MEMORY = 1;
    static constexpr uint32_t MIN_POLLING_INTERVAL = 10;
    static constexpr uint32_t MAX_PERCENTAGE = 100;
    static constexpr uint32_t DEFAULT_MIN_POST_WARNING_DELAY = 60;
    static constexpr uint32_t DEFAULT_SOFT_THRESHOLD_DELTA = 10;
    static constexpr uint32_t DEFAULT_HARD_THRESHOLD_DELTA = 20;
    static constexpr const char* RFC_POLLING_INTERVAL = "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Resourcemonitor.PollingInterval";
    static constexpr const char* RFC_LOW_MEMORY_WARNING = "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Resourcemonitor.LowMemoryWarningEnabled";
    static constexpr const char* RFC_WARNING_MIN_MEM = "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Resourcemonitor.WarningThresholdMinMemory";
    static constexpr const char* RFC_WARNING_MAX_SWAP = "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Resourcemonitor.WarningThresholdMaxSwapPercentage";
    static constexpr const char* RFC_WARNING_MAX_PSI = "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Resourcemonitor.WarningThresholdMaxPsi";
    static constexpr const char* RFC_MIN_POST_WARNING_DELAY = "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Resourcemonitor.MinPostWarningDelay";
    static constexpr const char* RFC_SOFT_DELTA = "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Resourcemonitor.SoftThresholdDeltaPercentage";
    static constexpr const char* RFC_HARD_DELTA = "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Resourcemonitor.HardThresholdDeltaPercentage";

    ResourceMonitorConfig();

    // Load configuration from the plugin configuration JSON.
    bool Load(const ResourceMonitorConfigJSON& json);

    // Getters used by the APIs
    bool PsiCheckingEnabled() const
    {
        return _psiCheckingEnabled;
    }

    bool ZramCheckingEnabled() const
    {
        return _zramCheckingEnabled;
    }

    bool LowMemoryWarningsEnabled() const
    {
        return _lowMemoryWarningsEnabled;
    }

    const std::string& PsiPath() const
    {
        return _psiPath;
    }

    PsiMetric GetPsiMetric() const
    {
        return _psiMetric;
    }


    uint32_t PsiExcessDuration() const
    {
        return _psiExcessDuration;
    }

    uint32_t PsiPostActionDelay() const
    {
        return _psiPostActionDelay;
    }

    uint64_t FlashSpaceForHibernatedAppsSize() const
    {
        return _flashSpaceForHibernatedAppsSize;
    }

    const std::string& FlashSpaceForHibernatedAppsMountPoint() const
    {
        return _flashSpaceForHibernatedAppsMountPoint;
    }

    uint32_t PollingInterval() const
    {
        return _pollingInterval;
    }

    uint64_t WarningThresholdMinMem() const
    {
        return _warningThresholdMinMem;
    }

    uint64_t SoftThresholdMinMem() const
    {
        return _softThresholdMinMem;
    }

    uint64_t HardThresholdMinMem() const
    {
        return _hardThresholdMinMem;
    }

    uint32_t WarningThresholdMaxSwapPercentage() const
    {
        return _warningThresholdMaxSwapPercentage;
    }

    uint32_t SoftThresholdMaxSwapPercentage() const
    {
        return _softThresholdMaxSwapPercentage;
    }

    uint32_t HardThresholdMaxSwapPercentage() const
    {
        return _hardThresholdMaxSwapPercentage;
    }

    double WarningThresholdMaxPsi() const
    {
        return _warningThresholdMaxPsi;
    }

    double SoftThresholdMaxPsi() const
    {
        return _softThresholdMaxPsi;
    }

    double HardThresholdMaxPsi() const
    {
        return _hardThresholdMaxPsi;
    }

    uint32_t SoftThresholdDeltaPercentage() const
    {
        return _softThresholdDeltaPercentage;
    }

    uint32_t HardThresholdDeltaPercentage() const
    {
        return _hardThresholdDeltaPercentage;
    }

    uint32_t MinPostWarningDelay() const
    {
        return _minPostWarningDelay;
    }

private:

    void SetDefaults();
    bool IsValidPsiPath(const std::string& value) const;
    bool IsValidPollingInterval(uint32_t value) const;
    bool IsValidPsiExcessDuration(uint32_t value) const;
    bool IsValidPsiPostActionDelay(uint32_t value) const;
    bool IsValidMemoryWarningThreshold(uint64_t value) const;
    bool IsValidPercentage(uint32_t value) const;

    bool CalculateMemoryThresholds();
    bool CalculateSwapThresholds();
    bool CalculatePsiThresholds();

private:

    bool _psiCheckingEnabled;
    bool _zramCheckingEnabled;
    bool _lowMemoryWarningsEnabled;

    std::string _psiPath;
    PsiMetric _psiMetric;

    uint32_t _psiExcessDuration;
    uint32_t _psiPostActionDelay;

    uint64_t _flashSpaceForHibernatedAppsSize;
    std::string _flashSpaceForHibernatedAppsMountPoint;

    uint32_t _pollingInterval;
    uint32_t _minPostWarningDelay;

    // User configurable warning thresholds.
    uint64_t _warningThresholdMinMem;
    uint32_t _warningThresholdMaxSwapPercentage;
    double _warningThresholdMaxPsi;


    uint32_t _softThresholdDeltaPercentage;
    uint32_t _hardThresholdDeltaPercentage;

    //RFC values
    const char* _pollingIntervalRfcValue;

    // Derived thresholds.
    uint64_t _softThresholdMinMem;
    uint64_t _hardThresholdMinMem;

    uint32_t _softThresholdMaxSwapPercentage;
    uint32_t _hardThresholdMaxSwapPercentage;

    double _softThresholdMaxPsi;
    double _hardThresholdMaxPsi;
};

// This represents the values coming from ResourceMonitor.conf.

class ResourceMonitorConfigJSON : public Core::JSON::Container {
public:
    class RootConfig : public Core::JSON::Container {
    public:
        RootConfig() // chweck naming convention
            : Core::JSON::Container()
            , PsiCheckingEnabled(true)
            , ZramCheckingEnabled(true)
            , PsiPath(ResourceMonitorConfig::DEFAULT_PSI_PATH)
            , PsiMetrics(PsiMetric::FULL_AVG60)
            , PsiExcessDuration(ResourceMonitorConfig::DEFAULT_PSI_EXCESS_DURATION)
            , PsiPostActionDelay(ResourceMonitorConfig::DEFAULT_PSI_POST_ACTION_DELAY)
            , FlashSpaceForHibernatedAppsSize(ResourceMonitorConfig::DEFAULT_FLASH_SIZE)
            , FlashSpaceForHibernatedAppsMountPoint(ResourceMonitorConfig::DEFAULT_FLASH_MOUNT_POINT)
            , PollingInterval(ResourceMonitorConfig::DEFAULT_POLLING_INTERVAL)
            , LowMemoryWarningsEnabled(true)
            , WarningThresholdMinMem(ResourceMonitorConfig::DEFAULT_WARNING_MIN_MEM)
            , WarningThresholdMaxSwapPercentage(ResourceMonitorConfig::DEFAULT_WARNING_MAX_SWAP)
            , WarningThresholdMaxPsi(ResourceMonitorConfig::DEFAULT_WARNING_MAX_PSI)
            , SoftThresholdDeltaPercentage(ResourceMonitorConfig::DEFAULT_SOFT_THRESHOLD_DELTA)
            , HardThresholdDeltaPercentage(ResourceMonitorConfig::DEFAULT_HARD_THRESHOLD_DELTA)
            , MinPostWarningDelay(ResourceMonitorConfig::DEFAULT_MIN_POST_WARNING_DELAY)
        {
            Add(_T("psi_checking_enabled"), &PsiCheckingEnabled);
            Add(_T("zram_checking_enabled"), &ZramCheckingEnabled);

            Add(_T("psi_path"), &PsiPath);
            Add(_T("psi_metrics"), &PsiMetrics);

            Add(_T("psi_excess_duration"), &PsiExcessDuration);
            Add(_T("psi_post_action_delay"), &PsiPostActionDelay);

            Add(_T("flash_space_for_hibernated_apps_size"), &FlashSpaceForHibernatedAppsSize);
            Add(_T("flash_space_for_hibernated_apps_mount_point"), &FlashSpaceForHibernatedAppsMountPoint);

            Add(_T("polling_interval"), &PollingInterval);

            Add(_T("low_memory_warnings_enabled"), &LowMemoryWarningsEnabled);

            Add(_T("warning_threshold_min_mem"), &WarningThresholdMinMem);
            Add(_T("warning_threshold_max_swap_percentage"), &WarningThresholdMaxSwapPercentage);
            Add(_T("warning_threshold_max_psi"), &WarningThresholdMaxPsi);
            Add(_T("soft_threshold_delta_percentage"), &SoftThresholdDeltaPercentage);
            Add(_T("hard_threshold_delta_percentage"), &HardThresholdDeltaPercentage);
            Add(_T("min_post_warning_delay"), &MinPostWarningDelay);
        }

        Core::JSON::Boolean PsiCheckingEnabled;
        Core::JSON::Boolean ZramCheckingEnabled;

        Core::JSON::String PsiPath;
        Core::JSON::EnumType<PsiMetric> PsiMetrics;

        Core::JSON::DecUInt32 PsiExcessDuration;
        Core::JSON::DecUInt32 PsiPostActionDelay;

        Core::JSON::DecUInt64 FlashSpaceForHibernatedAppsSize;
        Core::JSON::String FlashSpaceForHibernatedAppsMountPoint;

        Core::JSON::DecUInt32 PollingInterval;

        Core::JSON::Boolean LowMemoryWarningsEnabled;

        Core::JSON::DecUInt64 WarningThresholdMinMem;
        Core::JSON::DecUInt32 WarningThresholdMaxSwapPercentage;
        Core::JSON::DecUInt32 WarningThresholdMaxPsi;
        Core::JSON::DecUInt32 SoftThresholdDeltaPercentage;
        Core::JSON::DecUInt32 HardThresholdDeltaPercentage;
        Core::JSON::DecUInt32 MinPostWarningDelay;
    };

    ResourceMonitorConfigJSON()
        : Core::JSON::Container()
        , Root()
    {
        Add(_T("root"), &Root);
    }

    RootConfig Root;
};

} // namespace Plugin
} // namespace WPEFramework