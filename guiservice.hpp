/**
 * guiservice.hpp
 * Service for throttled GUI price updates.
 */
#ifndef GUI_SERVICE_HPP
#define GUI_SERVICE_HPP

#include <chrono>
#include <thread>
#include <fstream>
#include "soa.hpp"
#include "products.hpp"
#include "pricingservice.hpp"
#include "bondpricingservice.hpp"
#include <iomanip>
#include "helperfunction.hpp"
using namespace std;

class GUIService;  // Forward declaration

class GUIServiceListener : public ServiceListener<Price<Bond>> {
private:
    Service<string, Price<Bond>>* service;

public:
    // Simplified constructor
    GUIServiceListener(Service<string, Price<Bond>>& guiService) : service(&guiService) {}

    // Simplified ProcessAdd - just forwards to service
    void ProcessAdd(Price<Bond>& data) override {
        service->OnMessage(data);
    }

    // Process a remove event to the Service
    void ProcessRemove(Price<Bond>& data) override {}

    // Process an update event to the Service
    void ProcessUpdate(Price<Bond>& data) override {}

    ~GUIServiceListener() {}
};

/**
 * GUI Service to throttle and display price updates.
 * Keyed on product identifier.
 */
class GUIService : public Service<string, Price<Bond>> {
private:
    map<string, Price<Bond>*> prices;
    vector<ServiceListener<Price<Bond>>*> listeners;
    GUIServiceListener* listener;
    
    // Move throttling-related members here
    chrono::system_clock::time_point lastUpdate;
    const chrono::milliseconds throttleInterval;
    int updateCount;
    const int maxUpdates;
    ofstream outFile;

public:
    // Constructor with initialization of throttling members
    GUIService(BondPricingService* bondPricingService) :
        lastUpdate(chrono::system_clock::now()),
        throttleInterval(30),
        updateCount(0),
        maxUpdates(100) {
        listener = new GUIServiceListener(*this);
        bondPricingService->AddListener(listener);
        outFile.open("gui.txt");
    }

    // Get data on our service given a key
    Price<Bond>& GetData(string key) override {
        return *prices[key];
    }

    // The callback that a Connector should invoke for any new or updated data
    void OnMessage(Price<Bond>& data) override {
        auto now = chrono::system_clock::now();
        string productId = data.GetProduct().GetProductId();
        prices[productId] = &data;
        if (updateCount >= maxUpdates) return;
        
        if (now - lastUpdate >= throttleInterval) {
            

            // Get current time with millisecond precision
            auto time_now = chrono::system_clock::to_time_t(now);
            auto ms_part = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
            outFile << "Timestamp: " << put_time(localtime(&time_now), "%H:%M:%S") 
                   << "." << setfill('0') << setw(3) << ms_part << " | "
                   << "Price Update " << ++updateCount << ": " << endl; 
            for (const auto& [productId, price] : prices) {
                outFile << productId << " "
                    << "Mid: " << convert_to_fractional(price->GetMid()) 
                    << " Spread: " << convert_to_256th(price->GetBidOfferSpread()) << endl;
            }
            
            lastUpdate += chrono::milliseconds(300);
            
            // Notify listeners
            for (auto& l : listeners) {
                l->ProcessAdd(*prices[productId]);
            }
        }
    }

    // Add a listener to the Service
    void AddListener(ServiceListener<Price<Bond>>* listener) override {
        listeners.push_back(listener);
    }

    // Get all listeners
    const vector<ServiceListener<Price<Bond>>*>& GetListeners() const override {
        return listeners;
    }

    // Destructor
    ~GUIService() {
        delete listener;
        if (outFile.is_open()) {
            outFile.close();
        }
    }
};

#endif
