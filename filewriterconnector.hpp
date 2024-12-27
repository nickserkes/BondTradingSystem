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
#include "soa.hpp"

#ifndef FILE_WRITER_CONNECTOR_HPP
#define FILE_WRITER_CONNECTOR_HPP
enum FileWriterConnectorType {POSITIONS, RISK, EXECUTIONS, STREAMING, INQUIRIES};

class FileWriterConnector : public Connector<std::string> {
private:
    std::string filename;
    std::ofstream file;

public:
    FileWriterConnector(const std::string& filename_, FileWriterConnectorType type) : filename(filename_) {
        file.open(filename, std::ios::out | std::ios::app);
        if (type == POSITIONS) {
            file << "Timestamp, CUSIP, Book, Position, [Book], [Position], [Book], [Position], Aggregate, Position" << std::endl;
        }
        else if (type == RISK) {
            file << "Timestamp, CUSIP, PV01, Quantity, PV01*Quantity, Grouping, CombinedRisk (PV01*Quantity)" << std::endl;
        }
        else if (type == EXECUTIONS) {
            file << "Timestamp, CUSIP, Side, OrderID, OrderType, Price, Quantity" << std::endl;
        }
        else if (type == STREAMING) {
            file << "Timestamp, CUSIP, Bid, BidPrice, Quantity, HiddenQuantity, Offer, OfferPrice, Quantity, HiddenQuantity" << std::endl;
        }
        else if (type == INQUIRIES) {
            file << "Timestamp, CUSIP, InquiryId, Side, Quantity, Price, State" << std::endl;
        }
    }

    void Publish(std::string& data) override {
        file << data << std::endl;
    }


    ~FileWriterConnector() {
        file.close();
    }
};

#endif