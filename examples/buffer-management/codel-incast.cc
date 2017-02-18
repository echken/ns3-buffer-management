#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/gnuplot.h"

#define BUFFER_SIZE 250     // 100 packets
#define FLOW_SIZE 1000000   // 1000kb

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CoDelIncast");

Gnuplot2dDataset cwndDataset;
Gnuplot2dDataset queuediscDataset;
Gnuplot2dDataset throughputDataset;

enum AQM {
    RED,
    CODEL
};


std::string
GetFormatedStr (std::string str, std::string terminal, AQM aqm, uint32_t interval, uint32_t target)
{
    std::stringstream ss;
    if (aqm == RED)
    {
        ss << "red_" << str << "." << terminal;
    }
    else
    {
        ss << "codel_" << str << "_i" << interval << "_t" << target << "." << terminal;
    }
    return ss.str ();
}

void
DoGnuPlot (AQM aqm, uint32_t interval, uint32_t target)
{
    Gnuplot cwndGnuplot (GetFormatedStr ("cwnd", "png", aqm, interval, target).c_str ());
    cwndGnuplot.SetTitle ("cwnd");
    cwndGnuplot.SetTerminal ("png");
    cwndGnuplot.AddDataset (cwndDataset);
    std::ofstream cwndGnuplotFile (GetFormatedStr ("cwnd", "plt", aqm, interval, target).c_str ());
    cwndGnuplot.GenerateOutput (cwndGnuplotFile);
    cwndGnuplotFile.close ();

    Gnuplot queuediscGnuplot (GetFormatedStr ("queue_disc", "png", aqm, interval, target).c_str ());
    queuediscGnuplot.SetTitle ("queue_disc");
    queuediscGnuplot.SetTerminal ("png");
    queuediscGnuplot.AddDataset (queuediscDataset);
    std::ofstream queuediscGnuplotFile (GetFormatedStr ("queue_disc", "plt", aqm, interval, target).c_str ());
    queuediscGnuplot.GenerateOutput (queuediscGnuplotFile);
    queuediscGnuplotFile.close ();

    Gnuplot throughputGnuplot (GetFormatedStr ("throughput", "png", aqm, interval, target).c_str ());
    throughputGnuplot.SetTitle ("throughput");
    throughputGnuplot.SetTerminal ("png");
    throughputGnuplot.AddDataset (throughputDataset);
    std::ofstream throughputGnuplotFile (GetFormatedStr ("throughput", "plt", aqm, interval, target).c_str ());
    throughputGnuplot.GenerateOutput (throughputGnuplotFile);
    throughputGnuplotFile.close ();
}

void
CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndDataset.Add (Simulator::Now ().GetSeconds (), newCwnd);
}

void
TraceCwnd ()
{
    Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeCallback (&CwndChange));
}

void
CheckQueueDiscSize (Ptr<QueueDisc> queue)
{
    uint32_t qSize = queue->GetNPackets ();
    queuediscDataset.Add (Simulator::Now ().GetSeconds (), qSize);
    Simulator::Schedule (Seconds (0.00001), &CheckQueueDiscSize, queue);
}

uint32_t accumRecvBytes;

void
CheckThroughput (Ptr<PacketSink> sink)
{
    uint32_t totalRecvBytes = sink->GetTotalRx ();
    uint32_t currentPeriodRecvBytes = totalRecvBytes - accumRecvBytes;
    accumRecvBytes = totalRecvBytes;
    Simulator::Schedule (Seconds (0.001), &CheckThroughput, sink);
    throughputDataset.Add (Simulator::Now().GetSeconds (), currentPeriodRecvBytes * 8 / 0.001);
}

static void
CongChange (TcpSocketState::TcpCongState_t oldCong, TcpSocketState::TcpCongState_t newCong)
{
    std::ofstream congRecord ("cong.txt", std::ios::out|std::ios::app);
    congRecord << Simulator::Now ().GetSeconds () << " " << TcpSocketState::TcpCongStateName[newCong] << std::endl;
}

static void
TraceCong ()
{
    Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/CongState", MakeCallback (&CongChange));
}

