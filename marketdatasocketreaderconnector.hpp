#ifndef MARKETDATASOCKETREADERCONNECTOR_HPP
#define MARKETDATASOCKETREADERCONNECTOR_HPP

#include <thread>
#include "soa.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <map>
#include <fstream>
#include <sstream>
#include <vector>
#include "pricingservice.hpp"
#include "products.hpp"
#include "pricesocketreaderconnector.hpp"
#include "tradebookingservice.hpp"
#include "marketdataservice.hpp"

extern std::map<std::string, Bond> bondMap;

class MarketDataSocketReaderConnector : public Connector<OrderBook<Bond>> {
private:
    int socketFd;
    Service<std::string, OrderBook<Bond>>* targetService;
    std::map<std::string,long> quantityMap;
    bool running;

public:
    MarketDataSocketReaderConnector(int port, Service<std::string, OrderBook<Bond>>* service) 
        : targetService(service), running(true) {
        // Create socket
        socketFd = socket(AF_INET, SOCK_STREAM, 0);
        if (socketFd < 0) {
            perror("Socket creation failed");
            exit(1);
        }

        // Add socket reuse option
        int opt = 1;
        if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt");
            exit(1);
        }

        // Bind to the port
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        if (::bind(socketFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
            perror("Bind failed");
            exit(1);
        }

        // Listen for connections
        if (listen(socketFd, 5) < 0) {
            perror("Listen failed");
            exit(1);
        }
        quantityMap = std::map<std::string,long>({{"10M",10000000},{"20M",20000000},{"30M",30000000},{"40M",40000000},{"50M",50000000}});
    }

    void Stop() {
        running = false;
        // Force socket to close to break accept() call
        close(socketFd);
    }

    void StartListening() {
        std::thread([this]() {
            std::cerr << "Socket listening started..." << std::endl;
            while (running) {
                sockaddr_in clientAddr{};
                socklen_t clientLen = sizeof(clientAddr);
                std::cerr << "Waiting for client connection..." << std::endl;
                int clientFd = accept(socketFd, (sockaddr*)&clientAddr, &clientLen);
                if (!running) break;
                if (clientFd < 0) {
                    perror("Accept failed");
                    continue;
                }
                std::cerr << "Client connected!" << std::endl;

                std::string accumulated;
                char buffer[1024];
                while (running) {
                    int bytesRead = read(clientFd, buffer, sizeof(buffer) - 1);
                    if (bytesRead <= 0) {
                        std::cerr << "Connection closed or error (bytesRead: " << bytesRead << ")" << std::endl;
                        break;
                    }
                    
                    buffer[bytesRead] = '\0';
                    accumulated += buffer;
                    
                    size_t pos;
                    while ((pos = accumulated.find('\n')) != std::string::npos) {
                        std::string line = accumulated.substr(0, pos);
                        if (!line.empty() && line[line.length()-1] == '\r') {
                            line = line.substr(0, line.length()-1);  // Remove \r if present
                        }
                        try {
                            OrderBook<Bond> orderbook_obj = MakeOrderBook(line);
                            Publish(orderbook_obj);
                        }
                        catch (const std::exception& e) {
                            std::cerr << "Error processing line: " << e.what() << std::endl;
                        }
                        accumulated = accumulated.substr(pos + 1);
                    }
                }
                std::cerr << "Client connection closed" << std::endl;
                close(clientFd);
            }
        }).detach();
    }


    // Override the Publish method to handle received data
    void Publish(OrderBook<Bond>& data) override {
        // Directly pass the data to the service
        targetService->OnMessage(data);
    }

    // Parse raw string into Price<Bond> object
    OrderBook<Bond> MakeOrderBook(std::string input) {
        // First check if the string is long enough
        if (input.length() < 41) {  
            throw std::runtime_error("Input string too short: " + input);
        }
        // Parse timestamp before first comma
        size_t comma_pos = input.find(',');
        if (comma_pos == string::npos) {
            throw std::runtime_error("No comma found in input: " + input);
        }
        std::string cusip = input.substr(0, comma_pos);
        
        // Move past the comma and space
        input = input.substr(comma_pos + 2);

        // Parse remaining items in groups of 3
        std::vector<std::string> items;
        int count = 0;
        PricingSide side = (PricingSide)0;
        double price = 0;
        long quantity = 0;
        vector<Order> bids;
        vector<Order> offers;
        try {
            while (!input.empty()) {
                comma_pos = input.find(',');
                if (comma_pos == string::npos) {
                    quantity = quantityMap.find(input)->second;
                    if (side == BID){
                        bids.push_back(Order(price, quantity, side));
                    }
                    else {
                        offers.push_back(Order(price, quantity, side));
                    }
                    break;
                }
                if (count == 0) {
                    side = (PricingSide)std::stoi(input.substr(0, comma_pos));
                }
                else if (count == 1) {
                    price = std::stod(input.substr(0, 3));
                    price = price + std::stod(input.substr(4, 2))/32;
                    if (input.substr(6, 1) == "+") {
                        price = price + 1.0/64.0;
                    }
                    else {
                        price = price + std::stod(input.substr(6, 1))/256;
                    }
                }
                else if (count == 2) {
                    quantity = quantityMap.find(input.substr(0, comma_pos))->second;
                    if (side == BID){
                        bids.push_back(Order(price, quantity, side));
                    }
                    else {
                        offers.push_back(Order(price, quantity, side));
                    }
                    count = -1;
                }
                input = input.substr(comma_pos + 2); // Skip comma and space
                count++;
            }
            if (bondMap.find(cusip) == bondMap.end()) {
                throw std::runtime_error("Unknown CUSIP: " + cusip);
            }
            return OrderBook<Bond>(bondMap.find(cusip)->second, bids, offers);
        }
        catch (const std::out_of_range& e) {
            throw std::runtime_error("Invalid string format: " + input);
        }
        catch (const std::invalid_argument& e) {
            throw std::runtime_error("Invalid number format in: " + input);
        }
    }

    ~MarketDataSocketReaderConnector() {
        Stop();
    }
};

#endif