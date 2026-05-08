# IoT People Detection for SmartCrossing Digital Twins

This project is part of a Scientific Initiation (IC) research conducted at the **NERDS Lab (Network Engineering and Robust Distributed Systems)**. 

The primary objective is to implement a robust and high-performance people detection system designed specifically for **SmartCrossing** scenarios within the context of **Digital Twins**. By monitoring pedestrian flow at crossings, the system provides real-time data to synchronize physical infrastructure with its digital counterpart, enabling advanced urban simulation and safety management.

### Edge Performance with C++
To ensure maximum efficiency at the **Edge Node**, this entire solution is implemented in **C++**. This architectural choice is critical for:
- **Low Latency:** Minimizing the time between frame capture and detection.
- **Resource Optimization:** Efficiently utilizing the hardware constraints of edge devices (such as Raspberry Pi or NVIDIA Jetson).
- **High Throughput:** Maintaining a high frame rate (FPS) to ensure no critical events are missed at the crossing.

### Integration with FIWARE
The system functions as an IoT Agent that translates physical observations into **NGSI-LD** compliant data. It communicates directly with a FIWARE Context Broker, ensuring that the SmartCrossing Digital Twin is always updated with accurate, real-time pedestrian counts and temporal data.
