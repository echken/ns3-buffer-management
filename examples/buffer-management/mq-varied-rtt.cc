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

// #define QUEUE_NUM 8

// #define FLOW_SIZE_MIN 3000  // 3k
// #define FLOW_SIZE_MAX 60000 // 60k

// The CDF in TrafficGenerator
extern "C"
{
#include "cdf.h"
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MQVariedRTT");

Gnuplot2dDataset queuediscDataset;

enum AQM {
    TCN,
    CODEL,
    PIE,
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

uint32_t sendSize[3];
void CheckThroughput (Ptr<PacketSink> sink, uint32_t senderID) {
    uint32_t totalRecvBytes = sink->GetTotalRx ();
    uint32_t currentPeriodRecvBytes = totalRecvBytes - sendSize[senderID];

    sendSize[senderID] = totalRecvBytes;

    Simulator::Schedule (Seconds (0.02), &CheckThroughput, sink, senderID);

    NS_LOG_UNCOND ("Flow: " << senderID << ", throughput (Gbps): " << currentPeriodRecvBytes * 8 / 0.02 / 1000000000);
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
    if (aqm == TCN)
    {
        ss << id << "_mq_s_tcn_" << str << "_t" << redThreshold << "_n" << numOfSenders << "_l" << load << "." << terminal;
    }
    else if (aqm == CODEL)
    {
        ss << id << "_mq_s_codel_" << str << "_i" << interval << "_t" << target << "_n" << numOfSenders << "_l" << load  << "." << terminal;
    }
    else if (aqm == XXX)
    {
        ss << id << "_mq_s_xxx_" << str << "_int" << redThreshold << "_i" << interval << "_t" << target << "_n" <<numOfSenders << "_l" << load << "." << terminal;
    }
    else if (aqm == PIE)
    {
        ss << id << "_mq_s_pie_" << str << "_i" << interval << "_t" << target << "_n" << numOfSenders << "_l" << load << "." << terminal;
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
    LogComponentEnable ("MQVariedRTT", LOG_LEVEL_INFO);
#endif

    std::string id = "";

    std::string transportProt = "DcTcp";
    std::string aqmStr = "TCN";
    AQM aqm;
    // double endTime = 10.0;
    double simEndTime = 0.4;

    uint32_t numOfSenders = 5;

    double load = 0.1;
    // std::string cdfFileName = "";
    // std::string rttCdfFileName = "";

    unsigned randomSeed = 0;
    // uint32_t flowNum = 1000;

    uint32_t bufferSize = 600;

    uint32_t TCNThreshold = 150;

    uint32_t CODELInterval = 200;
    uint32_t CODELTarget = 10;

    uint32_t xxxInterval = 200;
    uint32_t xxxTarget = 10;
    uint32_t xxxMarkingThreshold = 150; // 65ms

    uint32_t pieTarget = 10;
    uint32_t pieInterval = 10; // 10ms

    // bool enableIncast = false;
    //
    bool isHighRTT = false;

    CommandLine cmd;
    cmd.AddValue ("id", "The running ID", id);
    cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, DcTcp", transportProt);
    cmd.AddValue ("AQM", "AQM to use: TCN, CODEL, PIE and XXX", aqmStr);

    // cmd.AddValue ("numOfSenders", "Concurrent senders", numOfSenders);
    // cmd.AddValue ("endTime", "Flow launch end time", endTime);
    cmd.AddValue ("simEndTime", "Simulation end time", simEndTime);
    // cmd.AddValue ("cdfFileName", "File name for flow distribution", cdfFileName);
    // cmd.AddValue ("rttCdfFileName", "File name for RTT distribution", rttCdfFileName);
    cmd.AddValue ("load", "Load of the network, 0.0 - 1.0", load);
    cmd.AddValue ("randomSeed", "Random seed, 0 for random generated", randomSeed);
    // cmd.AddValue ("flowNum", "Total flow num", flowNum);

    cmd.AddValue ("bufferSize", "The buffer size", bufferSize);

    cmd.AddValue ("TCNThreshold", "The threshold for TCN", TCNThreshold);

    cmd.AddValue ("CODELInterval", "The interval parameter in CODEL", CODELInterval);
    cmd.AddValue ("CODELTarget", "The target parameter in CODEL", CODELTarget);


    cmd.AddValue ("XXXInterval", "The persistent interval for XXX", xxxInterval);
    cmd.AddValue ("XXXTarget", "The persistent target for XXX", xxxTarget);
    cmd.AddValue ("XXXMarkingThreshold", "The instantaneous marking threshold for XXX", xxxMarkingThreshold);

    cmd.AddValue ("PIETarget", "The pie delay target", pieTarget);
    cmd.AddValue ("PIEInterval", "The interval used to update PIE p", pieInterval);

    cmd.AddValue ("HighRTT", "Whether to enable high RTT", isHighRTT);

    // cmd.AddValue ("enableIncast", "Whether to enable incast", enableIncast);

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

    if (aqmStr.compare ("TCN") == 0)
    {
        aqm = TCN;
    }
    else if (aqmStr.compare ("CODEL") == 0)
    {
        aqm = CODEL;
    }
    else if (aqmStr.compare ("XXX") == 0)
    {
        aqm = XXX;
        TCNThreshold = xxxMarkingThreshold;
        CODELTarget = xxxTarget;
        CODELInterval = xxxInterval;
    }
    else if (aqmStr.compare ("PIE") == 0)
    {
        aqm = PIE;
        CODELTarget = pieTarget;
        CODELInterval = pieInterval;
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

    // TCN Configuration
    Config::SetDefault ("ns3::TCNQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::TCNQueueDisc::MaxPackets", UintegerValue (bufferSize));
    Config::SetDefault ("ns3::TCNQueueDisc::Threshold", TimeValue (MicroSeconds (TCNThreshold)));


    // RED Configuration
    /*
    Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1400));
    Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (bufferSize));
    Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (false));
    */

    // XXX Configuration
    Config::SetDefault ("ns3::XXXQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::XXXQueueDisc::MaxPackets", UintegerValue (bufferSize));
    Config::SetDefault ("ns3::XXXQueueDisc::InstantaneousMarkingThreshold", TimeValue (MicroSeconds (xxxMarkingThreshold)));
    Config::SetDefault ("ns3::XXXQueueDisc::PersistentMarkingTarget", TimeValue (MicroSeconds (xxxTarget)));
    Config::SetDefault ("ns3::XXXQueueDisc::PersistentMarkingInterval", TimeValue (MicroSeconds (xxxInterval)));

    // PIE Configuration
    Config::SetDefault ("ns3::PieQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::PieQueueDisc::MeanPktSize", UintegerValue (1400));
    Config::SetDefault ("ns3::PieQueueDisc::Tupdate", TimeValue (MicroSeconds (pieInterval)));
    Config::SetDefault ("ns3::PieQueueDisc::QueueLimit", UintegerValue (bufferSize));
    Config::SetDefault ("ns3::PieQueueDisc::QueueDelayReference", TimeValue (MicroSeconds (pieTarget)));

    /*
    NS_LOG_INFO ("Loading RTT CDF");
    struct cdf_table *rttCdfTable = new cdf_table ();
    init_cdf (rttCdfTable);
    load_cdf (rttCdfTable, rttCdfFileName.c_str ());
    NS_LOG_INFO ("AVG RTT: " << avg_cdf (rttCdfTable));
    */

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

    TrafficControlHelper tc;

    NS_LOG_INFO ("Assign IP address");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");

    // For sender queues, we use the original drop tail queues

    for (uint32_t i = 0; i < numOfSenders; ++i)
    {
        uint32_t linkLatency = isHighRTT == false ? 30 : 150;
        NS_LOG_INFO ("Generate link latency: " << linkLatency);
        p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(linkLatency)));
        p2p.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
        p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (bufferSize));

        NodeContainer nodeContainer = NodeContainer (senders.Get (i), switches.Get (0));
        NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);
        Ipv4InterfaceContainer ipv4InterfaceContainer = ipv4.Assign (netDeviceContainer);
        ipv4.NewNetwork ();
        tc.Uninstall (netDeviceContainer);
    }

    p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(50)));
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
    p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (5));

    NodeContainer switchToRecvNodeContainer = NodeContainer (switches.Get (0), receivers.Get (0));
    NetDeviceContainer switchToRecvNetDeviceContainer = p2p.Install (switchToRecvNodeContainer);


    Ptr<DWRRQueueDisc> dwrrQdisc = CreateObject<DWRRQueueDisc> ();
    Ptr<Ipv4SimplePacketFilter> filter = CreateObject<Ipv4SimplePacketFilter> ();

    dwrrQdisc->AddPacketFilter (filter);

    ObjectFactory innerQueueFactory;
    if (aqm == TCN)
    {
        innerQueueFactory.SetTypeId ("ns3::TCNQueueDisc");
    }
    else if (aqm == CODEL)
    {
        innerQueueFactory.SetTypeId ("ns3::CoDelQueueDisc");
    }
    else if (aqm == PIE)
    {
        innerQueueFactory.SetTypeId ("ns3::PieQueueDisc");
    }
    else
    {
        innerQueueFactory.SetTypeId ("ns3::XXXQueueDisc");
    }

    /*
    for (uint32_t i = 0; i < QUEUE_NUM; ++i)
    {
        Ptr<QueueDisc> queueDisc = innerQueueFactory.Create<QueueDisc> ();
        dwrrQdisc->AddDWRRClass (queueDisc, i, 1500);
    }
    */

    Ptr<QueueDisc> queueDisc1 = innerQueueFactory.Create<QueueDisc> ();
    Ptr<QueueDisc> queueDisc2 = innerQueueFactory.Create<QueueDisc> ();
    Ptr<QueueDisc> queueDisc3 = innerQueueFactory.Create<QueueDisc> ();

    dwrrQdisc->AddDWRRClass (queueDisc1, 0, 3000);
    dwrrQdisc->AddDWRRClass (queueDisc2, 1, 1500);
    dwrrQdisc->AddDWRRClass (queueDisc3, 2, 1500);

    Ptr<NetDevice> device = switchToRecvNetDeviceContainer.Get (0);
    Ptr<TrafficControlLayer> tcl = device->GetNode ()->GetObject<TrafficControlLayer> ();

    dwrrQdisc->SetNetDevice (device);
    tcl->SetRootQueueDiscOnDevice (device, dwrrQdisc);

    tc.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "Limit", UintegerValue (bufferSize));
    Ipv4InterfaceContainer switchToRecvIpv4Container = ipv4.Assign (switchToRecvNetDeviceContainer);

    //free_cdf (rttCdfTable);

    NS_LOG_INFO ("Setting up routing table");

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    /*
    NS_LOG_INFO ("Initialize CDF table");
    struct cdf_table* cdfTable = new cdf_table ();
    init_cdf (cdfTable);
    load_cdf (cdfTable, cdfFileName.c_str ());

    NS_LOG_INFO ("Calculating request rate");
    double requestRate = load * 10e9 / (8 * avg_cdf (cdfTable)) / numOfSenders;
    NS_LOG_INFO ("Average request rate: " << requestRate << " per second per sender");
    */

    NS_LOG_INFO ("Initialize random seed: " << randomSeed);
    if (randomSeed == 0)
    {
        srand ((unsigned)time (NULL));
    }
    else
    {
        srand (randomSeed);
    }

    uint16_t basePort = 8080;

    NS_LOG_INFO ("Install 3 large TCP flows");
    for (uint32_t i = 0; i < 3; ++i)
    {
        BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (switchToRecvIpv4Container.GetAddress (1), basePort));
        source.SetAttribute ("MaxBytes", UintegerValue (0)); // 150kb
        source.SetAttribute ("SendSize", UintegerValue (1400));
        source.SetAttribute ("SimpleTOS", UintegerValue (i));
        ApplicationContainer sourceApps = source.Install (senders.Get (i));
        sourceApps.Start (Seconds (0.1 * i));
        sourceApps.Stop (Seconds (simEndTime));

        PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), basePort++));
        ApplicationContainer sinkApp = sink.Install (switchToRecvNodeContainer.Get (1));
        sinkApp.Start (Seconds (0.0));
        sinkApp.Stop (Seconds (simEndTime));
        Ptr<PacketSink> pktSink = sinkApp.Get (0)->GetObject<PacketSink> ();
        Simulator::ScheduleNow (&CheckThroughput, pktSink, i);
    }


    NS_LOG_INFO ("Install 100 short TCP flows");
    for (uint32_t i = 0; i < 100; ++i)
    {
        double startTime = rand_range (0.0, 0.4);
        uint32_t tos = rand_range (0, 3);
        BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (switchToRecvIpv4Container.GetAddress (1), basePort));
        source.SetAttribute ("MaxBytes", UintegerValue (28000)); // 14kb
        source.SetAttribute ("SendSize", UintegerValue (1400));
        source.SetAttribute ("SimpleTOS", UintegerValue (tos));
        ApplicationContainer sourceApps = source.Install (senders.Get (2 + i % 2));
        sourceApps.Start (Seconds (startTime));
        sourceApps.Stop (Seconds (simEndTime));

        PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), basePort++));
        ApplicationContainer sinkApp = sink.Install (switchToRecvNodeContainer.Get (1));
        sinkApp.Start (Seconds (0));
        sinkApp.Stop (Seconds (simEndTime));
    }


    /*
    NS_LOG_INFO ("Install background application");

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
            source.SetAttribute ("SimpleTOS", UintegerValue (i % QUEUE_NUM));
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
    */

    /*

    NS_LOG_INFO ("Install incast application");

    double incast_period = endTime / 10;

    for (uint32_t i = 0; i < numOfSenders; ++i)
    {
        double startTime = 0.0 + incast_period;
        while (startTime < endTime)
        {
            BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (switchToRecvIpv4Container.GetAddress (1), basePort));
            source.SetAttribute ("MaxBytes", UintegerValue (rand_range (FLOW_SIZE_MIN, FLOW_SIZE_MAX)));
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
    */

    NS_LOG_INFO ("Start Tracing System");

    queuediscDataset.SetTitle ("queue_disc");
    queuediscDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

    Simulator::ScheduleNow (&CheckQueueDiscSize, dwrrQdisc);

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

    flowMonitor->SerializeToXmlFile(GetFormatedStr (id, "flow_monitor", "xml", aqm, load, CODELInterval, CODELTarget, TCNThreshold, numOfSenders), true, true);
    linkMonitor->OutputToFile (GetFormatedStr (id, "link_monitor", "xml", aqm, load, CODELInterval, CODELTarget, TCNThreshold, numOfSenders), &DefaultFormat);

    Simulator::Destroy ();
    // free_cdf (cdfTable);

    DoGnuPlot (id, aqm, load, CODELInterval, CODELTarget, TCNThreshold, numOfSenders);

    return 0;
}
