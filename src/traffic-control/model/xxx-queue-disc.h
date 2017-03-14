#ifndef XXX_QUEUE_DISC_H
#define XXX_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"

namespace ns3 {

class XXXTimestampTag : public Tag
{
public:
    XXXTimestampTag ();

    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;

    virtual uint32_t GetSerializedSize (void) const;
    virtual void Serialize (TagBuffer i) const;
    virtual void Deserialize (TagBuffer i);
    virtual void Print (std::ostream &os) const;

    Time GetTxTime (void) const;
private:
    uint64_t m_creationTime;
};

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

    /**
     * Whether the persistent marking should work
     * @param p the packet to judge
     * @param now the current time
     * @return true if it should be marked
     */
    bool OkToMark (Ptr<Packet> p, Time now);

    Time ControlLaw (void);

    uint32_t m_maxPackets;                  //!< Max # of packets accepted by the queue
    uint32_t m_maxBytes;                    //!< Max # of bytes accepted by the queue
    Queue::QueueMode     m_mode;            //!< The operating mode (Bytes or packets)

    uint32_t m_instantMarkingThreshold;     //!< The instantaneous marking threshold

    Time m_persistentMarkingInterval;       //!< The time interval used in persistent marking
    Time m_persistentMarkingTarget;         //!< The time target used in persistent marking

    Time m_firstAboveTime;                  //!< The time to be above the target for the first time

    bool m_marking;                         //!< XXX has been in the marking state
    Time m_markNext;                        //!< The scheduled next drop time
    uint32_t m_markCount;
};

}

#endif
