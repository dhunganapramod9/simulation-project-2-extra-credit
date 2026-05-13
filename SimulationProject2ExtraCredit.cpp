// Pramod Dhungana
// Simulation Project 2 Extra Credit

#include <iostream>
#include <queue>
#include <vector>
#include <deque>
#include <random>
#include <iomanip>
#include <string>
#include <functional>

using namespace std;

const int DEPARTURE_LIMIT = 10000;
const int TRACE_LIMIT = 20;
const int CUSTOMER_POOL_SIZE = 100;

const double ARRIVAL_RATE = 8.0;    // lambda0
const double CENTRAL_RATE = 10.0;   // central server service rate
const double SLEEP_RATE = 5.0;      // lambda2

enum EventType {
    ARRIVAL,
    CENTRAL_DONE,
    SERVER_DONE,
    SERVER_WAKE
};

enum ServerStatus {
    BUSY,
    IDLE,
    VACATION
};

struct Customer {
    int id;
    double arrivalTime;
    bool inUse;

    Customer() {
        id = -1;
        arrivalTime = 0.0;
        inUse = false;
    }
};

struct Event {
    double time;
    EventType type;
    int serverId;
    Customer* customer;
    int eventId;

    bool operator>(const Event& other) const {
        if (time == other.time) {
            return eventId > other.eventId;
        }
        return time > other.time;
    }
};

struct Server {
    ServerStatus status;
    deque<Customer*> line;

    Server() {
        status = IDLE;
    }
};

struct Result {
    double average;
    double variance;
    int lostArrivals;
    int poolExhaustions;
    int maxPoolUsed;
};

string getEventName(EventType type) {
    if (type == ARRIVAL) return "ARRIVAL";
    if (type == CENTRAL_DONE) return "CENTRAL_DONE";
    if (type == SERVER_DONE) return "SERVER_DONE";
    return "WAKE";
}

char statusLetter(ServerStatus status) {
    if (status == BUSY) return 'B';
    if (status == IDLE) return 'I';
    return 'V';
}

class Simulation {
private:
    int n;
    bool systemTwo;

    double currentTime;
    int nextCustomerId;
    int eventCounter;
    int departures;
    int lostArrivals;
    int poolExhaustions;
    int poolUsed;
    int maxPoolUsed;

    bool centralBusy;
    bool centralBlocked;
    Customer* centralCustomer;

    deque<Customer*> centralQueue;
    vector<Server> servers;
    vector<double> serverRates;

    Customer customerPool[CUSTOMER_POOL_SIZE];
    queue<int> freePool;

    double sumTimes;
    double sumTimesSquared;

    mt19937 rng;
    priority_queue<Event, vector<Event>, greater<Event> > eventList;

public:
    Simulation(int numServers, bool isSystemTwo, int seed) {
        n = numServers;
        systemTwo = isSystemTwo;

        currentTime = 0.0;
        nextCustomerId = 0;
        eventCounter = 0;
        departures = 0;
        lostArrivals = 0;
        poolExhaustions = 0;
        poolUsed = 0;
        maxPoolUsed = 0;

        centralBusy = false;
        centralBlocked = false;
        centralCustomer = nullptr;

        sumTimes = 0.0;
        sumTimesSquared = 0.0;

        rng.seed(seed);
        servers.resize(n);

        for (int i = 0; i < CUSTOMER_POOL_SIZE; i++) {
            freePool.push(i);
        }

        if (n == 3) {
            serverRates.push_back(14.0);
            serverRates.push_back(15.0);
            serverRates.push_back(20.0);
        } else {
            serverRates.push_back(14.0);
            serverRates.push_back(15.0);
            serverRates.push_back(20.0);
            serverRates.push_back(20.0);
            serverRates.push_back(20.0);
        }
    }

    double exponentialTime(double rate) {
        exponential_distribution<double> dist(rate);
        return dist(rng);
    }

    Customer* getCustomerFromPool() {
        if (freePool.empty()) {
            poolExhaustions++;
            return nullptr;
        }

        int index = freePool.front();
        freePool.pop();

        customerPool[index].id = nextCustomerId++;
        customerPool[index].arrivalTime = currentTime;
        customerPool[index].inUse = true;

        poolUsed++;
        if (poolUsed > maxPoolUsed) {
            maxPoolUsed = poolUsed;
        }

        return &customerPool[index];
    }

    void releaseCustomer(Customer* customer) {
        if (customer == nullptr) {
            return;
        }

        int index = (int)(customer - customerPool);

        customer->id = -1;
        customer->arrivalTime = 0.0;
        customer->inUse = false;

        freePool.push(index);
        poolUsed--;
    }

    void schedule(double time, EventType type, int serverId, Customer* customer) {
        Event e;
        e.time = time;
        e.type = type;
        e.serverId = serverId;
        e.customer = customer;
        e.eventId = eventCounter++;

        eventList.push(e);
    }

    int chooseAwakeServer() {
        int bestServer = -1;
        int bestLoad = 1000000;

        for (int i = 0; i < n; i++) {
            if (servers[i].status != VACATION) {
                int load = (int)servers[i].line.size();

                if (servers[i].status == BUSY) {
                    load++;
                }

                if (load < bestLoad) {
                    bestLoad = load;
                    bestServer = i;
                }
            }
        }

        return bestServer;
    }

