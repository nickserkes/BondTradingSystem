#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>

#ifndef FILEREADERCONNECTOR_HPP
#define FILEREADERCONNECTOR_HPP

class FileReaderConnector : public Connector<std::string> {
private:
    std::string filename;
    int socketFd;
    std::string targetIp;
    int targetPort;
    bool running;

public:
    FileReaderConnector(const std::string& file, const std::string& ip, int port)
        : filename(file), targetIp(ip), targetPort(port), running(true) {
        // Create socket
        socketFd = socket(AF_INET, SOCK_STREAM, 0);
        if (socketFd < 0) {
            perror("Socket creation failed");
            exit(1);
        }

        // Connect to the target address
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(targetPort);
        inet_pton(AF_INET, targetIp.c_str(), &serverAddr.sin_addr);

        if (connect(socketFd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            perror("Connection failed");
            exit(1);
        }
    }

    void Publish(std::string& data) override {
        std::string message = data + "\n";
        send(socketFd, message.c_str(), message.size(), 0);
    }

    void Stop() {
        running = false;
        // Send a proper shutdown signal before closing
        shutdown(socketFd, SHUT_WR);  // Signal we're done writing
        close(socketFd);
    }

    void ReadFromFileAndPublish() {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return;
        }

        std::string line;
        while (running && std::getline(file, line)) {
            //std::string time_str = line.substr(0,34);
            //std::cerr << "Passing data: " << time_str << std::endl;
            Publish(line);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));  // Slow down for testing
        }
        
        Stop();
        file.close();
    }

    ~FileReaderConnector() {
        close(socketFd);
    }
};

#endif