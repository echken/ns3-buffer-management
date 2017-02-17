#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CoDelDcTcp");

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

void CheckThroughput (Ptr<PacketSink> sink)
{
    uint32_t totalRecvBytes = sink->GetTotalRx ();
    uint32_t currentPeriodRecvBytes = totalRecvBytes - accumRecvBytes;
    accumRecvBytes = totalRecvBytes;
    Simulator::Schedule (Seconds (0.001), &CheckThroughput, sink);
    throughputDataset.Add (Simulator::Now().GetSeconds (), currentPeriodRecvBytes * 8 / 0.001);
}

int main (int argc, char *argv[])
{
#if 1
    LogComponentEnable ("CoDelDcTcp", LOG_LEVEL_INFO);
#endif

    std::string transportProt = "DcTcp";
    std::string aqmStr = "CODEL";
    AQM aqm;

    uint32_t CODELInterval = 50;
    uint32_t CODELTarget = 40;

    CommandLine cmd;
    cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, DcTcp", transportProt);
    cmd.AddValue ("AQM", "AQM to use: RED, CODEL", aqmStr);
    cmd.AddValue ("CODELInterval", "The interval parameter in CODEL", CODELInterval);
    cmd.AddValue ("CODELTarget", "The target parameter in CODEL", CODELTarget);
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
    Config::SetDefault ("ns3::CoDelQueueDisc::MaxPackets", UintegerValue (250));
    Config::SetDefault ("ns3::CoDelQueueDisc::Target", TimeValue (MicroSeconds (CODELTarget)));
    Config::SetDefault ("ns3::CoDelQueueDisc::Interval", TimeValue (MicroSeconds (CODELInterval)));

    // RED Configuration
    Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1400));
    Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (250));
    Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (false));

    NS_LOG_INFO ("Create nodes.");
    NodeContainer c;
    c.Create (4);

    NodeContainer n0n2 = NodeContainer (c.Get (0), c.Get(2));
    NodeContainer n1n2 = NodeContainer (c.Get (1), c.Get(2));
    NodeContainer n2n3 = NodeContainer (c.Get (2), c.Get(3));

    NS_LOG_INFO ("Install Internet stack");
    InternetStackHelper internet;
    internet.Install (c);

    NS_LOG_INFO ("Install channel");
    PointToPointHelper p2p;

    p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
    p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(10)));

    p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (10));

    NetDeviceContainer d0d2 = p2p.Install (n0n2);
    NetDeviceContainer d1d2 = p2p.Install (n1n2);
    NetDeviceContainer d2d3 = p2p.Install (n2n3);

    TrafficControlHelper tc;
    if (aqm == CODEL)
    {
        tc.SetRootQueueDisc ("ns3::CoDelQueueDisc");
    }
    else
    {
        tc.SetRootQueueDisc ("ns3::RedQueueDisc", "MinTh", DoubleValue (20),
                                                  "MaxTh", DoubleValue (20));
    }

    QueueDiscContainer qd0d2 = tc.Install (d0d2);
    QueueDiscContainer qd1d2 = tc.Install (d1d2);
    QueueDiscContainer qd2d3 = tc.Install (d2d3);

    NS_LOG_INFO ("Assign IP address");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4.Assign (d0d2);
    ipv4.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign (d1d2);
    ipv4.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign (d2d3);

    NS_LOG_INFO ("Setting up routing table");

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    NS_LOG_INFO ("Install TCP based application");

    uint16_t port1 = 8080;
    uint16_t port2 = 8081;

    BulkSendHelper source1 ("ns3::TcpSocketFactory", InetSocketAddress (i2i3.GetAddress(1), port1));
    source1.SetAttribute ("MaxBytes", UintegerValue (0));
    source1.SetAttribute ("SendSize", UintegerValue (1400));
    ApplicationContainer sourceApps1 = source1.Install (c.Get (0));
    sourceApps1.Start (Seconds (0.0));
    sourceApps1.Stop (Seconds (1.0));

    PacketSinkHelper sink1 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port1));
    ApplicationContainer sinkApp1 = sink1.Install (c.Get (3));
    sinkApp1.Start (Seconds (0.0));
    sinkApp1.Stop (Seconds (1.0));

    BulkSendHelper source2 ("ns3::TcpSocketFactory", InetSocketAddress (i2i3.GetAddress(1), port2));
    source2.SetAttribute ("MaxBytes", UintegerValue (0));
    source2.SetAttribute ("SendSize", UintegerValue (1400));
    ApplicationContainer sourceApps2 = source2.Install (c.Get (1));
    sourceApps2.Start (Seconds (0.0));
    sourceApps2.Stop (Seconds (1.0));

    PacketSinkHelper sink2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port2));
    ApplicationContainer sinkApp2 = sink2.Install (c.Get (3));
    sinkApp2.Start (Seconds (0.0));
    sinkApp2.Stop (Seconds (1.0));

    NS_LOG_INFO ("Start Tracing System");

    cwndDataset.SetTitle ("cwnd");
    cwndDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

    queuediscDataset.SetTitle ("queue_disc");
    queuediscDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

    throughputDataset.SetTitle ("throughput");
    throughputDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

    Simulator::Schedule (Seconds (0.00001), &TraceCwnd);
    Simulator::ScheduleNow (&CheckQueueDiscSize, qd2d3.Get (0));
    Simulator::ScheduleNow (&CheckThroughput, sinkApp1.Get (0)->GetObject<PacketSink> ());

    NS_LOG_INFO ("Run Simulations");

    Simulator::Stop (Seconds (0.1));
    Simulator::Run ();

    Simulator::Destroy ();

    DoGnuPlot (aqm, CODELInterval, CODELTarget);

    return 0;
}
