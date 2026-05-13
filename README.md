# Simulation Project 2 Extra Credit

Name: Pramod Dhungana

## Description

This project simulates a network of queues with a central server and multiple downstream servers. Customers arrive using exponential interarrival times, move through the central server, and are then assigned to regular servers.

The simulation compares two systems:

1. System 1: The central server sends customers to an awake server. Downstream server queues are allowed.
2. System 2: The central server sends customers only to an awake idle/free server. If no free server exists, the central server blocks and new arrivals disappear.

The simulation runs for 10,000 completed customer departures and compares results for n = 3 and n = 5 servers.

## Extra Credit Features

This version includes the extra credit additions:

1. Server status is printed at each clock time:
   - B = Busy
   - I = Idle
   - V = Vacation

2. A customer pool array of 100 customer objects is used.
   When a customer finishes service, the customer object is returned to the pool and reused.

3. Server vacation/wake-up events are included in the Future Event List.
   These events are marked with `[RED]` in the output to make inactive server events easier to identify.

## Rates Used

- External arrival rate lambda0 = 8
- Central server rate = 10
- Sleep/vacation ending rate lambda2 = 5
- Server 1 rate = 14
- Server 2 rate = 15
- Server 3 rate = 20
- Server 4 rate = 20
- Server 5 rate = 20

## Compile

```bash
g++ -std=c++11 SimulationProject2ExtraCredit.cpp -o extra
