#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/object-factory.h"
#include "xxx-queue-disc.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/string.h"

#define DEFAULT_XXX_LIMIT 100

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("XXXQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (XXXQueueDisc);

XXXTimestampTag::XXXTimestampTag ()
    : m_creationTime (Simulator::Now ().GetTimeStep ())
{

}

TypeId
XXXTimestampTag::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::XXXTimestampTag")
        .SetParent<Tag> ()
        .AddConstructor<XXXTimestampTag> ()
    ;
    return tid;
}

TypeId
XXXTimestampTag::GetInstanceTypeId (void) const
{
    return GetTypeId ();
}

uint32_t
XXXTimestampTag::GetSerializedSize (void) const
{
    return sizeof (uint64_t);
}

void
XXXTimestampTag::Serialize (TagBuffer i) const
{
    i.WriteU64 (m_creationTime);
}

void
XXXTimestampTag::Deserialize (TagBuffer i)
{
    m_creationTime = i.ReadU64 ();
}

void
XXXTimestampTag::Print (std::ostream &os) const
{
    os << "CreationTime=" << m_creationTime;
}

Time
XXXTimestampTag::GetTxTime (void) const
{
    return TimeStep (m_creationTime);
}

TypeId
XXXQueueDisc::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::XXXQueueDisc")
      .SetParent<Object> ()
      .SetGroupName ("TrafficControl")
      .AddConstructor<XXXQueueDisc> ()
      .AddAttribute ("Mode", "Whether to use Bytes (see MaxBytes) or Packets (see MaxPackets) as the maximum queue size metric.",
              EnumValue (Queue::QUEUE_MODE_BYTES),
              MakeEnumAccessor (&XXXQueueDisc::m_mode),
              MakeEnumChecker (Queue::QUEUE_MODE_BYTES, "QUEUE_MODE_BYTES",
                               Queue::QUEUE_MODE_PACKETS, "QUEUE_MODE_PACKETS"))
      .AddAttribute ("MaxPackets", "The maximum number of packets accepted by this XXXQueueDisc.",
              UintegerValue (DEFAULT_XXX_LIMIT),
              MakeUintegerAccessor (&XXXQueueDisc::m_maxPackets),
              MakeUintegerChecker<uint32_t> ())
      .AddAttribute ("MaxBytes", "The maximum number of bytes accepted by this XXXQueueDisc.",
              UintegerValue (1500 * DEFAULT_XXX_LIMIT),
              MakeUintegerAccessor (&XXXQueueDisc::m_maxBytes),
              MakeUintegerChecker<uint32_t> ())
      .AddAttribute ("InstantaneousMarkingThreshold", "The marking threshold for instantaneous queue length",
              StringValue ("20us"),
              MakeTimeAccessor (&XXXQueueDisc::m_instantMarkingThreshold),
              MakeTimeChecker ())
      .AddAttribute ("PersistentMarkingInterval", "The persistent marking interval",
              StringValue ("100us"),
              MakeTimeAccessor (&XXXQueueDisc::m_persistentMarkingInterval),
              MakeTimeChecker ())
      .AddAttribute ("PersistentMarkingTarget", "The persistent marking threshold to control queue delay",
              StringValue ("10us"),
              MakeTimeAccessor (&XXXQueueDisc::m_persistentMarkingTarget),
              MakeTimeChecker ())
    ;
    return tid;
}

XXXQueueDisc::XXXQueueDisc ()
    : m_firstAboveTime (0),
      m_marking (false),
      m_markNext (0),
      m_markCount (0)
{
    NS_LOG_FUNCTION (this);
}

XXXQueueDisc::~XXXQueueDisc ()
{
    NS_LOG_FUNCTION (this);
}

bool
XXXQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION (this << item);

    Ptr<Packet> p = item->GetPacket ();
    if (m_mode == Queue::QUEUE_MODE_PACKETS && (GetInternalQueue (0)->GetNPackets () + 1 > m_maxPackets))
    {
        Drop (item);
        return false;
    }

    if (m_mode == Queue::QUEUE_MODE_BYTES && (GetInternalQueue (0)->GetNBytes () + item->GetPacketSize () > m_maxBytes))
    {
        Drop (item);
        return false;
    }

    //TODO Add Time Tag
    XXXTimestampTag tag;
    p->AddPacketTag (tag);

    GetInternalQueue (0)->Enqueue (item);

    return true;
}

