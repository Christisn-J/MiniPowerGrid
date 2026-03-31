#!/bin/bash
# start_sim_dashboard.sh

# Start C++ Simulation
(cd simulator && ./bin/power_network)

# Start React Dashboard
(cd dashboard && npm start) &

