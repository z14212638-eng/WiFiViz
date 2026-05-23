#include "ns3/QNs3.h"
#include "ns3/SniffUtils.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char* argv[])
{
    bool enableVisualizer = false;
    bool precise = true;
    uint32_t rough = 1;
    CommandLine cmd(__FILE__);
    cmd.AddValue("enable-visualizer", "Enable Ns3Visualizer timeline capture", enableVisualizer);
    cmd.AddValue("precise", "Capture every PPDU without down-sampling", precise);
    cmd.AddValue("rough", "Down-sample PPDU timeline to 1/N", rough);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    // ===== Nodes =====
    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNodes;
    staNodes.Create(1);

    // ===== Wi-Fi (IMPORTANT: Yans) =====
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid("ppdu-test");

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "QosSupported", BooleanValue(true));

    NetDeviceContainer apDevices =
        wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "QosSupported", BooleanValue(true),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices =
        wifi.Install(phy, mac, staNodes);

    // ===== Mobility =====
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    // ===== Internet =====
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    address.Assign(apDevices);
    address.Assign(staDevices);

    // ===== Application =====
    UdpEchoServerHelper server(9);
    auto serverApp = server.Install(apNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper client(
        Ipv4Address("10.1.1.1"), 9);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    auto clientApp = client.Install(staNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ===== SniffUtils =====
    NetDeviceContainer allDevices = QNs3Helper::MergeDevices(apDevices, staDevices);
    QNs3Helper::ConfigureVisualizerSampling(precise, rough);
    Ptr<SniffUtils> sniffer =
        QNs3Helper::MaybeEnableVisualizer(enableVisualizer, allDevices, 10, true);

    // ===== Run =====
    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
