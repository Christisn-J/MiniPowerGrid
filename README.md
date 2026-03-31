# MiniPowerGrid

MiniPowerGrid is a small simulation of an electrical distribution network with generators, consumers, batteries, and power lines.  
The simulation runs in C++ and sends live network data via WebSocket to a React dashboard for visualization.

This project is suitable for:
- Smart Grid simulation
- Microgrid / distribution network experiments
- Visualization of power flows
- Blackout and overload simulation

---

# Project Structure
```
MiniPowerGrid/
├── dashboard/ # React Dashboard (visualization)
├── simulator/ # C++ Simulation
│   ├── src/
│   ├── include/
│   ├── bin/
│   ├── log/
│   └── testcases/
├── execute.sh # Starts Simulator + Dashboard
└── README.md
```

---

# Features

- Generators (Solar, Wind)
- Consumers (City, Industry, Residential)
- Battery model with State of Charge (SOC)
- Line overload handling
- Voltage drops
- Blackout simulation
- WebSocket live data
- JSON logging
- Configurable network via JSON

---

# Installation

## Requirements

- C++17
- Boost (json, asio, beast)
- Node.js + npm
- Make

Mac (Homebrew):
```bash
brew install boost node
```

---

# Getting Started

```
cd simulator
make
cd ..
./execute.sh
```

---

# License

This project is licensed under the MIT License.
See the LICENSE file for details.
