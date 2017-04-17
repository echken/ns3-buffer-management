#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/link-monitor-module.h"
#include "ns3/gnuplot.h"

// The CDF in TrafficGenerator
extern "C"
{
#include "cdf.h"
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VariedRTT");

Gnuplot2dDataset queuediscDataset;

enum AQM {
    RED,
    CODEL,
    XXX
};

// Port from Traffic Generator // Acknowledged to https://github.com/HKUST-SING/TrafficGenerator/blob/master/src/common/common.c
double
poission_gen_interval(double avg_rate) {
    if (avg_rate > 0)
        return -logf(1.0 - (double)rand() / RAND_MAX) / avg_rate;
    else
        return 0;
}

template<typename T> T
rand_range (T min, T max)
{
    return min + ((double)max - min) * rand () / RAND_MAX;
}

std::string
GetFormatedStr (std::string id, std::string str, std::string terminal, AQM aqm, double load, uint32_t interval, uint32_t target, uint32_t redThreshold, uint32_t numOfSenders)
{
    std::stringstream ss;
    if (aqm == RED)
    {
        ss << id << "_vr_s_red_" << str << "_t" << redThreshold << "_n" << numOfSenders << "_l" << load << "." << terminal;
    }
    else if (aqm == CODEL)
    {
        ss << id << "_vr_s_codel_" << str << "_i" << interval << "_t" << target << "_n" << numOfSenders << "_l" << load  << "." << terminal;
    }
    else if (aqm == XXX)
    {
        ss << id << "_vr_s_xxx_" << str << "_int" << redThreshold << "_i" << interval << "_t" << target << "_n" <<numOfSenders << "_l" << load << "." << terminal;
    }
    return ss.str ();
}

void
DoGnuPlot (std::string id, AQM aqm, double load, uint32_t interval, uint32_t target, uint32_t redThreshold, uint32_t numOfSenders)
{
    Gnuplot queuediscGnuplot (GetFormatedStr (id, "queue_disc", "png", aqm, load, interval, target, redThreshold, numOfSenders).c_str ());
    queuediscGnuplot.SetTitle ("queue_disc");
    queuediscGnuplot.SetTerminal ("png");
    queuediscGnuplot.AddDataset (queuediscDataset);
    std::ofstream queuediscGnuplotFile (GetFormatedStr (id, "queue_disc", "plt", aqm, load, interval, target, redThreshold, numOfSenders).c_str ());
    queuediscGnuplot.GenerateOutput (queuediscGnuplotFile);
    queuediscGnuplotFile.close ();
}

void
CheckQueueDiscSize (Ptr<QueueDisc> queue)
{
    uint32_t qSize = queue->GetNPackets ();
    queuediscDataset.Add (Simulator::Now ().GetSeconds (), qSize);
    Simulator::Schedule (Seconds (0.00001), &CheckQueueDiscSize, queue);
}

std::string
DefaultFormat (struct LinkProbe::LinkStats stat)
{
    std::ostringstream oss;
    oss << stat.txLinkUtility << ",";
    return oss.str ();
}

int main (int argc, char *argv[])
{
#if 1
    LogComponentEnable ("VariedRTT", LOG_LEVEL_INFO);
#endif

    std::string id = "";

    std::string transportProt = "DcTcp";
    std::string aqmStr = "CODEL";
    AQM aqm;
    double endTime = 10.0;
    double simEndTime = 15.0;

    uint32_t numOfSenders = 8;

    uint32_t CODELInterval = 150;
    uint32_t CODELTarget = 10;

    double load = 0.1;
    std::string cdfFileName = "";
    std::string rttCdfFileName = "";

    unsigned randomSeed = 0;
    uint32_t flowNum = 1000;

    uint32_t redMarkingThreshold = 30;
    uint32_t bufferSize = 120;

    uint32_t xxxInterval = 150;
    uint32_t xxxTarget = 10;
    uint32_t xxxMarkingThreshold = 30;

    CommandLine cmd;
    cmd.AddValue ("id", "The running ID", id);
    cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, DcTcp", transportProt);
    cmd.AddValue ("AQM", "AQM to use: RED, CODEL", aqmStr);
    cmd.AddValue ("CODELInterval", "The interval parameter in CODEL", CODELInterval);
    cmd.AddValue ("CODELTarget", "The target parameter in CODEL", CODELTarget);
    cmd.AddValue ("numOfSenders", "Concurrent senders", numOfSenders);
    cmd.AddValue ("endTime", "Flow launch end time", endTime);
    cmd.AddValue ("simEndTime", "Simulation end time", simEndTime);
    cmd.AddValue ("cdfFileName", "File name for flow distribution", cdfFileName);
    cmd.AddValue ("rttCdfFileName", "File name for RTT distribution", rttCdfFileName);
    cmd.AddValue ("load", "Load of the network, 0.0 - 1.0", load);
    cmd.AddValue ("randomSeed", "Random seed, 0 for random generated", randomSeed);
    cmd.AddValue ("flowNum", "Total flow num", flowNum);
    cmd.AddValue ("redMarkingThreshold", "The RED marking threshold", redMarkingThreshold);
    cmd.AddValue ("bufferSize", "The buffer size", bufferSize);

    cmd.AddValue ("XXXInterval", "The persistent interval for XXX", xxxInterval);
    cmd.AddValue ("XXXTarget", "The persistent target for XXX", xxxTarget);
    cmd.AddValue ("XXXMarkingThreshold", "The instantaneous marking threshold for XXX", xxxMarkingThreshold);

    cmd.Parse (argc, argv);

    if (transportProt.compare ("Tcp") == 0)
    {
        Config::SetDefault ("ns3::TcpSocketBase::Target", BooleanValue (false));
    }
    else if (transportProt.compare ("DcTcp") == 0)
    {
        NS_LOG_INFO ("Enabling DcTcp");
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpDCTCP::GetTypeId ()));
    }
    else
    {
        return 0;
    }

    if (aqmStr.compare ("RED") == 0)
    {
        aqm = RED;
    }
    else if (aqmStr.compare ("CODEL") == 0)
    {
        aqm = CODEL;
    }
    else if (aqmStr.compare ("XXX") == 0)
    {
        aqm = XXX;
        redMarkingThreshold = xxxMarkingThreshold;
        CODELTarget = xxxTarget;
        CODELInterval = xxxInterval;
    }
    else
    {
        return 0;
    }

    // TCP Configuration
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(1400));
    Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (0));
    Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (MilliSeconds (5)));
    Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
    Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (5)));
    Config::SetDefault ("ns3::TcpSocketBase::ClockGranularity", TimeValue (MicroSeconds (100)));
    Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue (MicroSeconds (40)));
    Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (160000000));
    Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (160000000));

    // CoDel Configuration
    Config::SetDefault ("ns3::CoDelQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::CoDelQueueDisc::MaxPackets", UintegerValue (bufferSize));
    Config::SetDefault ("ns3::CoDelQueueDisc::Target", TimeValue (MicroSeconds (CODELTarget)));
    Config::SetDefault ("ns3::CoDelQueueDisc::Interval", TimeValue (MicroSeconds (CODELInterval)));

    // RED Configuration
    Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1400));
    Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (bufferSize));
    Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (false));

    // XXX Configuration
    Config::SetDefault ("ns3::XXXQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::XXXQueueDisc::MaxPackets", UintegerValue (bufferSize));
    Config::SetDefault ("ns3::XXXQueueDisc::InstantaneousMarkingThreshold", UintegerValue (xxxMarkingThreshold));
    Config::SetDefault ("ns3::XXXQueueDisc::PersistentMarkingTarget", TimeValue (MicroSeconds (xxxTarget)));
    Config::SetDefault ("ns3::XXXQueueDisc::PersistentMarkingInterval", TimeValue (MicroSeconds (xxxInterval)));

    NS_LOG_INFO ("Loading RTT CDF");
    struct cdf_table *rttCdfTable = new cdf_table ();
    init_cdf (rttCdfTable);
    load_cdf (rttCdfTable, rttCdfFileName.c_str ());
    NS_LOG_INFO ("AVG RTT: " << avg_cdf (rttCdfTable));

    NS_LOG_INFO ("Setting up nodes.");
    NodeContainer senders;
    senders.Create (numOfSenders);

    NodeContainer receivers;
    receivers.Create (1);

    NodeContainer switches;
    switches.Create (1);

    InternetStackHelper internet;
    internet.Install (senders);
    internet.Install (switches);
    internet.Install (receivers);

    PointToPointHelper p2p;

    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
    p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (5));

    TrafficControlHelper tc;
    if (aqm == CODEL)
    {
        tc.SetRootQueueDisc ("ns3::CoDelQueueDisc");
    }
    else if (aqm == RED)
    {
        tc.SetRootQueueDisc ("ns3::RedQueueDisc", "MinTh", DoubleValue (redMarkingThreshold),
                                                  "MaxTh", DoubleValue (redMarkingThreshold));
    }
    else
    {
        tc.SetRootQueueDisc ("ns3::XXXQueueDisc");
    }

    NS_LOG_INFO ("Assign IP address");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");

    for (uint32_t i = 0; i < numOfSenders; ++i)
    {
        uint32_t linkLatency = gen_random_cdf (rttCdfTable) - 5;
        NS_LOG_INFO ("Generate link latency: " << linkLatency);
        p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(linkLatency)));
        NodeContainer nodeContainer = NodeContainer (senders.Get (i), switches.Get (0));
        NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);
        QueueDiscContainer queuediscDataset = tc.Install (netDeviceContainer);
        Ipv4InterfaceContainer ipv4InterfaceContainer = ipv4.Assign (netDeviceContainer);
        ipv4.NewNetwork ();
    }

    p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(5)));
    NodeContainer switchToRecvNodeContainer = NodeContainer (switches.Get (0), receivers.Get (0));
    NetDeviceContainer switchToRecvNetDeviceContainer = p2p.Install (switchToRecvNodeContainer);
    QueueDiscContainer switchToRecvQueueDiscContainer = tc.Install (switchToRecvNetDeviceContainer);
    Ipv4InterfaceContainer switchToRecvIpv4Container = ipv4.Assign (switchToRecvNetDeviceContainer);

    free_cdf (rttCdfTable);

    NS_LOG_INFO ("Setting up routing table");

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    NS_LOG_INFO ("Initialize CDF table");
    struct cdf_table* cdfTable = new cdf_table ();
    init_cdf (cdfTable);
    load_cdf (cdfTable, cdfFileName.c_str ());

    NS_LOG_INFO ("Calculating request rate");
    double requestRate = load * 10e9 / (8 * avg_cdf (cdfTable)) / numOfSenders;
    NS_LOG_INFO ("Average request rate: " << requestRate << " per second per sender");

    NS_LOG_INFO ("Initialize random seed: " << randomSeed);
    if (randomSeed == 0)
    {
        srand ((unsigned)time (NULL));
    }
    else
    {
        srand (randomSeed);
    }

    NS_LOG_INFO ("Install background application");

    uint16_t basePort = 8080;

    for (uint32_t i = 0; i < numOfSenders; ++i)
    {
        uint32_t totalFlow = 0;
        double startTime = 0.0 + poission_gen_interval (requestRate);
        while (startTime < endTime && totalFlow < (flowNum / numOfSenders))
        {
            uint32_t flowSize = gen_random_cdf (cdfTable);
            BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (switchToRecvIpv4Container.GetAddress (1), basePort));
            source.SetAttribute ("MaxBytes", UintegerValue (flowSize));
            source.SetAttribute ("SendSize", UintegerValue (1400));
            ApplicationContainer sourceApps = source.Install (senders.Get (i));
            sourceApps.Start (Seconds (startTime));
            sourceApps.Stop (Seconds (simEndTime));

            PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), basePort));
            ApplicationContainer sinkApp = sink.Install (switchToRecvNodeContainer.Get (1));
            sinkApp.Start (Seconds (0.0));
            sinkApp.Stop (Seconds (simEndTime));

            ++totalFlow;
            ++basePort;
            startTime += poission_gen_interval (requestRate);
        }
    }

    NS_LOG_INFO ("Install incast application");

    double incast_period = endTime / 10;

    for (uint32_t i = 0; i < numOfSenders; ++i)
    {
        double startTime = 0.0 + incast_period;
        while (startTime < endTime)
        {
            BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (switchToRecvIpv4Container.GetAddress (1), basePort));
            source.SetAttribute ("MaxBytes", UintegerValue (10000));
            source.SetAttribute ("SendSize", UintegerValue (1400));
            ApplicationContainer sourceApps = source.Install (senders.Get (i));
            sourceApps.Start (Seconds (startTime));
            sourceApps.Stop (Seconds (simEndTime));

            PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), basePort));
            ApplicationContainer sinkApp = sink.Install (switchToRecvNodeContainer.Get (1));
            sinkApp.Start (Seconds (0.0));
            sinkApp.Stop (Seconds (simEndTime));

            ++basePort;
            startTime += incast_period;
        }
    }


    NS_LOG_INFO ("Start Tracing System");

    queuediscDataset.SetTitle ("queue_disc");
    queuediscDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

    Simulator::ScheduleNow (&CheckQueueDiscSize, switchToRecvQueueDiscContainer.Get (0));

    NS_LOG_INFO ("Enabling Flow Monitor");
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    NS_LOG_INFO ("Enabling Link Monitor");
    Ptr<LinkMonitor> linkMonitor = Create<LinkMonitor> ();
    Ptr<Ipv4LinkProbe> linkProbe = Create<Ipv4LinkProbe> (switches.Get (0), linkMonitor);
    linkProbe->SetProbeName ("Bottle-Neck");
    linkProbe->SetCheckTime (Seconds (0.001));
    linkProbe->SetDataRateAll (DataRate (1e10));

    linkMonitor->Start (Seconds (0.0));
    linkMonitor->Stop (Seconds (simEndTime));

    NS_LOG_INFO ("Run Simulations");

    Simulator::Stop (Seconds (simEndTime));
    Simulator::Run ();

    flowMonitor->SerializeToXmlFile(GetFormatedStr (id, "flow_monitor", "xml", aqm, load, CODELInterval, CODELTarget, redMarkingThreshold, numOfSenders), true, true);
    linkMonitor->OutputToFile (GetFormatedStr (id, "link_monitor", "xml", aqm, load, CODELInterval, CODELTarget, redMarkingThreshold, numOfSenders), &DefaultFormat);

    Simulator::Destroy ();
    free_cdf (cdfTable);

    DoGnuPlot (id, aqm, load, CODELInterval, CODELTarget, redMarkingThreshold, numOfSenders);

    return 0;
}
