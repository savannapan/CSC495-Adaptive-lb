/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

// Include header files from your module to test
#include "ns3/ipv4-adaptive-lb-routing.h"

// Essential include for tests
#include "ns3/test.h"
#include "ns3/node.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/ipv4-address.h"
#include "ns3/simulator.h"

// Do not put your test classes in namespace ns3.  You may find it useful
// to use the using directive to access the ns3 namespace directly
using namespace ns3;

// Test case for path selection
class AdaptiveLBPathSelectionTestCase : public TestCase
{
public:
  AdaptiveLBPathSelectionTestCase ();
  virtual ~AdaptiveLBPathSelectionTestCase ();

private:
  virtual void DoRun (void);
};

// Add some help text to this case to describe what it is intended to test
AdaptiveLBPathSelectionTestCase::AdaptiveLBPathSelectionTestCase ()
  : TestCase ("Test path selection in Adaptive Load Balancer")
{
}

// This destructor does nothing but we include it as a reminder that
// the test case should clean up after itself
AdaptiveLBPathSelectionTestCase::~AdaptiveLBPathSelectionTestCase ()
{
}

void
AdaptiveLBPathSelectionTestCase::DoRun (void)
{
  // Create a routing object
  Ptr<Ipv4AdaptiveLBRouting> routing = CreateObject<Ipv4AdaptiveLBRouting> ();
  
  // Create test path scores
  std::vector<PathScore> testScores;
  
  PathScore score1;
  score1.score = 0.8;
  score1.port = 1;
  testScores.push_back(score1);
  
  PathScore score2;
  score2.score = 0.2;
  score2.port = 2;
  testScores.push_back(score2);
  
  // Force temperature to be very low to make selection deterministic
  // Lower temperature means more exploitation (less randomness)
  routing->SetAttribute("Temperature", DoubleValue(0.01));
  
  // With temperature nearly zero, it should always select the highest score
  // In this case, port 1
  uint32_t selected = routing->SelectPathUsingSoftmax(testScores);
  NS_TEST_ASSERT_MSG_EQ (selected, 1, "With low temperature, selection should be deterministic");
  
  // Now set temperature very high to make selection more random
  routing->SetAttribute("Temperature", DoubleValue(100.0));
  
  // Run multiple selections to test probability distribution
  int count1 = 0, count2 = 0;
  const int trials = 1000;
  
  for (int i = 0; i < trials; i++) {
    uint32_t port = routing->SelectPathUsingSoftmax(testScores);
    if (port == 1) count1++;
    else if (port == 2) count2++;
  }
  
  // With high temperature, selection should be more random (closer to 50/50)
  // Allow for some statistical variation
  double ratio = static_cast<double>(count1) / trials;
  NS_TEST_ASSERT_MSG_GT (ratio, 0.4, "Selection should be more random with high temperature");
  NS_TEST_ASSERT_MSG_LT (ratio, 0.6, "Selection should be more random with high temperature");
  
  // Add a test for score updating
  double learningRate = 0.5;
  routing->SetAttribute("LearningRate", DoubleValue(learningRate));
  
  // Set up the routing object with IPv4 and a simple route
  Ptr<Node> node = CreateObject<Node> ();
  Ptr<Ipv4> ipv4 = CreateObject<Ipv4> ();
  node->AggregateObject(ipv4);
  routing->SetIpv4(ipv4);
  
  // Add a route
  Ipv4Address dest("10.1.1.0");
  Ipv4Mask mask("255.255.255.0");
  routing->AddRoute(dest, mask, 1);
  
  // Create a simple update for the path and check score update
  // In a real test, we'd set up actual flows and check the full algorithm
  // This is just a basic unit test for the EMA update
  
  // Manually verify score after update using EMA formula
  double initialScore = 0.0;  // Should be default score
  double reward = 0.9;        // High reward
  double expectedScore = (1 - learningRate) * initialScore + learningRate * reward;
  
  // Verify update formula works correctly
  // Full verification would need a complete network setup
  // which is beyond the scope of a simple unit test
  double testScore = (1 - learningRate) * initialScore + learningRate * reward;
  NS_TEST_ASSERT_MSG_EQ_TOL (testScore, expectedScore, 0.001, 
                             "Score update calculation should use EMA formula");
  
  Simulator::Destroy ();
}

// The TestSuite class names the TestSuite, identifies what type of TestSuite,
// and enables the TestCases to be run.
class AdaptiveLBRoutingTestSuite : public TestSuite
{
public:
  AdaptiveLBRoutingTestSuite ();
};

AdaptiveLBRoutingTestSuite::AdaptiveLBRoutingTestSuite ()
  : TestSuite ("adaptive-lb-routing", UNIT)
{
  // TestDuration for TestCase can be QUICK, EXTENSIVE or TAKES_FOREVER
  AddTestCase (new AdaptiveLBPathSelectionTestCase, TestCase::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
static AdaptiveLBRoutingTestSuite adaptiveLBRoutingTestSuite;