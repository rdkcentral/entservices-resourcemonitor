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

#pragma once

#include "Module.h"
#include "ResourceMonitorConfig.h"
#include <interfaces/IResourceMonitor.h>
#include <interfaces/IVictimSelector.h>
#include <interfaces/IConfiguration.h>
#include <interfaces/json/JResourceMonitor.h>
#include <interfaces/json/JsonData_ResourceMonitor.h>


namespace WPEFramework {
    namespace Plugin {
        class ResourceMonitor : public PluginHost::IPlugin, public PluginHost::JSONRPC {
        private:
            class Notification : public RPC::IRemoteConnection::INotification, public Exchange::IResourceMonitor::INotification
            {
                private:
                    Notification() = delete;
                    Notification(const Notification&) = delete;
                    Notification& operator=(const Notification&) = delete;
                    ResourceMonitor& _parent;

                public:
                    explicit Notification(ResourceMonitor* parent)
                        : _parent(*parent)
                    {
                        ASSERT(parent != nullptr);
                    }
                    virtual ~Notification()
                    {
                    }

                    BEGIN_INTERFACE_MAP(Notification)
                        INTERFACE_ENTRY(Exchange::IResourceMonitor::INotification)
                        INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                    END_INTERFACE_MAP

                     void Activated(RPC::IRemoteConnection*) override
                    {
                    }

                    void Deactivated(RPC::IRemoteConnection *connection) override
                    {
                        _parent.Deactivated(connection);
                    }

                    void OnReconciliationComplete(const string& appId, const bool targetRamAchieved) override
                    {
                        Exchange::JResourceMonitor::Event::OnReconciliationComplete(_parent, appId, targetRamAchieved);
                    }
            };

        public:
            ResourceMonitor(const ResourceMonitor&) = delete;
            ResourceMonitor& operator=(const ResourceMonitor&) = delete;

            ResourceMonitor();
            virtual ~ResourceMonitor();

            BEGIN_INTERFACE_MAP(ResourceMonitor)
                INTERFACE_ENTRY(PluginHost::IPlugin)
                INTERFACE_ENTRY(PluginHost::IDispatcher)
                INTERFACE_AGGREGATE(WPEFramework::Exchange::IResourceMonitor, mResourceMonitor)
            END_INTERFACE_MAP

            // IPlugin Methods
            const string Initialize(PluginHost::IShell* service) override;
            void Deinitialize(PluginHost::IShell* service) override;
            string Information() const override;

        private:
            void Deactivated(RPC::IRemoteConnection* connection);
            PluginHost::IShell* mService;
            uint32_t _connectionId{};
            WPEFramework::Exchange::IResourceMonitor* mResourceMonitor{};
            Core::Sink<Notification> mResourceMonitorNotification;
            Exchange::IConfiguration* mConfigure;
        };
    } // Plugin
} // WPEFramework
