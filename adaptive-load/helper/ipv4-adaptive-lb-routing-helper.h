/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef IPV4_ADAPTIVE_LB_ROUTING_HELPER_H
#define IPV4_ADAPTIVE_LB_ROUTING_HELPER_H

#include "ns3/ipv4-adaptive-lb-routing.h"
#include "ns3/ipv4-routing-helper.h"

namespace ns3 {

/**
 * \brief Helper class that adds the Adaptive Load Balancer routing protocol to IPv4 nodes
 */
class Ipv4AdaptiveLBRoutingHelper : public Ipv4RoutingHelper
{
public:
    /**
     * \brief Constructor
     */
    Ipv4AdaptiveLBRoutingHelper ();
    
    /**
     * \brief Copy constructor
     */
    Ipv4AdaptiveLBRoutingHelper (const Ipv4AdaptiveLBRoutingHelper&);

    /**
     * \brief Create a copy of this helper
     */
    Ipv4AdaptiveLBRoutingHelper *Copy (void) const;

    /**
     * \brief Create and return an Ipv4AdaptiveLBRouting routing protocol object.
     *
     * \param node The node for which this routing protocol should be created
     * \returns Newly created routing protocol object
     */
    virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;

    /**
     * \brief Get the Adaptive Load Balancer routing protocol from an existing IPv4 stack
     *
     * \param ipv4 IPv4 stack for which to get the routing protocol
     * \returns Pointer to the Adaptive LB routing protocol
     */
    Ptr<Ipv4AdaptiveLBRouting> GetAdaptiveLBRouting (Ptr<Ipv4> ipv4) const;
};

}

#endif /* IPV4_ADAPTIVE_LB_ROUTING_HELPER_H */