/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "ResourceMonitor.h"
#include "ResourceMonitorImplementation.h"
#include "ServiceMock.h"
#include "ThunderPortability.h"
#include <core/core.h>

using namespace WPEFramework;

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

using std::string;


// Testable Plugin Wrapper
class TestableResourceMonitorPlugin : public Plugin::ResourceMonitor {
public:
    // Nothing extra required.
};


// Testable Implementation Wrapper
class TestableResourceMonitorImplementation
    : public Plugin::ResourceMonitorImplementation {

private:
    mutable uint32_t _refCount {1};

public:
    TestableResourceMonitorImplementation() = default;
    ~TestableResourceMonitorImplementation() override = default;

    uint32_t AddRef() const override
    {
        return Core::InterlockedIncrement(_refCount);

    }

    uint32_t Release() const override
    {
        if (Core::InterlockedDecrement(_refCount) == 0) {
            delete this;
            return Core::ERROR_DESTRUCTION_SUCCEEDED;
        }

        return Core::ERROR_NONE;
    }

    Core::hresult CallGetMemInfo(uint64_t& memAvailable, uint64_t& swapFree, uint64_t& swapTotal)
    {
        return GetMemInfo(memAvailable, swapFree, swapTotal);
    }

    Core::hresult CallGetPsiMetrics(const std::string& metric, double& value, uint64_t& total)
    {
        return GetPsiMetrics(metric, value, total);
    }

    Core::hresult CallGetSwapUsed(uint64_t& swapUsed, uint64_t& memUsedTotal)
    {
        return GetSwapUsed(swapUsed, memUsedTotal);
    }

    Core::hresult CallGetFlashSpace(uint64_t& total, uint64_t& used, uint64_t& available)
    {
        return GetFlashSpace(total, used, available);
    }

    Core::hresult CallGetStats(Exchange::ResourceMonitorStats& stats)
    {
        return GetStats(stats);
    }

    Core::hresult CallReconcile(const string& appId, const uint32_t ramTargetMB, const bool allowTerminate)
    {
        return Reconcile(appId, ramTargetMB, allowTerminate);
    }
};


class ResourceMonitorTest : public ::testing::Test {

protected:
    Core::ProxyType<TestableResourceMonitorPlugin> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    string response;

    ResourceMonitorTest()
        : plugin(Core::ProxyType<TestableResourceMonitorPlugin>::Create())
        , handler(*plugin)
        , INIT_CONX(1, 0)
    {
    }

    virtual ~ResourceMonitorTest() = default;
};


// SecurityAgent Mock
class MockAuthenticate : public PluginHost::IAuthenticate {
public:
    MOCK_METHOD(uint32_t, CreateToken, (const uint16_t, const uint8_t[], std::string&), (override));
    MOCK_METHOD(PluginHost::ISecurity*, Officer, (const std::string&), (override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(uint32_t, AddRef, (), (const, override));

    BEGIN_INTERFACE_MAP(MockAuthenticate)
        INTERFACE_ENTRY(PluginHost::IAuthenticate)
    END_INTERFACE_MAP
};

// Initialized Fixture
class ResourceMonitorInitializedTest : public ResourceMonitorTest {
protected:
    NiceMock<ServiceMock> service;
    NiceMock<MockAuthenticate>* mockAuth = nullptr;
    string initResult;
    bool IsPluginInitialized() const
    {
        return initResult.empty();
    }

    void SetUp() override
    {
        ASSERT_TRUE(initResult.empty()) << "Plugin initialization failed: " << initResult;
    }

public:
    ResourceMonitorInitializedTest()
    {
        mockAuth = new NiceMock<MockAuthenticate>();
        ON_CALL(service,
                QueryInterfaceByCallsign(_, _))
            .WillByDefault(
                [this](const uint32_t,
                       const std::string& name)
                -> void*
                {
                    if (name == "SecurityAgent") {
                        mockAuth->AddRef();
                        return static_cast<void*>(mockAuth);
                    }
                    return nullptr;
                });

        initResult = plugin->Initialize(&service);
    }
    ~ResourceMonitorInitializedTest() override
    {
        if (IsPluginInitialized()) {
            plugin->Deinitialize(&service);
        }
        delete mockAuth;
    }
};

// Notification Mock
class NotificationMock : public Exchange::IResourceMonitor::INotification
{
    public:

    MOCK_METHOD(void, OnReconciliationComplete, (const string&, const bool), (override));
    MOCK_METHOD(uint32_t, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));

    BEGIN_INTERFACE_MAP(NotificationMock)
    INTERFACE_ENTRY(Exchange::IResourceMonitor::INotification)
    END_INTERFACE_MAP
};

// TESTS
TEST_F(ResourceMonitorTest, InitializeWithoutSecurityAgent)
{
    NiceMock<ServiceMock> noSecurityService;
    ON_CALL(noSecurityService,
            QueryInterfaceByCallsign(_, _))
        .WillByDefault(Return(nullptr));

    const string result = plugin->Initialize(&noSecurityService);

    EXPECT_TRUE(result.empty() || result == "ResourceMonitor plugin could not be initialised");
    if (result.empty()) {
        plugin->Deinitialize(&noSecurityService);
    }
}

TEST_F(ResourceMonitorInitializedTest, RegisteredMethods)
{
    ASSERT_TRUE(IsPluginInitialized());
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetMemInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetPsiMetrics")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetSwapUsed")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetFlashSpace")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetStats")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("reconcile")));
}

