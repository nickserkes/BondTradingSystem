#ifndef SOCKETREADERCONNECTOR_HPP
#define SOCKETREADERCONNECTOR_HPP

#include <iostream>
#include <thread>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "soa.hpp"

template<typename V>
class SocketReaderConnector : public Connector<V> {
private:
    int socketFd;
    Service<std::string, V>* targetService;

public:
    SocketReaderConnector(int port, Service<std::string, V>* service)
        : targetService(service) {
        // Create socket
        socketFd = socket(AF_INET, SOCK_STREAM, 0);
        if (socketFd < 0) {
            perror("Socket creation failed");
            exit(1);
        }

        // Bind to the port
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        if (::bind(socketFd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            perror("Bind failed");
            exit(1);
        }

        // Listen for connections
        if (listen(socketFd, 5) < 0) {
            perror("Listen failed");
            exit(1);
        }
    }

    // Override the Publish method to handle received data
    void Publish(V& data) override {
        // Directly pass the data to the service
        targetService->OnMessage(data);
    }

    // Start listening for incoming connections and forward data to Publish
    void StartListening() {
        std::thread([this]() {
            while (true) {
                sockaddr_in clientAddr{};
                socklen_t clientLen = sizeof(clientAddr);
                int clientFd = accept(socketFd, (sockaddr*)&clientAddr, &clientLen);
                if (clientFd < 0) {
                    perror("Accept failed");
                    continue;
                }

                char buffer[1024] = {0};
                int bytesRead = read(clientFd, buffer, sizeof(buffer) - 1);
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0'; // Null-terminate the string
                    V data = std::stod(buffer); // Simple deserialization example
                    Publish(data); // Use Publish to send the data to the service
                }
                close(clientFd);
            }
        }).detach();
    }

    ~SocketReaderConnector() {
        close(socketFd);
    }
};

#endif