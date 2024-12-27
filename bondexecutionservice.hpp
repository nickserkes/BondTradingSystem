/**
 * bondexecutionservice.hpp
 * Executes orders on the market.
 */
#ifndef BOND_EXECUTION_SERVICE_HPP
#define BOND_EXECUTION_SERVICE_HPP
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include "soa.hpp"
#include "products.hpp"
#include "bondalgoexecutionservice.hpp"
#include "executionservice.hpp"
#include "tradebookingservice.hpp"
using namespace std;

// Connector to publish executions via socket
class BondExecutionServiceConnector : public Connector<ExecutionOrder<Bond>> {
private:
    int socketFd;
    bool running;
    int port;
    vector<int> clientSockets;  // Store connected clients

public:
    BondExecutionServiceConnector(int _port) : port(_port) {
        // Create socket
        socketFd = socket(AF_INET, SOCK_STREAM, 0);
        if (socketFd < 0) {
            perror("Socket creation failed");
            exit(1);
        }

        // Configure server address
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        // Bind socket
        if (::bind(socketFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
            perror("Bind failed");
            exit(1);
        }

        // Listen for connections
        if (listen(socketFd, 5) < 0) {
            perror("Listen failed");
            exit(1);
        }

        running = true;
    }

    void Publish(ExecutionOrder<Bond>& data) override {
        string orderType = "";
        if (data.GetOrderType() == MARKET) {
            orderType = "MARKET";
        }
        string side = "";
        if (data.GetSide() == BID) {
            side = "BUY";
        }
        else {
            side = "SELL";
        }
        
        string msg = data.GetProduct().GetProductId() + "," + 
                    data.GetOrderId() + "," +
                    orderType + "," +
                    side + "," +
                    to_string(data.GetPrice()) + "," + 
                    to_string(data.GetVisibleQuantity()) + "\n";

        // Non-blocking check for new connections
        fd_set readSet;
        struct timeval timeout{0, 0};  // Non-blocking
        FD_ZERO(&readSet);
        FD_SET(socketFd, &readSet);
        
        if (select(socketFd + 1, &readSet, nullptr, nullptr, &timeout) > 0) {
            sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            int clientFd = accept(socketFd, (sockaddr*)&clientAddr, &clientLen);
            if (clientFd >= 0) {
                clientSockets.push_back(clientFd);
            }
        }

        // Send to all connected clients
        auto it = clientSockets.begin();
        while (it != clientSockets.end()) {
            if (send(*it, msg.c_str(), msg.length(), MSG_NOSIGNAL) < 0) {
                close(*it);
                it = clientSockets.erase(it);  // Remove disconnected client
            } else {
                ++it;
            }
        }
    }

    ~BondExecutionServiceConnector() {
        running = false;
        for (int clientFd : clientSockets) {
            close(clientFd);
        }
        close(socketFd);
    }
};

class BondExecutionServiceListener : public ServiceListener<AlgoExecution<Bond>> {
private:
    ExecutionService<Bond>* service;

public:
    BondExecutionServiceListener(ExecutionService<Bond>* _service) : service(_service) {}

    // Process an add event to the Service
    void ProcessAdd(AlgoExecution<Bond>& data) override {
    service->OnMessage(data.GetExecutionOrder());
    }

    // Process a remove event to the Service
    void ProcessRemove(AlgoExecution<Bond>& data) override {}

    // Process an update event to the Service
    void ProcessUpdate(AlgoExecution<Bond>& data) override {}
};

/**
 * Bond Streaming Service to publish bond price streams.
 */
class BondExecutionService : public ExecutionService<Bond> {
private:
    map<string, ExecutionOrder<Bond>> executionOrders;
    vector<ServiceListener<ExecutionOrder<Bond>>*> listeners;
    BondExecutionServiceConnector* connector;
    BondExecutionServiceListener* listener;
    BondMarketDataService* marketDataService;
    //int orderIDs = 1;

public:
    BondExecutionService(BondAlgoExecutionService* algoExecutionService, BondMarketDataService* _marketDataService) {
        connector = new BondExecutionServiceConnector(3000);  // Use port 3000 for streaming
        listener = new BondExecutionServiceListener(this);
        algoExecutionService->AddListener(listener);
        marketDataService = _marketDataService;
    }

    BondExecutionServiceListener* GetListener() {
        return listener;
    }

    // Get data on our service given a key
    ExecutionOrder<Bond>& GetData(string key) override {
        return executionOrders.find(key)->second;
    }

    void ExecuteOrder(const ExecutionOrder<Bond>& order, Market market) override {}

    // The callback that a Connector should invoke for any new or updated data
    void OnMessage(ExecutionOrder<Bond>& data) override {
        string productId = data.GetProduct().GetProductId();
        executionOrders.insert_or_assign(productId, data);
        connector->Publish(data);
        // Notify all listeners
        for (auto& listener : listeners) {
            listener->ProcessAdd(data);
        }
    }

    // Add a listener to the Service for callbacks on add, remove, and update events
    void AddListener(ServiceListener<ExecutionOrder<Bond>>* listener) override {
        listeners.push_back(listener);
    }

    // Get all listeners on the Service
    const vector<ServiceListener<ExecutionOrder<Bond>>*>& GetListeners() const override {
        return listeners;
    }

    ~BondExecutionService() {
        delete connector;
        delete listener;
    }
};

#endif

