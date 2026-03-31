# MiniPowerGrid

MiniPowerGrid is a small simulation of an electrical distribution network with generators, consumers, batteries, and power lines.  
The simulation runs in C++ and sends live network data via WebSocket to a React dashboard for visualization.

---

# Installation

## Prerequisites

- C++17
- Boost (json, asio, beast)
- Node.js + npm
- Make

### macOS (Homebrew)
```
bash
brew install boost node
```

### Initialize React Dashboard

After installing Node.js and npm, set up the dashboard environment:
```
cd dashboard
npm install        # install React dependencies
export NODE_ENV=development   # set environment variable
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

# License

This project is licensed under the MIT License.
See the LICENSE file for details.
