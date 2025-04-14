/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ipv4-adaptive-lb-routing.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/net-device.h"
#include "ns3/channel.h"
#include "ns3/node.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/point-to-point-net-device.h"

#include <algorithm>
#include <limits>
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Ipv4AdaptiveLBRouting");

NS_OBJECT_ENSURE_REGISTERED (Ipv4AdaptiveLBRouting);

TypeId
Ipv4AdaptiveLBRouting::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Ipv4AdaptiveLBRouting")
      .SetParent<Object> ()
      .SetGroupName ("AdaptiveLoadBalancer")
      .AddConstructor<Ipv4AdaptiveLBRouting> ()
      .AddAttribute ("LearningRate", "Learning rate (alpha) for EMA updates",
                     DoubleValue (0.2),
                     MakeDoubleAccessor (&Ipv4AdaptiveLBRouting::m_learningRate),
                     MakeDoubleChecker<double> (0.0, 1.0))
      .AddAttribute ("TargetUtilization", "Optimal link utilization target",
                     DoubleValue (0.7),
                     MakeDoubleAccessor (&Ipv4AdaptiveLBRouting::m_targetUtilization),
                     MakeDoubleChecker<double> (0.0, 1.0))
      .AddAttribute ("Temperature", "Temperature for softmax function",
                     DoubleValue (1.0),
                     MakeDoubleAccessor (&Ipv4AdaptiveLBRouting::m_temperature),
                     MakeDoubleChecker<double> (0.01))
      .AddAttribute ("UpdateInterval", "How often to update scores (ms)",
                     TimeValue (MilliSeconds (10)),
                     MakeTimeAccessor (&Ipv4AdaptiveLBRouting::m_updateInterval),
                     MakeTimeChecker ())
  ;

  return tid;
}

Ipv4AdaptiveLBRouting::Ipv4AdaptiveLBRouting ()
    : m_learningRate (0.2),
      m_targetUtilization (0.7),
      m_temperature (1.0),
      m_updateInterval (MilliSeconds (10)),
      m_nextFlowId (0)
{
  NS_LOG_FUNCTION (this);
  
  // Schedule periodic updates
  Simulator::Schedule (m_updateInterval, &Ipv4AdaptiveLBRouting::PeriodicScoreUpdate, this);
}

Ipv4AdaptiveLBRouting::~Ipv4AdaptiveLBRouting ()
{
  NS_LOG_FUNCTION (this);
}

void
Ipv4AdaptiveLBRouting::SetSimulationTime(double time)
{
  NS_LOG_FUNCTION (this << time);
  m_simulationTime = time;
  NS_LOG_INFO("Set simulation time to " << m_simulationTime << " seconds");
}

void
Ipv4AdaptiveLBRouting::AddRoute (Ipv4Address network, Ipv4Mask networkMask, uint32_t port)
{
  NS_LOG_LOGIC (this << " Add Adaptive LB routing entry: " << network << "/" << networkMask << " would go through port: " << port);
  AdaptiveLBRouteEntry adaptiveRouteEntry;
  adaptiveRouteEntry.network = network;
  adaptiveRouteEntry.networkMask = networkMask;
  adaptiveRouteEntry.port = port;
  m_routeEntryList.push_back (adaptiveRouteEntry);
  
  // Initialize score for this path with default values
  PathScore score;
  score.score = 0.0;
  score.utilization = 0.0;
  score.queueDepth = 0;
  score.lastUpdated = Simulator::Now ();
  score.port = port;
  
  // Add to score map for all known destinations
  // This will be updated as we learn about destinations
  for (std::vector<AdaptiveLBRouteEntry>::iterator it = m_routeEntryList.begin(); 
       it != m_routeEntryList.end(); ++it) {
    m_pathScores[it->network][port] = score;
  }
}

std::vector<AdaptiveLBRouteEntry>
Ipv4AdaptiveLBRouting::LookupAdaptiveLBRouteEntries (Ipv4Address dest)
{
  std::vector<AdaptiveLBRouteEntry> adaptiveRouteEntries;
  std::vector<AdaptiveLBRouteEntry>::iterator itr = m_routeEntryList.begin ();
  for ( ; itr != m_routeEntryList.end (); ++itr)
  {
    if((*itr).networkMask.IsMatch(dest, (*itr).network))
    {
      adaptiveRouteEntries.push_back (*itr);
    }
  }
  return adaptiveRouteEntries;
}

