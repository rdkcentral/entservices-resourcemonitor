/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "L2Tests.h"
#include "L2TestsMock.h"

#include <interfaces/IResourceMonitor.h>

using namespace WPEFramework;
using namespace testing;

#define RESOURCEMONITOR_CALLSIGN "org.rdk.ResourceMonitor"

class ResourceMonitorL2Test : public L2TestMocks
{
protected:

    ResourceMonitorL2Test()
    {
        EXPECT_EQ(Core::ERROR_NONE,
                  ActivateService(RESOURCEMONITOR_CALLSIGN));
    }

    ~ResourceMonitorL2Test() override
    {
        EXPECT_EQ(Core::ERROR_NONE,
                  DeactivateService(RESOURCEMONITOR_CALLSIGN));
    }
};


TEST_F(ResourceMonitorL2Test, GetMemInfo)
{
    JsonObject params;
    JsonObject result;

    EXPECT_EQ(Core::ERROR_NONE,
        InvokeServiceMethod(
            RESOURCEMONITOR_CALLSIGN,
            "GetMemInfo",
            params,
            result));

    EXPECT_TRUE(result.HasLabel("memAvailable"));
    EXPECT_TRUE(result.HasLabel("swapFree"));
    EXPECT_TRUE(result.HasLabel("swapTotal"));
}

TEST_F(ResourceMonitorL2Test, GetPsiMetrics)
{
    JsonObject params;
    JsonObject result;

    params["metric"] = "full_avg10";

    EXPECT_EQ(Core::ERROR_NONE,
        InvokeServiceMethod(
            RESOURCEMONITOR_CALLSIGN,
            "GetPsiMetrics",
            params,
            result));

    EXPECT_TRUE(result.HasLabel("value"));
}

TEST_F(ResourceMonitorL2Test, GetSwapUsed)
{
    JsonObject params;
    JsonObject result;

    EXPECT_EQ(Core::ERROR_NONE,
        InvokeServiceMethod(
            RESOURCEMONITOR_CALLSIGN,
            "GetSwapUsed",
            params,
            result));

    EXPECT_TRUE(result.HasLabel("swapUsed"));
    EXPECT_TRUE(result.HasLabel("memUsedTotal"));
}

TEST_F(ResourceMonitorL2Test, GetFlashSpace)
{
    JsonObject params;
    JsonObject result;

    EXPECT_EQ(Core::ERROR_NONE,
        InvokeServiceMethod(
            RESOURCEMONITOR_CALLSIGN,
            "GetFlashSpace",
            params,
            result));

    EXPECT_TRUE(result.HasLabel("total"));
    EXPECT_TRUE(result.HasLabel("used"));
    EXPECT_TRUE(result.HasLabel("available"));
}

TEST_F(ResourceMonitorL2Test, GetStats)
{
    JsonObject params;
    JsonObject result;

    EXPECT_EQ(Core::ERROR_NONE,
        InvokeServiceMethod(
            RESOURCEMONITOR_CALLSIGN,
            "GetStats",
            params,
            result));

    EXPECT_TRUE(result.HasLabel("systemMemTotal"));
    EXPECT_TRUE(result.HasLabel("systemMemUsed"));
    EXPECT_TRUE(result.HasLabel("systemMemAvailable"));

    EXPECT_TRUE(result.HasLabel("swapTotal"));
    EXPECT_TRUE(result.HasLabel("swapFree"));
    EXPECT_TRUE(result.HasLabel("swapUsed"));

    EXPECT_TRUE(result.HasLabel("flashSpaceTotal"));
    EXPECT_TRUE(result.HasLabel("flashSpaceUsed"));
    EXPECT_TRUE(result.HasLabel("flashSpaceAvailable"));
}


TEST_F(ResourceMonitorL2Test, MultipleStatsCalls)
{
    JsonObject params;

    for (int i = 0; i < 20; i++)
    {
        JsonObject result;
        EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(RESOURCEMONITOR_CALLSIGN, "GetStats", params, result));
    }
}

TEST_F(ResourceMonitorL2Test, Reconcile)
{
    JsonObject params;
    JsonObject result;

    params["appId"] = "Netflix";
    params["ramTargetMB"] = 100;
    params["allowTerminate"] = false;

    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(RESOURCEMONITOR_CALLSIGN, "reconcile", params, result));
}

TEST_F(ResourceMonitorL2Test, ReconcileWithoutAppId)
{
    JsonObject params;
    JsonObject result;

    params["ramTargetMB"] = 100;
    params["allowTerminate"] = false;

    EXPECT_NE(Core::ERROR_NONE, InvokeServiceMethod(RESOURCEMONITOR_CALLSIGN, "reconcile", params, result));
}

TEST_F(ResourceMonitorL2Test, ReconcileWithoutRamTarget)
{
    JsonObject params;
    JsonObject result;

    params["appId"] = "Netflix";
    params["allowTerminate"] = false;

    EXPECT_NE(Core::ERROR_NONE, InvokeServiceMethod(RESOURCEMONITOR_CALLSIGN, "reconcile", params, result));
}

TEST_F(ResourceMonitorL2Test, QueueMultipleReconcileRequests)
{
    JsonObject p1, p2;
    JsonObject result;

    p1["appId"] = "Netflix";
    p1["ramTargetMB"] = 100;
    p1["allowTerminate"] = true;

    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(RESOURCEMONITOR_CALLSIGN, "reconcile", p1, result));

    p2["appId"] = "YouTube";
    p2["ramTargetMB"] = 200;
    p2["allowTerminate"] = true;

    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(RESOURCEMONITOR_CALLSIGN, "reconcile", p2, result));
}

TEST_F(ResourceMonitorL2Test, GetPsiMetricsAllSupported)
{
    const char* metrics[] = {
        "some_avg10",
        "some_avg60",
        "some_avg300",
        "full_avg10",
        "full_avg60",
        "full_avg300"
    };

    for (const auto& metric : metrics) {
        JsonObject params;
        JsonObject result;

        params["metric"] = metric;

        EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(RESOURCEMONITOR_CALLSIGN, "GetPsiMetrics", params, result));
    }
}

TEST_F(ResourceMonitorL2Test, MultipleGetMemInfoCalls)
{
    JsonObject params;

    for (int i = 0; i < 50; i++) {
        JsonObject result;
        EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(RESOURCEMONITOR_CALLSIGN, "GetMemInfo", params, result));
    }
}

TEST_F(ResourceMonitorL2Test, GetStatsValuesSanity)
{
    JsonObject params;
    JsonObject result;

    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(RESOURCEMONITOR_CALLSIGN, "GetStats", params, result));

    EXPECT_GE(
        result["systemMemTotal"].Number(),
        result["systemMemAvailable"].Number());
}