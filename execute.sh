#!/bin/bash
# start_sim_dashboard.sh

# Start React Dashboard
(cd dashboard && npm start) &

# Start C++ Simulation
(cd simulator && ./bin/power_network)