uint32_t
Ipv4AdaptiveLBRouting::CalculateQueueLength (uint32_t interface)
{
  Ptr<Ipv4L3Protocol> ipv4L3Protocol = DynamicCast<Ipv4L3Protocol> (m_ipv4);
  if (!ipv4L3Protocol)
  {
    NS_LOG_ERROR (this << " Adaptive LB routing cannot work other than Ipv4L3Protocol");
    return 0;
  }

  uint32_t totalLength = 0;

  const Ptr<NetDevice> netDevice = this->m_ipv4->GetNetDevice (interface);

  if (netDevice->IsPointToPoint ())
  {
    Ptr<PointToPointNetDevice> p2pNetDevice = DynamicCast<PointToPointNetDevice> (netDevice);
    if (p2pNetDevice)
    {
      totalLength += p2pNetDevice->GetQueue ()->GetNBytes ();
    }
  }

  Ptr<TrafficControlLayer> tc = ipv4L3Protocol->GetNode ()->GetObject<TrafficControlLayer> ();

  if (!tc)
  {
    return totalLength;
  }

  Ptr<QueueDisc> queueDisc = tc->GetRootQueueDiscOnDevice (netDevice);
  if (queueDisc)
  {
    totalLength += queueDisc->GetNBytes ();
  }

  return totalLength;
}

double
Ipv4AdaptiveLBRouting::CalculateUtilization (uint32_t interface)
{
  // In a real implementation, this would be based on actual throughput
  // For simplicity, we'll use queue depth as a proxy for utilization
  // A more sophisticated implementation would track bytes over time
  
  const Ptr<NetDevice> netDevice = this->m_ipv4->GetNetDevice (interface);
  if (!netDevice)
  {
    return 0.0;
  }
  
  uint32_t queueLength = CalculateQueueLength (interface);
  
  uint32_t capacity = 1000000; // Assume 1 MB/s as default reference
  
  // Try to get actual capacity from specific device types
  if (netDevice->IsPointToPoint())
  {
    Ptr<PointToPointNetDevice> p2pDev = DynamicCast<PointToPointNetDevice>(netDevice);
    if (p2pDev)
    {
      if (p2pDev->GetQueue())
      {
        capacity = 10000000; // 10 MB/s
      }
    }
  }
  
  // Use queue length relative to a notional "full buffer" as utilization proxy
  // In a real implementation, this would track actual throughput
  double utilization = static_cast<double>(queueLength) / (capacity * 0.1); // 100ms buffer as reference
  
  return std::min(1.0, utilization);
}

Ptr<Ipv4Route>
Ipv4AdaptiveLBRouting::ConstructIpv4Route (uint32_t port, Ipv4Address destAddress)
{
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (port);
  Ptr<Channel> channel = dev->GetChannel ();
  uint32_t otherEnd = (channel->GetDevice (0) == dev) ? 1 : 0;
  Ptr<Node> nextHop = channel->GetDevice (otherEnd)->GetNode ();
  uint32_t nextIf = channel->GetDevice (otherEnd)->GetIfIndex ();
  Ipv4Address nextHopAddr = nextHop->GetObject<Ipv4>()->GetAddress(nextIf,0).GetLocal();
  Ptr<Ipv4Route> route = Create<Ipv4Route> ();
  route->SetOutputDevice (m_ipv4->GetNetDevice (port));
  route->SetGateway (nextHopAddr);
  route->SetSource (m_ipv4->GetAddress (port, 0).GetLocal ());
  route->SetDestination (destAddress);
  return route;
}

double
Ipv4AdaptiveLBRouting::SoftmaxFunction (double value, double temperature)
{
  return std::exp(value / temperature);
}

uint32_t
Ipv4AdaptiveLBRouting::SelectPathUsingSoftmax (const std::vector<PathScore>& pathScores)
{
  if (pathScores.empty()) {
    NS_LOG_ERROR ("No paths available for softmax selection");
    return 0;
  }
  
  // Calculate softmax probabilities
  std::vector<double> probabilities;
  double sum = 0.0;
  
  for (std::vector<PathScore>::const_iterator it = pathScores.begin(); 
       it != pathScores.end(); ++it) {
    double probability = SoftmaxFunction(it->score, m_temperature);
    probabilities.push_back(probability);
    sum += probability;
  }
  
  // Normalize to get probabilities
  for (std::vector<double>::iterator it = probabilities.begin(); 
       it != probabilities.end(); ++it) {
    *it /= sum;
  }
  
  // Select path based on probabilities
  double r = ((double) rand() / (RAND_MAX));
  double cumulativeProbability = 0.0;
  
  for (size_t i = 0; i < probabilities.size(); i++) {
    cumulativeProbability += probabilities[i];
    if (r <= cumulativeProbability) {
      return pathScores[i].port;
    }
  }
  
  // Fallback to the last path if we get here (shouldn't happen)
  return pathScores.back().port;
}

