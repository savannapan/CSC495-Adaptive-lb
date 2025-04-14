/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-adaptive-lb-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdaptiveLBRoutingExample");

/**
 * This example demonstrates the Adaptive Load Balancer routing protocol
 * in a simple topology with multiple paths. The example:
 * 
 * 1. Creates a dumbbell topology with multiple parallel links.
 * 2. Installs the Adaptive Load Balancer routing protocol.
 * 3. Generates traffic with different patterns.
 * 4. Measures performance metrics including flow completion time, 
 *    link utilization, and path selection distribution.
 */
int
main (int argc, char *argv[])
{
  std::cout << "Starting adaptive-lb example..." << std::endl;
  // Enable logging for the Adaptive Load Balancer
  LogComponentEnable ("Ipv4AdaptiveLBRouting", LOG_LEVEL_INFO);
  
  // Configure command line parameters
  bool verbose = true;
  uint32_t nPaths = 4;       // Number of parallel paths
  double simTime = 30.0;     // Seconds
  double learningRate = 0.2; // Alpha parameter
  double temperature = 1.0;  // Softmax temperature
  
  CommandLine cmd;
  cmd.AddValue ("verbose", "Enable verbose output", verbose);
  cmd.AddValue ("nPaths", "Number of parallel paths", nPaths);
  cmd.AddValue ("simTime", "Simulation time (seconds)", simTime);
  cmd.AddValue ("learningRate", "Learning rate (alpha)", learningRate);
  cmd.AddValue ("temperature", "Softmax temperature", temperature);
  cmd.Parse (argc, argv);
  
  std::cout << "Command line parameters parsed" << std::endl;

  if (verbose) {
    LogComponentEnable ("AdaptiveLBRoutingExample", LOG_LEVEL_INFO);
  }
  
  NS_LOG_INFO ("Creating topology with " << nPaths << " parallel paths");
  
  // Create nodes
  NodeContainer leftNodes, rightNodes;
  NodeContainer routers;
  
  leftNodes.Create (1);  // Source node
  rightNodes.Create (1); // Destination node
  routers.Create (2);    // Left and right routers
  
  // Create the parallel point-to-point links
  NodeContainer parallelLinks[nPaths];
  NetDeviceContainer parallelDevices[nPaths];
  
  // Different link capacities and latencies to create asymmetry
  std::vector<DataRate> linkRates;
  std::vector<Time> linkDelays;
  
  // Configure different link characteristics
  for (uint32_t i = 0; i < nPaths; i++) {
    // Vary link rates from 5Mbps to 20Mbps
    std::ostringstream rateStr;
    rateStr << (5 + (i * 5)) << "Mbps";
    linkRates.push_back (DataRate (rateStr.str()));
    
    // Vary link delays from 2ms to 20ms
    linkDelays.push_back (MilliSeconds (2 + i * 6));
  }
  
  // Create point-to-point helper
  PointToPointHelper p2p;
  
  // Create the left link (source to router)
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));
  
  NetDeviceContainer leftLink = p2p.Install (leftNodes.Get (0), routers.Get (0));
  
  // Create the right link (router to destination)
  NetDeviceContainer rightLink = p2p.Install (routers.Get (1), rightNodes.Get (0));
  
  // Create the parallel links between routers
  for (uint32_t i = 0; i < nPaths; i++) {
    // Configure this path's characteristics
    p2p.SetDeviceAttribute ("DataRate", DataRateValue (linkRates[i]));
    p2p.SetChannelAttribute ("Delay", TimeValue (linkDelays[i]));
    
    // Connect the routers with this path
    parallelLinks[i] = NodeContainer (routers.Get (0), routers.Get (1));
    parallelDevices[i] = p2p.Install (parallelLinks[i]);
    
    NS_LOG_INFO ("Path " << i << ": Rate=" << linkRates[i] << 
                 ", Delay=" << linkDelays[i]);
  }
  
  // Install internet stack
  InternetStackHelper internet;
  
  // Create and configure the Adaptive Load Balancer helper
  Ipv4AdaptiveLBRoutingHelper adaptiveLB;
  
  // Create list routing to combine adaptive LB with static routing
  Ipv4ListRoutingHelper listRouting;
  Ipv4StaticRoutingHelper staticRouting;
  
  // Add static routing first (lower priority)
  listRouting.Add (staticRouting, 0);
  
  // Add adaptive load balancing with higher priority
  listRouting.Add (adaptiveLB, 10);
  
  // Set the routing helper for all nodes
  internet.SetRoutingHelper (listRouting);
  
  // Install the internet stack on all nodes
  internet.Install (leftNodes);
  internet.Install (rightNodes);
  internet.Install (routers);
  
  // Assign IPv4 addresses
  Ipv4AddressHelper ipv4;
  
  // Left link
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer leftInterfaces = ipv4.Assign (leftLink);
  
  // Right link
  ipv4.SetBase ("10.1.10.0", "255.255.255.0");
  Ipv4InterfaceContainer rightInterfaces = ipv4.Assign (rightLink);
  
  // Parallel links
  Ipv4InterfaceContainer routerInterfaces[nPaths];
  for (uint32_t i = 0; i < nPaths; i++) {
    std::ostringstream subnet;
    subnet << "10.1." << (i + 2) << ".0";
    ipv4.SetBase (subnet.str().c_str(), "255.255.255.0");
    routerInterfaces[i] = ipv4.Assign (parallelDevices[i]);
  }
  
  // Get pointers to our routing objects
  Ptr<Ipv4> ipv4Left = routers.Get (0)->GetObject<Ipv4> ();
  Ptr<Ipv4AdaptiveLBRouting> adaptiveRouting = adaptiveLB.GetAdaptiveLBRouting (ipv4Left);
  
  std::cout << "Got routing object: " << (adaptiveRouting ? "yes" : "no") << std::endl;

  if (adaptiveRouting) {
    // Set the simulation time
    adaptiveRouting->SetSimulationTime(simTime);
    
    // Set the Adaptive LB parameters from command line
    adaptiveRouting->SetAttribute ("LearningRate", DoubleValue (learningRate));
    adaptiveRouting->SetAttribute ("Temperature", DoubleValue (temperature));
    adaptiveRouting->SetAttribute ("TargetUtilization", DoubleValue (0.7));
    adaptiveRouting->SetAttribute ("UpdateInterval", TimeValue (MilliSeconds (10)));
    
    std::cout << "Attributes set successfully" << std::endl;
  } else {
    std::cout << "ERROR: Failed to get Adaptive LB routing object" << std::endl;
    return 1;
  }

  // Add routes for adaptive load balancing
  // For each parallel path, add a route from the left router to the destination
  for (uint32_t i = 0; i < nPaths; i++) {
    adaptiveRouting->AddRoute (
      rightInterfaces.GetAddress (1),       // Destination IP (right node)
      Ipv4Mask ("255.255.255.255"),         // Destination mask (single host)
      i + 1                                 // Interface index (parallel link)
    );
  }
  
  // Set up static routes for the source and destination nodes
  Ptr<Ipv4StaticRouting> staticRoutingLeft = 
    staticRouting.GetStaticRouting (leftNodes.Get (0)->GetObject<Ipv4> ());
  
  Ptr<Ipv4StaticRouting> staticRoutingRight = 
    staticRouting.GetStaticRouting (rightNodes.Get (0)->GetObject<Ipv4> ());
  
  // Add default routes
  staticRoutingLeft->SetDefaultRoute (leftInterfaces.GetAddress (1), 1);
  staticRoutingRight->SetDefaultRoute (rightInterfaces.GetAddress (0), 1);
  
  // Create applications to generate traffic
  
  // Create a simple UDP echo application
  uint16_t port = 9;
  
  // Create an on/off application to send UDP packets
  OnOffHelper onoff ("ns3::UdpSocketFactory", 
                     Address (InetSocketAddress (rightInterfaces.GetAddress (1), port)));
  onoff.SetConstantRate (DataRate ("5Mbps"), 1000);
  
  // Add some jitter to start time to avoid perfect synchronization
  Ptr<UniformRandomVariable> startTimeRng = CreateObject<UniformRandomVariable> ();
  startTimeRng->SetAttribute ("Min", DoubleValue (0.0));
  startTimeRng->SetAttribute ("Max", DoubleValue (0.1));
  
  // Create multiple flows with different start times
  uint32_t nFlows = 20;
  ApplicationContainer apps;
  
  for (uint32_t i = 0; i < nFlows; i++) {
    double startTime = 1.0 + startTimeRng->GetValue();
    double stopTime = simTime - 1.0;
    
    // Every third flow is large (elephant flow)
    if (i % 3 == 0) {
      onoff.SetConstantRate (DataRate ("8Mbps"), 1500);
    } else {
      // Others are smaller (mice flows)
      onoff.SetConstantRate (DataRate ("2Mbps"), 500);
    }
    
    ApplicationContainer flow = onoff.Install (leftNodes.Get (0));
    flow.Start (Seconds (startTime));
    flow.Stop (Seconds (stopTime));
    
    apps.Add (flow);
  }
  
  // Create a packet sink on the right node
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                        Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  apps.Add (sink.Install (rightNodes.Get (0)));
  
  // Enable flow monitoring for statistics
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  
  // Configure trace output
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("adaptive-lb-routing-example.tr"));
  
  // Run simulation
  NS_LOG_INFO ("Running simulation for " << simTime << " seconds");
  
  std::cout << "About to start simulation for " << simTime << " seconds..." << std::endl;

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  
  std::cout << "Simulation completed!" << std::endl;
  

  // Print flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  
  std::cout << std::endl << "*** Flow Statistics ***" << std::endl;
  std::cout << "  Tx Packets:   " << stats.begin()->second.txPackets << std::endl;
  std::cout << "  Rx Packets:   " << stats.begin()->second.rxPackets << std::endl;
  std::cout << "  Throughput:   " << stats.begin()->second.rxBytes * 8.0 / (simTime * 1000000) << " Mbps" << std::endl;
  std::cout << "  Mean Delay:   " << stats.begin()->second.delaySum.GetSeconds () / stats.begin()->second.rxPackets << " seconds" << std::endl;
  std::cout << "  Packet Loss:  " << 100.0 * (stats.begin()->second.txPackets - stats.begin()->second.rxPackets) / stats.begin()->second.txPackets << "%" << std::endl;
  
  // Print summary statistics
  std::cout << std::endl << "*** Summary Statistics ***" << std::endl;
  double totalThroughput = 0.0;
  double totalPackets = 0.0;
  double totalDelay = 0.0;
  double totalLoss = 0.0;
  
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {
    totalThroughput += i->second.rxBytes * 8.0 / (simTime * 1000000);
    totalPackets += i->second.rxPackets;
    if (i->second.rxPackets > 0) {
      totalDelay += i->second.delaySum.GetSeconds () / i->second.rxPackets;
    }
    totalLoss += 100.0 * (i->second.txPackets - i->second.rxPackets) / i->second.txPackets;
  }
  
  std::cout << "  Average Throughput: " << totalThroughput / stats.size() << " Mbps" << std::endl;
  std::cout << "  Average Delay:      " << totalDelay / stats.size() << " seconds" << std::endl;
  std::cout << "  Average Packet Loss: " << totalLoss / stats.size() << "%" << std::endl;
  
  Simulator::Destroy ();
  return 0;
}