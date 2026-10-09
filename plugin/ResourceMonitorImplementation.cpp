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


#include "ResourceMonitorImplementation.h"
#include "ResourceMonitorConfig.h"
#include "UtilsLogging.h"
#include <tracing/Logging.h>

#include <stdlib.h>
#include <errno.h>
#include <string>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <algorithm>
#include <sstream>

namespace WPEFramework {
    namespace Plugin {
    SERVICE_REGISTRATION(ResourceMonitorImplementation, 1, 0, 0)
    ResourceMonitorImplementation* ResourceMonitorImplementation::_instance = nullptr;
        ResourceMonitorImplementation::ResourceMonitorImplementation()
            : _peakSystemMemUsed(0)
            , _peakGpuMemUsed(0)
            , _peakSwapUsed(0)
            , _peakFlashSpaceUsed(0)
            , _reactiveMonitorRunning(false)
            , _warningStartTime(0)
            , _warningPrinted(false)
            , _psiThresholdExceededSince(0)
            , _lastPsiActionTime(0)
            , _reconcileAppSystemMemoryLimitKb(0)
            , _reconcileInProgress(false)
            , _workerRunning(true)
            , _evictionInProgress(false)
            , _reconcileAppId("")
            , _victimSelector(nullptr)
            , _victimSelectorNotification(this)
            , _adminLock()
            , _rservice(nullptr)
            {
                LOGINFO("ResourceMonitorImplementation constructor invoked");
                _instance = this;
            }

        ResourceMonitorImplementation::~ResourceMonitorImplementation() 
        {
            LOGINFO("ResourceMonitorImplementation Destructor");
            _reactiveMonitorRunning = false;

            if (_reactiveMonitorThread.joinable()) {
                _reactiveMonitorThread.join();
            }
            if (_victimSelector != nullptr) {
                _victimSelector->Unregister(&_victimSelectorNotification);
                _victimSelector->Release();
                _victimSelector = nullptr;
                LOGINFO("VictimSelector notification unregistered");
            }
            {
                std::lock_guard<std::mutex> lock(_reconcileMutex);
                _workerRunning.store(false);
                _reconcileConditionVariable.notify_all();
            }

            if (_reconcileThread.joinable()) {
                _reconcileThread.join();
            }

            if (_rservice != nullptr) {
                _rservice->Release();
                _rservice = nullptr;
            }

            if (_instance == this) {
                _instance = nullptr;
            }
        }

        Core::hresult ResourceMonitorImplementation::Configure(PluginHost::IShell* service)
        {
            SYSLOG(Logging::Startup, (_T("VictimSelector Integration entry")));
            Core::hresult result = Core::ERROR_BAD_REQUEST;
            if (nullptr == service) {
                SYSLOG(Logging::Startup, (_T("service is not valid")));
                LOGERR("VictimSelector configuration failed: service is null");
            } else {
                _rservice = service;
                _rservice->AddRef();
                _victimSelector = _rservice->QueryInterfaceByCallsign<WPEFramework::Exchange::IVictimSelector>("org.rdk.VictimSelector");
                if (_victimSelector == nullptr) {
                    LOGERR("VictimSelector Interface is unavailable. ResourceMonitor will continue without Evict support");
                    result = Core::ERROR_NONE;
                } else {
                    _victimSelector->Register(&_victimSelectorNotification);
                    LOGINFO("VictimSelector notification registered");
                    result = Core::ERROR_NONE;
                }
            }
            SYSLOG(Logging::Startup, (_T("VictimSelector exit: result=%d"), result));
            return result;
        }

        Core::hresult ResourceMonitorImplementation::SetConfig(const string& config)
        {
            ResourceMonitorConfigJSON configJSON;
            if (!config.empty()) {
                if (!configJSON.FromString(config)) {
                    LOGERR("ResourceMonitorImplementation: invalid configuration");
                    return Core::ERROR_BAD_REQUEST;
                }
            }

            _config.Load(configJSON);
            LOGINFO("ResourceMonitorImplementation configuration loaded");

            if (!_reactiveMonitorRunning) {
                _reactiveMonitorRunning = true;
               _reactiveMonitorThread = std::thread(&ResourceMonitorImplementation::ReactiveMonitorLoop, this);
            }

            if (!_reconcileThread.joinable()) {
                _reconcileThread = std::thread(&ResourceMonitorImplementation::ReconcileWorker, this);
            }
            return Core::ERROR_NONE;

        }

