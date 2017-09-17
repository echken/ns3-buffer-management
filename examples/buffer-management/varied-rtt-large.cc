#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-conga-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-drb-routing-helper.h"
#include "ns3/ipv4-xpath-routing-helper.h"
#include "ns3/ipv4-tlb.h"
#include "ns3/ipv4-clove.h"
#include "ns3/ipv4-tlb-probing.h"
#include "ns3/link-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/tcp-resequence-buffer.h"
#include "ns3/ipv4-drill-routing-helper.h"
#include "ns3/ipv4-letflow-routing-helper.h"

#include <vector>
#include <map>
#include <utility>
#include <set>

// The CDF in TrafficGenerator
extern "C"
{
#include "cdf.h"
}

#define LINK_CAPACITY_BASE    1000000000          // 1Gbps
#define BUFFER_SIZE 600                           // 250 packets

// The flow port range, each flow will be assigned a random port number within this range
#define PORT_START 10000
#define PORT_END 50000

// Adopted from the simulation from WANG PENG
// Acknowledged to https://williamcityu@bitbucket.org/williamcityu/2016-socc-simulation.git
#define PACKET_SIZE 1400

#define PRESTO_RATIO 10

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VariedRTTLarge");

enum RunMode {
  ECMP,
};

enum AQM {
  TCN,
  CODEL,
  XXX
};

// Acknowledged to https://github.com/HKUST-SING/TrafficGenerator/blob/master/src/common/common.c
double poission_gen_interval(double avg_rate)
{
  if (avg_rate > 0)
    return -logf(1.0 - (double)rand() / RAND_MAX) / avg_rate;
  else
    return 0;
}

template<typename T>
T rand_range (T min, T max)
{
  return min + ((double)max - min) * rand () / RAND_MAX;
}

void install_applications (int fromLeafId, NodeContainer servers, double requestRate, struct cdf_table *cdfTable,
                           long &flowCount, long &totalFlowSize, int SERVER_COUNT, int LEAF_COUNT, double START_TIME, double END_TIME, double FLOW_LAUNCH_END_TIME)
{
  NS_LOG_INFO ("Install applications:");
  for (int i = 0; i < SERVER_COUNT; i++)
    {
      int fromServerIndex = fromLeafId * SERVER_COUNT + i;

      double startTime = START_TIME + poission_gen_interval (requestRate);
      while (startTime < FLOW_LAUNCH_END_TIME)
        {
          flowCount ++;
          uint16_t port = rand_range (PORT_START, PORT_END);

          int destServerIndex = fromServerIndex;
          while (destServerIndex >= fromLeafId * SERVER_COUNT && destServerIndex < fromLeafId * SERVER_COUNT + SERVER_COUNT)
            {
              destServerIndex = rand_range (0, SERVER_COUNT * LEAF_COUNT);
            }

          Ptr<Node> destServer = servers.Get (destServerIndex);
          Ptr<Ipv4> ipv4 = destServer->GetObject<Ipv4> ();
          Ipv4InterfaceAddress destInterface = ipv4->GetAddress (1,0);
          Ipv4Address destAddress = destInterface.GetLocal ();

          BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (destAddress, port));
          uint32_t flowSize = gen_random_cdf (cdfTable);

          totalFlowSize += flowSize;
          source.SetAttribute ("SendSize", UintegerValue (PACKET_SIZE));
          source.SetAttribute ("MaxBytes", UintegerValue(flowSize));

          // Install apps
          ApplicationContainer sourceApp = source.Install (servers.Get (fromServerIndex));
          sourceApp.Start (Seconds (startTime));
          sourceApp.Stop (Seconds (END_TIME));

          // Install packet sinks
          PacketSinkHelper sink ("ns3::TcpSocketFactory",
                                 InetSocketAddress (Ipv4Address::GetAny (), port));
          ApplicationContainer sinkApp = sink.Install (servers. Get (destServerIndex));
          sinkApp.Start (Seconds (START_TIME));
          sinkApp.Stop (Seconds (END_TIME));

          /*
            NS_LOG_INFO ("\tFlow from server: " << fromServerIndex << " to server: "
            << destServerIndex << " on port: " << port << " with flow size: "
            << flowSize << " [start time: " << startTime <<"]");
          */

          startTime += poission_gen_interval (requestRate);
        }
    }
}

