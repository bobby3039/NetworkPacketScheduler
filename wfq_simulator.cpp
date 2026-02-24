/**
 * @file wfq_simulator.cpp
 * @brief Weighted Fair Queuing (WFQ) Packet Scheduler Simulation.
 * Simulates a single network link shared by multiple traffic sources.
 * Uses virtual finish times (VFT) to approximate Generalized Processor Sharing (GPS).
 * Drop Policy: Drops the packet with the smallest VFT when the buffer is full.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <queue>
#include <random>
#include <iomanip>
#include <stdexcept>
#include <cmath>

/// @brief Represents a single network packet.
struct Packet {
    long id;
    int sourceID;
    int size;           // Packet size in bytes
    double weight;      // Source weight
    double arrivalTime; // Time the packet entered the system
    double virtualFinishTime; // WFQ VFT

    // Min-heap comparator for priority_queue (smallest VFT first)
    bool operator>(const Packet& other) const {
        return virtualFinishTime > other.virtualFinishTime;
    }
};

/// @brief Represents a discrete simulation event (Arrival or Departure).
struct Event {
    enum Type { PACKET_ARRIVAL, PACKET_DEPARTURE };
    
    Type type;
    double time;
    int sourceID;
    Packet packet;

    Event(Type t, double tm, int srcID) : type(t), time(tm), sourceID(srcID) {}
    Event(Type t, double tm, const Packet& p) : type(t), time(tm), sourceID(p.sourceID), packet(p) {}

    // Min-heap comparator (earliest time first)
    bool operator>(const Event& other) const {
        return time > other.time;
    }
};

/// @brief Traffic source configuration, WFQ state, and random generators.
struct Source {
    int id;
    double packetRate; 
    int minSize;       
    int maxSize;       
    double weight;     
    double startTime;  
    double endTime;    
    
    double lastFinishTime = 0.0; // Tracks F_{k-1} for this specific source

    std::exponential_distribution<double> arrivalDist;
    std::uniform_int_distribution<int> sizeDist;

    Source(int id, double rate, int min, int max, double w, double start, double end)
        : id(id), packetRate(rate), minSize(min), maxSize(max), weight(w),
          startTime(start), endTime(end), arrivalDist(rate), sizeDist(min, max) {}
};

/// @brief Tracks simulation statistics for a specific source.
struct SourceStats {
    long packetsGenerated = 0;
    long packetsTransmitted = 0;
    long packetsDropped = 0;
    double bytesTransmitted = 0.0;
    double totalDelay = 0.0;
};

/// @brief Encapsulates the WFQ Simulation engine and state.
class WFQSimulator {
private:
    int numSources = 0;
    double simulationTime = 0.0;
    double linkCapacity = 0.0;
    size_t bufferSize = 0;

    double currentTime = 0.0;
    double systemVirtualTime = 0.0;
    bool linkBusy = false;
    long nextPacketId = 1;

    std::vector<Source> sources;
    std::vector<SourceStats> stats;
    
    // WFQ Buffer: Min-priority queue based on Virtual Finish Time
    std::priority_queue<Packet, std::vector<Packet>, std::greater<Packet>> packetBuffer;
    
    // Global Event Queue: Min-priority queue based on Event Time
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> eventQueue;
    
    std::default_random_engine generator;

    void scheduleEvent(const Event& e) {
        if (e.time <= simulationTime) {
            eventQueue.push(e);
        }
    }

    void startNextTransmission() {
        if (linkBusy || packetBuffer.empty()) return;

        linkBusy = true;
        Packet packetToTransmit = packetBuffer.top();
        packetBuffer.pop();

        // Update system virtual time based on the starting packet
        double virtualStartTime = packetToTransmit.virtualFinishTime - (packetToTransmit.size / packetToTransmit.weight);
        systemVirtualTime = virtualStartTime;

        double transmissionTime = packetToTransmit.size / linkCapacity;
        scheduleEvent(Event(Event::PACKET_DEPARTURE, currentTime + transmissionTime, packetToTransmit));
    }

    void handleArrivalEvent(const Event& e) {
        int srcID = e.sourceID;
        Source& src = sources[srcID];

        // 1. Schedule next arrival
        double nextArrivalTime = currentTime + src.arrivalDist(generator);
        if (nextArrivalTime < src.endTime) {
            scheduleEvent(Event(Event::PACKET_ARRIVAL, nextArrivalTime, srcID));
        }

        // 2. Generate packet
        Packet newPacket;
        newPacket.id = nextPacketId++;
        newPacket.sourceID = srcID;
        newPacket.size = src.sizeDist(generator);
        newPacket.weight = src.weight;
        newPacket.arrivalTime = currentTime;

        // --- WFQ Core Logic: Calculate VFT ---
        double virtualStartTime = std::max(systemVirtualTime, src.lastFinishTime);
        newPacket.virtualFinishTime = virtualStartTime + (newPacket.size / src.weight);
        src.lastFinishTime = newPacket.virtualFinishTime;
        
        stats[srcID].packetsGenerated++;

        // 3. Buffer management (Drop packet with smallest VFT if full)
        if (packetBuffer.size() < bufferSize) {
            packetBuffer.push(newPacket);
        } else {
            // Drop policy: pop the top (smallest VFT), record drop, then push new packet
            Packet packetToDrop = packetBuffer.top();
            packetBuffer.pop();
            
            stats[packetToDrop.sourceID].packetsDropped++;
            packetBuffer.push(newPacket);
        }

        // 4. Try transmission
        startNextTransmission();
    }

    void handleDepartureEvent(const Event& e) {
        linkBusy = false;
        const Packet& p = e.packet;
        int srcID = p.sourceID;

        stats[srcID].bytesTransmitted += p.size;
        stats[srcID].packetsTransmitted++;
        stats[srcID].totalDelay += (currentTime - p.arrivalTime);

        startNextTransmission();
    }

public:
    /**
     * @brief Parses configuration from the input file.
     */
    void loadConfig(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open input file: " + filename);
        }

        std::string line;
        if (!std::getline(file, line)) throw std::runtime_error("Empty config file.");
        
        std::stringstream ss(line);
        ss >> numSources >> simulationTime >> linkCapacity >> bufferSize;
        stats.resize(numSources);

        for (int i = 0; i < numSources; ++i) {
            if (!std::getline(file, line)) throw std::runtime_error("Missing source configurations.");
            std::stringstream ss_src(line);
            
            double rate, weight, tb_frac, te_frac;
            int minSz, maxSz;
            ss_src >> rate >> minSz >> maxSz >> weight >> tb_frac >> te_frac;
            
            sources.emplace_back(i, rate, minSz, maxSz, weight, 
                                 tb_frac * simulationTime, te_frac * simulationTime);
        }
    }

    /**
     * @brief Executes the discrete-event simulation loop.
     */
    void run() {
        for (const auto& src : sources) {
            scheduleEvent(Event(Event::PACKET_ARRIVAL, src.startTime, src.id));
        }

        while (!eventQueue.empty()) {
            Event currentEvent = eventQueue.top();
            eventQueue.pop();
            
            currentTime = currentEvent.time;
            if (currentTime > simulationTime) break;

            if (currentEvent.type == Event::PACKET_ARRIVAL) {
                handleArrivalEvent(currentEvent);
            } else {
                handleDepartureEvent(currentEvent);
            }
        }
    }

    /**
     * @brief Calculates metrics and outputs them to the provided stream.
     */
    void printResults(std::ostream& out) const {
        long totGen = 0, totTrans = 0, totDrop = 0;
        double totBytes = 0.0, totDelay = 0.0;
        double sum_x = 0.0, sum_x_sq = 0.0;

        for (int i = 0; i < numSources; ++i) {
            totGen += stats[i].packetsGenerated;
            totTrans += stats[i].packetsTransmitted;
            totDrop += stats[i].packetsDropped;
            totBytes += stats[i].bytesTransmitted;
            totDelay += stats[i].totalDelay;

            // WFQ Fairness relies on weight-normalized throughput
            double x_i = (sources[i].weight > 0) ? (stats[i].bytesTransmitted / sources[i].weight) : 0.0;
            sum_x += x_i;
            sum_x_sq += (x_i * x_i);
        }

        double util = (totBytes / linkCapacity) / simulationTime;
        double avgDelay = totTrans > 0 ? (totDelay / totTrans) : 0.0;
        double dropProb = totGen > 0 ? (double)totDrop / totGen : 0.0;
        double fairness = sum_x_sq > 0 ? ((sum_x * sum_x) / (numSources * sum_x_sq)) : 0.0;

        out << std::fixed << std::setprecision(6);
        out << "## System-Level Performance Metrics (WFQ)\n"
            << "1. Server Utilization:   " << util << "\n"
            << "2. Avg. Packet Delay:    " << avgDelay << " s\n"
            << "3. Packet Drop Prob.:    " << dropProb << "\n"
            << "4. Fairness Index:       " << fairness << "\n\n";

        out << "## Per-Source Statistics\n"
            << "---------------------------------------------------------------------------------------\n"
            << "Src | Weight | Gen'd Pkts | Trans'd Pkts | Drop'd Pkts | Drop Rate | Avg Delay (s) | Thruput (B/s)\n"
            << "---------------------------------------------------------------------------------------\n";

        for (int i = 0; i < numSources; ++i) {
            double dropRate = stats[i].packetsGenerated > 0 ? 
                              (double)stats[i].packetsDropped / stats[i].packetsGenerated : 0.0;
            double sDelay = stats[i].packetsTransmitted > 0 ? 
                            stats[i].totalDelay / stats[i].packetsTransmitted : 0.0;
            double thruput = stats[i].bytesTransmitted / simulationTime;

            out << std::setw(3) << i << " | "
                << std::setw(6) << sources[i].weight << " | "
                << std::setw(10) << stats[i].packetsGenerated << " | "
                << std::setw(12) << stats[i].packetsTransmitted << " | "
                << std::setw(11) << stats[i].packetsDropped << " | "
                << std::setw(9) << std::setprecision(4) << dropRate << " | "
                << std::setw(13) << std::setprecision(6) << sDelay << " | "
                << std::setw(13) << std::setprecision(2) << thruput << "\n";
        }
        out << "---------------------------------------------------------------------------------------\n";
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>\n";
        return 1;
    }

    std::string inputFilename = argv[1];
    std::string outputFilename = "wfq_output_" + inputFilename;

    try {
        WFQSimulator simulator;
        simulator.loadConfig(inputFilename);
        simulator.run();

        // Print to file
        std::ofstream outputFile(outputFilename);
        if (!outputFile) throw std::runtime_error("Could not create output file.");
        simulator.printResults(outputFile);

        // Print to console
        std::cout << "\n--- WFQ Results for " << inputFilename << " ---\n";
        simulator.printResults(std::cout);
        std::cout << "\nFull results written to " << outputFilename << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}