int main (int argc, char *argv[])
{
#if 1
    LogComponentEnable ("CoDelIncast", LOG_LEVEL_INFO);
#endif

    std::string transportProt = "DcTcp";
    std::string aqmStr = "CODEL";
    AQM aqm;
    double endTime = 0.01;

    uint32_t numOfSenders = 10;

    uint32_t CODELInterval = 50;
    uint32_t CODELTarget = 40;

    CommandLine cmd;
    cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, DcTcp", transportProt);
    cmd.AddValue ("AQM", "AQM to use: RED, CODEL", aqmStr);
    cmd.AddValue ("CODELInterval", "The interval parameter in CODEL", CODELInterval);
    cmd.AddValue ("CODELTarget", "The target parameter in CODEL", CODELTarget);
    cmd.AddValue ("numOfSenders", "Concurrent senders", numOfSenders);
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
    Config::SetDefault ("ns3::CoDelQueueDisc::MaxPackets", UintegerValue (BUFFER_SIZE));
    Config::SetDefault ("ns3::CoDelQueueDisc::Target", TimeValue (MicroSeconds (CODELTarget)));
    Config::SetDefault ("ns3::CoDelQueueDisc::Interval", TimeValue (MicroSeconds (CODELInterval)));

    // RED Configuration
    Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1400));
    Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (BUFFER_SIZE));
    Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (false));

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
    p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(10)));
    p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (5));

    TrafficControlHelper tc;
    if (aqm == CODEL)
    {
        tc.SetRootQueueDisc ("ns3::CoDelQueueDisc");
    }
    else
    {
        tc.SetRootQueueDisc ("ns3::RedQueueDisc", "MinTh", DoubleValue (65),
                                                  "MaxTh", DoubleValue (65));
    }

    NS_LOG_INFO ("Assign IP address");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");

    for (uint32_t i = 0; i < numOfSenders; ++i)
    {
        NodeContainer nodeContainer = NodeContainer (senders.Get (i), switches.Get (0));
        NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);
        QueueDiscContainer queuediscDataset = tc.Install (netDeviceContainer);
        Ipv4InterfaceContainer ipv4InterfaceContainer = ipv4.Assign (netDeviceContainer);
        ipv4.NewNetwork ();
    }

    NodeContainer switchToRecvNodeContainer = NodeContainer (switches.Get (0), receivers.Get (0));
    NetDeviceContainer switchToRecvNetDeviceContainer = p2p.Install (switchToRecvNodeContainer);
    QueueDiscContainer switchToRecvQueueDiscContainer = tc.Install (switchToRecvNetDeviceContainer);
    Ipv4InterfaceContainer switchToRecvIpv4Container = ipv4.Assign (switchToRecvNetDeviceContainer);

    NS_LOG_INFO ("Setting up routing table");

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    NS_LOG_INFO ("Install TCP based application");

    uint16_t basePort = 8080;
    Ptr<PacketSink> packetSink;

    for (uint32_t i = 0; i < numOfSenders; ++i)
    {
        BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (switchToRecvIpv4Container.GetAddress (1), basePort + i));
        source.SetAttribute ("MaxBytes", UintegerValue (FLOW_SIZE));
        source.SetAttribute ("SendSize", UintegerValue (1400));
        ApplicationContainer sourceApps = source.Install (senders.Get (i));
        sourceApps.Start (Seconds (0.0));
        sourceApps.Stop (Seconds (endTime));

        PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), basePort + i));
        ApplicationContainer sinkApp = sink.Install (switchToRecvNodeContainer. Get (1));
        if (i == 0)
        {
            packetSink = sinkApp.Get (0)->GetObject<PacketSink> ();
        }
        sinkApp.Start (Seconds (0.0));
        sinkApp.Stop (Seconds (endTime));
    }

    NS_LOG_INFO ("Start Tracing System");

    cwndDataset.SetTitle ("cwnd");
    cwndDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

    queuediscDataset.SetTitle ("queue_disc");
    queuediscDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

    throughputDataset.SetTitle ("throughput");
    throughputDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

    remove ("cong.txt");
    Simulator::Schedule (Seconds (0.00001), &TraceCwnd);
    Simulator::Schedule (Seconds (0.00001), &TraceCong);
    Simulator::ScheduleNow (&CheckQueueDiscSize, switchToRecvQueueDiscContainer.Get (0));
    Simulator::ScheduleNow (&CheckThroughput, packetSink);

    NS_LOG_INFO ("Run Simulations");

    Simulator::Stop (Seconds (endTime));
    Simulator::Run ();

    Simulator::Destroy ();

    DoGnuPlot (aqm, CODELInterval, CODELTarget);

    return 0;
}
