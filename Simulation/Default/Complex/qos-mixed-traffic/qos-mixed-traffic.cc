#include "ns3/wifiviz.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

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

    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNodes;
    staNodes.Create(3);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("HeMcs7"),
                                 "ControlMode",
                                 StringValue("HeMcs0"));

    WifiMacHelper mac;
    Ssid ssid("wifiviz-qos-mixed");

    mac.SetType("ns3::StaWifiMac",
                "Ssid",
                SsidValue(ssid),
                "QosSupported",
                BooleanValue(true),
                "ActiveProbing",
                BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid",
                SsidValue(ssid),
                "QosSupported",
                BooleanValue(true));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0.0, 0.0, 0.0));
    positions->Add(Vector(-4.0, -3.0, 0.0));
    positions->Add(Vector(5.0, 1.0, 0.0));
    positions->Add(Vector(2.0, 5.0, 0.0));
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.2.1.0", "255.255.255.0");
    NetDeviceContainer ipDevices;
    ipDevices.Add(apDevices);
    ipDevices.Add(staDevices);
    Ipv4InterfaceContainer interfaces = address.Assign(ipDevices);

    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        const uint16_t port = 9200 + i;
        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(staNodes.Get(i));
        serverApps.Start(Seconds(0.5));
        serverApps.Stop(Seconds(simulationTime));

        UdpClientHelper client(interfaces.GetAddress(1 + i), port);
        client.SetAttribute("MaxPackets", UintegerValue(100000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(12 + 8 * i)));
        client.SetAttribute("PacketSize", UintegerValue(600 + 350 * i));
        ApplicationContainer clientApps = client.Install(apNode.Get(0));
        clientApps.Start(Seconds(1.0 + 0.25 * i));
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
