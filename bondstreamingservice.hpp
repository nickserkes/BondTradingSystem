/**
 * bondstreamingservice.hpp
 * Defines the data types and Service for streaming bond prices.
 */
#ifndef BOND_STREAMING_SERVICE_HPP
#define BOND_STREAMING_SERVICE_HPP
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include "soa.hpp"
#include "products.hpp"
#include "bondalgostreamingservice.hpp"
#include "streamingservice.hpp"
using namespace std;

// Connector to publish streams via socket
class BondStreamingServiceConnector : public Connector<AlgoStream<Bond>> {
private:
    int socketFd;
    bool running;
    int port;
    vector<int> clientSockets;  // Store connected clients

public:
    BondStreamingServiceConnector(int _port) : port(_port) {
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

    void Publish(AlgoStream<Bond>& data) override {
        auto now = chrono::system_clock::now();
        auto time_now = chrono::system_clock::to_time_t(now);
        auto ms_part = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
        
        stringstream ss;
        ss << put_time(localtime(&time_now), "%H:%M:%S") << "."
           << setfill('0') << setw(3) << ms_part << ","
           << data.GetPriceStream().GetProduct().GetProductId() << ","
           << data.GetPriceStream().GetBidOrder().GetPrice() << ","
           << data.GetPriceStream().GetOfferOrder().GetPrice() << ","
           << data.GetPriceStream().GetBidOrder().GetVisibleQuantity() << ","
           << data.GetPriceStream().GetBidOrder().GetHiddenQuantity() << ","
           << data.GetPriceStream().GetOfferOrder().GetVisibleQuantity() << ","
           << data.GetPriceStream().GetOfferOrder().GetHiddenQuantity() << "\n";
           
        string msg = ss.str();

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

    ~BondStreamingServiceConnector() {
        running = false;
        for (int clientFd : clientSockets) {
            close(clientFd);
        }
        close(socketFd);
    }
};

//class BondStreamingService;

class BondStreamingServiceListener : public ServiceListener<AlgoStream<Bond>> {
private:
    Service<string, AlgoStream<Bond>>* service;

public:
    BondStreamingServiceListener(Service<string, AlgoStream<Bond>>* _service) : service(_service) {}

    // Process an add event to the Service
    void ProcessAdd(AlgoStream<Bond>& data) override {
    service->OnMessage(data);
    }

    // Process a remove event to the Service
    void ProcessRemove(AlgoStream<Bond>& data) override {}

    // Process an update event to the Service
    void ProcessUpdate(AlgoStream<Bond>& data) override {}
};

/**
 * Bond Streaming Service to publish bond price streams.
 */
class BondStreamingService : public Service<string, AlgoStream<Bond>> {
private:
    map<string, AlgoStream<Bond>> algoStreams;
    vector<ServiceListener<AlgoStream<Bond>>*> listeners;
    BondStreamingServiceConnector* connector;
    BondStreamingServiceListener* listener;

public:
    BondStreamingService(BondAlgoStreamingService* algoStreamingService) {
        connector = new BondStreamingServiceConnector(9000);  // Use port 9000 for streaming
        listener = new BondStreamingServiceListener(this);
        algoStreamingService->AddListener(listener);
    }

    // Get data on our service given a key
    AlgoStream<Bond>& GetData(string key) override {
        return algoStreams.find(key)->second;
    }

    // The callback that a Connector should invoke for any new or updated data
    void OnMessage(AlgoStream<Bond>& data) override {
        string productId = data.GetPriceStream().GetProduct().GetProductId();
        algoStreams.insert_or_assign(productId, data);
        // Publish to Connector
        connector->Publish(data);
        // Notify all listeners
        for (auto& listener : listeners) {
            listener->ProcessAdd(data);
        }
    }

    // Add a listener to the Service for callbacks on add, remove, and update events
    void AddListener(ServiceListener<AlgoStream<Bond>>* listener) override {
        listeners.push_back(listener);
    }

    // Get all listeners on the Service
    const vector<ServiceListener<AlgoStream<Bond>>*>& GetListeners() const override {
        return listeners;
    }

    ~BondStreamingService() {
        delete connector;
        delete listener;
    }
};

#endif

