#ifndef DELAY_QUEUE_DISC_H
#define DELAY_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include "ns3/nstime.h"
#include <list>

namespace ns3 {

  class DelayClass: public Object
  {
  public:
    static TypeId GetTypeId (void);

    DelayClass ();
    
    Ptr<QueueDisc> qdisc;
    Time delay;
  };

  class DelayQueueDisc: public QueueDisc
  {
  public:
    static TypeId GetTypeId (void);
    
    DelayQueueDisc ();
    virtual ~DelayQueueDisc ();

    void AddDelayClass (Ptr<QueueDisc> qdisc, int32_t cl);

  private:
    virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
    virtual Ptr<QueueDiscItem> DoDequeue (void);
    virtual Ptr<const QueueDiscItem> DoPeek (void) const;
    virtual bool CheckConfig (void);
    virtual void InitializeParams (void);

    std::map<uint32_t, Ptr<DelayClass> > m_delayClasses;
    std::map<uint32_t, EventId> m_events;
    Ptr<QueueDisc> outQueue;
  };

}

#endif
