#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/delay-queue-disc.h"

namespace ns3 {
  
  NS_LOG_COMPONENT_DEFINE ("DelayQueueDisc");
  NS_OBJECT_ENSURE_REGISTERED (DelayQueueDisc);

  TypeId
  DelayClass::GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::DelayClass")
      .SetParent<Object> ()
      .SetGroupName ("TrafficControl")
      .AddConstructor<DelayClass> ()
      ;

    return tid;
  }

  DelayClass::DelayClass ()
  {
    NS_LOG_FUNCTION (this);
  }


  TypeId
  DelayQueueDisc::GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::DelayQueueDisc")
      .SetParent<Object> ()
      .SetGroupName ("TrafficControl")
      .AddConstructor<DelayQueueDisc> ()
      ;
    return tid;
  }

  DelayQueueDisc::DelayQueueDisc ()
  {
    NS_LOG_FUNCTION (this);
  }

  DelayQueueDisc::~DelayQueueDisc ()
  {
    NS_LOG_FUNCTION (this);
    m_delayClasses.clear ();
  }

  void
  DelayQueueDisc::AddDelayClass (Ptr<QueueDisc> qdisc, int32_t cl, Time delay)
  {
    Ptr<DelayClass> delayClass = CreateObject<DelayClass> ();
    delayClass->qdisc = qdisc;
    delayClass->delay = delay;
    m_delayClasses[cl] = delayClass;
  }

  void
  DelayQueueDisc::AddOutQueue (Ptr<QueueDisc> qdisc)
  {
    m_outQueue = qdisc;
  }

  void
  DelayQueueDisc::FetchToOutQueue (Ptr<QueueDisc> fromQueueDisc)
  {
    Ptr<const QueueDiscItem> item = 0;
    item = fromQueueDisc->Dequeue ();

    if (item == 0)
      {
        NS_LOG_ERROR ("Cannot fetch from qdisc, which is impossible to happen!");
        return;
      }

    m_outQueue->Enqueue (ConstCast<QueueDiscItem> (item));
  }

  bool
  DelayQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
  {
    NS_LOG_FUNCTION (this << item);
    Ptr<DelayClass> delayClass = 0;

    int32_t cl = Classify (item);

    std::map<int32_t, Ptr<DelayClass> >::iterator itr = m_delayClasses.find (cl);
    if (itr == m_delayClasses.end ())
      {
        NS_LOG_ERROR ("Cannot find class, dropping the packet");
        Drop (item);
        return false;
      }

    delayClass = itr->second;
    Ptr<QueueDisc> qdisc = delayClass->qdisc;

    qdisc->Enqueue (item);
    Simulator::Schedule (delayClass->delay, &DelayQueueDisc::FetchToOutQueue, this, qdisc);

    return true; 
  }

  Ptr<QueueDiscItem>
  DelayQueueDisc::DoDequeue (void)
  {
    return m_outQueue->Dequeue ();
  }

  Ptr<const QueueDiscItem>
  DelayQueueDisc::DoPeek (void) const
  {
    return m_outQueue->Peek ();
  }

  bool
  DelayQueueDisc::CheckConfig (void)
  {
    NS_LOG_FUNCTION (this);
    return true;
  }

  void 
  DelayQueueDisc::InitializeParams (void)
  {
    NS_LOG_FUNCTION (this);
    m_outQueue->Initialize ();
    std::map<int32_t, Ptr<DelayClass> >::iterator itr = m_delayClasses.begin ();
    for (; itr != m_delayClasses.end (); ++itr)
      {
        itr->second->qdisc->Initialize ();
      }
  }


}
