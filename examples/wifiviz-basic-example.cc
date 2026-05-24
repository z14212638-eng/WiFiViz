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
    bool enableViz = false;
    bool launchViewer = false;
    bool precise = true;
    uint32_t rough = 1;
    double simulationTime = 5.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enable-wifiviz", "Enable WiFiViz trace capture", enableViz);
    cmd.AddValue("launch-viewer", "Launch the WiFiViz timeline viewer", launchViewer);
    cmd.AddValue("precise", "Record every PPDU when true", precise);
    cmd.AddValue("rough", "Sample one PPDU out of rough records when precise=false", rough);
    cmd.AddValue("simulation-time", "Simulation duration in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNode;
    staNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("HtMcs7"),
                                 "ControlMode",
                                 StringValue("HtMcs0"));

    WifiMacHelper mac;
    Ssid ssid("wifiviz-basic");

    mac.SetType("ns3::StaWifiMac",
                "Ssid",
                SsidValue(ssid),
                "ActiveProbing",
                BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNode);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNode);
    apNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    staNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(5.0, 0.0, 0.0));

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    NetDeviceContainer allIpDevices;
    allIpDevices.Add(apDevices);
    allIpDevices.Add(staDevices);
    Ipv4InterfaceContainer interfaces = address.Assign(allIpDevices);

    const uint16_t port = 9000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(staNode.Get(0));
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(100000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    client.SetAttribute("PacketSize", UintegerValue(1200));
    ApplicationContainer clientApps = client.Install(apNode.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    NetDeviceContainer tracedDevices = WiFiVizHelper::MergeDevices(apDevices, staDevices);
    WiFiVizHelper::ConfigureVisualizerSampling(precise, rough);
    Ptr<SniffUtils> viz =
        WiFiVizHelper::MaybeEnableVisualizer(enableViz, tracedDevices, simulationTime, launchViewer);
    (void)viz;

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
