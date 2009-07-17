/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 IITP RAS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Based on 
 *      NS-2 AODV model developed by the CMU/MONARCH group and optimized and
 *      tuned by Samir Das and Mahesh Marina, University of Cincinnati;
 * 
 *      AODV-UU implementation by Erik Nordström of Uppsala University
 *      http://core.it.uu.se/core/index.php/AODV-UU
 *
 * Authors: Elena Borovkova <borovkovaes@iitp.ru>
 *          Pavel Boyko <boyko@iitp.ru>
 */
#include "aodv-rtable.h"
#include <algorithm>
#include "ns3/test.h"

namespace ns3 {
namespace aodv {

/*
  The Routing Table
 */


RoutingTableEntry::RoutingTableEntry(Ptr<NetDevice> dev, Ipv4Address dst, bool vSeqNo, u_int32_t seqNo, Ipv4Address iface, u_int16_t  hops,
                              Ipv4Address nextHop, Time lifetime)
                            : m_validSeqNo(vSeqNo), m_seqNo(seqNo), m_hops(hops), m_lastHopCount(hops), m_lifeTime(lifetime), m_reqCount(0)
{
  m_ipv4Route = Create<Ipv4Route> ();
  m_ipv4Route->SetDestination(dst);
  m_ipv4Route->SetGateway(nextHop);
  m_ipv4Route->SetSource(iface);
  m_ipv4Route->SetOutputDevice(dev);
  m_flag = RTF_UP;
  for(uint8_t i = 0; i < MAX_HISTORY; ++i)
    {
     rt_disc_latency[i] = 0;
    }
  hist_indx = 0;
}

RoutingTableEntry::~RoutingTableEntry()
{
}

bool
RoutingTableEntry::InsertPrecursor(Ipv4Address id)
{
  if (! LookupPrecursor(id))
    {
      m_precursorList.push_back(id);
      return true;
    }
  else return false;
}

bool
RoutingTableEntry::LookupPrecursor(Ipv4Address id)
{
  for(std::vector<Ipv4Address>::const_iterator i = m_precursorList.begin(); i != m_precursorList.end(); ++i)
    if (*i == id) return true;
  return false;
}

bool
RoutingTableEntry::DeletePrecursor(Ipv4Address id)
{
  std::vector<Ipv4Address>::iterator i = std::remove(m_precursorList.begin(), m_precursorList.end(), id);
  if(i == m_precursorList.end())
    return false;
  else
    m_precursorList.erase (i, m_precursorList.end());
  return true;
}

void
RoutingTableEntry::DeleteAllPrecursors()
{
  m_precursorList.clear();
}

bool
RoutingTableEntry::IsPrecursorListEmpty() const
{
  return m_precursorList.empty();
}

void
RoutingTableEntry::Down ()
{
  if(m_flag == RTF_DOWN) return;

  m_lastHopCount = m_hops;
  m_hops = INFINITY2;
  m_flag = RTF_DOWN;
  m_lifeTime = Seconds (DELETE_PERIOD);
}

void
RoutingTableEntry::Print(std::ostream & os) const
{
  os << m_ipv4Route->GetDestination() << "\t" << m_ipv4Route->GetGateway() << "\t"
     << m_ipv4Route->GetSource() << "\t" << m_lifeTime << "\n";
}

/*
  The Routing Table
 */

bool
RoutingTable::LookupRoute(Ipv4Address id, RoutingTableEntry & rt) const
{
  std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = m_ipv4AddressEntry.find(id);
  if (i == m_ipv4AddressEntry.end()) return false;
  rt = (*i).second;
  return true;
}

bool
RoutingTable::DeleteRoute(Ipv4Address dst)
{
  if(m_ipv4AddressEntry.erase (dst) != 0) return true;
  return false;
}

bool
RoutingTable::AddRoute(RoutingTableEntry const & rt)
{
  Ptr<NetDevice> dev;
  RoutingTableEntry dummy(dev);
  if(LookupRoute(rt.GetDestination(), dummy))
  	return false;
  m_ipv4AddressEntry.insert(std::make_pair(rt.GetDestination(), rt));
  return true;
}

bool
RoutingTable::Update(Ipv4Address dst, RoutingTableEntry & rt)
{
  std::map<Ipv4Address, RoutingTableEntry>::iterator i = m_ipv4AddressEntry.find(dst);
  if (i == m_ipv4AddressEntry.end()) return false;
  i->second = rt;
  return true;
}

void
RoutingTable::SetEntryState (Ipv4Address id, uint8_t state)
{
  std::map<Ipv4Address, RoutingTableEntry>::iterator i = m_ipv4AddressEntry.find(id);
  if (i != m_ipv4AddressEntry.end()) (*i).second.SetFlag(state);
}

void
RoutingTable::Print(std::ostream &os) const
{
  os << "\nAODV Routing table\n"
     << "Destination\tGateway\t\tInterface\tExpire\n";
  for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = m_ipv4AddressEntry.begin(); i != m_ipv4AddressEntry.end(); ++i)
    {
      i->second.Print(os);
    }
  os << "\n";

}

#if 0

#ifdef RUN_SELF_TESTS
/// Unit test for aodv_rqueue
struct AodvRtableTest : public Test
{
  AodvRtableTest () : Test ("AODV/Rtable"), result(true) {}
  virtual bool RunTests();
  bool result;
};

/// Test instance
static AodvRtableTest g_AodvRtableTest;

bool
AodvRtableTest::RunTests ()
{
  RoutingTableEntry entry1;
  Ipv4Address dst1("3.3.3.3");
  Ipv4Address dst2("1.2.3.4");
  RoutingTableEntry entry2 = RoutingTableEntry(dst2, true, 34, Ipv4Address("2.3.4.5"), 1, Ipv4Address("5.5.5.5"),  Seconds(5));
  NS_TEST_ASSERT( !(entry1 == dst1) );
  NS_TEST_ASSERT(entry2 == dst2);
  
  entry2.InsertPrecursor(dst1);
  NS_TEST_ASSERT(entry2.LookupPrecursor(dst1));
  NS_TEST_ASSERT(!entry2.DeletePrecursor(dst2));
  NS_TEST_ASSERT(entry2.DeletePrecursor(dst1));
  NS_TEST_ASSERT(entry2.pc_empty());
  entry2.InsertPrecursor(dst2);
  NS_TEST_ASSERT(entry2.InsertPrecursor(dst1));
  NS_TEST_ASSERT(!entry2.InsertPrecursor(dst2));
  entry2.DeleteAllPrecursors();
  NS_TEST_ASSERT(entry2.pc_empty());
  
  RoutingTable rtable;
  rtable.AddRoute(entry2);
  rtable.Print(std::cout);
  entry2.SetLifeTime(Seconds(10));
  NS_TEST_ASSERT(rtable.Update(entry2.GetDst(), entry2));
  NS_TEST_ASSERT(rtable.LookupRoute(dst2, entry1));
  NS_TEST_ASSERT(entry1.GetLifeTime() == entry2.GetLifeTime());
  NS_TEST_ASSERT(entry1 == dst2);
  NS_TEST_ASSERT(!rtable.LookupRoute(dst1, entry1));
  NS_TEST_ASSERT(rtable.DeleteRoute(dst2));
  NS_TEST_ASSERT(!rtable.DeleteRoute(dst2));
  NS_TEST_ASSERT(!rtable.LookupRoute(dst2, entry1));
  
  return result;
}
#endif

#endif

}}