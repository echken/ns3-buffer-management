#ifndef XXX_QUEUE_DISC_H
#define XXX_QUEUE_DISC_H

#include "ns3/queue-disc.h"

namespace ns3 {

class XXXQueueDisc : public QueueDisc
{
public:

    static TypeId GetTypeId (void);

    XXXQueueDisc ();

    virtual ~XXXQueueDisc ();

private:
    // Operations offered by multi queue disc should be the same as queue disc
    virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
    virtual Ptr<QueueDiscItem> DoDequeue (void);
    virtual Ptr<const QueueDiscItem> DoPeek (void) const;
    virtual bool CheckConfig (void);
    virtual void InitializeParams (void);

    /**
     * Add ECN marking to the queue disc item
     * @param item the item to mark
     * @return true if it is successfully marked
     */
    bool MarkingECN (Ptr<QueueDiscItem> item);

    uint32_t m_maxPackets;                  //!< Max # of packets accepted by the queue
    uint32_t m_maxBytes;                    //!< Max # of bytes accepted by the queue
    Queue::QueueMode     m_mode;            //!< The operating mode (Bytes or packets)

    uint32_t m_instantMarkingThreshold;     //!< The instantaneous marking threshold
};

}

#endif