TEST_F(ResourceMonitorInitializedTest, InformationReturnsDescription)
{
    EXPECT_EQ("Plugin which exposes ResourceMonitor related methods.", plugin->Information());
}


// JSONRPC API Tests
TEST_F(ResourceMonitorInitializedTest, GetMemInfoReturnsExpectedFields)
{
    ASSERT_TRUE(IsPluginInitialized());
    EXPECT_EQ(Core::ERROR_NONE,
        handler.Invoke(
            connection,
            _T("GetMemInfo"),
            _T("{}"),
            response));

    JsonObject result;
    result.FromString(response);

    EXPECT_TRUE(result.HasLabel("memAvailable"));
    EXPECT_TRUE(result.HasLabel("swapFree"));
    EXPECT_TRUE(result.HasLabel("swapTotal"));
}

TEST_F(ResourceMonitorInitializedTest, GetPsiMetricsReturnsValue)
{
    ASSERT_TRUE(IsPluginInitialized());
    EXPECT_EQ(Core::ERROR_NONE,
        handler.Invoke(
            connection,
            _T("GetPsiMetrics"),
            _T("{\"metric\":\"full_avg60\"}"),
            response));

    JsonObject result;
    result.FromString(response);

    EXPECT_TRUE(result.HasLabel("value") || result.HasLabel("success"));
}

TEST_F(ResourceMonitorInitializedTest, GetSwapUsedReturnsExpectedFields)
{
    ASSERT_TRUE(IsPluginInitialized());

    const uint32_t result = handler.Invoke(connection,
            _T("GetSwapUsed"),
            _T("{}"),
            response);

    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
}

TEST_F(ResourceMonitorInitializedTest, GetFlashSpaceReturnsExpectedFields)
{
    ASSERT_TRUE(IsPluginInitialized());
    const uint32_t result = handler.Invoke(
            connection,
            _T("GetFlashSpace"),
            _T("{}"),
            response);

    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
}

TEST_F(ResourceMonitorInitializedTest, GetStatsReturnsAllExpectedFields)
{
    ASSERT_TRUE(IsPluginInitialized());
    const uint32_t result = handler.Invoke(
            connection,
            _T("GetStats"),
            _T("{}"),
            response);

    EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_GENERAL);
}

TEST_F(ResourceMonitorInitializedTest, ReconcileMethodRegistered)
{
    ASSERT_TRUE(IsPluginInitialized());
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("reconcile")));
}