Ptr<QueueDiscItem>
XXXQueueDisc::DoDequeue (void)
{
    NS_LOG_FUNCTION (this);

    Time now = Simulator::Now ();

    bool instantaneousMarking = false;
    bool persistentMarking = false;

    if (GetInternalQueue (0)->IsEmpty ())
    {
        return NULL;
    }


    Ptr<QueueDiscItem> item = StaticCast<QueueDiscItem> (GetInternalQueue (0)->Dequeue ());
    Ptr<Packet> p = item->GetPacket ();

    XXXTimestampTag tag;
    bool found = p->RemovePacketTag (tag);
    if (!found)
    {
        NS_LOG_ERROR ("Cannot find the XXX Timestamp Tag");
        return NULL;
    }

    Time sojournTime = now - tag.GetTxTime ();

     // First we check the instantaneous queue length
    if (sojournTime > m_instantMarkingThreshold)
    {
        instantaneousMarking = true;
    }

    //Second we check the persistent marking
    bool okToMark = OkToMark (p, sojournTime, now);
    if (m_marking)
    {
        if (!okToMark)
        {
            m_marking = false;
        }
        else if (now >= m_markNext)
        {
            m_markCount ++;
            m_markNext = now + XXXQueueDisc::ControlLaw ();
            persistentMarking = true;
        }
    }
    else
    {
        if (okToMark)
        {
            m_marking = true;
            m_markCount = 1;
            m_markNext = now + m_persistentMarkingInterval;
            persistentMarking = true;
        }

    }

    if (instantaneousMarking || persistentMarking)
    {
        if (!XXXQueueDisc::MarkingECN (item))
        {
            NS_LOG_ERROR ("Cannot mark ECN");
            // return NULL;
            return item; // Hey buddy, if the packet is not ECN supported, we should never drop it
        }
    }

    return item;
}

Ptr<const QueueDiscItem>
XXXQueueDisc::DoPeek (void) const
{
    NS_LOG_FUNCTION (this);
    if (GetInternalQueue (0)->IsEmpty ())
    {
        return NULL;
    }

    Ptr<const QueueDiscItem> item = StaticCast<const QueueDiscItem> (GetInternalQueue (0)->Peek ());

    return item;
}

bool
XXXQueueDisc::CheckConfig (void)
{
    if (GetNInternalQueues () == 0)
    {
        Ptr<Queue> queue = CreateObjectWithAttributes<DropTailQueue> ("Mode", EnumValue (m_mode));
        if (m_mode == Queue::QUEUE_MODE_PACKETS)
        {
            queue->SetMaxPackets (m_maxPackets);
        }
        else
        {
            queue->SetMaxBytes (m_maxBytes);
        }
        AddInternalQueue (queue);
    }

    if (GetNInternalQueues () != 1)
    {
        NS_LOG_ERROR ("XXXQueueDisc needs 1 internal queue");
        return false;
    }

    return true;
}


void
XXXQueueDisc::InitializeParams (void)
{
    NS_LOG_FUNCTION (this);
}

bool
XXXQueueDisc::MarkingECN (Ptr<QueueDiscItem> item)
{
    Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem> (item);
    if (ipv4Item == 0)   {
        NS_LOG_ERROR ("Cannot convert the queue disc item to ipv4 queue disc item");
        return false;
    }

    Ipv4Header header = ipv4Item -> GetHeader ();

    if (header.GetEcn () != Ipv4Header::ECN_ECT1)   {
        NS_LOG_ERROR ("Cannot mark because the ECN field is not ECN_ECT1");
        return false;
    }

    header.SetEcn(Ipv4Header::ECN_CE);
    ipv4Item->SetHeader(header);
    return true;
}

bool
XXXQueueDisc::OkToMark (Ptr<Packet> p, Time sojournTime, Time now)
{
    if (sojournTime < m_persistentMarkingTarget)
    {
        m_firstAboveTime = Time (0);
        return false;
    }
    else
    {
        if (m_firstAboveTime == Time (0))
        {
            m_firstAboveTime = now + m_persistentMarkingInterval;
        }
        else if (now > m_firstAboveTime)
        {
            return true;
        }
        return false;
    }
}

Time
XXXQueueDisc::ControlLaw (void)
{
    uint64_t timeStep = m_persistentMarkingInterval.GetTimeStep ();
    timeStep = static_cast<uint64_t> (timeStep / sqrt (static_cast<double> (m_markCount)));
    return TimeStep (timeStep);
}

}
