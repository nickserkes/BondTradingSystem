#include "inquiryservice.hpp"
#include "products.hpp"
#include "soa.hpp"
#include "inquirysocketreaderconnector.hpp"

#ifndef BONDINQUIRYSERVICE_HPP
#define BONDINQUIRYSERVICE_HPP

class BondInquiryService : public InquiryService<Bond> {

private:
    std::map<std::string, Inquiry<Bond>*> inquiries;
    std::vector<ServiceListener<Inquiry<Bond>>*> listeners;
    InquirySocketReaderConnector* clientConnector;

public:
    BondInquiryService() {}

    void OnMessage(Inquiry<Bond>& data) override {
       inquiries[data.GetInquiryId()] = &data;
        for (auto listener : listeners) {
            listener->ProcessAdd(data);
        }
        //ADD IN THE PRICE AND SEND BACCK TO CONNECTOR
        if (data.GetState() == InquiryState::RECEIVED) {
            SendQuote(data.GetInquiryId(), 100.0);
        }
    }   

    void SendQuote(const string &inquiryId, double price) override {
        Inquiry<Bond>& inquiry = *inquiries[inquiryId];
        inquiry.SetPrice(price);
        clientConnector->ReceiveQuote(inquiry);
    }

    void RejectInquiry(const string &inquiryId) override {}

    Inquiry<Bond>& GetData(string key) override {
        if (inquiries.find(key) != inquiries.end()) {
            return *inquiries[key];
        }
        else {
            throw std::invalid_argument("Key not found");
        }
    }

    void AddClientConnector(InquirySocketReaderConnector* connector) {
        clientConnector = connector;
    }

    void AddListener(ServiceListener<Inquiry<Bond>>* listener) override {
        listeners.push_back(listener);
    }

    const std::vector<ServiceListener<Inquiry<Bond>>*>& GetListeners() const override {
        return listeners;
    }

    ~BondInquiryService() {
        inquiries.clear();
    }
};

#endif