int main (int argc, char *argv[])
{
#if 1
  LogComponentEnable ("VariedRTTLarge", LOG_LEVEL_INFO);
#endif

  // Command line parameters parsing
  std::string id = "0";
  std::string runModeStr = "ECMP";
  unsigned randomSeed = 0;
  std::string cdfFileName = "";
  double load = 0.0;
  std::string transportProt = "DcTcp";

  std::string aqmStr = "CODEL";

  // The simulation starting and ending time
  double START_TIME = 0.0;
  double END_TIME = 0.25;

  double FLOW_LAUNCH_END_TIME = 0.1;

  uint32_t linkLatency = 10;

  int SERVER_COUNT = 8;
  int SPINE_COUNT = 4;
  int LEAF_COUNT = 4;
  int LINK_COUNT = 1;

  uint64_t spineLeafCapacity = 10;
  uint64_t leafServerCapacity = 10;

  uint32_t TCNThreshold = 150;
  
  uint32_t CODELInterval = 150;
  uint32_t CODELTarget = 10;
  
  uint32_t xxxInterval = 150;
  uint32_t xxxTarget = 10;
  uint32_t xxxMarkingThreshold = 30;

  CommandLine cmd;
  cmd.AddValue ("ID", "Running ID", id);
  cmd.AddValue ("StartTime", "Start time of the simulation", START_TIME);
  cmd.AddValue ("EndTime", "End time of the simulation", END_TIME);
  cmd.AddValue ("FlowLaunchEndTime", "End time of the flow launch period", FLOW_LAUNCH_END_TIME);
  cmd.AddValue ("runMode", "Running mode of this simulation: Conga, Conga-flow, Presto, Weighted-Presto, DRB, FlowBender, ECMP, Clove, DRILL, LetFlow", runModeStr);
  cmd.AddValue ("randomSeed", "Random seed, 0 for random generated", randomSeed);
  cmd.AddValue ("cdfFileName", "File name for flow distribution", cdfFileName);
  cmd.AddValue ("load", "Load of the network, 0.0 - 1.0", load);
  cmd.AddValue ("transportProt", "Transport protocol to use: Tcp, DcTcp", transportProt);
  cmd.AddValue ("linkLatency", "Link latency, should be in MicroSeconds", linkLatency);

  cmd.AddValue ("serverCount", "The Server count", SERVER_COUNT);
  cmd.AddValue ("spineCount", "The Spine count", SPINE_COUNT);
  cmd.AddValue ("leafCount", "The Leaf count", LEAF_COUNT);
  cmd.AddValue ("linkCount", "The Link count", LINK_COUNT);

  cmd.AddValue ("spineLeafCapacity", "Spine <-> Leaf capacity in Gbps", spineLeafCapacity);
  cmd.AddValue ("leafServerCapacity", "Leaf <-> Server capacity in Gbps", leafServerCapacity);

  cmd.AddValue ("AQM", "AQM to use: RED, CODEL", aqmStr);
  cmd.AddValue ("CODELInterval", "The interval parameter in CODEL", CODELInterval);
  cmd.AddValue ("CODELTarget", "The target parameter in CODEL", CODELTarget);

  cmd.AddValue ("TCNThreshold", "The threshold for TCN", TCNThreshold);

  cmd.AddValue ("XXXInterval", "The persistent interval for XXX", xxxInterval);
  cmd.AddValue ("XXXTarget", "The persistent target for XXX", xxxTarget);
  cmd.AddValue ("XXXMarkingThreshold", "The instantaneous marking threshold for XXX", xxxMarkingThreshold);


  cmd.Parse (argc, argv);

  uint64_t SPINE_LEAF_CAPACITY = spineLeafCapacity * LINK_CAPACITY_BASE;
  uint64_t LEAF_SERVER_CAPACITY = leafServerCapacity * LINK_CAPACITY_BASE;
  Time LINK_LATENCY = MicroSeconds (linkLatency);

  RunMode runMode;
  if (runModeStr.compare ("ECMP") == 0)
    {
      runMode = ECMP;
    }
  else
    {
      NS_LOG_ERROR ("The running mode should be ECMP");
      return 0;
    }

  if (load < 0.0 || load >= 1.0)
    {
      NS_LOG_ERROR ("The network load should within 0.0 and 1.0");
      return 0;
    }

  AQM aqm;
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
  else
    {
      return 0;
    }

  if (transportProt.compare ("DcTcp") == 0)
    {
      NS_LOG_INFO ("Enabling DcTcp");
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpDCTCP::GetTypeId ()));

      // TCN Configuration
      Config::SetDefault ("ns3::TCNQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
      Config::SetDefault ("ns3::TCNQueueDisc::MaxPackets", UintegerValue (BUFFER_SIZE));
      Config::SetDefault ("ns3::TCNQueueDisc::Threshold", TimeValue (MicroSeconds (TCNThreshold)));

      // CoDel Configuration
      Config::SetDefault ("ns3::CoDelQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
      Config::SetDefault ("ns3::CoDelQueueDisc::MaxPackets", UintegerValue (BUFFER_SIZE));
      Config::SetDefault ("ns3::CoDelQueueDisc::Target", TimeValue (MicroSeconds (CODELTarget)));
      Config::SetDefault ("ns3::CoDelQueueDisc::Interval", TimeValue (MicroSeconds (CODELInterval)));

      // XXX Configuration
      Config::SetDefault ("ns3::XXXQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
      Config::SetDefault ("ns3::XXXQueueDisc::MaxPackets", UintegerValue (BUFFER_SIZE));
      Config::SetDefault ("ns3::XXXQueueDisc::InstantaneousMarkingThreshold", TimeValue (MicroSeconds (xxxMarkingThreshold)));
      Config::SetDefault ("ns3::XXXQueueDisc::PersistentMarkingTarget", TimeValue (MicroSeconds (xxxTarget)));
      Config::SetDefault ("ns3::XXXQueueDisc::PersistentMarkingInterval", TimeValue (MicroSeconds (xxxInterval)));
    }

  NS_LOG_INFO ("Config parameters");
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(PACKET_SIZE));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (0));
  Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (MilliSeconds (5)));

  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
  Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (5)));
  Config::SetDefault ("ns3::TcpSocketBase::ClockGranularity", TimeValue (MicroSeconds (100)));
  Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue (MicroSeconds (80)));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (160000000));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (160000000));

  NodeContainer spines;
  spines.Create (SPINE_COUNT);
  NodeContainer leaves;
  leaves.Create (LEAF_COUNT);
  NodeContainer servers;
  servers.Create (SERVER_COUNT * LEAF_COUNT);

  NS_LOG_INFO ("Install Internet stacks");
  InternetStackHelper internet;
  Ipv4GlobalRoutingHelper globalRoutingHelper;

  internet.SetRoutingHelper (globalRoutingHelper);
  Config::SetDefault ("ns3::Ipv4GlobalRouting::PerflowEcmpRouting", BooleanValue(true));

  internet.Install (servers);
  internet.Install (spines);
  internet.Install (leaves);

  NS_LOG_INFO ("Install channels and assign addresses");

  PointToPointHelper p2p;
  Ipv4AddressHelper ipv4;

  NS_LOG_INFO ("Configuring servers");
  // Setting servers
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (LEAF_SERVER_CAPACITY)));
  p2p.SetChannelAttribute ("Delay", TimeValue(LINK_LATENCY));
  p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (10));

  ipv4.SetBase ("10.1.0.0", "255.255.255.0");

  for (int i = 0; i < LEAF_COUNT; i++)
    {
      ipv4.NewNetwork ();

      for (int j = 0; j < SERVER_COUNT; j++)
        {
          int serverIndex = i * SERVER_COUNT + j;
          NodeContainer nodeContainer = NodeContainer (leaves.Get (i), servers.Get (serverIndex));
          NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);

          //TODO We should change this, at endhost we are not going to mark ECN but add delay using delay queue disc
              
          Ptr<DelayQueueDisc> delayQueueDisc = CreateObject<DelayQueueDisc> ();
          Ptr<Ipv4SimplePacketFilter> filter = CreateObject<Ipv4SimplePacketFilter> ();

          delayQueueDisc->AddPacketFilter (filter);

          Ptr<PfifoFastQueueDisc> pfifoQueueDisc = CreateObject<PfifoFastQueueDisc> ();
          delayQueueDisc->AddOutQueue (pfifoQueueDisc);

          ObjectFactory innerQueueFactory;
          innerQueueFactory.SetTypeId ("ns3::PfifoFastQueueDisc");

          Ptr<QueueDisc> queueDisc1 = innerQueueFactory.Create<QueueDisc> ();
          Ptr<QueueDisc> queueDisc2 = innerQueueFactory.Create<QueueDisc> ();
          Ptr<QueueDisc> queueDisc3 = innerQueueFactory.Create<QueueDisc> ();
          Ptr<QueueDisc> queueDisc4 = innerQueueFactory.Create<QueueDisc> ();
          Ptr<QueueDisc> queueDisc5 = innerQueueFactory.Create<QueueDisc> ();

          delayQueueDisc->AddDelayClass (queueDisc1, 0, MicroSeconds (1));
          delayQueueDisc->AddDelayClass (queueDisc2, 1, MicroSeconds (20));
          delayQueueDisc->AddDelayClass (queueDisc3, 2, MicroSeconds (50));
          delayQueueDisc->AddDelayClass (queueDisc4, 3, MicroSeconds (80));
          delayQueueDisc->AddDelayClass (queueDisc5, 4, MicroSeconds (160));

          ObjectFactory switchSideQueueFactory;

          if (aqm == TCN)
            {
              switchSideQueueFactory.SetTypeId ("ns3::TCNQueueDisc");
            }
          else if (aqm == CODEL)
            {
              switchSideQueueFactory.SetTypeId ("ns3::CoDelQueueDisc");
            }
          else
            {
              switchSideQueueFactory.SetTypeId ("ns3::XXXQueueDisc");
            }
          Ptr<QueueDisc> switchSideQueueDisc = switchSideQueueFactory.Create<QueueDisc> ();

          Ptr<NetDevice> netDevice0 = netDeviceContainer.Get (0);
          Ptr<TrafficControlLayer> tcl0 = netDevice0->GetNode ()->GetObject<TrafficControlLayer> ();

          delayQueueDisc->SetNetDevice (netDevice0);
          tcl0->SetRootQueueDiscOnDevice (netDevice0, delayQueueDisc);

          Ptr<NetDevice> netDevice1 = netDeviceContainer.Get (1);
          Ptr<TrafficControlLayer> tcl1 = netDevice1->GetNode ()->GetObject<TrafficControlLayer> ();
          tcl1->SetRootQueueDiscOnDevice (netDevice1, switchSideQueueDisc);

          Ipv4InterfaceContainer interfaceContainer = ipv4.Assign (netDeviceContainer);

          NS_LOG_INFO ("Leaf - " << i << " is connected to Server - " << j << " with address "
                       << interfaceContainer.GetAddress(0) << " <-> " << interfaceContainer.GetAddress (1)
                       << " with port " << netDeviceContainer.Get (0)->GetIfIndex () << " <-> " << netDeviceContainer.Get (1)->GetIfIndex ());
        }
    }

  NS_LOG_INFO ("Configuring switches");
  // Setting up switches
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (SPINE_LEAF_CAPACITY)));

  for (int i = 0; i < LEAF_COUNT; i++)
    {
      for (int j = 0; j < SPINE_COUNT; j++)
        {

          for (int l = 0; l < LINK_COUNT; l++)
            {
              ipv4.NewNetwork ();

              NodeContainer nodeContainer = NodeContainer (leaves.Get (i), spines.Get (j));
              NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);
              ObjectFactory switchSideQueueFactory;

              if (aqm == TCN)
                {
                  switchSideQueueFactory.SetTypeId ("ns3::TCNQueueDisc");
                }
              else if (aqm == CODEL)
                {
                  switchSideQueueFactory.SetTypeId ("ns3::CoDelQueueDisc");
                }
              else
                {
                  switchSideQueueFactory.SetTypeId ("ns3::XXXQueueDisc");
                }

              Ptr<QueueDisc> leafQueueDisc = switchSideQueueFactory.Create<QueueDisc> ();

              Ptr<NetDevice> netDevice0 = netDeviceContainer.Get (0);
              Ptr<TrafficControlLayer> tcl0 = netDevice0->GetNode ()->GetObject<TrafficControlLayer> ();
              leafQueueDisc->SetNetDevice (netDevice0);
              tcl0->SetRootQueueDiscOnDevice (netDevice0, leafQueueDisc);

              Ptr<QueueDisc> spineQueueDisc = switchSideQueueFactory.Create<QueueDisc> ();

              Ptr<NetDevice> netDevice1 = netDeviceContainer.Get (1);
              Ptr<TrafficControlLayer> tcl1 = netDevice1->GetNode ()->GetObject<TrafficControlLayer> ();
              spineQueueDisc->SetNetDevice (netDevice1);
              tcl1->SetRootQueueDiscOnDevice (netDevice1, spineQueueDisc);


              Ipv4InterfaceContainer ipv4InterfaceContainer = ipv4.Assign (netDeviceContainer);
              NS_LOG_INFO ("Leaf - " << i << " is connected to Spine - " << j << " with address "
                           << ipv4InterfaceContainer.GetAddress(0) << " <-> " << ipv4InterfaceContainer.GetAddress (1)
                           << " with port " << netDeviceContainer.Get (0)->GetIfIndex () << " <-> " << netDeviceContainer.Get (1)->GetIfIndex ()
                           << " with data rate " << spineLeafCapacity);

            }
        }
    }

  NS_LOG_INFO ("Populate global routing tables");

  double oversubRatio = static_cast<double>(SERVER_COUNT * LEAF_SERVER_CAPACITY) / (SPINE_LEAF_CAPACITY * SPINE_COUNT * LINK_COUNT);
  NS_LOG_INFO ("Over-subscription ratio: " << oversubRatio);

  NS_LOG_INFO ("Initialize CDF table");
  struct cdf_table* cdfTable = new cdf_table ();
  init_cdf (cdfTable);
  load_cdf (cdfTable, cdfFileName.c_str ());

  NS_LOG_INFO ("Calculating request rate");
  double requestRate = load * LEAF_SERVER_CAPACITY * SERVER_COUNT / oversubRatio / (8 * avg_cdf (cdfTable)) / SERVER_COUNT;
  NS_LOG_INFO ("Average request rate: " << requestRate << " per second");

  NS_LOG_INFO ("Initialize random seed: " << randomSeed);
  if (randomSeed == 0)
    {
      srand ((unsigned)time (NULL));
    }
  else
    {
      srand (randomSeed);
    }

  NS_LOG_INFO ("Create applications");

  long flowCount = 0;
  long totalFlowSize = 0;

  for (int fromLeafId = 0; fromLeafId < LEAF_COUNT; fromLeafId ++)
    {
      install_applications(fromLeafId, servers, requestRate, cdfTable, flowCount, totalFlowSize, SERVER_COUNT, LEAF_COUNT, START_TIME, END_TIME, FLOW_LAUNCH_END_TIME);
    }

  NS_LOG_INFO ("Total flow: " << flowCount);

  NS_LOG_INFO ("Actual average flow size: " << static_cast<double> (totalFlowSize) / flowCount);

  NS_LOG_INFO ("Enabling flow monitor");

  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  NS_LOG_INFO ("Enabling link monitor");

  Ptr<LinkMonitor> linkMonitor = Create<LinkMonitor> ();
  for (int i = 0; i < SPINE_COUNT; i++)
    {
      std::stringstream name;
      name << "Spine " << i;
      Ptr<Ipv4LinkProbe> spineLinkProbe = Create<Ipv4LinkProbe> (spines.Get (i), linkMonitor);
      spineLinkProbe->SetProbeName (name.str ());
      spineLinkProbe->SetCheckTime (Seconds (0.01));
      spineLinkProbe->SetDataRateAll (DataRate (SPINE_LEAF_CAPACITY));
    }
  for (int i = 0; i < LEAF_COUNT; i++)
    {
      std::stringstream name;
      name << "Leaf " << i;
      Ptr<Ipv4LinkProbe> leafLinkProbe = Create<Ipv4LinkProbe> (leaves.Get (i), linkMonitor);
      leafLinkProbe->SetProbeName (name.str ());
      leafLinkProbe->SetCheckTime (Seconds (0.01));
      leafLinkProbe->SetDataRateAll (DataRate (SPINE_LEAF_CAPACITY));
    }

  linkMonitor->Start (Seconds (START_TIME));
  linkMonitor->Stop (Seconds (END_TIME));

  flowMonitor->CheckForLostPackets ();

  std::stringstream flowMonitorFilename;
  std::stringstream linkMonitorFilename;

  flowMonitorFilename << id << "-1-large-load-" << LEAF_COUNT << "X" << SPINE_COUNT << "-" << load << "-"  << transportProt <<"-" << aqmStr << "-" << TCNThreshold << "-" << CODELTarget << "-" << CODELInterval << "-";
  linkMonitorFilename << id << "-1-large-load-" << LEAF_COUNT << "X" << SPINE_COUNT << "-" << load << "-"  << transportProt <<"-" << aqmStr << "-" << TCNThreshold << "-" << CODELTarget << "-" << CODELInterval << "-";

  if (runMode == ECMP)
    {
      flowMonitorFilename << "ecmp-simulation-";
      linkMonitorFilename << "ecmp-simulation-";
    }

  flowMonitorFilename << randomSeed << "-";
  linkMonitorFilename << randomSeed << "-";

  flowMonitorFilename << "b" << BUFFER_SIZE << ".xml";
  linkMonitorFilename << "b" << BUFFER_SIZE << "-link-utility.out";

  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (END_TIME));
  Simulator::Run ();

  flowMonitor->SerializeToXmlFile(flowMonitorFilename.str (), true, true);
  linkMonitor->OutputToFile (linkMonitorFilename.str (), &LinkMonitor::DefaultFormat);

  Simulator::Destroy ();
  free_cdf (cdfTable);
  NS_LOG_INFO ("Stop simulation");
}
