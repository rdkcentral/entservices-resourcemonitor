/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
*/

#include "ResourceMonitor.h" 
#include "Module.h"
#include <tracing/Logging.h>
#include "UtilsLogging.h"
#include <interfaces/IResourceMonitor.h>
#include <interfaces/json/JResourceMonitor.h>

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework {
    namespace Plugin {
        namespace {
            static Metadata<ResourceMonitor> metadata(
            // Version
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
            );
        }

        SERVICE_REGISTRATION(ResourceMonitor, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

        ResourceMonitor::ResourceMonitor()
            : mService(nullptr)
            , _connectionId(0)
            , mResourceMonitor(nullptr)
            , mResourceMonitorNotification(this)
            , mConfigure(nullptr)
        {
            LOGINFO("ResourceMonitor Constructor");
        }

        ResourceMonitor::~ResourceMonitor()
        {
            LOGINFO("ResourceMonitor Destructor");
        }

        const string ResourceMonitor::Initialize(PluginHost::IShell* service) {
            ASSERT(nullptr == mService);
            ASSERT(nullptr == mResourceMonitor);
            ASSERT(nullptr == mConfigure);
            ASSERT(0 == _connectionId);

            SYSLOG(Logging::Startup, (_T("ResourceMonitor::Initialize: PID=%u"), getpid()));
            string result;
            if (nullptr == service) {
                LOGERR("ResourceMonitor initialization failed: service is null");
                result = "ResourceMonitor received an invalid service";
            } else {
                const string configLine = service->ConfigLine();
                mService = service;
                mService->AddRef();
                mService->Register(&mResourceMonitorNotification);
                LOGINFO("Getting ResourceMonitorImplementation");

                mResourceMonitor = mService->Root<Exchange::IResourceMonitor>(_connectionId, 5000, _T("ResourceMonitorImplementation"));

                if(nullptr == mResourceMonitor) {
                    LOGERR("ResourceMonitor initialization failed: implementation could not be created");
                    result = "ResourceMonitor implementation could not be created";
                } else {
                    LOGINFO("ResourceMonitorImplementation obtained successfully");
                    Exchange::JResourceMonitor::Register(*this, mResourceMonitor);
                    const uint32_t configResult = mResourceMonitor->SetConfig(configLine);
                    if (configResult != Core::ERROR_NONE) {
                        LOGERR("ResourceMonitor SetConfig failed: status=%d", configResult);
                        result = "ResourceMonitor plugin could not be configured";
                    } else {
                        mConfigure = mResourceMonitor->QueryInterface<Exchange::IConfiguration>();
                        if (nullptr == mConfigure) {
                            LOGERR("ResourceMonitor initialization failed: implementation has no configuration interface");
                            result = "ResourceMonitor implementation has no configuration interface";
                        } else {
                            const Core::hresult configureStatus = mConfigure->Configure(mService);
                            if (Core::ERROR_NONE != configureStatus) {
                                LOGERR("ResourceMonitor initialization failed: configuration returned status=%d", configureStatus);
                                result = "ResourceMonitor could not be configured";
                            } else {
                                mResourceMonitor->Register(&mResourceMonitorNotification);
                                LOGINFO("ResourceMonitorImplementation ready");
                            }
                        }
                    }
                }
                if (!result.empty()) {
                    Deinitialize(service);
                } 
            }
            return result;
        }

        void ResourceMonitor::Deinitialize(PluginHost::IShell* service) {
            ASSERT(mService == service);

            SYSLOG(Logging::Shutdown, (string(_T("ResourceMonitor::Deinitialize"))));
            if (nullptr != mResourceMonitor) {
                mResourceMonitor->Unregister(&mResourceMonitorNotification);
                Exchange::JResourceMonitor::Unregister(*this);
                RPC::IRemoteConnection* connection = mService->RemoteConnection(_connectionId);
                mResourceMonitor->Release();
                mResourceMonitor = nullptr;
                if (connection != nullptr)
                {
                    try
                    {
                        connection->Terminate();
                        LOGERR("Connection terminated successfully");
                    }
                    catch (const std::exception& e)
                    {
                        std::string errorMessage = "Failed to terminate connection: ";
                        errorMessage += e.what();
                        LOGERR("%s", errorMessage.c_str());
                    }
                    connection->Release();
                }
            }
            _connectionId = 0;
            mService->Unregister(&mResourceMonitorNotification);
            mService->Release();
            mService = nullptr;
            SYSLOG(Logging::Shutdown, (string(_T("ResourceMonitor de-initialised"))));
        }

        string ResourceMonitor::Information() const {
            return "Plugin which exposes ResourceMonitor related methods.";
        }

        void ResourceMonitor::Deactivated(RPC::IRemoteConnection* connection)
        {
            if (connection->Id() == _connectionId) {
                ASSERT(nullptr != mService);
                Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(mService, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
            }
        }


    } // Plugin
} // WPEFramework