        Core::hresult ResourceMonitorImplementation::Register(WPEFramework::Exchange::IResourceMonitor::INotification* notification)
        {
            if (notification == nullptr) {
                return Core::ERROR_BAD_REQUEST;
            }
            _adminLock.Lock();
            if (std::find(_notifications.begin(), _notifications.end(), notification) == _notifications.end()) {
                notification->AddRef();
                _notifications.push_back(notification);
                LOGINFO("ResourceMonitor: notification registered");
            }
            _adminLock.Unlock();
            return Core::ERROR_NONE;
        }

        Core::hresult ResourceMonitorImplementation::Unregister(WPEFramework::Exchange::IResourceMonitor::INotification* notification)
        {
            if (notification == nullptr) {
                return Core::ERROR_BAD_REQUEST;
            }
            _adminLock.Lock();
            auto index = std::find(_notifications.begin(), _notifications.end(), notification);
            if (index != _notifications.end()) {
                (*index)->Release();
                _notifications.erase(index);

                LOGINFO("ResourceMonitor: notification unregistered");
                _adminLock.Unlock();
                return Core::ERROR_NONE;
            }
            _adminLock.Unlock();
            return Core::ERROR_NOT_EXIST;
        }

        void ResourceMonitorImplementation::OnReconciliationComplete(const string& appId, const bool targetRamAchieved)
        {
            LOGINFO("ResourceMonitor: OnReconciliationComplete(appId=%s result=%s)", appId.c_str(), targetRamAchieved ? _T("true") : _T("false"));
            std::list<WPEFramework::Exchange::IResourceMonitor::INotification* > notifications;
            _adminLock.Lock();
            notifications = _notifications;
            for (auto notification : notifications) {
                notification->AddRef();
            }
            _adminLock.Unlock();
            for (auto notification : notifications) {
                notification->OnReconciliationComplete(appId, targetRamAchieved);
                notification->Release();
            }
        }

        Core::hresult ResourceMonitorImplementation::GetMemInfo(uint64_t& memAvailable, uint64_t& swapFree, uint64_t& swapTotal)
        {
            memAvailable = 0;
            swapFree = 0;
            swapTotal = 0;
            std::string key = "";
            uint64_t value = 0;
            std::string unit = "";

            LOGINFO("Entering GetMemInfo()");
            std::ifstream file("/proc/meminfo");

            if (!file.is_open()) {
                LOGERR("Unable to open /proc/meminfo");
                return Core::ERROR_GENERAL;
            }

            while (file >> key >> value >> unit) {
                if (key == "MemAvailable:") {
                    memAvailable = value;
                }
                else if (key == "SwapFree:") {
                    swapFree = value;
                }
                else if (key == "SwapTotal:") {
                    swapTotal = value;
                }
            }

            LOGINFO("MemAvailable = %llu KiB",static_cast<unsigned long long>(memAvailable));
            LOGINFO("SwapFree = %llu KiB",static_cast<unsigned long long>(swapFree));
            LOGINFO("SwapTotal=%llu KiB",static_cast<unsigned long long>(swapTotal));
            return Core::ERROR_NONE;
        }

        Core::hresult ResourceMonitorImplementation::GetPsiMetrics(const string& metric, double& value, uint64_t& total)
        {
            const PsiMetric m = _config.GetPsiMetric();
            const char* group = "";
            const char* avg = "";
            std::string line = "";
            std::string token = "";

            LOGINFO("Entering GetPsiMetrics()");
            value = 0.0;
            total = 0;
            if (!_config.PsiCheckingEnabled())
                return Core::ERROR_UNAVAILABLE;

            switch (m) {
                case PsiMetric::SOME_AVG10:
                    group = "some";
                    avg = "avg10=";
                    break;

                case PsiMetric::SOME_AVG60:
                    group = "some";
                    avg = "avg60=";
                    break;

                case PsiMetric::SOME_AVG300:
                    group = "some";
                    avg = "avg300=";
                    break;

                case PsiMetric::FULL_AVG10:
                    group = "full";
                    avg = "avg10=";
                    break;

                case PsiMetric::FULL_AVG60:
                    group = "full";
                    avg = "avg60=";
                    break;

                case PsiMetric::FULL_AVG300:
                    group = "full";
                    avg = "avg300=";
                    break;

                default:
                    return Core::ERROR_BAD_REQUEST;
            }

            std::ifstream file(_config.PsiPath());

            if (!file.is_open())
                return Core::ERROR_GENERAL;

            while (std::getline(file, line)) {
                std::stringstream ss(line);
                ss >> token;
                if (token != group)
                    continue;
                while (ss >> token) {
                    if (token.rfind(avg, 0) == 0) {
                        try {
                            value = std::stod(
                                token.substr(std::string(avg).length()));
                            return Core::ERROR_NONE;
                        }
                        catch (...) {
                            return Core::ERROR_GENERAL;
                        }
                    }
                    if (token.rfind("total=", 0) == 0) {
                        try {
                            total = std::stoull(token.substr(std::string("total=").length()));
                        }
                        catch (...) {
                            return Core::ERROR_GENERAL;
                        }
                    }
                }
                return Core::ERROR_NONE;
            }
            return Core::ERROR_UNKNOWN_KEY;
        }