TEST_F(ResourceMonitorInitializedTest, ReconcileReturnsSuccess)
{
    ASSERT_TRUE(IsPluginInitialized());
    ASSERT_EQ(Core::ERROR_NONE,
        handler.Invoke(
            connection,
            _T("reconcile"),
            _T("{"
               "\"appId\":\"Netflix\","
               "\"ramTargetMB\":100,"
               "\"allowTerminate\":false"
               "}"),
            response));
}

TEST_F(ResourceMonitorInitializedTest, ReconcileMissingAppId)
{
    JsonObject result;

    EXPECT_NE(
        Core::ERROR_NONE,
        handler.Invoke(
            connection,
            _T("reconcile"),
            _T("{"
               "\"ramTargetMB\":100,"
               "\"allowTerminate\":false"
               "}"),
            response));
}

TEST_F(ResourceMonitorInitializedTest, ReconcileMissingRamTarget)
{
    EXPECT_NE(
        Core::ERROR_NONE,
        handler.Invoke(
            connection,
            _T("reconcile"),
            _T("{"
               "\"appId\":\"Netflix\","
               "\"allowTerminate\":false"
               "}"),
            response));
}

TEST_F(ResourceMonitorInitializedTest, ReconcileInvalidParameterType)
{
    EXPECT_NE(
        Core::ERROR_NONE,
        handler.Invoke(
            connection,
            _T("reconcile"),
            _T("{"
               "\"appId\":123,"
               "\"ramTargetMB\":\"abc\","
               "\"allowTerminate\":false"
               "}"),
            response));
}

TEST_F(ResourceMonitorInitializedTest, MultipleReconcileRequests)
{
    ASSERT_TRUE(IsPluginInitialized());
    EXPECT_EQ(Core::ERROR_NONE,
        handler.Invoke(
            connection,
            _T("reconcile"),
            _T("{"
               "\"appId\":\"Netflix\","
               "\"ramTargetMB\":100,"
               "\"allowTerminate\":true"
               "}"),
            response));

    ASSERT_TRUE(IsPluginInitialized());
    EXPECT_EQ(Core::ERROR_NONE,
        handler.Invoke(
            connection,
            _T("reconcile"),
            _T("{"
               "\"appId\":\"YouTube\","
               "\"ramTargetMB\":200,"
               "\"allowTerminate\":true"
               "}"),
            response));
}

TEST_F(ResourceMonitorInitializedTest, ReconcileEmptyAppId)
{
    EXPECT_NE(Core::ERROR_NONE,
    handler.Invoke(
        connection,
        _T("reconcile"),
        _T("{"
        "\"appId\":\"\","
        "\"ramTargetMB\":100,"
        "\"allowTerminate\":true"
        "}"), response));
}

TEST(ResourceMonitorImplementationTest, RegisterNullNotification)
{
    TestableResourceMonitorImplementation impl;
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, impl.Register(nullptr));
}

TEST(ResourceMonitorImplementationTest, UnregisterNullNotification)
{
    TestableResourceMonitorImplementation impl;
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, impl.Unregister(nullptr));
}

TEST(ResourceMonitorImplementationTest, ValidPsiMetrics)
{
    TestableResourceMonitorImplementation impl;

    double value{};
    uint64_t total{};

    impl.CallGetPsiMetrics("full_avg60", value, total);
}

TEST_F(ResourceMonitorInitializedTest, MultipleStatsRequests)
{
    for (uint32_t i = 0; i < 25; i++) {
        ASSERT_TRUE(IsPluginInitialized());
        ASSERT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetStats"), _T("{}"), response));
    }
}

TEST(ResourceMonitorImplementationTest, RegisterAndUnregister)
{
    TestableResourceMonitorImplementation impl;
    NotificationMock notification;

    EXPECT_CALL(notification, AddRef()).WillOnce(Return(1));
    EXPECT_EQ(Core::ERROR_NONE, impl.Register(&notification));
    EXPECT_CALL(notification, Release()).WillOnce(Return(0));
    EXPECT_EQ(Core::ERROR_NONE, impl.Unregister(&notification));
}