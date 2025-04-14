# CSC495 Adaptive Load Balancing Code

This repository contains a basic implementation of adaptive load balancing for the NS3 network simulator.

## Installation and Setup

1. Create a Docker container:
   ```bash
   docker run -it -v ~/ns3-load-balance-project:/ns3-load-balance gcc:4.9 /bin/bash
   ```
2. Navigate to the project folder:
   ```bash
   cd /ns3-load-balance
   ```
3. Clone the NS3 load balancing repository:
   ```bash
   git clone https://github.com/snowzjx/ns3-load-balance.git .
   ```
4. Configure and build:
   ```bash
   ./waf -d optimized --enable-examples configure
   ./waf
   ```

## Running the Simulation

1. Create a sub-folder called `adaptive-load` under `src`, and add the adaptive-lb files in this subfolder
   
Run the adaptive load balancing example with:

```bash
./waf --run "adaptive-lb-routing-example --simTime=30 --nPaths=4 --learningRate=0.2 --temperature=1.0 --verbose"
```

### Configurable Parameters:

- `simTime`: Simulation duration in seconds
- `nPaths`: Number of available paths
- `learningRate`: Learning rate for path score updates (α)
- `temperature`: Temperature parameter for softmax function (τ)
- `verbose`: Enable verbose logging

## Implementation Details

### Performance Monitoring
- Tracks metrics including queue lengths (`CalculateQueueLength`), link utilization (`CalculateUtilization`), and flow completion times
- Metrics are collected both periodically and upon flow completion

### Score Calculation
- Implements Exponential Moving Average (EMA) formula:
  ```
  double newScore = (1 - m_learningRate) * oldScore + m_learningRate * reward;
  ```
- Reward function incorporates utilization, queue depth, and flow completion time

### Decision Engine
- Implements softmax probability distribution for path selection:
  ```
  double probability = SoftmaxFunction(it->score, m_temperature);
  ```
- Creates a probability distribution favoring higher-scoring paths while allowing exploration

### Feedback Loop
- Updates path scores upon flow completion (`UpdatePathScores`) and periodically (`PeriodicScoreUpdate`)
- Incorporates feedback from observed performance

## Key Parameters

- Learning rate (α): `m_learningRate` (default: 0.2)
- Target utilization: `m_targetUtilization` (default: 0.7)
- Temperature (τ): `m_temperature` (default: 1.0)
- Update interval: `m_updateInterval` (default: 10ms)

## Algorithm Implementation

The path selection and scoring mechanisms follow this approach:

1. Reward function considers:
   - Utilization relative to target
   - Queue depth
   - Flow completion time
2. Path selection uses softmax probability distribution
3. Adaptation mechanism follows a three-step process:
   - Path selection for new flows
   - Score updates upon flow completion
   - Periodic updates of network-wide metrics

## Future Work

### Extended Simulation Duration
- Current simulations are short (30 seconds), resulting in high packet loss rates and limited convergence time
- Longer simulations (5-10 minutes) would allow the learning algorithm to stabilize
- Longer runs would better demonstrate self-optimization properties
- High packet loss (66-70%) suggests initial parameter tuning phase needs more time

### Comparative Analysis
- Direct comparisons with other load balancing strategies (ECMP, WCMP, Least Congested)
- Measure and compare:
  - Flow completion time distributions
  - Link utilization balance
  - Adaptation to sudden traffic pattern changes
  - Resilience to link failures

### Parameter Sensitivity Analysis
- Systematic testing of different values for learning rate, temperature, and update frequency
- Identify optimal parameter settings for different workloads (web search, data mining, ML)
- Analyze how parameter configurations affect convergence time and stability