        Core::hresult ResourceMonitorImplementation::GetFlashSpace(uint64_t& total, uint64_t& used, uint64_t& available)
        {
        const std::string& mountPoint = _config.FlashSpaceForHibernatedAppsMountPoint();
        const uint64_t configuredSizeMB = _config.FlashSpaceForHibernatedAppsSize();
        const uint64_t configuredSizeBytes = configuredSizeMB * 1024ULL * 1024ULL;
        uint64_t directoryUsedBytes = 0;

        LOGINFO("Entering GetFlashSpace()");
    
        // Ensure directory exists
        struct stat statBuffer {};
        if (stat(mountPoint.c_str(), &statBuffer) != 0) {
            if (errno == ENOENT) {
                if (mkdir(mountPoint.c_str(), 0744) != 0) {
                    LOGERR("Failed to create hibernated apps path=%s", mountPoint.c_str());
                    return Core::ERROR_GENERAL;
                }
                LOGINFO("Created hibernated apps path=%s", mountPoint.c_str());
            } else {
                LOGERR("Failed to access hibernated apps path=%s", mountPoint.c_str());
                return Core::ERROR_GENERAL;
            }
        }
    
        // Query filesystem space
        struct statvfs vfs {};
        if (statvfs(mountPoint.c_str(), &vfs) != 0) {
            LOGERR("statvfs failed for %s, errno=%d", mountPoint.c_str(), errno);
            return Core::ERROR_GENERAL;
        }

        std::string command = "du -sb " + mountPoint + " 2>/dev/null";
        FILE* pipe = popen(command.c_str(), "r");
        if (pipe != nullptr) {
            unsigned long long size = 0;
            if (fscanf(pipe, "%llu", &size) == 1) {
                directoryUsedBytes = static_cast<uint64_t>(size);
            }
            pclose(pipe);
        }

        total = configuredSizeBytes;
        used = directoryUsedBytes;
        available = (used >= total) ? 0 : (total - used);
    
        LOGINFO("Flash mount point=%s Total=%llu Used=%llu Available=%llu",
            mountPoint.c_str(),
            static_cast<unsigned long long>(total),
            static_cast<unsigned long long>(used),
            static_cast<unsigned long long>(available));

        return Core::ERROR_NONE;
        }


        Core::hresult ResourceMonitorImplementation::GetSwapUsed(uint64_t& swapUsed, uint64_t& memUsedTotal)
        {
            uint64_t origDataSize = 0;
            uint64_t comprDataSize = 0;
            swapUsed = 0;
            memUsedTotal = 0;
            LOGINFO("Entering GetSwapUsed()");

            // Read orig_data_size from mm_stat
            std::ifstream mmStat("/sys/block/zram0/mm_stat");

            if (!mmStat.is_open()) {
                LOGERR("Failed to open /sys/block/zram0/mm_stat");
                return Core::ERROR_GENERAL;
            }
            mmStat >> origDataSize >> comprDataSize >> memUsedTotal;
            if (mmStat.fail()) {
                LOGERR("Failed to parse /sys/block/zram0/mm_stat");
                return Core::ERROR_GENERAL;
            }

            swapUsed = origDataSize;
            LOGINFO("SwapUsed = %llu bytes, MemUsedTotal = %llu bytes",
                static_cast<unsigned long long>(swapUsed),
                static_cast<unsigned long long>(memUsedTotal));
            return Core::ERROR_NONE;
        }