double
Ipv4AdaptiveLBRouting::CalculateReward (uint32_t port, Time flowCompletionTime)
{
  double utilization = CalculateUtilization(port);
  uint32_t queueDepth = CalculateQueueLength(port);
  
  // Calculate reward based on several factors:
  // 1. Utilization relative to target (penalty for both under and over-utilization)
  // 2. Queue depth (lower is better)
  // 3. Flow completion time (lower is better)
  
  double utilizationScore = 1.0 - std::abs(utilization - m_targetUtilization);
  
  // Normalize queue depth - this would be tuned to the network
  double maxQueueReference = 1000000; // 1MB as reference
  double queueScore = 1.0 - std::min(1.0, queueDepth / maxQueueReference);
  
  // Flow completion time score - lower is better
  double maxFctReference = 0.1; // 100ms as reference
  double fctScore = 1.0 - std::min(1.0, flowCompletionTime.GetSeconds() / maxFctReference);
  
  // Weighted combination of scores
  // Weights would be tuned based on network priorities
  double reward = 0.4 * utilizationScore + 0.3 * queueScore + 0.3 * fctScore;
  
  return reward;
}

void
Ipv4AdaptiveLBRouting::UpdatePathScores (uint32_t port, Ipv4Address destAddress, Time flowCompletionTime)
{
  // Get the path score for this port and destination
  if (m_pathScores.find(destAddress) == m_pathScores.end() || 
      m_pathScores[destAddress].find(port) == m_pathScores[destAddress].end()) {
    NS_LOG_WARN ("Attempting to update score for unknown path");
    return;
  }
  
  // Calculate reward
  double reward = CalculateReward(port, flowCompletionTime);
  
  // Update score using Exponential Moving Average
  double oldScore = m_pathScores[destAddress][port].score;
  double newScore = (1 - m_learningRate) * oldScore + m_learningRate * reward;
  
  // Update other metrics
  m_pathScores[destAddress][port].score = newScore;
  m_pathScores[destAddress][port].utilization = CalculateUtilization(port);
  m_pathScores[destAddress][port].queueDepth = CalculateQueueLength(port);
  m_pathScores[destAddress][port].lastUpdated = Simulator::Now();
  
  NS_LOG_INFO ("Updated path score for port " << port << " to " << destAddress << 
               ": old=" << oldScore << ", new=" << newScore << 
               ", util=" << m_pathScores[destAddress][port].utilization);
}

void
Ipv4AdaptiveLBRouting::PeriodicScoreUpdate ()
{
  NS_LOG_FUNCTION (this);
  
  double currentTime = Simulator::Now().GetSeconds();
  std::cout << "PeriodicScoreUpdate at " << currentTime << "s (max: " << m_simulationTime << "s)" << std::endl;
  
  // Explicitly check if we should stop
  if (currentTime >= m_simulationTime) {
    std::cout << "Reached simulation end time, stopping periodic updates" << std::endl;
    return;  // Exit without scheduling another update
  }

  // Update path scores for all destinations and paths
  std::map<Ipv4Address, std::map<uint32_t, PathScore> >::iterator destIt;
  for (destIt = m_pathScores.begin(); destIt != m_pathScores.end(); ++destIt) {
    std::map<uint32_t, PathScore>::iterator pathIt;
    for (pathIt = destIt->second.begin(); pathIt != destIt->second.end(); ++pathIt) {
      uint32_t port = pathIt->second.port;
      pathIt->second.utilization = CalculateUtilization(port);
      pathIt->second.queueDepth = CalculateQueueLength(port);
      
      // Small continuous adjustment based on current conditions
      double utilizationDiff = std::abs(pathIt->second.utilization - m_targetUtilization);
      double utilizationPenalty = utilizationDiff * 0.1; // Small continuous adjustment
      
      // Small update using just the utilization metrics
      double newScore = pathIt->second.score - utilizationPenalty;
      pathIt->second.score = std::max(0.0, newScore); // Keep scores non-negative
      
      NS_LOG_DEBUG ("Periodic update for port " << port << ": " 
                   "util=" << pathIt->second.utilization << 
                   ", score=" << pathIt->second.score);
    }
  }
  
   // Schedule next update, but only if it won't exceed simulation time
   Time nextUpdateTime = m_updateInterval;
   if (currentTime + nextUpdateTime.GetSeconds() < m_simulationTime) {
     std::cout << "Scheduling next update at " << (currentTime + nextUpdateTime.GetSeconds()) << "s" << std::endl;
     Simulator::Schedule(nextUpdateTime, &Ipv4AdaptiveLBRouting::PeriodicScoreUpdate, this);
   } else {
     std::cout << "Next update would exceed simulation time, not scheduling" << std::endl;
   }
}


/* Inherit From Ipv4RoutingProtocol */
Ptr<Ipv4Route>
Ipv4AdaptiveLBRouting::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
  NS_LOG_ERROR (this << " Adaptive LB routing does not support local routing output");
  return 0;
}

