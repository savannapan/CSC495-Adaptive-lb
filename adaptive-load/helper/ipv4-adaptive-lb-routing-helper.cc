/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ipv4-adaptive-lb-routing-helper.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Ipv4AdaptiveLBRoutingHelper");

Ipv4AdaptiveLBRoutingHelper::Ipv4AdaptiveLBRoutingHelper ()
{
  // Nothing to do here for now
}

Ipv4AdaptiveLBRoutingHelper::Ipv4AdaptiveLBRoutingHelper (const Ipv4AdaptiveLBRoutingHelper&)
{
  // Nothing to do here for now
}


Ipv4AdaptiveLBRoutingHelper*
Ipv4AdaptiveLBRoutingHelper::Copy (void) const
{
  return new Ipv4AdaptiveLBRoutingHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
Ipv4AdaptiveLBRoutingHelper::Create (Ptr<Node> node) const
{
  Ptr<Ipv4AdaptiveLBRouting> adaptiveLBRouting = CreateObject<Ipv4AdaptiveLBRouting> ();
  return adaptiveLBRouting;
}

Ptr<Ipv4AdaptiveLBRouting>
Ipv4AdaptiveLBRoutingHelper::GetAdaptiveLBRouting (Ptr<Ipv4> ipv4) const
{
  Ptr<Ipv4RoutingProtocol> ipv4rp = ipv4->GetRoutingProtocol ();
  if (DynamicCast<Ipv4AdaptiveLBRouting> (ipv4rp))
  {
    return DynamicCast<Ipv4AdaptiveLBRouting> (ipv4rp);
  }
  if (DynamicCast<Ipv4ListRouting> (ipv4rp))
  {
    Ptr<Ipv4ListRouting> lrp = DynamicCast<Ipv4ListRouting> (ipv4rp);
    int16_t priority;
    for (uint32_t i = 0; i < lrp->GetNRoutingProtocols ();  i++)
    {
      Ptr<Ipv4RoutingProtocol> temp = lrp->GetRoutingProtocol (i, priority);
      if (DynamicCast<Ipv4AdaptiveLBRouting> (temp))
      {
        return DynamicCast<Ipv4AdaptiveLBRouting> (temp);
      }
    }
  }

  return 0;
}

}  // namespace ns3