        Core::hresult ResourceMonitorImplementation::GetStats(WPEFramework::Exchange::ResourceMonitorStats& stats)
        {
            uint64_t memAvailable = 0;
            uint64_t swapFree = 0;
            uint64_t swapTotal = 0;
            uint64_t memTotal = 0;
            std::string key = "";
            uint64_t value = 0;
            std::string unit = "";
            uint64_t swapUsed = 0;
            uint64_t memUsedTotal = 0;
            uint64_t flashTotal = 0;
            uint64_t flashUsed = 0;
            uint64_t flashAvailable = 0;

            LOGINFO("Entering GetStats");
            memset(&stats, 0, sizeof(stats));
            GetMemInfo(memAvailable, swapFree, swapTotal);

            std::ifstream mem("/proc/meminfo");
            while (mem >> key >> value >> unit) {
                if (key == "MemTotal:")
                    memTotal = value;
            }

            stats.systemMemTotal = memTotal;
            stats.systemMemAvailable = memAvailable;
            stats.systemMemUsed = memTotal - memAvailable;

            if (stats.systemMemUsed > _peakSystemMemUsed)
            {
                _peakSystemMemUsed = stats.systemMemUsed;
            }
            stats.peakSystemMemUsed = _peakSystemMemUsed;

            // GPU (TO BE IMPLEMENTED)

            stats.gpuMemTotal = 0;
            stats.gpuMemUsed = 0;
            stats.gpuMemAvailable = 0;
            stats.peakGpuMemUsed = _peakGpuMemUsed;

            GetSwapUsed(swapUsed, memUsedTotal);

            stats.swapTotal = swapTotal;
            stats.swapFree = swapFree;
            stats.swapUsed = swapUsed;

            if (swapUsed > _peakSwapUsed)
            {
                _peakSwapUsed = swapUsed;
            }
            stats.peakSwapUsed = _peakSwapUsed;

            GetFlashSpace(flashTotal, flashUsed, flashAvailable);

            stats.flashSpaceTotal = flashTotal;
            stats.flashSpaceUsed = flashUsed;
            stats.flashSpaceAvailable = flashAvailable;

            if (flashUsed > _peakFlashSpaceUsed)
            {
                _peakFlashSpaceUsed = flashUsed;
            }
            stats.peakFlashSpaceUsed = _peakFlashSpaceUsed;
            LOGINFO("Leaving GetStats");
            return Core::ERROR_NONE;
        }

        uint32_t ResourceMonitorImplementation::CalculateSwapPercentage(uint64_t swapFree, uint64_t swapTotal) const
        {
            if (swapTotal == 0) {
                return 0;
            }
            return static_cast<uint32_t>((swapTotal - swapFree) * 100ULL / swapTotal);
        }

        void ResourceMonitorImplementation::ReactiveMonitorLoop()
        {
            uint64_t memAvailable = 0;
            uint64_t swapFree = 0;
            uint64_t swapTotal, total = 0;
            double psiFullAvg60 = 0.0;
            uint32_t memResult = 0;
            LOGINFO("Reactive monitor started");

            while (_reactiveMonitorRunning.load()) {
                memResult = GetMemInfo(memAvailable, swapFree, swapTotal);

                if (memResult != Core::ERROR_NONE) {
                    LOGERR("Reactive monitor: GetMemInfo failed");
                }

                if (_config.PsiCheckingEnabled()) {
                    const uint32_t psiResult = GetPsiMetrics("full_avg60", psiFullAvg60, total);
                    LOGINFO("GOT PSI metrics!");
                    if (psiResult != Core::ERROR_NONE) {
                        LOGERR("Reactive monitor: GetPsiMetrics failed");
                    }
                }

                EvaluateResourceThresholds(memAvailable, swapFree, swapTotal, psiFullAvg60);
                // Wait until next poll
                std::this_thread::sleep_for(std::chrono::milliseconds(_config.PollingInterval()));
            }
            LOGINFO("Reactive monitor stopped");
        }

