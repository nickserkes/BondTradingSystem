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
#include "inquiryservice.hpp"
#ifndef INQUIRYSOCKETREADERCONNECTOR_HPP
#define INQUIRYSOCKETREADERCONNECTOR_HPP


class InquirySocketReaderConnector : public Connector<Inquiry<Bond>> {
private:
    int socketFd;
    InquiryService<Bond>* targetService;
    std::map<std::string,long> quantityMap;
    bool running;

public:
    InquirySocketReaderConnector(int port, InquiryService<Bond>* service) 
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
                            Inquiry<Bond> inquiry_obj = MakeInquiry(line);
                            Publish(inquiry_obj);
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
    void Publish(Inquiry<Bond>& data) override {
        // Directly pass the data to the service
        targetService->OnMessage(data);
    }

    void ReceiveQuote(Inquiry<Bond>& data){
        data.ChangeState(InquiryState::QUOTED);
        Publish(data);
        data.ChangeState(InquiryState::DONE);
        Publish(data);
    }

    // Parse raw string into Price<Bond> object
    Inquiry<Bond> MakeInquiry(std::string input) {
        // First check if the string is long enough
        if (input.length() < 25) {  
            throw std::runtime_error("Input string too short: " + input);
        }

        try {
            std::string inquiryId = input.substr(0, 6);
            std::string cusip = input.substr(8, 9);
            Side side = (Side)stoi(input.substr(19, 1));
            long quantity = stoi(input.substr(22, 3));
            if (bondMap.find(cusip) == bondMap.end()) {
                throw std::runtime_error("Unknown CUSIP: " + cusip);
            }
            return Inquiry<Bond>(inquiryId, bondMap.find(cusip)->second, side, quantity, 0.0, InquiryState::RECEIVED);
        }
        catch (const std::out_of_range& e) {
            throw std::runtime_error("Invalid string format: " + input);
        }
        catch (const std::invalid_argument& e) {
            throw std::runtime_error("Invalid number format in: " + input);
        }
    }

    ~InquirySocketReaderConnector() {
        Stop();
    }
};

#endif  // INQUIRYSOCKETREADERCONNECTOR_HPP