bool
Ipv4AdaptiveLBRouting::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                            UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                            LocalDeliverCallback lcb, ErrorCallback ecb)
{
  NS_LOG_LOGIC (this << " RouteInput: " << p << " Ip header: " << header);

  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);

  Ptr<Packet> packet = ConstCast<Packet> (p);

  Ipv4Address destAddress = header.GetDestination();

  // Adaptive LB routing only supports unicast
  if (destAddress.IsMulticast() || destAddress.IsBroadcast()) {
    NS_LOG_ERROR (this << " Adaptive LB routing only supports unicast");
    ecb (packet, header, Socket::ERROR_NOROUTETOHOST);
    return false;
  }

  // Check if input device supports IP forwarding
  uint32_t iif = m_ipv4->GetInterfaceForDevice (idev);
  if (m_ipv4->IsForwarding (iif) == false) {
    NS_LOG_ERROR (this << " Forwarding disabled for this interface");
    ecb (packet, header, Socket::ERROR_NOROUTETOHOST);
    return false;
  }

  // Find all possible paths to destination
  std::vector<AdaptiveLBRouteEntry> allPorts = LookupAdaptiveLBRouteEntries (destAddress);

  if (allPorts.empty ())
  {
    NS_LOG_ERROR (this << " Adaptive LB routing cannot find routing entry");
    ecb (packet, header, Socket::ERROR_NOROUTETOHOST);
    return false;
  }

  // Create flow ID for this packet
  uint32_t flowId = m_nextFlowId++;
  
  // Collect path scores for available paths
  std::vector<PathScore> availablePathScores;
  for (std::vector<AdaptiveLBRouteEntry>::const_iterator it = allPorts.begin(); 
       it != allPorts.end(); ++it) {
    uint32_t port = it->port;
    
    // Initialize score for this path-destination if it doesn't exist
    if (m_pathScores.find(destAddress) == m_pathScores.end() || 
        m_pathScores[destAddress].find(port) == m_pathScores[destAddress].end()) {
      PathScore newScore;
      newScore.score = 0.0;
      newScore.utilization = CalculateUtilization(port);
      newScore.queueDepth = CalculateQueueLength(port);
      newScore.lastUpdated = Simulator::Now();
      newScore.port = port;
      m_pathScores[destAddress][port] = newScore;
    }
    
    availablePathScores.push_back(m_pathScores[destAddress][port]);
  }
  
  // Use softmax to select path probabilistically based on scores
  uint32_t selectedPort = SelectPathUsingSoftmax(availablePathScores);
  
  // Record flow start time for later performance evaluation
  FlowRecord record;
  record.flowId = flowId;
  record.startTime = Simulator::Now();
  record.port = selectedPort;
  record.bytesSent = p->GetSize();
  m_activeFlows[selectedPort].push(record);
  
  // Create route based on selected port
  Ptr<Ipv4Route> route = ConstructIpv4Route(selectedPort, destAddress);
  
  // When the packet is forwarded, it will trigger a flow completion event
  // That would update our path scores based on the actual performance
  
  NS_LOG_INFO ("Adaptive LB routing chooses interface: " << selectedPort << 
               " with score: " << m_pathScores[destAddress][selectedPort].score);
  
  // Forward packet
  ucb (route, packet, header);
  
  // Schedule a fake "flow completion" event
  // In a real implementation, we would track actual flow completion
  Time delay = Seconds(packet->GetSize() * 8.0 / 1e6); // Assume 1 Mbps as a reference rate
  
  Simulator::Schedule(delay, &Ipv4AdaptiveLBRouting::UpdatePathScores, 
                     this, selectedPort, destAddress, delay);

  return true;
}

void
Ipv4AdaptiveLBRouting::NotifyInterfaceUp (uint32_t interface)
{
  // Initialize any per-interface state here, nothing for now
}

void
Ipv4AdaptiveLBRouting::NotifyInterfaceDown (uint32_t interface)
{
  // Clean up any per-interface state here, nothing for now
}

void
Ipv4AdaptiveLBRouting::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  // Handle address addition if needed, nothing for now
}

void
Ipv4AdaptiveLBRouting::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  // Handle address removal if needed, nothing for now
}

void
Ipv4AdaptiveLBRouting::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_LOG_LOGIC (this << "Setting up Ipv4: " << ipv4);
  NS_ASSERT (m_ipv4 == 0 && ipv4 != 0);
  m_ipv4 = ipv4;
}

void
Ipv4AdaptiveLBRouting::PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const
{
  // Implement routing table printing functionality if needed, , nothing for now
}

void
Ipv4AdaptiveLBRouting::DoDispose (void)
{
  // Clean up any resources that need explicit disposal
  m_ipv4 = 0;
  Ipv4RoutingProtocol::DoDispose ();
}

}  // namespace ns3