        void ResourceMonitorImplementation::EvaluateResourceThresholds(uint64_t memAvailable, uint64_t swapFree, uint64_t swapTotal, double psiFullAvg60)
        {
            bool warningCondition = false;
            uint32_t swapPercentage = 0;
            uint64_t now = 0, swapUsed = 0, memUsedTotal = 0 , swapTotalBytes = 0, total =0;

            LOGINFO("Low_memory_warning_enabled=%u", _config.LowMemoryWarningsEnabled());
            if (_config.LowMemoryWarningsEnabled()) {
                LOGINFO("INFO memAvailable=%llu", static_cast<unsigned long long>(memAvailable));
                if (memAvailable) {
                    if (memAvailable <= _config.HardThresholdMinMem()) {
                        LOGWARN("Hard MEMORY threshold exceeded");
                        Evict("HardMemoryThresholdReached", "Kill");
                    }
                    else if (memAvailable <= _config.SoftThresholdMinMem()) {
                        LOGWARN("Soft MEMORY threshold exceeded");
                        Evict("SoftMemoryThresholdReached", "terminate");
                    }
                    else if (memAvailable <= _config.WarningThresholdMinMem()) {
                        warningCondition = true;
                    }
                }

                if (_config.ZramCheckingEnabled()) {
                    GetSwapUsed(swapUsed, memUsedTotal);
                    swapTotalBytes = swapTotal * 1024ULL;
                    if (swapTotalBytes > 0) {
                        double percent = (static_cast<double>(swapUsed) * 100.0) / static_cast<double>(swapTotalBytes);
                        swapPercentage = static_cast<uint32_t>(percent + 0.5); // round to nearest
                    }

                    LOGINFO("SWAP percentage: %.2f", (static_cast<double>(swapUsed) * 100.0) / static_cast<double>(swapTotalBytes));
                    if (swapPercentage >= _config.HardThresholdMaxSwapPercentage()) {
                        LOGWARN("Hard SWAP threshold exceeded");
                        Evict("HardSwapThresholdReached", "Kill");
                    }
                    else if (swapPercentage >= _config.SoftThresholdMaxSwapPercentage()) {
                        LOGWARN("Soft SWAP threshold exceeded");
                        Evict("SoftSwapThresholdReached", "terminate");
                    }
                    else if (swapPercentage >= _config.WarningThresholdMaxSwapPercentage()) {
                        warningCondition = true;
                    }
                }

                if (_config.PsiCheckingEnabled()) {
                    GetPsiMetrics("full_avg60", psiFullAvg60, total);
                    LOGINFO("psiFullAvg60=%.2f", psiFullAvg60);
                    const uint64_t nowMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
                    if (psiFullAvg60 >= _config.HardThresholdMaxPsi()) {
                        if (_psiThresholdExceededSince == 0) {
                            _psiThresholdExceededSince = nowMs;
                        }
                        if ((nowMs - _psiThresholdExceededSince) >= _config.PsiExcessDuration()) {
                            if ((nowMs - _lastPsiActionTime) >= _config.PsiPostActionDelay()) {
                                LOGWARN("Hard PSI threshold exceeded");
                                Evict("HardPsiThresholdExceeded", "Kill");
                                _lastPsiActionTime = nowMs;
                            }
                        }
                    }
                    else if (psiFullAvg60 >= _config.SoftThresholdMaxPsi()) {
                        if (_psiThresholdExceededSince == 0) {
                            _psiThresholdExceededSince = nowMs;
                        }
                        if ((nowMs - _psiThresholdExceededSince) >= _config.PsiExcessDuration()) {
                            if ((nowMs - _lastPsiActionTime) >= _config.PsiPostActionDelay()) {
                                LOGWARN("Soft PSI threshold exceeded");
                                Evict("SoftPsiThresholdExceeded", "terminate");
                                _lastPsiActionTime = nowMs;
                            }
                        }
                    }
                    else {
                        _psiThresholdExceededSince = 0;
                        if (psiFullAvg60 >= _config.WarningThresholdMaxPsi()) {
                            warningCondition = true;
                        }
                    }
                }

                if (!warningCondition) {
                    _warningStartTime = 0;
                    _warningPrinted = false;
                }
                else {
                    now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
                    if (_warningStartTime == 0) {
                        _warningStartTime = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
                    }

                    if (!_warningPrinted && ((now - _warningStartTime) >= _config.MinPostWarningDelay())) {
                        LOGWARN("warning threshold exceeded for %u seconds", _config.MinPostWarningDelay());
                        _warningPrinted = true;
                    }
                }
            }
        }

