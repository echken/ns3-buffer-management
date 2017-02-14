#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

#define QUEUE_NUM 8

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MQCoDel");

Ptr<DWRRQueueDisc>
CreateDWRRQueueDisc ()
{
    Ptr<DWRRQueueDisc> qdisc = CreateObject<DWRRQueueDisc> ();
    Ptr<Ipv4SimplePacketFilter> filter = CreateObject<Ipv4SimplePacketFilter> ();
    qdisc->AddPacketFilter (filter);

    for (int i = 0; i < QUEUE_NUM; i++)
    {
        Ptr<CoDelQueueDisc> codelQueueDisc = CreateObject<CoDelQueueDisc> ();
        qdisc->AddDWRRClass (codelQueueDisc, i, 14000); //Quantum 14kb
    }
    return qdisc;
}

void
InstallQueueDisc (NetDeviceContainer c)
{
    for (NetDeviceContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
        Ptr<NetDevice> d = *i;
        Ptr<TrafficControlLayer> tc = d->GetNode ()->GetObject<TrafficControlLayer> ();
        Ptr<DWRRQueueDisc> qdisc = CreateDWRRQueueDisc ();
        qdisc->SetNetDevice (d);
        tc->SetRootQueueDiscOnDevice (d, qdisc);
    }
}

int main (int argc, char *argv[])
{
#if 1
    LogComponentEnable ("MQCoDel", LOG_LEVEL_INFO);
#endif

    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(1400));
    Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (0));
    Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (MilliSeconds (5)));
    Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
    Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (5)));
    Config::SetDefault ("ns3::TcpSocketBase::ClockGranularity", TimeValue (MicroSeconds (100)));
    Config::SetDefault ("ns3::TcpSocketBase::TLB", BooleanValue (true));
    Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue (MicroSeconds (80)));


    std::string transportProt = "DcTcp";

    CommandLine cmd;
    cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, DcTcp", transportProt);

    cmd.Parse (argc, argv);

    if (transportProt.compare ("Tcp") == 0)
    {
        Config::SetDefault ("ns3::TcpSocketBase::ECN", BooleanValue (false));
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

    NS_LOG_INFO ("Create nodes.");
    NodeContainer c;
    c.Create (6);

    NodeContainer n0n1 = NodeContainer (c.Get (0), c.Get(1));
    NodeContainer n1n2 = NodeContainer (c.Get (1), c.Get(2));
    NodeContainer n1n3 = NodeContainer (c.Get (1), c.Get(3));
    NodeContainer n2n4 = NodeContainer (c.Get (2), c.Get(4));
    NodeContainer n3n4 = NodeContainer (c.Get (3), c.Get(4));
    NodeContainer n4n5 = NodeContainer (c.Get (4), c.Get(5));

    NS_LOG_INFO ("Install Internet stack");
    InternetStackHelper internet;
    internet.SetTLB (true);
    internet.Install (c.Get (0));
    internet.Install (c.Get (2));
    internet.Install (c.Get (3));
    internet.Install (c.Get (5));
    internet.Install (c.Get (1));
    internet.Install (c.Get (4));

    NS_LOG_INFO ("Install channel");
    PointToPointHelper p2p;

    Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_BYTES"));
    Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1400));
    Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (250 * 1400));
    Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (false));

    TrafficControlHelper tc;
    tc.SetRootQueueDisc ("ns3::RedQueueDisc", "MinTh", DoubleValue (65 * 1400),
                                              "MaxTh", DoubleValue (65 * 1400));

    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
    p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(10)));

    p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (10));

    NetDeviceContainer d0d1 = p2p.Install (n0n1);
    NetDeviceContainer d1d2 = p2p.Install (n1n2);
    NetDeviceContainer d2d4 = p2p.Install (n2n4);
    NetDeviceContainer d4d5 = p2p.Install (n4n5);
    NetDeviceContainer d1d3 = p2p.Install (n1n3);
    NetDeviceContainer d3d4 = p2p.Install (n3n4);


    InstallQueueDisc (d0d1);
    InstallQueueDisc (d1d2);
    InstallQueueDisc (d2d4);
    InstallQueueDisc (d4d5);
    InstallQueueDisc (d1d3);
    InstallQueueDisc (d3d4);

    NS_LOG_INFO ("Assign IP address");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign (d0d1);
    ipv4.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign (d1d2);
    ipv4.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i4 = ipv4.Assign (d2d4);
    ipv4.SetBase ("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i3 = ipv4.Assign (d1d3);
    ipv4.SetBase ("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i4 = ipv4.Assign (d3d4);
    ipv4.SetBase ("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer i4i5 = ipv4.Assign (d4d5);

    NS_LOG_INFO ("Setting up routing table");

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    NS_LOG_INFO ("Install TCP based application");

    uint16_t port = 8080;

    for (int i = 0; i < QUEUE_NUM; i ++)
    {
        BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (i4i5.GetAddress(1), port + i));
        source.SetAttribute ("SimpleTOS", UintegerValue (i));
        source.SetAttribute ("MaxBytes", UintegerValue (0));
        source.SetAttribute ("SendSize", UintegerValue (1400));
        ApplicationContainer sourceApps = source.Install (c.Get (0));
        sourceApps.Start (Seconds (0.0));
        sourceApps.Stop (Seconds (0.1));

        PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port + i));
        ApplicationContainer sinkApp = sink.Install (c.Get (5));
        sinkApp.Start (Seconds (0.0));
        sinkApp.Stop (Seconds (0.1));
    }

    NS_LOG_INFO ("Run Simulations");

    Simulator::Stop (Seconds (0.1));
    Simulator::Run ();

    Simulator::Destroy ();

    return 0;
}
