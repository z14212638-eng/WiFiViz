#include "ns3/wifiviz.h"

#include "ns3/core-module.h"
#include "ns3/loopback-net-device.h"
#include "ns3/network-module.h"
#include "ns3/test.h"

#include <cstdlib>
#include <string>

using namespace ns3;

namespace
{

class WiFiVizMergeDevicesTestCase : public TestCase
{
  public:
    WiFiVizMergeDevicesTestCase()
        : TestCase("WiFiVizHelper merges device containers without duplicates")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(2);

        Ptr<LoopbackNetDevice> first = CreateObject<LoopbackNetDevice>();
        Ptr<LoopbackNetDevice> second = CreateObject<LoopbackNetDevice>();
        nodes.Get(0)->AddDevice(first);
        nodes.Get(1)->AddDevice(second);

        NetDeviceContainer left;
        left.Add(first);
        left.Add(second);

        NetDeviceContainer right;
        right.Add(second);

        NetDeviceContainer merged = WiFiVizHelper::MergeDevices(left, right);
        NS_TEST_ASSERT_MSG_EQ(merged.GetN(), 2, "duplicate devices must be removed");
        NS_TEST_ASSERT_MSG_EQ(merged.Get(0), first, "first device order should be preserved");
        NS_TEST_ASSERT_MSG_EQ(merged.Get(1), second, "second device order should be preserved");
    }
};

class WiFiVizSamplingEnvironmentTestCase : public TestCase
{
  public:
    WiFiVizSamplingEnvironmentTestCase()
        : TestCase("WiFiVizHelper configures sampling environment")
    {
    }

  private:
    void DoRun() override
    {
        unsetenv("WIFIVIZ_PRECISE");
        unsetenv("WIFIVIZ_SAMPLE_RATE");

        WiFiVizHelper::ConfigureVisualizerSampling(true, 10);
        NS_TEST_ASSERT_MSG_EQ(std::string(std::getenv("WIFIVIZ_PRECISE")),
                              "1",
                              "precise mode should be marked in the environment");
        NS_TEST_ASSERT_MSG_EQ(std::string(std::getenv("WIFIVIZ_SAMPLE_RATE")),
                              "1",
                              "precise mode should force sample rate 1");

        WiFiVizHelper::ConfigureVisualizerSampling(false, 4);
        NS_TEST_ASSERT_MSG_EQ(std::getenv("WIFIVIZ_PRECISE"),
                              nullptr,
                              "sampled mode should clear the precise marker");
        NS_TEST_ASSERT_MSG_EQ(std::string(std::getenv("WIFIVIZ_SAMPLE_RATE")),
                              "4",
                              "sampled mode should export the rough value");

        WiFiVizHelper::ConfigureVisualizerSampling(false, 1);
        NS_TEST_ASSERT_MSG_EQ(std::getenv("WIFIVIZ_PRECISE"),
                              nullptr,
                              "non-precise rough=1 should not export precise marker");
        NS_TEST_ASSERT_MSG_EQ(std::getenv("WIFIVIZ_SAMPLE_RATE"),
                              nullptr,
                              "non-precise rough=1 should clear sample rate");
    }
};

class WiFiVizDisabledPathTestCase : public TestCase
{
  public:
    WiFiVizDisabledPathTestCase()
        : TestCase("WiFiVizHelper disabled path returns null")
    {
    }

  private:
    void DoRun() override
    {
        NetDeviceContainer emptyDevices;
        Ptr<SniffUtils> viz =
            WiFiVizHelper::MaybeEnableVisualizer(false, emptyDevices, 1.0, false);
        NS_TEST_ASSERT_MSG_EQ(viz, nullptr, "disabled visualizer should not create SniffUtils");
    }
};

class WiFiVizTestSuite : public TestSuite
{
  public:
    WiFiVizTestSuite()
        : TestSuite("wifiviz", Type::UNIT)
    {
        AddTestCase(new WiFiVizMergeDevicesTestCase, TestCase::Duration::QUICK);
        AddTestCase(new WiFiVizSamplingEnvironmentTestCase, TestCase::Duration::QUICK);
        AddTestCase(new WiFiVizDisabledPathTestCase, TestCase::Duration::QUICK);
    }
};

static WiFiVizTestSuite g_wifiVizTestSuite;

} // namespace
