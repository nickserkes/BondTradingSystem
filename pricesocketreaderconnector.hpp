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

#ifndef PRICESOCKETREADERCONNECTOR_HPP
#define PRICESOCKETREADERCONNECTOR_HPP


// Create a map of bond codes to Bond objects from TBonds.csv
std::map<std::string, Bond> GetBondMap() {
    std::map<std::string, Bond> bondMap;
    std::ifstream file("TBonds.csv");
    
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open TBonds.csv" << std::endl;
        return bondMap;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::vector<std::string> tokens;
        std::string token;
        
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }
        
        if (tokens.size() < 5) {
            std::cerr << "Warning: Skipping malformed line in CSV" << std::endl;
            continue;
        }
        
        try {
            std::string productId = tokens[0];
            std::string ticker = tokens[2];
            float coupon = std::stof(tokens[3]);
            std::string maturityStr = tokens[4];
            
            std::vector<std::string> dateParts;
            std::stringstream dateStream(maturityStr);
            std::string datePart;
            
            while (std::getline(dateStream, datePart, '/')) {
                dateParts.push_back(datePart);
            }
            
            if (dateParts.size() != 3) {
                std::cerr << "Warning: Invalid date format for product " << productId << std::endl;
                continue;
            }
            
            int month = std::stoi(dateParts[0]);
            int day = std::stoi(dateParts[1]);
            int year = std::stoi(dateParts[2]);
            
            if (year < 100) {
                year += 2000;
            }
            
            date maturityDate(year, month, day);
            Bond bond(productId, CUSIP, ticker, coupon, maturityDate);
            
            auto insertResult = bondMap.insert(std::make_pair(productId, bond));
            if (!insertResult.second) {
                std::cerr << "Failed to insert bond with ID: " << productId << " (duplicate key?)" << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error processing line: " << e.what() << std::endl;
            continue;
        }
    }
    
    file.close();
    return bondMap;
}

std::map<std::string, Bond> bondMap = GetBondMap();



class PricesSocketReaderConnector : public Connector<Price<Bond>> {
private:
    int socketFd;
    Service<std::string, Price<Bond>>* targetService;
    bool running;

public:
    PricesSocketReaderConnector(int port, Service<std::string, Price<Bond>>* service) 
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
                            Price<Bond> price_obj = MakePrice(line);
                            Publish(price_obj);
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
    void Publish(Price<Bond>& data) override {
        // Directly pass the data to the service
        targetService->OnMessage(data);
    }

    // Parse raw string into Price<Bond> object
    Price<Bond> MakePrice(std::string input) {
        // First check if the string is long enough
        if (input.length() < 21) {  
            throw std::runtime_error("Input string too short: " + input);
        }

        try {
            std::string CUSIP = input.substr(0, 9);
            std::string raw_price = input.substr(11, 7);
            double spread = stod(input.substr(20, 1))/128;
            
            // Validate CUSIP exists in bondMap
            if (bondMap.find(CUSIP) == bondMap.end()) {
                throw std::runtime_error("Unknown CUSIP: " + CUSIP);
            }

            double price = stod(raw_price.substr(0,3));
            price = price + stod(raw_price.substr(4,2))/32;
            if (raw_price.substr(6,1) == "+") {
                price = price + 1.0/64.0;
            }
            else {
                price = price + stod(raw_price.substr(6,1))/256;
            }
            
            return Price<Bond>(bondMap[CUSIP], price, spread);
        }
        catch (const std::out_of_range& e) {
            throw std::runtime_error("Invalid string format: " + input);
        }
        catch (const std::invalid_argument& e) {
            throw std::runtime_error("Invalid number format in: " + input);
        }
    }

    ~PricesSocketReaderConnector() {
        Stop();
    }
};

#endif