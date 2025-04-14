/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef IPV4_ADAPTIVE_LB_ROUTING_H
#define IPV4_ADAPTIVE_LB_ROUTING_H

#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-route.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include <vector>
#include <map>
#include <queue>

namespace ns3 {

struct AdaptiveLBRouteEntry {
    Ipv4Address network;
    Ipv4Mask networkMask;
    uint32_t port;
};

struct PathScore {
    double score;
    double utilization;
    uint32_t queueDepth;
    Time lastUpdated;
    uint32_t port;
};

struct FlowRecord {
    uint32_t flowId;
    Time startTime;
    Time endTime;
    uint32_t bytesSent;
    uint32_t port;
};

class Ipv4AdaptiveLBRouting : public Ipv4RoutingProtocol {
public:
    Ipv4AdaptiveLBRouting ();
    ~Ipv4AdaptiveLBRouting ();

    static TypeId GetTypeId (void);

    void AddRoute (Ipv4Address network, Ipv4Mask networkMask, uint32_t port);
    std::vector<AdaptiveLBRouteEntry> LookupAdaptiveLBRouteEntries (Ipv4Address dest);

    // New method to set simulation time
    void SetSimulationTime(double time);

    // Path metrics
    uint32_t CalculateQueueLength (uint32_t interface);
    double CalculateUtilization (uint32_t interface);
    Ptr<Ipv4Route> ConstructIpv4Route (uint32_t port, Ipv4Address destAddress);

    // Path scoring and selection
    uint32_t SelectPathUsingSoftmax (const std::vector<PathScore>& pathScores);
    void UpdatePathScores (uint32_t port, Ipv4Address destAddress, Time flowCompletionTime);
    double CalculateReward (uint32_t port, Time flowCompletionTime);

    /* Inherit From Ipv4RoutingProtocol */
    virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
    virtual bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                             UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                             LocalDeliverCallback lcb, ErrorCallback ecb);
    virtual void NotifyInterfaceUp (uint32_t interface);
    virtual void NotifyInterfaceDown (uint32_t interface);
    virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
    virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
    virtual void SetIpv4 (Ptr<Ipv4> ipv4);
    virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const;
    virtual void DoDispose (void);

private:
    // Tunable parameters
    double m_learningRate;     // Alpha for EMA updates
    double m_targetUtilization; // Optimal utilization target
    double m_temperature;       // Temperature for softmax function
    Time m_updateInterval;      // How often to update scores
    double m_simulationTime;    // Total simulation time

    // State information
    std::map<Ipv4Address, std::map<uint32_t, PathScore> > m_pathScores; // Scores for each destination and path
    std::map<uint32_t, std::queue<FlowRecord> > m_activeFlows; // Tracks active flows on each path
    std::map<uint32_t, Time> m_lastUpdateTime; // Last update time for each path
    uint32_t m_nextFlowId; // Counter for generating flow IDs

    // NS-3 related
    Ptr<Ipv4> m_ipv4;
    std::vector<AdaptiveLBRouteEntry> m_routeEntryList;

    // Helper methods
    double SoftmaxFunction(double value, double temperature);
    void PeriodicScoreUpdate();
};

} // namespace ns3

#endif /* IPV4_ADAPTIVE_LB_ROUTING_H */