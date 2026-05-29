#include "ns3/wifiviz.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

#include <string>

using namespace ns3;

int
main(int argc, char* argv[])
{
    bool enableWiFiViz = false;
    bool launchViewer = false;
    bool precise = true;
    uint32_t rough = 1;
    double simulationTime = 10.0;
    uint32_t packetSize = 1200;
    std::string perFlowRate = "100Mbps";

    CommandLine cmd(__FILE__);
    cmd.AddValue("enable-wifiviz", "Enable WiFiViz PPDU capture", enableWiFiViz);
    cmd.AddValue("launch-viewer", "Launch the WiFiViz timeline viewer", launchViewer);
    cmd.AddValue("precise", "Record every PPDU when true", precise);
    cmd.AddValue("rough", "Sample one PPDU out of rough records when precise=false", rough);
    cmd.AddValue("simulation-time", "Simulation duration in seconds", simulationTime);
    cmd.AddValue("packet-size", "UDP payload size in bytes", packetSize);
    cmd.AddValue("per-flow-rate", "Offered load per downlink flow", perFlowRate);
    cmd.Parse(argc, argv);

    constexpr uint32_t nBss = 3;
    constexpr uint32_t stasPerBss = 2;
    constexpr uint32_t nSta = nBss * stasPerBss;

    NodeContainer apNodes;
    apNodes.Create(nBss);

    NodeContainer staNodes;
    staNodes.Create(nSta);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> sharedChannel = channel.Create();

    YansWifiPhyHelper phy;
    phy.SetChannel(sharedChannel);
    phy.Set("ChannelSettings", StringValue("{36, 20, BAND_5GHZ, 0}"));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("HeMcs4"),
                                 "ControlMode",
                                 StringValue("HeMcs0"));

    WifiMacHelper mac;
    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;

    for (uint32_t bss = 0; bss < nBss; ++bss)
    {
        Ssid ssid("wifiviz-three-bss-" + std::to_string(bss + 1));

        NodeContainer bssStaNodes;
        for (uint32_t sta = 0; sta < stasPerBss; ++sta)
        {
            bssStaNodes.Add(staNodes.Get(bss * stasPerBss + sta));
        }

        mac.SetType("ns3::StaWifiMac",
                    "Ssid",
                    SsidValue(ssid),
                    "QosSupported",
                    BooleanValue(true),
                    "ActiveProbing",
                    BooleanValue(false));
        staDevices.Add(wifi.Install(phy, mac, bssStaNodes));

        mac.SetType("ns3::ApWifiMac",
                    "Ssid",
                    SsidValue(ssid),
                    "QosSupported",
                    BooleanValue(true));
        apDevices.Add(wifi.Install(phy, mac, apNodes.Get(bss)));
    }

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0.0, 0.0, 0.0));     // AP 1
    positions->Add(Vector(14.0, 0.0, 0.0));    // AP 2
    positions->Add(Vector(7.0, 11.0, 0.0));    // AP 3
    positions->Add(Vector(2.0, 2.0, 0.0));     // BSS 1 STA 1
    positions->Add(Vector(4.0, -2.0, 0.0));    // BSS 1 STA 2
    positions->Add(Vector(12.0, 2.0, 0.0));    // BSS 2 STA 1
    positions->Add(Vector(10.0, -2.0, 0.0));   // BSS 2 STA 2
    positions->Add(Vector(6.0, 8.0, 0.0));     // BSS 3 STA 1
    positions->Add(Vector(9.0, 8.0, 0.0));     // BSS 3 STA 2
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    NodeContainer allNodes;
    allNodes.Add(apNodes);
    allNodes.Add(staNodes);
    mobility.Install(allNodes);

    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.33.1.0", "255.255.255.0");
    NetDeviceContainer ipDevices;
    ipDevices.Add(apDevices);
    ipDevices.Add(staDevices);
    Ipv4InterfaceContainer interfaces = address.Assign(ipDevices);

    for (uint32_t bss = 0; bss < nBss; ++bss)
    {
        for (uint32_t sta = 0; sta < stasPerBss; ++sta)
        {
            const uint32_t staIndex = bss * stasPerBss + sta;
            const uint16_t port = 9330 + staIndex;

            PacketSinkHelper sink("ns3::UdpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), port));
            ApplicationContainer sinkApps = sink.Install(staNodes.Get(staIndex));
            sinkApps.Start(Seconds(0.5));
            sinkApps.Stop(Seconds(simulationTime));

            OnOffHelper onoff("ns3::UdpSocketFactory",
                              InetSocketAddress(interfaces.GetAddress(nBss + staIndex), port));
            onoff.SetAttribute("DataRate", DataRateValue(DataRate(perFlowRate)));
            onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
            onoff.SetAttribute("OnTime",
                               StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            onoff.SetAttribute("OffTime",
                               StringValue("ns3::ConstantRandomVariable[Constant=0]"));

            ApplicationContainer sourceApps = onoff.Install(apNodes.Get(bss));
            sourceApps.Start(Seconds(1.0 + 0.05 * staIndex));
            sourceApps.Stop(Seconds(simulationTime));
        }
    }

    NetDeviceContainer tracedDevices = WiFiVizHelper::MergeDevices(apDevices, staDevices);
    WiFiVizHelper::ConfigureVisualizerSampling(precise, rough);
    Ptr<SniffUtils> viz =
        WiFiVizHelper::MaybeEnableVisualizer(enableWiFiViz,
                                             tracedDevices,
                                             simulationTime,
                                             launchViewer);
    (void)viz;

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
