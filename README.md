
# Discrete-Event Packet Queuing Simulator (FCFS & WFQ)



## Overview
This project is a discrete-event network simulator written in C++11. It models a single network link with a finite buffer shared by multiple traffic sources. The simulator evaluates and compares two distinct packet scheduling algorithms—**First-Come-First-Serve (FCFS)** and **Weighted Fair Queuing (WFQ)**.

The simulation tracks packet-level events (arrivals and departures) across logical time to calculate system performance metrics, including server utilization, average packet delay, drop probability, and Jain’s Fairness Index.

## Features
* **Object-Oriented Architecture:** Encapsulates simulation state and utilizes `std::priority_queue` for highly efficient $O(\log N)$ discrete-event handling.
* **First-Come-First-Serve (FCFS):** Implements a standard FIFO processing queue. Uses a tail-drop policy where incoming packets are dropped if the buffer is full upon arrival.
* **Weighted Fair Queuing (WFQ):** Approximates Generalized Processor Sharing (GPS) by calculating a Virtual Finish Time (VFT) for each packet. Implements a specialized min-priority drop policy (drops the packet with the *smallest* VFT in the queue when the buffer is full).
* **Statistical Tracking:** Generates detailed system-level and per-source performance metrics, outputting to both the console and a detailed text report.

## Input File Configuration
The simulator reads the network environment and source parameters from a text file. 

**Line 1 (Global Parameters):** `<NUM_SOURCES> <SIMULATION_TIME> <LINK_CAPACITY (B/s)> <BUFFER_SIZE (packets)>`

**Following Lines (One for each source):** `<PACKET_RATE (pkts/s)> <MIN_SIZE (Bytes)> <MAX_SIZE (Bytes)> <WEIGHT> <START_TIME_FRACTION> <END_TIME_FRACTION>`

*Example:*
```text
3 100.0 10000.0 50
50.0 100 500 1.0 0.0 1.0
100.0 100 500 2.0 0.0 1.0
25.0 100 500 0.5 0.2 0.8```


1. Compilation
Navigate to the project directory in your terminal and compile the simulators using g++ (requires C++11 support):
# Compile the FCFS simulator
g++ -std=c++11 fcfs_simulator.cpp -o fcf

# Compile the WFQ simulator
g++ -std=c++11 wfq_simulator.cpp -o wfq

2. Running the Simulation
Run the compiled executables by passing your configuration file as a command-line argument.

# Run FCFS with input_a.txt
./fcfs input_a.txt

# Run WFQ with input_b.txt
./wfq input_b.txt

3. Understanding the Output Metrics
For every run, the simulator generates a detailed output file. The results include:

System-Level Metrics: Overall Server Utilization, Average Packet Delay, and overall Packet Drop Probability.

Fairness: Jain's Fairness Index based on throughput (normalized by weight for the WFQ algorithm).

Per-Source Statistics: Generated/Transmitted/Dropped counts, individual drop rates, average delays, and effective throughput (Bytes/sec).