        void ResourceMonitorImplementation::ProcessReconcileRequest(const ReconcileRequest& request)
        {
            uint64_t memAvailable = 0;
            uint64_t swapFree = 0;
            uint64_t swapTotal = 0;
            uint64_t remainingMemory = 0;

            LOGINFO("Processing Reconcile: appId=%s ramTargetMB=%llu allowTerminate=%s", request.appId.c_str(), static_cast<unsigned long long>(request.appMemoryLimitKb / 1024ULL), request.allowTerminate ? _T("true") : _T("false"));

            const Core::hresult result = GetMemInfo(memAvailable, swapFree, swapTotal);

            if (result != Core::ERROR_NONE) {
                LOGERR("Reconcile: GetMemInfo failed");
                CompleteReconcile(false);
                return;
            }

            remainingMemory = (memAvailable > request.appMemoryLimitKb) ? (memAvailable - request.appMemoryLimitKb) : 0;
            LOGINFO(
                "Reconcile: memAvailable=%llu appMemoryLimit=%llu remaining=%llu",
                static_cast<unsigned long long>(memAvailable),
                static_cast<unsigned long long>(request.appMemoryLimitKb),
                static_cast<unsigned long long>(remainingMemory));

            // Enough memory available.
            if (remainingMemory > _config.HardThresholdMinMem()) {
                LOGINFO("Reconcile: sufficient memory");
                CompleteReconcile(true);
                return;
            }

            //Not enough memory and allowTerminate=true.
            if (!request.allowTerminate) {
                LOGWARN("Reconcile: insufficient memory and termination is not allowed");
                CompleteReconcile(false);
                return;
            }

            // Not enough memory and allowTerminate=true -> VictimSelector evict.
            LOGINFO("Reconcile: insufficient memory, requesting VictimSelector eviction");
            const Core::hresult evictionResult = Evict("systemMemoryLow", "terminate");
            if (evictionResult != Core::ERROR_NONE) {
                LOGERR("Reconcile: VictimSelector eviction request failed: %u", evictionResult);
                CompleteReconcile(false);
                return;
            }
        }

        Core::hresult ResourceMonitorImplementation::Reconcile(const string& appId, const uint32_t ramTargetMB, const bool allowTerminate) {
            ReconcileRequest request;
            request.appId = appId;
            request.appMemoryLimitKb = static_cast<uint64_t>(ramTargetMB) * 1024ULL;
            request.allowTerminate = allowTerminate;

            if (appId.empty()) {
                return Core::ERROR_BAD_REQUEST;
            }
            LOGINFO("Entering Reconcile(): appId=%s ramTargetMB=%u allowTerminate=%s", appId.c_str(), ramTargetMB, allowTerminate ? _T("true") : _T("false"));
            {
                std::lock_guard<std::mutex> lock(_reconcileMutex);
                _reconcileRequests.push(request);
                _reconcileConditionVariable.notify_one();
            }
            LOGINFO("Queued reconcile request for appId=%s, queueSize=%zu", appId.c_str(), _reconcileRequests.size());
            return Core::ERROR_NONE;
        }

        void ResourceMonitorImplementation::ReconcileWorker()
        {
            LOGINFO("Reconcile Worker!");
            while (_workerRunning.load()) {
                ReconcileRequest request;
                {
                    std::unique_lock<std::mutex> lock(_reconcileMutex);
                    _reconcileConditionVariable.wait(
                        lock,
                        [this] {
                            return !_workerRunning ||
                            (!_reconcileInProgress &&
                            !_reconcileRequests.empty());
                        });

                    if (!_workerRunning) {
                        break;
                    }
                    request = _reconcileRequests.front();
                    LOGINFO("POP appId=%s queueSizeBeforePop=%zu", request.appId.c_str(), _reconcileRequests.size());
                    _reconcileRequests.pop();
                    _reconcileInProgress = true;
                    _reconcileAppId = request.appId;
                    _reconcileAppSystemMemoryLimitKb = request.appMemoryLimitKb;
                }
                ProcessReconcileRequest(request);
            }
        }
        
        void ResourceMonitorImplementation::CompleteReconcile(const bool targetRamAchieved)
        {
            string completedAppId;
            {
                std::lock_guard<std::mutex> lock(_reconcileMutex);
                completedAppId = _reconcileAppId;
                _reconcileAppId.clear();
                _reconcileAppSystemMemoryLimitKb = 0;
                _reconcileInProgress = false;
            }
            OnReconciliationComplete(completedAppId, targetRamAchieved);
            _reconcileConditionVariable.notify_one();
        }

