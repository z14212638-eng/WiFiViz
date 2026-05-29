#include "ns3/wifiviz.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

using namespace ns3;

namespace
{

using Clock = std::chrono::steady_clock;

double
ElapsedMs(const Clock::time_point& start, const Clock::time_point& end)
{
    return std::chrono::duration<double, std::milli>(end - start).count();
}

void
PrintCsvHeader()
{
    std::cout << "WIFIVIZ_BENCHMARK_HEADER,"
              << "run_id,enable_wifiviz,n_sta,simulation_time_s,packet_size_bytes,"
              << "interval_us,precise,rough,rng_run,received_packets,viz_init_ms,"
              << "setup_ms,simulator_run_ms,destroy_ms,total_ms" << std::endl;
}

} // namespace

int
main(int argc, char* argv[])
{
    bool enableViz = false;
    bool precise = true;
    bool printHeader = false;
    uint32_t rough = 1;
    uint32_t nSta = 8;
    uint32_t packetSize = 1200;
    uint32_t intervalUs = 1000;
    uint32_t rngRun = 1;
    uint32_t runId = 0;
    double simulationTime = 20.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enable-wifiviz", "Enable WiFiViz data collection", enableViz);
    cmd.AddValue("precise", "Record every PPDU when true", precise);
    cmd.AddValue("rough", "Sample one PPDU out of rough records when precise=false", rough);
    cmd.AddValue("nSta", "Number of Wi-Fi station nodes", nSta);
    cmd.AddValue("n-sta", "Number of Wi-Fi station nodes", nSta);
    cmd.AddValue("packet-size", "UDP payload size in bytes", packetSize);
    cmd.AddValue("interval-us", "Per-flow UDP packet interval in microseconds", intervalUs);
    cmd.AddValue("simulation-time", "Simulation duration in seconds", simulationTime);
    cmd.AddValue("rng-run", "ns-3 RNG run number", rngRun);
    cmd.AddValue("run-id", "External benchmark repetition id", runId);
    cmd.AddValue("print-header", "Print the CSV header line", printHeader);
    cmd.Parse(argc, argv);

    if (nSta == 0)
    {
        NS_FATAL_ERROR("nSta must be greater than zero");
    }
    if (simulationTime <= 1.1)
    {
        NS_FATAL_ERROR("simulation-time must be greater than 1.1 seconds");
    }

    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(rngRun);

    if (printHeader)
    {
        PrintCsvHeader();
    }

    const auto totalStart = Clock::now();

    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNodes;
    staNodes.Create(nSta);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("ChannelSettings", StringValue("{6, 20, BAND_2_4GHZ, 0}"));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("HtMcs7"),
                                 "ControlMode",
                                 StringValue("HtMcs0"));

    WifiMacHelper mac;
    Ssid ssid("wifiviz-overhead");

    mac.SetType("ns3::StaWifiMac",
                "Ssid",
                SsidValue(ssid),
                "ActiveProbing",
                BooleanValue(false),
                "QosSupported",
                BooleanValue(true));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid",
                SsidValue(ssid),
                "QosSupported",
                BooleanValue(true));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);
    apNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    for (uint32_t i = 0; i < nSta; ++i)
    {
        const double x = 3.0 + static_cast<double>(i % 4) * 2.0;
        const double y = static_cast<double>(i / 4) * 2.0;
        staNodes.Get(i)->GetObject<MobilityModel>()->SetPosition(Vector(x, y, 0.0));
    }

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.10.0.0", "255.255.0.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    address.Assign(apDevices);

    ApplicationContainer serverApps;
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nSta; ++i)
    {
        const uint16_t port = static_cast<uint16_t>(9000 + i);

        UdpServerHelper server(port);
        serverApps.Add(server.Install(staNodes.Get(i)));

        UdpClientHelper client(staInterfaces.GetAddress(i), port);
        client.SetAttribute("MaxPackets", UintegerValue(std::numeric_limits<uint32_t>::max()));
        client.SetAttribute("Interval", TimeValue(MicroSeconds(intervalUs)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientApps.Add(client.Install(apNode.Get(0)));
    }

    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(simulationTime));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    NetDeviceContainer tracedDevices = WiFiVizHelper::MergeDevices(apDevices, staDevices);
    WiFiVizHelper::ConfigureVisualizerSampling(precise, rough);

    const auto vizInitStart = Clock::now();
    Ptr<SniffUtils> viz =
        WiFiVizHelper::MaybeEnableVisualizer(enableViz, tracedDevices, simulationTime, false);
    (void)viz;
    const auto vizInitEnd = Clock::now();

    Simulator::Stop(Seconds(simulationTime));

    const auto runStart = Clock::now();
    Simulator::Run();
    const auto runEnd = Clock::now();

    uint64_t receivedPackets = 0;
    for (uint32_t i = 0; i < serverApps.GetN(); ++i)
    {
        Ptr<UdpServer> server = DynamicCast<UdpServer>(serverApps.Get(i));
        if (server)
        {
            receivedPackets += server->GetReceived();
        }
    }

    const auto destroyStart = Clock::now();
    Simulator::Destroy();
    const auto destroyEnd = Clock::now();
    const auto totalEnd = Clock::now();

    const double vizInitMs = ElapsedMs(vizInitStart, vizInitEnd);
    const double runMs = ElapsedMs(runStart, runEnd);
    const double destroyMs = ElapsedMs(destroyStart, destroyEnd);
    const double totalMs = ElapsedMs(totalStart, totalEnd);
    const double setupMs = totalMs - vizInitMs - runMs - destroyMs;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "WIFIVIZ_BENCHMARK_CSV,"
              << runId << ","
              << (enableViz ? 1 : 0) << ","
              << nSta << ","
              << simulationTime << ","
              << packetSize << ","
              << intervalUs << ","
              << (precise ? 1 : 0) << ","
              << rough << ","
              << rngRun << ","
              << receivedPackets << ","
              << vizInitMs << ","
              << setupMs << ","
              << runMs << ","
              << destroyMs << ","
              << totalMs << std::endl;

    return 0;
}
