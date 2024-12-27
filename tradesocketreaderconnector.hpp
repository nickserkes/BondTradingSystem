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

#ifndef TRADESOCKETREADERCONNECTOR_HPP
#define TRADESOCKETREADERCONNECTOR_HPP


class TradesSocketReaderConnector : public Connector<Trade<Bond>> {
private:
    int socketFd;
    Service<std::string, Trade<Bond>>* targetService;
    bool running;

public:
    TradesSocketReaderConnector(int port, Service<std::string, Trade<Bond>>* service) 
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
                            Trade<Bond> trade_obj = MakeTrade(line);
                            Publish(trade_obj);
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
    void Publish(Trade<Bond>& data) override {
        // Directly pass the data to the service
        targetService->OnMessage(data);
    }

    // Parse raw string into Price<Bond> object
    Trade<Bond> MakeTrade(std::string input) {
        // First check if the string is long enough
        if (input.length() < 41) {  
            throw std::runtime_error("Input string too short: " + input);
        }

        try {
            std::string CUSIP = input.substr(0, 9);
            std::string tradeID = input.substr(11, 6);
            double price = stod(input.substr(19, 3));
            std::string book = input.substr(24,5);
            long quantity = stol(input.substr(31, 7));
            Side side = input.substr(40, 1) == "0" ? BUY : SELL;
            
            // Validate CUSIP exists in bondMap
            if (bondMap.find(CUSIP) == bondMap.end()) {
                throw std::runtime_error("Unknown CUSIP: " + CUSIP);
            }
            return Trade<Bond>(bondMap.find(CUSIP)->second, tradeID, price, book, quantity, side);
        }
        catch (const std::out_of_range& e) {
            throw std::runtime_error("Invalid string format: " + input);
        }
        catch (const std::invalid_argument& e) {
            throw std::runtime_error("Invalid number format in: " + input);
        }
    }

    ~TradesSocketReaderConnector() {
        Stop();
    }
};

#endif