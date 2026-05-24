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

    CommandLine cmd(__FILE__);
    cmd.AddValue("enable-wifiviz", "Enable WiFiViz PPDU capture", enableWiFiViz);
    cmd.AddValue("launch-viewer", "Launch the WiFiViz timeline viewer", launchViewer);
    cmd.AddValue("precise", "Record every PPDU when true", precise);
    cmd.AddValue("rough", "Sample one PPDU out of rough records when precise=false", rough);
    cmd.AddValue("simulation-time", "Simulation duration in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes;
    staNodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> sharedChannel = channel.Create();

    YansWifiPhyHelper phy;
    phy.SetChannel(sharedChannel);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("HtMcs4"),
                                 "ControlMode",
                                 StringValue("HtMcs0"));

    WifiMacHelper mac;
    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;

    for (uint32_t i = 0; i < 2; ++i)
    {
        Ssid ssid("wifiviz-bss-" + std::to_string(i));

        mac.SetType("ns3::StaWifiMac",
                    "Ssid",
                    SsidValue(ssid),
                    "ActiveProbing",
                    BooleanValue(false));
        staDevices.Add(wifi.Install(phy, mac, staNodes.Get(i)));

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        apDevices.Add(wifi.Install(phy, mac, apNodes.Get(i)));
    }

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(-8.0, 0.0, 0.0));
    positions->Add(Vector(8.0, 0.0, 0.0));
    positions->Add(Vector(-5.0, 1.0, 0.0));
    positions->Add(Vector(5.0, -1.0, 0.0));
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    NodeContainer allNodes;
    allNodes.Add(apNodes);
    allNodes.Add(staNodes);
    mobility.Install(allNodes);

    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.3.1.0", "255.255.255.0");
    NetDeviceContainer ipDevices;
    ipDevices.Add(apDevices);
    ipDevices.Add(staDevices);
    Ipv4InterfaceContainer interfaces = address.Assign(ipDevices);

    for (uint32_t i = 0; i < 2; ++i)
    {
        const uint16_t port = 9300 + i;
        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(staNodes.Get(i));
        serverApps.Start(Seconds(0.5));
        serverApps.Stop(Seconds(simulationTime));

        UdpClientHelper client(interfaces.GetAddress(2 + i), port);
        client.SetAttribute("MaxPackets", UintegerValue(100000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(16)));
        client.SetAttribute("PacketSize", UintegerValue(1200));
        ApplicationContainer clientApps = client.Install(apNodes.Get(i));
        clientApps.Start(Seconds(1.0 + 0.15 * i));
        clientApps.Stop(Seconds(simulationTime));
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