    int chooseFreeServer() {
        for (int i = 0; i < n; i++) {
            if (servers[i].status == IDLE) {
                return i;
            }
        }

        return -1;
    }

    void startCentralIfPossible() {
        if (centralBusy || centralBlocked || centralQueue.empty()) {
            return;
        }

        centralBusy = true;
        centralCustomer = centralQueue.front();
        centralQueue.pop_front();

        double finishTime = currentTime + exponentialTime(CENTRAL_RATE);
        schedule(finishTime, CENTRAL_DONE, -1, centralCustomer);
    }

    void sendCustomerToServer(Customer* customer, int serverId) {
        if (servers[serverId].status == IDLE) {
            servers[serverId].status = BUSY;

            double finishTime = currentTime + exponentialTime(serverRates[serverId]);
            schedule(finishTime, SERVER_DONE, serverId, customer);
        } else {
            servers[serverId].line.push_back(customer);
        }
    }

    string handleArrival() {
        schedule(currentTime + exponentialTime(ARRIVAL_RATE), ARRIVAL, -1, nullptr);

        if (systemTwo && centralBlocked) {
            lostArrivals++;
            return "arrival disappeared because central server is blocked";
        }

        Customer* customer = getCustomerFromPool();

        if (customer == nullptr) {
            lostArrivals++;
            return "arrival disappeared because customer pool is empty";
        }

        centralQueue.push_back(customer);
        startCentralIfPossible();

        return "customer joined central queue";
    }

    string handleCentralDone(Customer* customer) {
        centralBusy = false;
        centralCustomer = nullptr;

        int serverId;

        if (systemTwo) {
            serverId = chooseFreeServer();
        } else {
            serverId = chooseAwakeServer();
        }

        if (serverId == -1) {
            centralBlocked = true;
            centralCustomer = customer;
            return "central server is blocked because no valid server is available";
        }

        sendCustomerToServer(customer, serverId);

        string action;

        if (systemTwo) {
            action = "central sent customer to free server S" + to_string(serverId + 1);
        } else {
            action = "central sent customer to awake server S" + to_string(serverId + 1);
        }

        startCentralIfPossible();

        return action;
    }

    string tryToMoveBlockedCustomer() {
        if (!centralBlocked) {
            return "";
        }

        int serverId;

        if (systemTwo) {
            serverId = chooseFreeServer();
        } else {
            serverId = chooseAwakeServer();
        }

        if (serverId == -1) {
            return "";
        }

        Customer* customer = centralCustomer;
        centralCustomer = nullptr;
        centralBlocked = false;

        sendCustomerToServer(customer, serverId);
        startCentralIfPossible();

        return " blocked central customer moved to server S" + to_string(serverId + 1);
    }

    string handleServerDone(int serverId, Customer* customer) {
        double timeInSystem = currentTime - customer->arrivalTime;

        departures++;
        sumTimes += timeInSystem;
        sumTimesSquared += timeInSystem * timeInSystem;

        releaseCustomer(customer);

        string action;

        if (!systemTwo && !servers[serverId].line.empty()) {
            Customer* nextCustomer = servers[serverId].line.front();
            servers[serverId].line.pop_front();

            servers[serverId].status = BUSY;

            double finishTime = currentTime + exponentialTime(serverRates[serverId]);
            schedule(finishTime, SERVER_DONE, serverId, nextCustomer);

            action = "S" + to_string(serverId + 1) + " completed customer and started next customer";
        } else {
            servers[serverId].status = VACATION;

            double wakeTime = currentTime + exponentialTime(SLEEP_RATE);
            schedule(wakeTime, SERVER_WAKE, serverId, nullptr);

            action = "S" + to_string(serverId + 1) + " completed customer and went on vacation";
        }

        string extra = tryToMoveBlockedCustomer();

        if (extra != "") {
            action += ";" + extra;
        }

        return action;
    }

    string handleServerWake(int serverId) {
        servers[serverId].status = IDLE;

        string action = "S" + to_string(serverId + 1) + " vacation ended and server is idle";

        string extra = tryToMoveBlockedCustomer();

        if (extra != "") {
            action += ";" + extra;
        }

        return action;
    }

    void printServerStatus() {
        cout << " | Status:";

        for (int i = 0; i < n; i++) {
            cout << " S" << i + 1 << "=" << statusLetter(servers[i].status);

            if (!systemTwo) {
                cout << "(Q" << servers[i].line.size() << ")";
            }
        }
    }

    void printFutureEventListPreview() {
        priority_queue<Event, vector<Event>, greater<Event> > copy = eventList;

        cout << " | FEL:";

        int count = 0;

        if (copy.empty()) {
            cout << " empty";
            return;
        }

        while (!copy.empty() && count < 5) {
            Event e = copy.top();
            copy.pop();

            cout << " " << getEventName(e.type) << "@"
                 << fixed << setprecision(3) << e.time;

            if (e.serverId != -1) {
                cout << "(S" << e.serverId + 1 << ")";
            }

            if (e.type == SERVER_WAKE) {
                cout << "[RED]";
            }

            count++;
        }
    }