        Core::hresult ResourceMonitorImplementation::Evict(const std::string& reason, const std::string& action)
        {
            Exchange::IVictimSelector::EvictionReason evictionReason = Exchange::IVictimSelector::EVICTION_REASON_RAM;
            Exchange::IVictimSelector::EvictionType evictionType = Exchange::IVictimSelector::EVICTION_TYPE_SOFT;
            LOGINFO("VictimSelector Evict is called!");

            if (_evictionInProgress.exchange(true)) {
                LOGWARN("Eviction already in progress");
                return Core::ERROR_INPROGRESS;
            }
            
            if (_victimSelector == nullptr) {
                _evictionInProgress = false;
                LOGWARN("VictimSelector is not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Mapping VictimSelector eviction types.
            if (action == "Kill") {
                evictionType = Exchange::IVictimSelector::EVICTION_TYPE_HARD;
            }
            else if (action == "terminate") {
                evictionType = Exchange::IVictimSelector::EVICTION_TYPE_SOFT;
            }
            else {
                _evictionInProgress = false;
                LOGWARN("Invalid VictimSelector action: %s", action.c_str());
                return Core::ERROR_BAD_REQUEST;
            }

            // RAM for now as GPU and FLASH are not implemented yet.
            evictionReason = Exchange::IVictimSelector::EVICTION_REASON_RAM;
            LOGINFO("Calling VictimSelector::Evict: reason=%s action=%s", reason.c_str(), action.c_str());

            const Core::hresult result = _victimSelector->Evict(evictionReason, evictionType);
            if (result != Core::ERROR_NONE) {
                _evictionInProgress = false;
                LOGERR("VictimSelector::Evict failed!: result=%u", result);
            }
            return result;
        }

        void ResourceMonitorImplementation::OnEvictComplete(const bool evicted, const Exchange::IVictimSelector::EvictErrorReason errorCode)
        {
            string appId = "";
            uint64_t appMemoryLimitKb = 0;
            uint64_t memAvailable = 0;
            uint64_t swapFree = 0;
            uint64_t swapTotal, remainingMemory = 0;
            Core::hresult result = Core::ERROR_NONE;
            _evictionInProgress = false;
            LOGINFO("VictimSelector::OnEvictComplete: evicted=%s errorCode=%u", evicted ? _T("true") : _T("false"), static_cast<uint32_t>(errorCode));

            // No active Reconcile. This eviction was triggered by the reactive resource monitoring path.
            _adminLock.Lock();
            if (!_reconcileInProgress) {
                _adminLock.Unlock();
                LOGINFO("VictimSelector eviction is not part of reconciliation");
                return;
            }

            appId = _reconcileAppId;
            appMemoryLimitKb = _reconcileAppSystemMemoryLimitKb;
            _adminLock.Unlock();

            // VictimSelector could not evict a victim.
            if (!evicted) {
                LOGERR("VictimSelector eviction failed for appId=%s errorCode=%u", appId.c_str(), static_cast<uint32_t>(errorCode));
                CompleteReconcile(false);
                return;
            }

            // VictimSelector successfully completed the eviction. Now check whether enough memory was actually recovered.
            result = GetMemInfo(memAvailable, swapFree, swapTotal);
            if (result != Core::ERROR_NONE) {
                LOGERR("Failed to get memory information after eviction");
                CompleteReconcile(false);
                return;
            }

            remainingMemory = (memAvailable > appMemoryLimitKb) ? (memAvailable - appMemoryLimitKb) : 0;
            LOGINFO(
                "After eviction: appId=%s memAvailable=%llu limit=%llu remaining=%llu",
                appId.c_str(),
                static_cast<unsigned long long>(memAvailable),
                static_cast<unsigned long long>(appMemoryLimitKb),
                static_cast<unsigned long long>(remainingMemory));

            // Reconciliation succeeded.
            if (remainingMemory > _config.HardThresholdMinMem()) {
                LOGINFO("Reconcile successful after VictimSelector eviction");
                CompleteReconcile(true);
                return;
            }

            // Still not enough memory. Ask VictimSelector for another victim. The SAME Reconcile remains active.
            LOGWARN("Still insufficient memory. Requesting another eviction");
            const Core::hresult evictionResult = Evict("systemMemoryLow", "terminate");
            if (evictionResult != Core::ERROR_NONE) {
                LOGERR("VictimSelector Evict failed during continued reconciliation");
                CompleteReconcile(false);
            }
        }
    } // namespace Plugin
} // namespace WPEFramework

