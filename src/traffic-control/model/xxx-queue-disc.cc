#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/object-factory.h"
#include "xxx-queue-disc.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ipv4-queue-disc-item.h"

#define DEFAULT_XXX_LIMIT 100

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("XXXQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (XXXQueueDisc);

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
              UintegerValue (DEFAULT_XXX_LIMIT / 4),
              MakeUintegerAccessor (&XXXQueueDisc::m_instantMarkingThreshold),
              MakeUintegerChecker<uint32_t> ())
    ;
    return tid;
}

XXXQueueDisc::XXXQueueDisc ()
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

    GetInternalQueue (0)->Enqueue (item);

    return true;
}

Ptr<QueueDiscItem>
XXXQueueDisc::DoDequeue (void)
{
    NS_LOG_FUNCTION (this);

    bool instantaneousMarking = false;
    bool persistentMarking = false;

    if (GetInternalQueue (0)->IsEmpty ())
    {
        return NULL;
    }

    // First we check the instantaneous queue length
    if ((m_mode == Queue::QUEUE_MODE_PACKETS && GetInternalQueue (0)->GetNPackets () > m_instantMarkingThreshold) ||
        (m_mode == Queue::QUEUE_MODE_BYTES && GetInternalQueue (0)->GetNBytes () > m_instantMarkingThreshold))
    {
        instantaneousMarking = true;
    }

    Ptr<QueueDiscItem> item = StaticCast<QueueDiscItem> (GetInternalQueue (0)->Dequeue ());

    if (instantaneousMarking || persistentMarking)
    {
        if (!XXXQueueDisc::MarkingECN (item))
        {
            NS_LOG_ERROR ("Cannot mark ECN");
            return NULL;
        }
    }

    return item;
}

Ptr<const QueueDiscItem>
XXXQueueDisc::DoPeek (void) const
{
    NS_LOG_FUNCTION (this);

    return NULL;
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

}