    void printTrace(Event e, int traceNumber, string action) {
        cout << fixed << setprecision(4);

        cout << setw(2) << traceNumber
             << " | clock=" << setw(8) << e.time
             << " | " << setw(12) << getEventName(e.type)
             << " | " << action;

        if (e.customer != nullptr && e.customer->id != -1) {
            cout << " | customer " << e.customer->id;
        }

        cout << " | CQ=" << centralQueue.size();

        if (centralBlocked) {
            cout << " C=BLOCKED";
        } else if (centralBusy) {
            cout << " C=BUSY";
        } else {
            cout << " C=IDLE";
        }

        printServerStatus();

        cout << " | pool=" << poolUsed << "/" << CUSTOMER_POOL_SIZE;
        cout << " | departures=" << departures;

        printFutureEventListPreview();

        cout << endl;
    }

    Result run() {
        schedule(exponentialTime(ARRIVAL_RATE), ARRIVAL, -1, nullptr);

        int traceNumber = 1;

        while (departures < DEPARTURE_LIMIT && !eventList.empty()) {
            Event e = eventList.top();
            eventList.pop();

            currentTime = e.time;

            string action;

            if (e.type == ARRIVAL) {
                action = handleArrival();
            } else if (e.type == CENTRAL_DONE) {
                action = handleCentralDone(e.customer);
            } else if (e.type == SERVER_DONE) {
                action = handleServerDone(e.serverId, e.customer);
            } else {
                action = handleServerWake(e.serverId);
            }

            if (traceNumber <= TRACE_LIMIT) {
                printTrace(e, traceNumber, action);
                traceNumber++;
            }
        }

        Result result;
        result.average = sumTimes / departures;
        result.variance = (sumTimesSquared / departures) - (result.average * result.average);
        result.lostArrivals = lostArrivals;
        result.poolExhaustions = poolExhaustions;
        result.maxPoolUsed = maxPoolUsed;

        return result;
    }
};

int main() {
    cout << fixed << setprecision(6);

    cout << "Name: Pramod Dhungana" << endl;
    cout << "Simulation Project 2 Extra Credit - Network of Queues" << endl;
    cout << "Departure goal: " << DEPARTURE_LIMIT << " completed customers" << endl;
    cout << "Customer pool size: " << CUSTOMER_POOL_SIZE << endl;
    cout << "External arrival rate lambda0 = " << ARRIVAL_RATE << endl;
    cout << "Central server rate = " << CENTRAL_RATE << endl;
    cout << "Sleep/vacation ending rate lambda2 = " << SLEEP_RATE << endl;
    cout << "Server rates: S1=14, S2=15, S3=20, S4=20, S5=20" << endl;
    cout << "Server states: B=Busy, I=Idle, V=Vacation" << endl;
    cout << "Vacation wake-up events in the FEL are marked with [RED]" << endl;

    int serverCounts[2] = {3, 5};

    for (int i = 0; i < 2; i++) {
        int n = serverCounts[i];

        cout << "\n============================================================\n";
        cout << "System 1: central sends to one awake server; downstream queues allowed" << endl;
        cout << "If multiple servers are awake, customer goes to the awake server with smallest load." << endl;
        cout << "n = " << n << endl;
        cout << "Trace for first 20 events" << endl;
        cout << "No | clock | event | action | current state after event | FEL preview" << endl;
        cout << "------------------------------------------------------------" << endl;

        Simulation system1(n, false, 100 + n);
        Result r1 = system1.run();

        cout << "------------------------------------------------------------" << endl;
        cout << "Completed departures: " << DEPARTURE_LIMIT << endl;
        cout << "Average time in system: " << r1.average << endl;
        cout << "Variance of time in system: " << r1.variance << endl;
        cout << "Maximum pool objects used: " << r1.maxPoolUsed << "/" << CUSTOMER_POOL_SIZE << endl;
        cout << "Pool exhaustions: " << r1.poolExhaustions << endl;

        cout << "\n============================================================\n";
        cout << "System 2: central sends only to an awake idle/free server" << endl;
        cout << "If no free server exists, central blocks and new arrivals disappear." << endl;
        cout << "n = " << n << endl;
        cout << "Trace for first 20 events" << endl;
        cout << "No | clock | event | action | current state after event | FEL preview" << endl;
        cout << "------------------------------------------------------------" << endl;

        Simulation system2(n, true, 200 + n);
        Result r2 = system2.run();

        cout << "------------------------------------------------------------" << endl;
        cout << "Completed departures: " << DEPARTURE_LIMIT << endl;
        cout << "Lost/disappeared arrivals: " << r2.lostArrivals << endl;
        cout << "Average time in system: " << r2.average << endl;
        cout << "Variance of time in system: " << r2.variance << endl;
        cout << "Maximum pool objects used: " << r2.maxPoolUsed << "/" << CUSTOMER_POOL_SIZE << endl;
        cout << "Pool exhaustions: " << r2.poolExhaustions << endl;
    }

    return 0;
}